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

#include <compiz-core.h>

CompBool
compObjectInit (CompObject     *object,
		CompObjectType *type)
{
    int i;

    if (!(*type->funcs.init) (object))
	return FALSE;

    for (i = 0; i < type->privates->nFuncs; i++)
	if (!(*type->privates->funcs[i].init) (object))
	    break;

    if (i == type->privates->nFuncs)
	return TRUE;

    while (--i >= 0)
	(*type->privates->funcs[i].fini) (object);

    (*type->funcs.fini) (object);

    return FALSE;
}

void
compObjectFini (CompObject     *object,
		CompObjectType *type)
{
    int i = type->privates->nFuncs;

    while (--i >= 0)
	(*type->privates->funcs[i].fini) (object);

    (*type->funcs.fini) (object);
}

static CompBool
forBaseObject (CompObject	      *object,
	       BaseObjectCallBackProc proc,
	       void		      *closure)
{
    return TRUE;
}

static const CompObjectType *
getType (CompObject *object)
{
    return getObjectType ();
}

static char *
queryObjectName (CompObject *object)
{
    return NULL;
}

static CompBool
forEachChildObject (CompObject		    *object,
		    ChildObjectCallBackProc proc,
		    void		    *closure)
{
    return TRUE;
}

static CompObject *
lookupChildObject (CompObject *parent,
		   const char *type,
		   const char *name)
{
    return NULL;
}

static CompBool
forEachInterface (CompObject		*object,
		  InterfaceCallBackProc proc,
		  void			*closure)
{
    if (!(*proc) (object, PROPERTIES_INTERFACE_NAME, closure))
	return FALSE;

    if (!(*proc) (object, VERSION_INTERFACE_NAME, closure))
	return FALSE;

    if (!(*proc) (object, METADATA_INTERFACE_NAME, closure))
	return FALSE;

    return TRUE;
}

static CompBool
forEachMethod (CompObject	  *object,
	       const char	  *interface,
	       MethodCallBackProc proc,
	       void		  *closure)
{
    if (strcmp (interface, PROPERTIES_INTERFACE_NAME) == 0)
    {
	if (!(*proc) (object, PROPERTIES_METHOD_SET_NAME, "ssv", 0, closure))
	    return FALSE;
    }
    else if (strcmp (interface, VERSION_INTERFACE_NAME) == 0)
    {
	if (!(*proc) (object, VERSION_METHOD_GET_NAME, "s", "i", closure))
	    return FALSE;
    }
    else if (strcmp (interface, METADATA_INTERFACE_NAME) == 0)
    {
	if (!(*proc) (object, METADATA_METHOD_GET_NAME, "s", "s", closure))
	    return FALSE;
    }

    return TRUE;
}

static CompBool
forEachSignal (CompObject	 *object,
	       const char	 *interface,
	       SignalCallBackProc proc,
	       void		 *closure)
{
    if (strcmp (interface, PROPERTIES_INTERFACE_NAME) == 0)
    {
	if (!(*proc) (object, PROPERTIES_SIGNAL_CHANGED_NAME, "ssv", closure))
	    return FALSE;
    }

    return TRUE;
}

static CompBool
forEachProp (CompObject	     *object,
	     const char	     *interface,
	     PropCallBackProc proc,
	     void	     *closure)
{
    return TRUE;
}

static CompBool
invokeObjectMethod (CompObject	     *object,
		    const char	     *interface,
		    const char	     *name,
		    const CompOption *in,
		    CompOption	     *out)
{
    if (strcmp (interface, VERSION_INTERFACE_NAME) == 0)
    {
	if (strcmp (name, VERSION_METHOD_GET_NAME) == 0)
	{
	    if (strcmp (in[0].value.s, PROPERTIES_INTERFACE_NAME) == 0 ||
		strcmp (in[0].value.s, VERSION_INTERFACE_NAME)	  == 0 ||
		strcmp (in[0].value.s, METADATA_INTERFACE_NAME)	  == 0)
	    {
		out[0].value.i = CORE_ABIVERSION;
		return TRUE;
	    }
	}
    }

    return FALSE;
}

static CompObjectVTable objectVTable = {
    forBaseObject,
    getType,
    queryObjectName,
    forEachChildObject,
    lookupChildObject,
    forEachInterface,
    forEachMethod,
    forEachSignal,
    forEachProp,
    invokeObjectMethod
};

static void
processObjectSignal (CompObject	      *object,
		     CompObject	      *source,
		     const char	      *interface,
		     const char	      *name,
		     const CompOption *out)
{
}

static CompBool
reallocObjectPrivates (CompObject *object,
		       int	  size)
{
    void *privates;

    privates = realloc (object->privates, size * sizeof (CompPrivate));
    if (!privates)
	return FALSE;

    object->privates = (CompPrivate *) privates;

    return TRUE;
}

static CompObjectPrivates objectPrivates = {
    NULL,
    0,
    NULL,
    0,
    reallocObjectPrivates
};

static CompBool
initObject (CompObject *object)
{
    object->id = ~0; /* XXX: remove id asap */

    object->vTable   = &objectVTable;
    object->privates = NULL;

    if (!reallocObjectPrivates (object, objectPrivates.len))
	return FALSE;

    object->processSignal = processObjectSignal;

    return TRUE;
}

static void
finiObject (CompObject *object)
{
    if (object->privates)
	free (object->privates);
}

static CompObjectType objectType = {
    "object",
    {
	initObject,
	finiObject
    },
    &objectPrivates
};

CompObjectType *
getObjectType (void)
{
    return &objectType;
}

typedef struct _ReallocObjectPrivatesContext {
    CompObjectType *type;
    int		   size;
} ReallocObjectPrivatesContext;

static CompBool
reallocBaseObjectPrivates (CompObject *object,
			   void       *closure)
{
    ReallocObjectPrivatesContext *pCtx =
	(ReallocObjectPrivatesContext *) closure;

    if ((*object->vTable->getType) (object) == pCtx->type)
	if (!(*pCtx->type->privates->realloc) (object, pCtx->size))
	    return FALSE;

    return (*object->vTable->forBaseObject) (object,
					     reallocBaseObjectPrivates,
					     closure);
}

static CompBool
reallocObjectPrivatesTree (CompChildObject *object,
			   void		   *closure)
{
    CompObjectVTable *vTable = object->base.vTable;

    if (!reallocBaseObjectPrivates (&object->base, closure))
	return FALSE;

    return (*vTable->forEachChildObject) (&object->base,
					  reallocObjectPrivatesTree,
					  closure);
}

static int
reallocObjectPrivate (int  size,
		      void *closure)
{
    ReallocObjectPrivatesContext ctx;

    ctx.type = (CompObjectType *) closure;
    ctx.size = size;

    if (!reallocBaseObjectPrivates (&core.base, (void *) &ctx))
	return FALSE;

    return (*core.base.vTable->forEachChildObject) (&core.base,
						    reallocObjectPrivatesTree,
						    (void *) &ctx);
}

int
compObjectAllocatePrivateIndex (CompObjectType *type)
{
    return allocatePrivateIndex (&type->privates->len,
				 &type->privates->indices,
				 reallocObjectPrivate,
				 (void *) type);
}

void
compObjectFreePrivateIndex (CompObjectType *type,
			    int	           index)
{
    freePrivateIndex (type->privates->len, type->privates->indices, index);
}

static void
processChildObjectSignal (CompObject	   *object,
			  CompObject	   *source,
			  const char	   *interface,
			  const char	   *name,
			  const CompOption *out)
{
    CompChildObject *child = (CompChildObject *) object;
    CompObject      *parent = child->parent;

    /* propagate to parent */
    if (parent)
	(*parent->processSignal) (parent, source, interface, name, out);

    UNWRAP (child, object, processSignal);
    (*object->processSignal) (object, source, interface, name, out);
    WRAP (child, object, processSignal, processChildObjectSignal);
}

CompBool
compChildObjectInit (CompChildObject  *object,
		     CompObjectTypeID id)
{
    if (!compObjectInit (&object->base, getObjectType ()))
	return FALSE;

    object->base.id = id; /* XXX: remove id asap */

    object->parent = NULL;

    WRAP (object, &object->base, processSignal, processChildObjectSignal);

    return TRUE;
}

void
compChildObjectFini (CompChildObject *object)
{
    UNWRAP (object, &object->base, processSignal);

    compObjectFini (&object->base, getObjectType ());
}

const CompObjectType *
compChildObjectParentType (CompChildObject *object)
{
    return getObjectType ();
}

typedef struct _FindTypeContext {
    const char	   *name;
    CompObjectType *type;
} FindTypeContext;

static CompBool
checkType (CompObjectType *type,
	   void		  *closure)
{
    FindTypeContext *ctx = (FindTypeContext *) closure;

    if (strcmp (ctx->name, type->name) == 0)
    {
	ctx->type = type;
	return FALSE;
    }

    return TRUE;
}

CompObjectType *
compObjectFindType (const char *name)
{
    FindTypeContext ctx;

    ctx.name = name;
    ctx.type = NULL;

    (*core.forEachObjectType) (checkType, (void *) &ctx);

    return ctx.type;
}

typedef struct _InitObjectContext {
    CompObjectType  *type;
    CompObjectFuncs *funcs;
    CompChildObject *object;
} InitObjectContext;

static CompBool
initTypedObjects (CompObject	  *o,
		  CompObjectType  *type,
		  CompObjectFuncs *funcs);

static CompBool
finiTypedObjects (CompObject	 *o,
		  CompObjectType *type,
		  CompObjectFuncs *funcs);

static CompBool
initObjectTree (CompChildObject *o,
		void		*closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    pCtx->object = o;

    return initTypedObjects (&o->base, pCtx->type, pCtx->funcs);
}

static CompBool
finiObjectTree (CompChildObject *o,
		void		*closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == o)
	return FALSE;

    return finiTypedObjects (&o->base, pCtx->type, pCtx->funcs);
}

static CompBool
initTypedObjects (CompObject	  *o,
		  CompObjectType  *type,
		  CompObjectFuncs *funcs)
{
    InitObjectContext ctx;

    ctx.type   = type;
    ctx.funcs  = funcs;
    ctx.object = NULL;

    if ((*o->vTable->getType) (o) == type)
	(*funcs->init) (o);

    if (!(*o->vTable->forEachChildObject) (o, initObjectTree, (void *) &ctx))
    {
	(*o->vTable->forEachChildObject) (o, finiObjectTree, (void *) &ctx);

	if ((*o->vTable->getType) (o) == type)
	    (*funcs->fini) (o);

	return FALSE;
    }

    return TRUE;
}

static CompBool
finiTypedObjects (CompObject	  *o,
		  CompObjectType  *type,
		  CompObjectFuncs *funcs)
{
    InitObjectContext ctx;

    ctx.type   = type;
    ctx.funcs  = funcs;
    ctx.object = NULL;

    (*o->vTable->forEachChildObject) (o, finiObjectTree, (void *) &ctx);

    if ((*o->vTable->getType) (o) == type)
	(*funcs->fini) (o);

    return TRUE;
}

static CompBool
compObjectInitPrivate (CompObjectPrivate *private)
{
    CompObjectType  *type;
    CompObjectFuncs *funcs;
    int		    index;

    type = compObjectFindType (private->name);
    if (!type)
	return FALSE;

    funcs = realloc (type->privates->funcs, (type->privates->nFuncs + 1) *
		     sizeof (CompObjectFuncs));
    if (!funcs)
	return FALSE;

    type->privates->funcs = funcs;

    index = compObjectAllocatePrivateIndex (type);
    if (index < 0)
	return FALSE;

    *(private->pIndex) = index;

    funcs[type->privates->nFuncs].init = private->init;
    funcs[type->privates->nFuncs].fini = private->fini;

    /* initialize all objects of this type */
    if (!initTypedObjects (&core.base, type, &funcs[type->privates->nFuncs]))
    {
	compObjectFreePrivateIndex (type, index);
	return FALSE;
    }

    type->privates->nFuncs++;

    return TRUE;
}

static void
compObjectFiniPrivate (CompObjectPrivate *private)
{
    CompObjectType  *type;
    CompObjectFuncs *funcs;
    int		    index;

    type = compObjectFindType (private->name);
    if (!type)
	return;

    index = *(private->pIndex);

    type->privates->nFuncs--;

    /* finalize all objects of this type */
    finiTypedObjects (&core.base, type,
		      &type->privates->funcs[type->privates->nFuncs]);

    compObjectFreePrivateIndex (type, index);

    funcs = realloc (type->privates->funcs, type->privates->nFuncs *
		     sizeof (CompObjectFuncs));
    if (funcs)
	type->privates->funcs = funcs;
}

CompBool
compObjectInitPrivates (CompObjectPrivate *private,
			int		  nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
	if (!compObjectInitPrivate (&private[i]))
	    break;

    if (i < nPrivate)
    {
	if (i)
	    compObjectFiniPrivates (private, i - 1);

	return FALSE;
    }

    return TRUE;
}

void
compObjectFiniPrivates (CompObjectPrivate *private,
			int		  nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
	compObjectFiniPrivate (&private[i]);
}

typedef struct _CheckContext {
    const char *name;
    const char *type;
} CheckContext;

static CompBool
checkSignalOrProp (CompObject *object,
		   const char *name,
		   const char *type,
		   void	      *closure)
{
    CheckContext *pCtx = (CheckContext *) closure;

    if (strcmp (name, pCtx->name) == 0)
    {
	pCtx->type = type;
	return FALSE;
    }

    return TRUE;
}

const char *
compObjectPropType (CompObject *object,
		    const char *interface,
		    const char *name)
{
    CheckContext ctx;

    ctx.name = name;
    ctx.type = NULL;

    (*object->vTable->forEachProp) (object, interface, checkSignalOrProp,
				    (void *) &ctx);

    return ctx.type;
}

const char *
compObjectSignalType (CompObject *object,
		      const char *interface,
		      const char *name)
{
    CheckContext ctx;

    ctx.name = name;
    ctx.type = NULL;

    (*object->vTable->forEachSignal) (object, interface, checkSignalOrProp,
				      (void *) &ctx);

    return ctx.type;
}
