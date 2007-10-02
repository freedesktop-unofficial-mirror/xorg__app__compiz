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

typedef struct _ForEachInterfaceContext {
    InterfaceCallBackProc proc;
    void		  *closure;
} ForEachInterfaceContext;

static CompBool
baseObjectForEachInterface (CompObject *object,
			    void       *closure)
{
    ForEachInterfaceContext *pCtx = (ForEachInterfaceContext *) closure;

    return (*object->vTable->forEachInterface) (object,
						pCtx->proc,
						pCtx->closure);
}

static CompBool
noopForEachInterface (CompObject	    *object,
		      InterfaceCallBackProc proc,
		      void		    *closure)
{
    ForEachInterfaceContext ctx;

    ctx.proc    = proc;
    ctx.closure = closure;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectForEachInterface,
					     (void *) &ctx);
}

typedef struct _ForEachMethodContext {
    const char	       *interface;
    MethodCallBackProc proc;
    void	       *closure;
} ForEachMethodContext;

static CompBool
baseObjectForEachMethod (CompObject *object,
			 void       *closure)
{
    ForEachMethodContext *pCtx = (ForEachMethodContext *) closure;

    return (*object->vTable->forEachMethod) (object,
					     pCtx->interface,
					     pCtx->proc,
					     pCtx->closure);
}

static CompBool
noopForEachMethod (CompObject	      *object,
		   const char	      *interface,
		   MethodCallBackProc proc,
		   void		      *closure)
{
    ForEachMethodContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectForEachMethod,
					     (void *) &ctx);
}

typedef struct _ForEachSignalContext {
    const char	       *interface;
    SignalCallBackProc proc;
    void	       *closure;
} ForEachSignalContext;

static CompBool
baseObjectForEachSignal (CompObject *object,
			 void       *closure)
{
    ForEachSignalContext *pCtx = (ForEachSignalContext *) closure;

    return (*object->vTable->forEachSignal) (object,
					     pCtx->interface,
					     pCtx->proc,
					     pCtx->closure);
}

static CompBool
noopForEachSignal (CompObject	      *object,
		   const char	      *interface,
		   SignalCallBackProc proc,
		   void		      *closure)
{
    ForEachSignalContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectForEachSignal,
					     (void *) &ctx);
}

typedef struct _ForEachPropContext {
    const char	     *interface;
    PropCallBackProc proc;
    void	     *closure;
} ForEachPropContext;

static CompBool
baseObjectForEachProp (CompObject *object,
		       void       *closure)
{
    ForEachPropContext *pCtx = (ForEachPropContext *) closure;

    return (*object->vTable->forEachProp) (object,
					   pCtx->interface,
					   pCtx->proc,
					   pCtx->closure);
}

static CompBool
noopForEachProp (CompObject	  *object,
		 const char	  *interface,
		 PropCallBackProc proc,
		 void		  *closure)
{
    ForEachPropContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectForEachProp,
					     (void *) &ctx);
}

typedef struct _InvokeMethodContext {
    const char       *interface;
    const char       *name;
    const CompOption *in;
    CompOption       *out;
} InvokeMethodContext;

static CompBool
baseObjectInvokeMethod (CompObject *object,
			void       *closure)
{
    InvokeMethodContext *pCtx = (InvokeMethodContext *) closure;

    return (*object->vTable->invokeMethod) (object,
					    pCtx->interface,
					    pCtx->name,
					    pCtx->in,
					    pCtx->out);
}

static CompBool
noopInvokeMethod (CompObject	   *object,
		  const char       *interface,
		  const char       *name,
		  const CompOption *in,
		  CompOption       *out)
{
    InvokeMethodContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.in	  = in;
    ctx.out	  = out;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectInvokeMethod,
					     (void *) &ctx);
}

typedef struct _GetTypeContext {
    const CompObjectType *result;
} GetTypeContext;

static CompBool
baseObjectGetType (CompObject *object,
		   void       *closure)
{
    GetTypeContext *pCtx = (GetTypeContext *) closure;

    pCtx->result = (*object->vTable->getType) (object);

    return TRUE;
}

static const CompObjectType *
noopGetType (CompObject *object)
{
    GetTypeContext ctx;

    (*object->vTable->forBaseObject) (object,
				      baseObjectGetType,
				      (void *) &ctx);

    return ctx.result;
}

typedef struct _QueryNameContext {
    char *result;
} QueryNameContext;

static CompBool
baseObjectQueryName (CompObject *object,
		     void       *closure)
{
    QueryNameContext *pCtx = (QueryNameContext *) closure;

    pCtx->result = (*object->vTable->queryName) (object);

    return TRUE;
}

static char *
noopQueryName (CompObject *object)
{
    QueryNameContext ctx;

    (*object->vTable->forBaseObject) (object,
				      baseObjectQueryName,
				      (void *) &ctx);

    return ctx.result;
}

typedef struct _ForEachChildObjectContext {
    ChildObjectCallBackProc proc;
    void		    *closure;
} ForEachChildObjectContext;

static CompBool
baseObjectForEachChildObject (CompObject *object,
			      void       *closure)
{
    ForEachChildObjectContext *pCtx = (ForEachChildObjectContext *) closure;

    return (*object->vTable->forEachChildObject) (object,
						  pCtx->proc,
						  pCtx->closure);
}

static CompBool
noopForEachChildObject (CompObject		*object,
			ChildObjectCallBackProc proc,
			void		        *closure)
{
    ForEachChildObjectContext ctx;

    ctx.proc    = proc;
    ctx.closure = closure;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectForEachChildObject,
					     (void *) &ctx);
}

typedef struct _LookupChildObjectContext {
    const char *type;
    const char *name;
    CompObject *result;
} LookupChildObjectContext;

static CompBool
baseObjectLookupChildObject (CompObject *object,
			     void       *closure)
{
    LookupChildObjectContext *pCtx = (LookupChildObjectContext *) closure;

    pCtx->result = (*object->vTable->lookupChildObject) (object,
							 pCtx->type,
							 pCtx->name);

    return TRUE;
}

static CompObject *
noopLookupChildObject (CompObject *object,
		       const char *type,
		       const char *name)
{
    LookupChildObjectContext ctx;

    ctx.type = type;
    ctx.name = name;

    (*object->vTable->forBaseObject) (object,
				      baseObjectLookupChildObject,
				      (void *) &ctx);

    return ctx.result;
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
    forEachInterface,
    forEachMethod,
    forEachSignal,
    forEachProp,
    invokeObjectMethod,
    getType,
    queryObjectName,
    forEachChildObject,
    lookupChildObject
};

static void
processObjectSignal (CompObject	      *object,
		     CompObject	      *source,
		     const char	      *interface,
		     const char	      *name,
		     const CompOption *out)
{
}

static CompObjectPrivates objectPrivates = {
    0,
    NULL,
    0,
    offsetof (CompObject, privates),
    NULL,
    0
};

static CompBool
initObject (CompObject *object)
{
    object->vTable = &objectVTable;

    if (!allocateObjectPrivates (object, &objectPrivates))
	return FALSE;

    object->processSignal = processObjectSignal;

    object->id = ~0; /* XXX: remove id asap */

    return TRUE;
}

static void
finiObject (CompObject *object)
{
    if (object->privates)
	free (object->privates);
}

static void
initObjectVTable (CompObjectVTable *vTable)
{
    ENSURE (vTable, forEachInterface, noopForEachInterface);
    ENSURE (vTable, forEachMethod,    noopForEachMethod);
    ENSURE (vTable, forEachSignal,    noopForEachSignal);
    ENSURE (vTable, forEachProp,      noopForEachProp);
    ENSURE (vTable, invokeMethod,     noopInvokeMethod);

    ENSURE (vTable, getType,   noopGetType);
    ENSURE (vTable, queryName, noopQueryName);

    ENSURE (vTable, forEachChildObject, noopForEachChildObject);
    ENSURE (vTable, lookupChildObject,  noopLookupChildObject);
}

static CompObjectType objectType = {
    "object",
    {
	initObject,
	finiObject
    },
    &objectPrivates,
    (InitVTableProc) initObjectVTable
};

CompObjectType *
getObjectType (void)
{
    return &objectType;
}

CompBool
allocateObjectPrivates (CompObject	   *object,
			CompObjectPrivates *objectPrivates)
{
    CompPrivate *privates, **pPrivates = (CompPrivate **)
	(((char *) object) + objectPrivates->offset);

    privates = allocatePrivates (objectPrivates->len,
				 objectPrivates->sizes,
				 objectPrivates->totalSize);
    if (!privates)
	return FALSE;

    *pPrivates = privates;

    return TRUE;
}

typedef struct _ForEachObjectPrivatesContext {
    CompObjectType       *type;
    PrivatesCallBackProc proc;
    void	         *data;
} ForEachObjectPrivatesContext;

static CompBool
forEachBaseObjectPrivates (CompObject *object,
			   void       *closure)
{
    ForEachObjectPrivatesContext *pCtx =
	(ForEachObjectPrivatesContext *) closure;

    if ((*object->vTable->getType) (object) == pCtx->type)
    {
	CompObjectPrivates *objectPrivates = pCtx->type->privates;
	CompPrivate	   **pPrivates = (CompPrivate **)
	    (((char *) object) + objectPrivates->offset);

	return (*pCtx->proc) (pPrivates, pCtx->data);
    }

    return (*object->vTable->forBaseObject) (object,
					     forEachBaseObjectPrivates,
					     closure);
}

static CompBool
forEachObjectPrivatesTree (CompChildObject *object,
			   void		   *closure)
{
    CompObjectVTable *vTable = object->base.vTable;

    if (!forEachBaseObjectPrivates (&object->base, closure))
	return FALSE;

    return (*vTable->forEachChildObject) (&object->base,
					  forEachObjectPrivatesTree,
					  closure);
}

static CompBool
forEachObjectPrivates (PrivatesCallBackProc proc,
		       void		    *data,
		       void		    *closure)
{
    ForEachObjectPrivatesContext ctx;

    ctx.type = (CompObjectType *) closure;
    ctx.proc = proc;
    ctx.data = data;

    if (!forEachBaseObjectPrivates (&core.base, (void *) &ctx))
	return FALSE;

    return (*core.base.vTable->forEachChildObject) (&core.base,
						    forEachObjectPrivatesTree,
						    (void *) &ctx);
}

int
compObjectAllocatePrivateIndex (CompObjectType *type,
				int	       size)
{
    return allocatePrivateIndex (&type->privates->len,
				 &type->privates->sizes,
				 &type->privates->totalSize,
				 size, forEachObjectPrivates,
				 (void *) type);
}

void
compObjectFreePrivateIndex (CompObjectType *type,
			    int	           index)
{
    freePrivateIndex (&type->privates->len,
		      &type->privates->sizes,
		      &type->privates->totalSize,
		      forEachObjectPrivates,
		      (void *) type,
		      index);
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

void
compInitChildObjectVTable (CompObjectVTable *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
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
initBaseObject (CompObject *object,
		void       *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    if ((*object->vTable->getType) (object) == pCtx->type)
	return (*pCtx->funcs->init) (object);

    return (*object->vTable->forBaseObject) (object,
					     initBaseObject,
					     closure);
}

static CompBool
finiBaseObject (CompObject *object,
		void       *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    if ((*object->vTable->getType) (object) == pCtx->type)
    {
	(*pCtx->funcs->fini) (object);
	return TRUE;
    }

    return (*object->vTable->forBaseObject) (object,
					     finiBaseObject,
					     closure);
}

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

    if (!initBaseObject (o, (void *) &ctx))
	return FALSE;

    if (!(*o->vTable->forEachChildObject) (o, initObjectTree, (void *) &ctx))
    {
	(*o->vTable->forEachChildObject) (o, finiObjectTree, (void *) &ctx);

	finiBaseObject (o, (void *) &ctx);

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

    finiBaseObject (o, (void *) &ctx);

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

    index = compObjectAllocatePrivateIndex (type, private->size);
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
    int	n = nPrivate;

    while (n--)
	compObjectFiniPrivate (&private[n]);
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
