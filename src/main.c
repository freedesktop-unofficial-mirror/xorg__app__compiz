/*
 * Copyright © 2005 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <compiz/core.h>
#include <compiz/root.h>

typedef struct _MainContext {
    CompFactory factory;
    CompRoot    root;
} MainContext;

char *programName;
char **programArgv;
int  programArgc;

char *backgroundImage = NULL;

REGION   emptyRegion;
REGION   infiniteRegion;
GLushort defaultColor[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
Window   currentRoot = 0;

int  defaultRefreshRate = 50;
char *defaultTextureFilter = "Good";

Bool shutDown = FALSE;
Bool restartSignal = FALSE;

CompWindow *lastFoundWindow = 0;
CompWindow *lastDamagedWindow = 0;

Bool replaceCurrentWm = FALSE;
Bool indirectRendering = FALSE;
Bool strictBinding = TRUE;
Bool noDetection = FALSE;
Bool useDesktopHints = TRUE;
Bool onlyCurrentScreen = FALSE;

#ifdef USE_COW
Bool useCow = TRUE;
#endif

CompMetadata coreMetadata;

static void
usage (void)
{
    printf ("Usage: %s "
	    "[--display DISPLAY] "
	    "[--bg-image PNG] "
	    "[--refresh-rate RATE]\n       "
	    "[--fast-filter] "
	    "[--indirect-rendering] "
	    "[--loose-binding] "
	    "[--replace]\n       "
	    "[--sm-disable] "
	    "[--sm-client-id ID] "
	    "[--no-detection]\n       "
	    "[--ignore-desktop-hints] "
	    "[--only-current-screen]"

#ifdef USE_COW
	    " [--use-root-window]\n       "
#else
	    "\n       "
#endif

	    "[--version] "
	    "[--help] "
	    "[PLUGIN]...\n",
	    programName);
}

void
compLogMessage (CompDisplay *d,
		const char   *componentName,
		CompLogLevel level,
		const char   *format,
		...)
{
    va_list args;
    char    message[2048];

    va_start (args, format);

    vsnprintf (message, 2048, format, args);

    if (d)
	(*d->logMessage) (d, componentName, level, message);
    else
	logMessage (d, componentName, level, message);

    va_end (args);
}

void
logMessage (CompDisplay	 *d,
	    const char	 *componentName,
	    CompLogLevel level,
	    const char	 *message)
{
    fprintf (stderr, "%s (%s) - %s: %s\n",
	      programName, componentName,
	      logLevelToString (level), message);
}

const char *
logLevelToString (CompLogLevel level)
{
    switch (level) {
    case CompLogLevelFatal:
	return "Fatal";
    case CompLogLevelError:
	return "Error";
    case CompLogLevelWarn:
	return "Warn";
    case CompLogLevelInfo:
	return "Info";
    case CompLogLevelDebug:
	return "Debug";
    default:
	break;
    }

    return "Unknown";
}

static void
signalHandler (int sig)
{
    int status;

    switch (sig) {
    case SIGCHLD:
	waitpid (-1, &status, WNOHANG | WUNTRACED);
	break;
    case SIGHUP:
	restartSignal = TRUE;
	break;
    case SIGINT:
    case SIGTERM:
	shutDown = TRUE;
    default:
	break;
    }
}

typedef struct _CompIOCtx {
    int	 offset;
    char *pluginData;
} CompIOCtx;

static int
readCoreXmlCallback (void *context,
		     char *buffer,
		     int  length)
{
    CompIOCtx *ctx = (CompIOCtx *) context;
    int	      offset = ctx->offset;
    int	      i, j;

    i = compReadXmlChunk ("<compiz><core><display>", &offset, buffer, length);

    for (j = 0; j < COMP_DISPLAY_OPTION_NUM; j++)
    {
	CompMetadataOptionInfo info = coreDisplayOptionInfo[j];

	switch (j) {
	case COMP_DISPLAY_OPTION_ACTIVE_PLUGINS:
	    if (ctx->pluginData)
		info.data = ctx->pluginData;
	default:
	    break;
	}

	i += compReadXmlChunkFromMetadataOptionInfo (&info,
						     &offset,
						     buffer + i,
						     length - i);
    }

    i += compReadXmlChunk ("</display><screen>", &offset,
			   buffer + i, length - 1);

    for (j = 0; j < COMP_SCREEN_OPTION_NUM; j++)
    {
	CompMetadataOptionInfo info = coreScreenOptionInfo[j];

	i += compReadXmlChunkFromMetadataOptionInfo (&info,
						     &offset,
						     buffer + i,
						     length - i);
    }

    i += compReadXmlChunk ("</screen></core></compiz>", &offset, buffer + i,
			   length - i);

    if (!offset && length > i)
	buffer[i++] = '\0';

    ctx->offset += i;

    return i;
}

typedef struct _ForEachObjectPrivatesContext {
    const char		 *name;
    PrivatesCallBackProc proc;
    void	         *data;
} ForEachObjectPrivatesContext;

static CompBool
forEachInterfacePrivates (CompObject	       *object,
			  const char	       *name,
			  size_t	       offset,
			  const CompObjectType *type,
			  void		       *closure)
{
    ForEachObjectPrivatesContext *pCtx =
	(ForEachObjectPrivatesContext *) closure;

    if (type && strcmp (type->name, pCtx->name) == 0)
    {
	CompPrivate **pPrivates = (CompPrivate **)
	    (((char *) object) + type->privatesOffset);

	return (*pCtx->proc) (pPrivates, pCtx->data);
    }

    return TRUE;
}

static CompBool
forEachObjectPrivatesTree (CompObject *object,
			   void	      *closure)
{
    if (!(*object->vTable->forEachInterface) (object,
					      forEachInterfacePrivates,
					      closure))
	return FALSE;

    return (*object->vTable->forEachChildObject) (object,
						  forEachObjectPrivatesTree,
						  closure);
}

typedef struct _PrivatesContext {
    const char *name;
    CompRoot   *root;
} PrivatesContext;

static CompBool
forEachObjectPrivates (PrivatesCallBackProc proc,
		       void		    *data,
		       void		    *closure)
{
    ForEachObjectPrivatesContext ctx;
    PrivatesContext		 *pCtx = (PrivatesContext *) closure;
    CompObject			 *core = pCtx->root->core;

    ctx.name = pCtx->name;
    ctx.proc = proc;
    ctx.data = data;

    if (!(*core->vTable->forEachInterface) (core,
					    forEachInterfacePrivates,
					    (void *) &ctx))
	return FALSE;

    return (*core->vTable->forEachChildObject) (core,
						forEachObjectPrivatesTree,
						(void *) &ctx);
}

static void
updateFactory (CompObjectFactory      *factory,
	       const char	      *name,
	       CompObjectPrivatesSize *size)
{
    int	i;

    for (i = 0; i < factory->nInstantiator; i++)
    {
	if (strcmp (factory->instantiator[i].type->name, name) == 0)
	{
	    factory->instantiator[i].size = *size;
	    break;
	}
    }
}

typedef struct _BranchContext {
    const char		   *name;
    CompObjectPrivatesSize *size;
} BranchContext;

static CompBool
forBranchInterface (CompObject		 *object,
		    const char		 *name,
		    size_t		 offset,
		    const CompObjectType *type,
		    void		 *closure)
{
    BranchContext *pCtx = (BranchContext *) closure;

    BRANCH (object);

    updateFactory (&b->factory, pCtx->name, pCtx->size);

    return TRUE;
}

static CompBool
forEachBranchTree (CompObject *object,
		   void       *closure)
{
    compForInterface (object, BRANCH_TYPE_NAME, forBranchInterface, closure);
    return (*object->vTable->forEachChildObject) (object,
						  forEachBranchTree,
						  closure);
}

static void
mainUpdatePrivatesSize (MainContext	       *m,
			const char	       *name,
			CompObjectPrivatesSize *size)
{
    BranchContext ctx;

    ctx.name = name;
    ctx.size = size;

    updateFactory (&m->factory.base, name, size);

    forEachBranchTree (m->root.core, (void *) &ctx);
}

static int
mainAllocatePrivateIndex (CompFactory *factory,
			  const char  *name,
			  int         size)
{
    MainContext		  *m = (MainContext *) factory;
    CompPrivatesSizeEntry *entry;
    PrivatesContext	  ctx;
    int			  index, i;

    ctx.root = &m->root;
    ctx.name = name;

    for (i = 0; i < m->factory.nEntry; i++)
    {
	if (strcmp (m->factory.entry[i].name, name) == 0)
	{
	    index = allocatePrivateIndex (&m->factory.entry[i].size.len,
					  &m->factory.entry[i].size.sizes,
					  &m->factory.entry[i].size.totalSize,
					  size, forEachObjectPrivates,
					  (void *) &ctx);
	    if (index >=0)
		mainUpdatePrivatesSize (m, name, &m->factory.entry[i].size);

	    return index;
	}
    }

    entry = realloc (m->factory.entry, sizeof (CompPrivatesSizeEntry) *
		     (m->factory.nEntry + 1));
    if (!entry)
	return -1;

    m->factory.entry = entry;

    entry[m->factory.nEntry].name = strdup (name);
    if (!entry[m->factory.nEntry].name)
	return -1;

    entry[m->factory.nEntry].size.len	    = 0;
    entry[m->factory.nEntry].size.sizes     = NULL;
    entry[m->factory.nEntry].size.totalSize = 0;

    i = m->factory.nEntry++;

    index = allocatePrivateIndex (&m->factory.entry[i].size.len,
				  &m->factory.entry[i].size.sizes,
				  &m->factory.entry[i].size.totalSize,
				  size, forEachObjectPrivates,
				  (void *) &ctx);
    if (index >= 0)
	mainUpdatePrivatesSize (m, name, &m->factory.entry[i].size);

    return index;
}

static void
mainFreePrivateIndex (CompFactory *factory,
		      const char  *name,
		      int	  index)
{
    MainContext     *m = (MainContext *) factory;
    PrivatesContext ctx;
    int		    i;

    ctx.root = &m->root;
    ctx.name = name;

    for (i = 0; i < m->factory.nEntry; i++)
    {
	if (strcmp (m->factory.entry[i].name, name) == 0)
	{
	    freePrivateIndex (&m->factory.entry[i].size.len,
			      &m->factory.entry[i].size.sizes,
			      &m->factory.entry[i].size.totalSize,
			      forEachObjectPrivates,
			      (void *) &ctx,
			      index);
	    mainUpdatePrivatesSize (m, name, &m->factory.entry[i].size);
	}
    }
}

int
main (int argc, char **argv)
{
    CompIOCtx ctx;
    char      *displayName = 0;
    char      *plugin[256];
    int	      i, nPlugin = 0;
    Bool      disableSm = FALSE;
    char      *clientId = NULL;
    char      *hostName;
    int	      displayNum;

    CompObjectInstantiator instantiator[] = {
	{ .type = getObjectType ()				      },
	{ .type = getBranchObjectType (),    .base = &instantiator[0] },
	{ .type = getContainerObjectType (), .base = &instantiator[0] },
	{ .type = getRootObjectType (),      .base = &instantiator[0] },
	{ .type = getCoreObjectType (),      .base = &instantiator[1] },
	{ .type = getDisplayObjectType (),   .base = &instantiator[0] },
	{ .type = getScreenObjectType (),    .base = &instantiator[0] },
	{ .type = getWindowObjectType (),    .base = &instantiator[0] }
    };

    MainContext context = {
	.factory.base.instantiator    = instantiator,
	.factory.base.nInstantiator   = N_ELEMENTS (instantiator),
	.factory.allocatePrivateIndex = mainAllocatePrivateIndex,
	.factory.freePrivateIndex     = mainFreePrivateIndex
    };

    programName = argv[0];
    programArgc = argc;
    programArgv = argv;

    signal (SIGHUP, signalHandler);
    signal (SIGCHLD, signalHandler);
    signal (SIGINT, signalHandler);
    signal (SIGTERM, signalHandler);

    emptyRegion.rects = &emptyRegion.extents;
    emptyRegion.numRects = 0;
    emptyRegion.extents.x1 = 0;
    emptyRegion.extents.y1 = 0;
    emptyRegion.extents.x2 = 0;
    emptyRegion.extents.y2 = 0;
    emptyRegion.size = 0;

    infiniteRegion.rects = &infiniteRegion.extents;
    infiniteRegion.numRects = 1;
    infiniteRegion.extents.x1 = MINSHORT;
    infiniteRegion.extents.y1 = MINSHORT;
    infiniteRegion.extents.x2 = MAXSHORT;
    infiniteRegion.extents.y2 = MAXSHORT;

    memset (&ctx, 0, sizeof (ctx));

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "--help"))
	{
	    usage ();
	    return 0;
	}
	else if (!strcmp (argv[i], "--version"))
	{
	    printf (PACKAGE_STRING "\n");
	    return 0;
	}
	else if (!strcmp (argv[i], "--display"))
	{
	    if (i + 1 < argc)
		displayName = argv[++i];
	}
	else if (!strcmp (argv[i], "--indirect-rendering"))
	{
	    indirectRendering = TRUE;
	}
	else if (!strcmp (argv[i], "--loose-binding"))
	{
	    strictBinding = FALSE;
	}
	else if (!strcmp (argv[i], "--ignore-desktop-hints"))
	{
	    useDesktopHints = FALSE;
	}
	else if (!strcmp (argv[i], "--only-current-screen"))
	{
	    onlyCurrentScreen = TRUE;
	}

#ifdef USE_COW
	else if (!strcmp (argv[i], "--use-root-window"))
	{
	    useCow = FALSE;
	}
#endif

	else if (!strcmp (argv[i], "--replace"))
	{
	    replaceCurrentWm = TRUE;
	}
	else if (!strcmp (argv[i], "--sm-disable"))
	{
	    disableSm = TRUE;
	}
	else if (!strcmp (argv[i], "--sm-client-id"))
	{
	    if (i + 1 < argc)
		clientId = argv[++i];
	}
	else if (!strcmp (argv[i], "--no-detection"))
	{
	    noDetection = TRUE;
	}
	else if (!strcmp (argv[i], "--bg-image"))
	{
	    if (i + 1 < argc)
		backgroundImage = argv[++i];
	}
	else if (*argv[i] == '-')
	{
	    compLogMessage (NULL, "core", CompLogLevelWarn,
			    "Unknown option '%s'\n", argv[i]);
	}
	else
	{
	    if (nPlugin < 256)
		plugin[nPlugin++] = argv[i];
	}
    }

    if (nPlugin)
    {
	int size = 256;

	for (i = 0; i < nPlugin; i++)
	    size += strlen (plugin[i]) + 16;

	ctx.pluginData = malloc (size);
	if (ctx.pluginData)
	{
	    char *ptr = ctx.pluginData;

	    ptr += sprintf (ptr, "<type>string</type><default>");

	    for (i = 0; i < nPlugin; i++)
		ptr += sprintf (ptr, "<value>%s</value>", plugin[i]);

	    ptr += sprintf (ptr, "</default>");
	}
    }

    xmlInitParser ();

    LIBXML_TEST_VERSION;

    if (!compInitMetadata (&coreMetadata))
    {
	compLogMessage (NULL, "core", CompLogLevelFatal,
			"Couldn't initialize core metadata");
	return 1;
    }

    if (!compAddMetadataFromIO (&coreMetadata,
				readCoreXmlCallback, NULL,
				&ctx))
	return 1;

    if (ctx.pluginData)
	free (ctx.pluginData);

    compAddMetadataFromFile (&coreMetadata, "core");

    if (!compObjectInit (&context.factory.base,
			 &context.root.u.base.base,
			 getRootObjectType ()))
	return 1;

    /* XXX: until core object is moved into the root object */
    context.root.core = &core.u.base.u.base;

    if (!initCore (&context.factory.base, &context.root.u.base.base))
	return 1;

    if (!disableSm)
	initSession (clientId);

    if (xcb_parse_display (displayName, &hostName, &displayNum, NULL))
    {
	char *error;

	if (!(*core.u.vTable->addDisplay) (&core,
					   hostName, displayNum,
					   &error))
	{
	    compLogMessage (NULL, "core", CompLogLevelWarn, error);
	    free (error);
	}

	free (hostName);
    }

    eventLoop (&context.root);

    if (!disableSm)
	closeSession ();

    finiCore (&context.factory.base, &context.root.u.base.base);

    compObjectFini (&context.factory.base,
		    &context.root.u.base.base,
		    getRootObjectType ());

    xmlCleanupParser ();

    if (restartSignal)
    {
	execvp (programName, programArgv);
	return 1;
    }

    return 0;
}
