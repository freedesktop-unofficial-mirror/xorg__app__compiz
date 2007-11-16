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
#include <glib.h>

#include <compiz/core.h>

static CompMetadata glibMetadata;

static int corePrivateIndex;

typedef struct _GLibWatch {
    CompWatchFdHandle handle;
    int		      index;
    CompCore	      *core;
} GLibWatch;

typedef struct _GLibCore {
    CompObjectVTableVec object;

    CompTimeoutHandle timeoutHandle;
    gint	      maxPriority;
    GPollFD	      *fds;
    gint	      fdsSize;
    gint	      nFds;
    GLibWatch	      *watch;
} GLibCore;

typedef void (*WakeUpProc) (CompCore *c);

typedef struct _GLibCoreVTable {
    CompCoreVTable base;
    WakeUpProc     wakeUp;
} GLibCoreVTable;

#define GET_GLIB_CORE(c)			       \
    ((GLibCore *) (c)->privates[corePrivateIndex].ptr)

#define GLIB_CORE(c)		     \
    GLibCore *gc = GET_GLIB_CORE (c)


static CommonMethod glibCoreMethod[] = {
    C_METHOD (wakeUp, "", "", GLibCoreVTable, marshal____)
};
#define INTERFACE_VERSION_glibCore 20071011

static const CommonInterface glibCoreInterface[] = {
    C_INTERFACE (glib, Core, GLibCoreVTable, _, _, X, _, _, _, _, _, _)
};

static void
glibDispatch (CompCore     *c,
	      GMainContext *context)
{
    int i;

    GLIB_CORE (c);

    g_main_context_check (context, gc->maxPriority, gc->fds, gc->nFds);
    g_main_context_dispatch (context);

    for (i = 0; i < gc->nFds; i++)
	compRemoveWatchFd (gc->watch[i].handle);
}

static void
glibPrepare (CompCore     *c,
	     GMainContext *context);

static Bool
glibDispatchAndPrepare (void *closure)
{
    CompCore     *c = (CompCore *) closure;
    GMainContext *context = g_main_context_default ();

    glibDispatch (c, context);
    glibPrepare (c, context);

    return FALSE;
}

static void
glibWakeup (CompCore *c)
{
    GLIB_CORE (c);

    if (gc->timeoutHandle)
    {
	compRemoveTimeout (gc->timeoutHandle);
	compAddTimeout (0, glibDispatchAndPrepare, (void *) c);

	gc->timeoutHandle = 0;
    }
}

static Bool
glibCollectEvents (void *closure)
{
    GLibWatch *watch = (GLibWatch *) closure;
    CompCore  *c = watch->core;

    GLIB_CORE (c);

    gc->fds[watch->index].revents |= compWatchFdEvents (watch->handle);

    glibWakeup (c);

    return TRUE;
}

static void
glibPrepare (CompCore     *c,
	     GMainContext *context)
{
    int nFds = 0;
    int timeout = -1;
    int i;

    GLIB_CORE (c);

    g_main_context_prepare (context, &gc->maxPriority);

    do
    {
	if (nFds > gc->fdsSize)
	{
	    if (gc->fds)
		free (gc->fds);

	    gc->fds = malloc ((sizeof (GPollFD) + sizeof (GLibWatch)) * nFds);
	    if (!gc->fds)
	    {
		nFds = 0;
		break;
	    }

	    gc->watch   = (GLibWatch *) (gc->fds + nFds);
	    gc->fdsSize = nFds;
	}

	nFds = g_main_context_query (context,
				     gc->maxPriority,
				     &timeout,
				     gc->fds,
				     gc->fdsSize);
    } while (nFds > gc->fdsSize);

    if (timeout < 0)
	timeout = INT_MAX;

    for (i = 0; i < nFds; i++)
    {
	gc->watch[i].core    = c;
	gc->watch[i].index   = i;
	gc->watch[i].handle  = compAddWatchFd (gc->fds[i].fd,
					       gc->fds[i].events,
					       glibCollectEvents,
					       &gc->watch[i]);
    }

    gc->nFds	      = nFds;
    gc->timeoutHandle =
	compAddTimeout (timeout, glibDispatchAndPrepare, c);
}

static CompBool
glibCoreForBaseObject (CompObject	      *object,
		       BaseObjectCallBackProc proc,
		       void		      *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    GLIB_CORE (GET_CORE (object));

    UNWRAP (&gc->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&gc->object, object, vTable, v.vTable);

    return status;
}

static CompBool
glibCoreForEachInterface (CompObject		*object,
			  InterfaceCallBackProc proc,
			  void			*closure)
{
    return handleForEachInterface (object,
				   glibCoreInterface,
				   N_ELEMENTS (glibCoreInterface),
				   NULL, proc, closure);
}

static GLibCoreVTable glibCoreObjectVTable = {
    .base.base.forBaseObject    = glibCoreForBaseObject,
    .base.base.forEachInterface = glibCoreForEachInterface,
    .base.base.forEachMethod    = commonForEachMethod,
    .base.base.version.get      = commonGetVersion,
    .base.base.metadata.get     = commonGetMetadata,
    .wakeUp			= glibWakeup
};

static CompBool
glibInitCore (CompCore *c)
{
    GLIB_CORE (c);

    if (!compObjectCheckVersion (&c->u.base, "object", CORE_ABIVERSION))
	return FALSE;

    gc->fds	      = NULL;
    gc->fdsSize	      = 0;
    gc->timeoutHandle = 0;

    WRAP (&gc->object, &c->u.base, vTable, &glibCoreObjectVTable.base.base);

    glibPrepare (c, g_main_context_default ());

    commonInterfacesAdded (&c->u.base,
			   glibCoreInterface,
			   N_ELEMENTS (glibCoreInterface));

    return TRUE;
}

static void
glibFiniCore (CompCore *c)
{
    GLIB_CORE (c);

    if (gc->timeoutHandle)
	compRemoveTimeout (gc->timeoutHandle);

    glibDispatch (c, g_main_context_default ());

    if (gc->fds)
	free (gc->fds);

    UNWRAP (&gc->object, &c->u.base, vTable);

    commonInterfacesRemoved (&c->u.base,
			     glibCoreInterface,
			     N_ELEMENTS (glibCoreInterface));
}

static CompObjectPrivate glibObj[] = {
    {
	"core",
	&corePrivateIndex, sizeof (GLibCore), &glibCoreObjectVTable.base,
	(InitObjectProc) glibInitCore,
	(FiniObjectProc) glibFiniCore
    }
};

static Bool
glibInit (CompPlugin *p)
{
    if (!compInitObjectMetadataFromInfo (&glibMetadata, p->vTable->name, 0, 0))
	return FALSE;

    compAddMetadataFromFile (&glibMetadata, p->vTable->name);

    if (!compObjectInitPrivates (glibObj, N_ELEMENTS (glibObj)))
    {
	compFiniMetadata (&glibMetadata);
	return FALSE;
    }

    return TRUE;
}

static void
glibFini (CompPlugin *p)
{
    compObjectFiniPrivates (glibObj, N_ELEMENTS (glibObj));
    compFiniMetadata (&glibMetadata);
}

CompPluginVTable glibVTable = {
    "glib",
    0, /* GetMetadata */
    glibInit,
    glibFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &glibVTable;
}
