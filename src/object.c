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
	if (!(*proc) (object, PROPERTIES_METHOD_SET_NAME, "ssv", "", closure))
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

CompBool
compObjectInit (CompObject       *object,
		CompObjectType   *type,
		CompObjectTypeID id)
{
    if (type->privateLen)
    {
	object->privates = malloc (type->privateLen * sizeof (CompPrivate));
	if (!object->privates)
	    return FALSE;
    }
    else
    {
	object->privates = NULL;
    }

    object->id     = id;
    object->type   = type;
    object->vTable = &objectVTable;

    object->processSignal = processObjectSignal;

    return TRUE;
}

void
compObjectFini (CompObject *object)
{
    if (object->privates)
	free (object->privates);
}

typedef struct _ReallocObjectPrivatesContext {
    CompObjectType *type;
    int		   size;
} ReallocObjectPrivatesContext;

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

static CompBool
reallocObjectPrivatesTree (CompChildObject *object,
			   void		   *closure)
{
    CompObjectVTable		 *vTable = object->base.vTable;
    ReallocObjectPrivatesContext *ctx =
	(ReallocObjectPrivatesContext *) closure;

    if (object->base.type == ctx->type)
	if (!reallocObjectPrivates (&object->base, ctx->size))
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
    void			 *privates;

    ctx.type = (CompObjectType *) closure;
    ctx.size = size;

    privates = realloc (ctx.type->privates, size * sizeof (CompPrivate));
    if (!privates)
	return FALSE;

    ctx.type->privates = privates;

    if (core.base.type == ctx.type)
	if (!reallocObjectPrivates (&core.base, size))
	    return FALSE;

    return (*core.base.vTable->forEachChildObject) (&core.base,
						    reallocObjectPrivatesTree,
						    (void *) &ctx);
}

int
compObjectAllocatePrivateIndex (CompObjectType *type)
{
    return allocatePrivateIndex (&type->privateLen,
				 &type->privateIndices,
				 reallocObjectPrivate,
				 (void *) type);
}

void
compObjectFreePrivateIndex (CompObjectType *type,
			    int	           index)
{
    freePrivateIndex (type->privateLen, type->privateIndices, index);
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
		     CompObjectType   *type,
		     CompObjectTypeID id)
{
    if (!compObjectInit (&object->base, type, id))
	return FALSE;

    object->parent = NULL;

    WRAP (object, &object->base, processSignal, processChildObjectSignal);

    return TRUE;
}

void
compChildObjectFini (CompChildObject *object)
{
    UNWRAP (object, &object->base, processSignal);

    compObjectFini (&object->base);
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
    CompChildObject *object;
} InitObjectContext;

static CompBool
initTypedObjects (CompObject	 *o,
		  CompObjectType *type);

static CompBool
finiTypedObjects (CompObject	 *o,
		  CompObjectType *type);

static CompBool
initObjectTree (CompChildObject *o,
		void		*closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    pCtx->object = o;

    return initTypedObjects (&o->base, pCtx->type);
}

static CompBool
finiObjectTree (CompChildObject *o,
		void		*closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == o)
	return FALSE;

    return finiTypedObjects (&o->base, pCtx->type);
}

static CompBool
initTypedObjects (CompObject	 *o,
		  CompObjectType *type)
{
    InitObjectContext ctx;

    ctx.type   = type;
    ctx.object = NULL;

    if (o->type == type)
	(*ctx.type->funcs->init) (o, ctx.type);

    if (!(*o->vTable->forEachChildObject) (o, initObjectTree, (void *) &ctx))
    {
	(*o->vTable->forEachChildObject) (o, finiObjectTree, (void *) &ctx);

	if (o->type == type)
	    (*ctx.type->funcs->fini) (o);

	return FALSE;
    }

    return TRUE;
}

static CompBool
finiTypedObjects (CompObject	 *o,
		  CompObjectType *type)
{
    InitObjectContext ctx;

    ctx.type   = type;
    ctx.object = NULL;

    (*o->vTable->forEachChildObject) (o, finiObjectTree, (void *) &ctx);

    if (o->type == type)
	(*ctx.type->funcs->fini) (o);

    return TRUE;
}

CompBool
compObjectInitPrivates (CompObjectPrivate *private,
			int		  nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
    {
	CompObjectType  *type;
	CompObjectFuncs *funcs;
	int	        index;

	type = compObjectFindType (private[i].name);
	if (!type)
	    break;

	index = compObjectAllocatePrivateIndex (type);
	if (index < 0)
	    break;

	*(private[i].pIndex) = index;

	/* wrap initialization functions */
	funcs	    = type->funcs;
	type->funcs = private[i].funcs;

	/* disable propagation of calls to init/fini */
	type->privates[index].ptr = NULL;

	/* initialize all objects of this type */
	if (!initTypedObjects (&core.base, type))
	{
	    compObjectFreePrivateIndex (type, index);
	    type->funcs = funcs;
	    break;
	}

	/* enable propagation of calls to init */
	type->privates[index].ptr = funcs;
    }

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
    {
	CompObjectType  *type;
	CompObjectFuncs *funcs;
	int	        index;

	type = compObjectFindType (private[i].name);
	if (!type)
	    break;

	index = *(private[i].pIndex);

	funcs = type->privates[index].ptr;

	/* disable propagation of calls to fini */
	type->privates[index].ptr = NULL;

	/* finish all objects of this type */
	finiTypedObjects (&core.base, type);

	/* unwrap initialization functions */
	type->funcs = funcs;

	compObjectFreePrivateIndex (type, index);
    }
}

CompBool
compObjectInitOther (CompObject	    *o,
		     CompObjectType *type,
		     int	    index)
{
    CompObjectFuncs *funcs = (CompObjectFuncs *) type->privates[index].ptr;

    if (funcs && !(*funcs->init) (o, type))
	return FALSE;

    return TRUE;
}

void
compObjectFiniOther (CompObject *o,
		     int	index)
{
    CompObjectFuncs *funcs = (CompObjectFuncs *) o->type->privates[index].ptr;

    if (funcs)
	(*funcs->fini) (o);
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
