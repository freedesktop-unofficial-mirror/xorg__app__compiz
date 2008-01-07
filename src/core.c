/*
 * Copyright © 2007 Novell, Inc.
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

#include <string.h>
#include <sys/stat.h>

#include <compiz/core.h>
#include <compiz/c-object.h>
#include <compiz/marshal.h>
#include <compiz/error.h>

CompCore core;

static const CMethod coreTypeMethod[] = {
    C_METHOD (addDisplay,    "si", "", CompCoreVTable, marshal__SI__E),
    C_METHOD (removeDisplay, "si", "", CompCoreVTable, marshal__SI__E)
};

static CChildObject coreTypeChildObject[] = {
    C_CHILD (displays, CompCoreData, CONTAINER_TYPE_NAME),
    C_CHILD (plugins, CompCoreData, CONTAINER_TYPE_NAME),
    C_CHILD (inputs, CompCoreData, CONTAINER_TYPE_NAME),
    C_CHILD (outputs, CompCoreData, CONTAINER_TYPE_NAME)
};

static CInterface coreInterface[] = {
    C_INTERFACE (core, Type, CompObjectVTable, _, X, _, _, _, _, _, X)
};

static void
coreGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    static const CMetadata template = {
	.interface  = coreInterface,
	.nInterface = N_ELEMENTS (coreInterface),
	.version    = COMPIZ_CORE_VERSION
    };

    cGetObjectProp (&GET_CORE (object)->data.base, &template, what, value);
}

static CompBool
noopAddDisplay (CompCore   *c,
		const char *hostName,
		int32_t	   displayNum,
		char	   **error)
{
    CompBool status;

    FOR_BASE (&c->u.base.u.base,
	      status = (*c->u.vTable->addDisplay) (c,
						   hostName,
						   displayNum,
						   error));

    return status;
}

static CompBool
addDisplay (CompCore   *c,
	    const char *hostName,
	    int32_t    displayNum,
	    char       **error)
{
    CompDisplay *d;

    for (d = c->displays; d; d = d->next)
    {
	if (d->displayNum == displayNum && strcmp (d->hostName, hostName) == 0)
	{
	    esprintf (error,
		      "Already connected to display %d on host \"%s\"",
		      displayNum, hostName);

	    return FALSE;
	}
    }

    if (!addDisplayOld (c, hostName, displayNum))
    {
	esprintf (error, "Failed to add display %d on host %s",
		  displayNum, hostName);

	return FALSE;
    }

    return TRUE;
}

static CompBool
noopRemoveDisplay (CompCore   *c,
		   const char *hostName,
		   int32_t    displayNum,
		   char	      **error)
{
    CompBool status;

    FOR_BASE (&c->u.base.u.base,
	      status = (*c->u.vTable->removeDisplay) (c,
						      hostName,
						      displayNum,
						      error));

    return status;
}

static CompBool
removeDisplay (CompCore   *c,
	       const char *hostName,
	       int32_t    displayNum,
	       char       **error)
{
    CompDisplay *d;

    for (d = c->displays; d; d = d->next)
	if (d->displayNum == displayNum && strcmp (d->hostName, hostName) == 0)
	    break;

    if (!d)
    {
	esprintf (error,
		  "No connection to display %d on host \"%s\" present",
		  displayNum, hostName);

	return FALSE;
    }

    removeDisplayOld (c, d);

    return TRUE;
}

static CompBool
initCorePluginForObject (CompPlugin *p,
			 CompObject *o)
{
    return TRUE;
}

static void
finiCorePluginForObject (CompPlugin *p,
			 CompObject *o)
{
}

static CompBool
setOptionForPlugin (CompObject      *object,
		    const char	    *plugin,
		    const char	    *name,
		    CompOptionValue *value)
{
    CompPlugin *p;

    p = findActivePlugin (plugin);
    if (p && p->vTable->setObjectOption)
	return (*p->vTable->setObjectOption) (p, object, name, value);

    return FALSE;
}

static void
coreObjectAdd (CompObject *object,
	       CompObject *child,
	       const char *name)
{
}

static void
coreObjectRemove (CompObject *object,
		  CompObject *child)
{
}

static void
fileWatchAdded (CompCore      *core,
		CompFileWatch *fileWatch)
{
}

static void
fileWatchRemoved (CompCore      *core,
		  CompFileWatch *fileWatch)
{
}

static CompCoreVTable coreObjectVTable = {
    .base.base.getProp = coreGetProp,
    .addDisplay	       = addDisplay,
    .removeDisplay     = removeDisplay
};

static CompBool
coreInitObject (const CompObjectInstantiator *instantiator,
		CompObject		     *object,
		const CompObjectFactory      *factory)
{
    CORE (object);

    if (!cObjectInit (instantiator, object, factory))
	return FALSE;

    c->u.base.u.base.id = COMP_OBJECT_TYPE_CORE; /* XXX: remove id asap */

    c->tmpRegion = XCreateRegion ();
    if (!c->tmpRegion)
    {
	cObjectFini (object);
	return FALSE;
    }

    c->outputRegion = XCreateRegion ();
    if (!c->outputRegion)
    {
	XDestroyRegion (c->tmpRegion);
	cObjectFini (object);
	return FALSE;
    }

    compInitOptionValue (&c->plugin);

    c->plugin.list.type     = CompOptionTypeString;
    c->plugin.list.nValue   = 1;
    c->plugin.list.value    = malloc (sizeof (CompOptionValue));
    c->plugin.list.value->s = strdup ("core");

    c->dirtyPluginList = TRUE;

    c->displays = NULL;
    c->plugins  = NULL;

    c->fileWatch	   = NULL;
    c->lastFileWatchHandle = 1;

    c->timeouts		 = NULL;
    c->lastTimeoutHandle = 1;

    c->watchFds	         = NULL;
    c->lastWatchFdHandle = 1;
    c->watchPollFds	 = NULL;
    c->nWatchFds	 = 0;

    gettimeofday (&c->lastTimeout, 0);

    c->initPluginForObject = initCorePluginForObject;
    c->finiPluginForObject = finiCorePluginForObject;

    c->setOptionForPlugin = setOptionForPlugin;

    c->objectAdd    = coreObjectAdd;
    c->objectRemove = coreObjectRemove;

    c->fileWatchAdded   = fileWatchAdded;
    c->fileWatchRemoved = fileWatchRemoved;

    c->sessionInit  = sessionInit;
    c->sessionFini  = sessionFini;
    c->sessionEvent = sessionEvent;

    return TRUE;
}

static void
coreFinalize (CompObject *object)
{
    CORE (object);

    compFiniOptionValue (&c->plugin, CompOptionTypeList);

    XDestroyRegion (c->outputRegion);
    XDestroyRegion (c->tmpRegion);

    cObjectFini (object);
}

static const CompCoreVTable noopCoreObjectVTable = {
    .base.base.finalize = coreFinalize,

    .addDisplay    = noopAddDisplay,
    .removeDisplay = noopRemoveDisplay
};

const CompObjectType *
getCoreObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CompObjectType template = {
	    .name.name     = CORE_TYPE_NAME,
	    .name.base     = BRANCH_TYPE_NAME,
	    .vTable.impl   = &coreObjectVTable.base.base,
	    .vTable.noop   = &noopCoreObjectVTable.base.base,
	    .vTable.size   = sizeof (coreObjectVTable),
	    .instance.init = coreInitObject
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

int
allocateCorePrivateIndex (void)
{
    return compObjectAllocatePrivateIndex (getCoreObjectType (), 0);
}

void
freeCorePrivateIndex (int index)
{
    compObjectFreePrivateIndex (getCoreObjectType (), index);
}

CompBool
initCore (const CompObjectFactory *factory,
	  CompObject	          *parent)
{
    CompPlugin *corePlugin;

    if (!compObjectInitByType (factory, &core.u.base.u.base,
			       getCoreObjectType ()))
	return FALSE;

    (*core.u.base.u.base.vTable->insertObject) (&core.u.base.u.base, parent,
    CORE_TYPE_NAME);

    corePlugin = loadPlugin ("core");
    if (!corePlugin)
    {
	compLogMessage (0, "core", CompLogLevelFatal,
			"Couldn't load core plugin");

	(*core.u.base.u.base.vTable->finalize) (&core.u.base.u.base);
	return FALSE;
    }

    if (!pushPlugin (corePlugin, &core.u.base))
    {
	compLogMessage (0, "core", CompLogLevelFatal,
			"Couldn't activate core plugin");
	unloadPlugin (corePlugin);
	(*core.u.base.u.base.vTable->finalize) (&core.u.base.u.base);
	return FALSE;
    }

    return TRUE;
}

void
finiCore (const CompObjectFactory *factory,
	  CompObject	          *parent)
{
    while (core.displays)
	(*core.u.vTable->removeDisplay) (&core,
					 core.displays->hostName,
					 core.displays->displayNum,
					 NULL);

    while (popPlugin (&core.u.base));

    (*core.u.base.u.base.vTable->removeObject) (&core.u.base.u.base);
    (*core.u.base.u.base.vTable->finalize) (&core.u.base.u.base);
}

void
addDisplayToCore (CompCore    *c,
		  CompDisplay *d)
{
    CompDisplay *prev;

    for (prev = c->displays; prev && prev->next; prev = prev->next);

    if (prev)
	prev->next = d;
    else
	c->displays = d;
}

CompFileWatchHandle
addFileWatch (const char	    *path,
	      int		    mask,
	      FileWatchCallBackProc callBack,
	      void		    *closure)
{
    CompFileWatch *fileWatch;

    fileWatch = malloc (sizeof (CompFileWatch));
    if (!fileWatch)
	return 0;

    fileWatch->path	= strdup (path);
    fileWatch->mask	= mask;
    fileWatch->callBack = callBack;
    fileWatch->closure  = closure;
    fileWatch->handle   = core.lastFileWatchHandle++;

    if (core.lastFileWatchHandle == MAXSHORT)
	core.lastFileWatchHandle = 1;

    fileWatch->next = core.fileWatch;
    core.fileWatch = fileWatch;

    (*core.fileWatchAdded) (&core, fileWatch);

    return fileWatch->handle;
}

void
removeFileWatch (CompFileWatchHandle handle)
{
    CompFileWatch *p = 0, *w;

    for (w = core.fileWatch; w; w = w->next)
    {
	if (w->handle == handle)
	    break;

	p = w;
    }

    if (w)
    {
	if (p)
	    p->next = w->next;
	else
	    core.fileWatch = w->next;

	(*core.fileWatchRemoved) (&core, w);

	if (w->path)
	    free (w->path);

	free (w);
    }
}

int
compObjectAllocatePrivateIndex (const CompObjectType *type,
				int	             size)
{
    const CompObjectFactory *f;
    CompFactory		    *factory;

    for (f = &core.u.base.factory; f->master; f = f->master);
    factory = (CompFactory *) f;

    return (*factory->allocatePrivateIndex) (factory, type->name.name, size);
}

void
compObjectFreePrivateIndex (const CompObjectType *type,
			    int	                 index)
{
    const CompObjectFactory *f;
    CompFactory		    *factory;

    for (f = &core.u.base.factory; f->master; f = f->master);
    factory = (CompFactory *) f;

    return (*factory->freePrivateIndex) (factory, type->name.name, index);
}
