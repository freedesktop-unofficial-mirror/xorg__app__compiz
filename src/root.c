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
#include <stdlib.h>

#include <compiz/root.h>

struct _CompSignal {
    struct _CompSignal *next;

    CompSerializedMethodCallHeader *header;
};

static CompBool
rootForBaseObject (CompObject	       *object,
		   BaseObjectCallBackProc proc,
		   void		       *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    ROOT (object);

    UNWRAP (&r->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&r->object, object, vTable, v.vTable);

    return status;
}

typedef struct _HandleSignalContext {
    const char *path;
    CompSignal *signal;
} HandleSignalContext;

static CompBool
handleSignal (CompObject *object,
	      void	 *closure)
{
    HandleSignalContext *pCtx = (HandleSignalContext *) closure;
    int			i;

    for (i = 0; pCtx->path[i] && object->name[i]; i++)
	if (pCtx->path[i] != object->name[i])
	    break;

    if (object->name[i] == '\0')
    {
	if (pCtx->path[i] == '/')
	{
	    HandleSignalContext ctx;

	    ctx.path   = &pCtx->path[++i];
	    ctx.signal = pCtx->signal;

	    (*object->vTable->forEachChildObject) (object,
						   handleSignal,
						   (void *) &ctx);
	}
	else if (pCtx->path[i] != '\0')
	{
	    return TRUE;
	}

	(*object->vTable->signal.signal) (object, &pCtx->path[i],
					  pCtx->signal->header->interface,
					  pCtx->signal->header->name,
					  pCtx->signal->header->signature,
					  pCtx->signal->header->value,
					  pCtx->signal->header->nValue);

	return FALSE;
    }

    return TRUE;
}

static void
processSignals (CompRoot *r)
{
    while (r->signal.head)
    {
	HandleSignalContext ctx;
	CompSignal	    *s = r->signal.head;

	if (s->next)
	    r->signal.head = s->next;
	else
	    r->signal.head = r->signal.tail = NULL;

	ctx.path   = s->header->path;
	ctx.signal = s;

	(*r->u.base.base.vTable->forEachChildObject) (&r->u.base.base,
						      handleSignal,
						      (void *) &ctx);

	(*r->u.base.base.vTable->signal.signal) (&r->u.base.base,
						 s->header->path,
						 s->header->interface,
						 s->header->name,
						 s->header->signature,
						 s->header->value,
						 s->header->nValue);

	free (s);
    }
}

static CompRootVTable rootObjectVTable = {
    .base.forBaseObject = rootForBaseObject,
    .processSignals     = processSignals
};

static CompBool
forCoreObject (CompObject	       *object,
	       ChildObjectCallBackProc proc,
	       void		       *closure)
{
    ROOT (object);

    if (!(*proc) (r->core, closure))
	return FALSE;

    return TRUE;
}

static CompBool
rootInitObject (CompObject *object)
{
    ROOT (object);

    r->signal.head = NULL;
    r->signal.tail = NULL;

    if (!compObjectInit (&r->u.base.base, getContainerObjectType ()))
	return FALSE;

    WRAP (&r->object, object, vTable, &rootObjectVTable.base);

    r->u.base.forEachChildObject = forCoreObject;

    r->core = NULL;

    return TRUE;
}

static void
rootFiniObject (CompObject *object)
{
    ROOT (object);

    UNWRAP (&r->object, object, vTable);

    compObjectFini (object, getContainerObjectType ());
}

static void
rootInitVTable (CompRootVTable *vTable)
{
    (*getContainerObjectType ()->initVTable) (vTable);
}

static CompObjectType rootObjectType = {
    ROOT_TYPE_NAME,
    {
	rootInitObject,
	rootFiniObject
    },
    0,
    NULL,
    (InitVTableProc) rootInitVTable
};

CompObjectType *
getRootObjectType (void)
{
    static int init = 0;

    if (!init)
    {
	rootInitVTable (&rootObjectVTable);
	init = 1;
    }
    return &rootObjectType;
}

void
compEmitSignedSignal (CompObject *object,
		      const char *interface,
		      const char *name,
		      const char *signature,
		      ...)
{
    CompObject *node;
    CompSignal *signal;
    int	       size;
    va_list    args;

    for (node = object; node->parent; node = node->parent);

    va_start (args, signature);

    size = compSerializeMethodCall (node,
				    object,
				    interface,
				    name,
				    signature,
				    args,
				    NULL,
				    0);

    signal = malloc (sizeof (CompSignal) + size);
    if (signal)
    {
	ROOT (node);

	signal->next   = NULL;
	signal->header = (CompSerializedMethodCallHeader *) (signal + 1);

	compSerializeMethodCall (node,
				 object,
				 interface,
				 name,
				 signature,
				 args,
				 signal->header,
				 size);

	if (r->signal.tail)
	    r->signal.tail->next = signal;
	else
	    r->signal.head = signal;

	r->signal.tail = signal;
    }

    va_end (args);
}
