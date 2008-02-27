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
#include <assert.h>

#include <compiz/root.h>
#include <compiz/core.h>

struct _CompSignal {
    struct _CompSignal *next;

    CompSerializedMethodCallHeader *header;
};

static CompBool
rootInitObject (const CompObjectInstantiator *instantiator,
		CompObject		     *object,
		const CompObjectFactory      *factory)
{
    const CompObjectInstantiator *base = instantiator->base;

    ROOT (object);

    r->signal.head = NULL;
    r->signal.tail = NULL;

    r->stack  = NULL;
    r->nStack = 0;

    r->request  = NULL;
    r->nRequest = 0;

    if (!(*base->init) (base, object, factory))
	return FALSE;

    WRAP (&r->object, object, vTable, instantiator->vTable);

    return TRUE;
}

static void
removeAllRequests (CompRoot *r)
{
    int i;

    if (r->request == r->stack)
	return;

    for (i = 0; i < r->nRequest; i++)
	free (r->request[i]);

    if (r->request)
	free (r->request);

    r->request  = r->stack;
    r->nRequest = 0;
}

static void
rootFinalize (CompObject *object)
{
    ROOT (object);

    removeAllRequests (r);
    r->request = NULL;

    /* remove plugins and empty signal buffer */
    (*r->u.vTable->processSignals) (r);

    /* --- unnecessary when 'core' global variable has been removed */
    while (object->nChild)
    {
	CompObject *child;

	child = (*object->vTable->removeChild) (object, object->child->name);
	(*child->vTable->finalize) (child);
    }
    /* --- */

    UNWRAP (&r->object, object, vTable);

    (*object->vTable->finalize) (object);
}

static void
rootGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    switch (what) {
    case COMP_PROP_BASE_VTABLE:
	*((const CompObjectVTable **) value) =
	    GET_ROOT (object)->object.vTable;
	break;
    case COMP_PROP_PRIVATES:
	*((CompPrivate **) value) = NULL;
	break;
    }
}

static void
rootSetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
}

static CompBool
rootAddChild (CompObject *object,
	      CompObject *child,
	      const char *name)
{
    CompBool status;
    int	     n = object->nChild;

    FOR_BASE (object,
	      status = (*object->vTable->addChild) (object, child, name));

    if (status)
	(*child->vTable->insertObject) (child, object, object->child[n].name);

    return status;
}

static CompObject *
rootRemoveChild (CompObject *object,
		 const char *name)
{
    CompObject *child;
    int	       i;

    for (i = 0; i < object->nChild; i++)
	if (strcmp (object->child[i].name, name) == 0)
	    break;

    if (i < object->nChild)
	(*object->child[i].ref->vTable->removeObject) (object->child[i].ref);

    FOR_BASE (object, child = (*object->vTable->removeChild) (object, name));

    return child;
}

static void
rootSignal (CompObject   *object,
	    const char   *path,
	    const char   *interface,
	    const char   *name,
	    const char   *signature,
	    CompAnyValue *value,
	    int	         nValue)
{
    static const char *pluginPath = "core/plugins";
    int		      i;

    ROOT (object);

    for (i = 0; path[i] == pluginPath[i]; i++);

    if (pluginPath[i] == '\0')
	(*r->u.vTable->updatePlugins) (r, "core");

    FOR_BASE (object, (*object->vTable->signal) (object,
						 path,
						 interface,
						 name,
						 signature,
						 value,
						 nValue));
}

static CompBool
processStackRequest (CompRoot *r)
{
    int i;

    if (r->request == r->stack)
	return FALSE;

    for (i = 0; i < r->nRequest && i < r->nStack; i++)
	if (strcmp (r->request[i], r->stack[i]) != 0)
	    break;

    if (i < r->nStack)
    {
	char **stack;

	r->nStack--;

	unloadPlugin (popPlugin (GET_BRANCH (r->u.base.child->ref)));

	free (r->stack[r->nStack]);

	stack = realloc (r->stack, r->nStack * sizeof (char *));
	if (stack || !r->nStack)
	    r->stack = stack;
    }
    else if (i < r->nRequest)
    {
	char *request;

	request = strdup (r->request[i]);
	if (request)
	{
	    char **stack;

	    stack = realloc (r->stack, (r->nStack + 1) * sizeof (char *));
	    if (stack)
	    {
		CompPlugin *p;

		p = loadPlugin (request);
		if (p)
		    pushPlugin (p, GET_BRANCH (r->u.base.child->ref));

		stack[r->nStack++] = request;
		r->stack = stack;
	    }
	}
    }

    if (i == r->nRequest && i == r->nStack)
	removeAllRequests (r);

    return TRUE;
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

	(*object->vTable->signal) (object, &pCtx->path[i],
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
    do
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

	    (*r->u.base.vTable->forEachChildObject) (&r->u.base,
						     handleSignal,
						     (void *) &ctx);

	    (*r->u.base.vTable->signal) (&r->u.base,
					 s->header->path,
					 s->header->interface,
					 s->header->name,
					 s->header->signature,
					 s->header->value,
					 s->header->nValue);

	    free (s);
	}
    } while (processStackRequest (r));
}

static void
updatePlugins (CompRoot	  *r,
	       const char *path)
{
    CompObject *plugins;
    char       **request;
    int	       i;

    plugins = compLookupDescendantVa (&r->u.base, "core", "plugins", NULL);
    if (!plugins)
	return;

    request = malloc (sizeof (char *) * plugins->nChild);
    if (plugins->nChild && !request)
	return;

    removeAllRequests (r);
    r->request = request;

    for (i = 0; i < plugins->nChild; i++)
	if ((*plugins->child[i].ref->vTable->getString) (plugins->child[i].ref,
							 0,
							 "value",
							 &request[r->nRequest],
							 0))
	    r->nRequest++;
}

static CompRootVTable rootObjectVTable = {
    .base.finalize    = rootFinalize,
    .base.getProp     = rootGetProp,
    .base.setProp     = rootSetProp,
    .base.addChild    = rootAddChild,
    .base.removeChild = rootRemoveChild,
    .base.signal      = rootSignal,

    .processSignals = processSignals,
    .updatePlugins  = updatePlugins
};

static const CompObjectType rootObjectType = {
    .name	   = COMPIZ_ROOT_TYPE_NAME,
    .version	   = COMPIZ_ROOT_VERSION,
    .base.name     = COMPIZ_OBJECT_TYPE_NAME,
    .base.version  = COMPIZ_OBJECT_VERSION,
    .vTable.impl   = &rootObjectVTable.base,
    .vTable.size   = sizeof (rootObjectVTable),
    .instance.init = rootInitObject
};

const CompObjectType *
getRootObjectType (void)
{
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
