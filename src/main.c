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
#include <compiz/box.h>

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
    const char		   *name;
    PrivatesCallBackProc   proc;
    void	           *data;
    const CompObjectVTable *vTable;
} ForEachObjectPrivatesContext;

static CompBool
forEachInterfacePrivates (CompObject		    *object,
			  const CompObjectInterface *interface,
			  void			    *closure)
{
    ForEachObjectPrivatesContext *pCtx =
	(ForEachObjectPrivatesContext *) closure;

    if (strcmp (interface->name, pCtx->name) == 0)
    {
	pCtx->vTable = object->vTable;
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

    if (!(*object->vTable->forEachInterface) (object,
					      forEachInterfacePrivates,
					      closure))
    {
	const CompObjectVTable *saveVTable = object->vTable;
	CompBool	       status;

	object->vTable = pCtx->vTable;
	status = (*pCtx->proc) (getPrivates, setPrivates, pCtx->data, object);
	object->vTable = saveVTable;

	if (!status)
	    return FALSE;
    }

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
    CompObject			 *root = &pCtx->root->u.base;

    ctx.name = pCtx->name;
    ctx.proc = proc;
    ctx.data = data;

    return (*root->vTable->forEachChildObject) (root,
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
	if (strcmp (node->base.interface->name, name) == 0)
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
forBranchInterface (CompObject		      *object,
		    const CompObjectInterface *interface,
		    void		      *closure)
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
    compForInterface (object,
		      COMPIZ_BRANCH_TYPE_NAME, forBranchInterface,
		      closure);

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

    forEachBranchTree (&m->root.u.base, (void *) &ctx);
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
    char *error;
    int  i;

    for (i = 0; i < n; i++)
    {
	if (!compFactoryInstallType (factory, types[i], &error))
	{
	    fprintf (stderr, "%s\n"
		     "Failed to register version '%d' of '%s' object type\n",
		     error, types[i]->version, types[i]->name);
	    free (error);
	    return 1;
	}
    }

    return 0;
}

int
main (int argc, char **argv)
{
    char *displayName = 0;
    Bool disableSm = FALSE;
    char *clientId = NULL;
    char *hostName;
    int	 displayNum, i;

    const CompObjectType *staticTypes[] = {
	getObjectType (),
	getDelegateObjectType (),
	getBranchObjectType (),
	getBoxObjectType (),
	getRootObjectType (),
	getCoreObjectType (),
	getDisplayObjectType (),
	getScreenObjectType (),
	getWindowObjectType (),
	getPropObjectType (),
	getBoolPropObjectType (),
	getIntPropObjectType (),
	getDoublePropObjectType (),
	getStringPropObjectType ()
    };

    MainContext context = {
	.factory.allocatePrivateIndex = mainAllocatePrivateIndex,
	.factory.freePrivateIndex     = mainFreePrivateIndex
    };

    ROOT (&context.root);
    OBJECT (&context.root);

    programName = argv[0];
    programArgc = argc;
    programArgv = argv;

    if (registerStaticObjectTypes (&context.factory.base,
				   staticTypes, N_ELEMENTS (staticTypes)))
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
    }

    xmlInitParser ();

    LIBXML_TEST_VERSION;

    if (!compInitMetadata (&coreMetadata))
    {
	compLogMessage (NULL, "core", CompLogLevelFatal,
			"Couldn't initialize core metadata");
	return 1;
    }

    compAddMetadataFromFile (&coreMetadata, "core");

    if (!compObjectInitByType (&context.factory.base, o, getRootObjectType ()))
	return 1;

    if (!compObjectInitByType (&context.factory.base,
			       &core.u.base.u.base,
			       getCoreObjectType ()))
	return 1;

    if (!(*o->vTable->addChild) (o, &core.u.base.u.base, "core"))
	return 1;

    for (i = 1; i < argc; i++)
    {
	CompObject *string;
	char	   *error;

	if (*argv[i] == '-')
	    continue;

	string =
	    (*core.u.base.u.vTable->newObject) (&core.u.base,
						&core.u.base.data.plugins,
						getStringPropObjectType (),
						NULL, &error);
	if (!string)
	{
	    fprintf (stderr, "%s\n", error);
	    free (error);
	    continue;
	}

	(*string->vTable->setString) (string,
				      getStringPropObjectType ()->name,
				      "value",
				      argv[i],
				      NULL);
    }

    (*r->u.vTable->processSignals) (r);

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

    eventLoop (r);

    if (!disableSm)
	closeSession ();

    (*o->vTable->finalize) (o);

    xmlCleanupParser ();

    if (restartSignal)
    {
	execvp (programName, programArgv);
	return 1;
    }

    return 0;
}
