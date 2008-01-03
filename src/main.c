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
#include <compiz/prop.h>

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

static CompPrivate *
getPrivates (void *closure)
{
    CompObject  *object = (CompObject *) closure;
    CompPrivate *privates;

    (*object->vTable->getProp) (object, COMP_PROP_PRIVATES,
				(void *) &privates);

    return privates;
}

static void
setPrivates (void	 *closure,
	     CompPrivate *privates)
{
    CompObject *object = (CompObject *) closure;

    (*object->vTable->setProp) (object, COMP_PROP_PRIVATES,
				(void *) &privates);
}

typedef struct _ForEachObjectPrivatesContext {
    const char		 *name;
    PrivatesCallBackProc proc;
    void	         *data;
    CompBool		 status;
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

    if (type && strcmp (type->name.name, pCtx->name) == 0)
    {
	pCtx->status = (*pCtx->proc) (getPrivates, setPrivates, pCtx->data,
				      object);
	return FALSE;
    }

    return TRUE;
}

static CompBool
forEachObjectPrivatesTree (CompObject *object,
			   void	      *closure)
{
    ForEachObjectPrivatesContext *pCtx =
	(ForEachObjectPrivatesContext *) closure;

    (*object->vTable->forEachInterface) (object,
					 forEachInterfacePrivates,
					 closure);

    if (!pCtx->status)
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

    ctx.name   = pCtx->name;
    ctx.proc   = proc;
    ctx.data   = data;
    ctx.status = TRUE;

    (*core->vTable->forEachInterface) (core,
				       forEachInterfacePrivates,
				       (void *) &ctx);

    if (!ctx.status)
	return FALSE;

    return (*core->vTable->forEachChildObject) (core,
						forEachObjectPrivatesTree,
						(void *) &ctx);
}

static void
updateFactory (CompObjectFactory  *factory,
	       const char	  *name,
	       CompObjectPrivates *privates)
{
    CompObjectInstantiatorNode *node;

    for (node = factory->instantiators; node; node = node->next)
    {
	if (strcmp (node->type->name.name, name) == 0)
	{
	    node->privates = *privates;
	    break;
	}
    }
}

typedef struct _BranchContext {
    const char	       *name;
    CompObjectPrivates *privates;
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

    updateFactory (&b->factory, pCtx->name, pCtx->privates);

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
mainUpdatePrivatesSize (MainContext	   *m,
			const char	   *name,
			CompObjectPrivates *privates)
{
    BranchContext ctx;

    ctx.name     = name;
    ctx.privates = privates;

    updateFactory (&m->factory.base, name, privates);

    forEachBranchTree (m->root.core, (void *) &ctx);
}

static int
mainAllocatePrivateIndex (CompFactory *factory,
			  const char  *name,
			  int         size)
{
    MainContext		   *m = (MainContext *) factory;
    CompObjectPrivatesNode *node;
    PrivatesContext	   ctx;
    int			   index;

    ctx.root = &m->root;
    ctx.name = name;

    for (node = m->factory.privates; node; node = node->next)
    {
	if (strcmp (node->name, name) == 0)
	{
	    index = allocatePrivateIndex (&node->privates.len,
					  &node->privates.sizes,
					  &node->privates.totalSize,
					  size, forEachObjectPrivates,
					  (void *) &ctx);
	    if (index >= 0)
		mainUpdatePrivatesSize (m, name, &node->privates);

	    return index;
	}
    }

    node = malloc (sizeof (CompObjectPrivatesNode) + strlen (name) + 1);
    if (!node)
	return -1;

    node->next		     = m->factory.privates;
    node->name		     = strcpy ((char *) (node + 1), name);
    node->privates.len	     = 0;
    node->privates.sizes     = NULL;
    node->privates.totalSize = 0;

    m->factory.privates = node;

    index = allocatePrivateIndex (&node->privates.len,
				  &node->privates.sizes,
				  &node->privates.totalSize,
				  size, forEachObjectPrivates,
				  (void *) &ctx);
    if (index >= 0)
	mainUpdatePrivatesSize (m, name, &node->privates);

    return index;
}

static void
mainFreePrivateIndex (CompFactory *factory,
		      const char  *name,
		      int	  index)
{
    MainContext		   *m = (MainContext *) factory;
    CompObjectPrivatesNode *node;
    PrivatesContext	   ctx;

    ctx.root = &m->root;
    ctx.name = name;

    for (node = m->factory.privates; node; node = node->next)
    {
	if (strcmp (node->name, name) == 0)
	{
	    freePrivateIndex (&node->privates.len,
			      &node->privates.sizes,
			      &node->privates.totalSize,
			      forEachObjectPrivates,
			      (void *) &ctx,
			      index);
	    mainUpdatePrivatesSize (m, name, &node->privates);
	}
    }
}

static int
registerStaticObjectTypes (CompObjectFactory	*factory,
			   const CompObjectType **types,
			   int			n)
{
    int i;

    for (i = 0; i < n; i++)
    {
	if (!compFactoryRegisterType (factory, 0, types[i]))
	{
	    fprintf (stderr, "Failed to register '%s' object type\n",
		     types[i]->name.name);
	    return 1;
	}
    }

    return 0;
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

    const CompObjectType **propTypes;
    int			 nPropTypes;

    const CompObjectType *staticTypes[] = {
	getObjectType (),
	getBranchObjectType (),
	getContainerObjectType (),
	getRootObjectType (),
	getCoreObjectType (),
	getDisplayObjectType (),
	getScreenObjectType (),
	getWindowObjectType (),
    };

    MainContext context = {
	.factory.allocatePrivateIndex = mainAllocatePrivateIndex,
	.factory.freePrivateIndex     = mainFreePrivateIndex
    };

    programName = argv[0];
    programArgc = argc;
    programArgv = argv;

    propTypes = getPropObjectTypes (&nPropTypes);

    if (registerStaticObjectTypes (&context.factory.base,
				   staticTypes, N_ELEMENTS (staticTypes)))
	return 1;

    if (registerStaticObjectTypes (&context.factory.base,
				   propTypes, nPropTypes))
	return 1;

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

    if (!compObjectInitByType (&context.factory.base,
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

    (*context.root.u.base.base.vTable->finalize) (&context.root.u.base.base);

    xmlCleanupParser ();

    if (restartSignal)
    {
	execvp (programName, programArgv);
	return 1;
    }

    return 0;
}
