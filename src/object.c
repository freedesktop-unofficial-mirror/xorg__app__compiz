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
marshal_I_S (CompObject *object,
	     int	(*method) (CompObject *,
				   const char *),
	     CompOption *in,
	     CompOption *out,
	     char       **error)
{
    out[0].value.i = (*method) (object, in[0].value.s);
    return TRUE;
}

static CompBool
marshal__SSV__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       CompOption *,
				       char       **),
		 CompOption *in,
		 CompOption *out,
		 char       **error)
{
    return (*method) (object, in[0].value.s, in[1].value.s, &in[2], error);
}

static CompBool
marshal__SS_V_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       CompOption *,
				       char       **),
		 CompOption *in,
		 CompOption *out,
		 char       **error)
{
    return (*method) (object, in[0].value.s, in[1].value.s, &out[0], error);
}

static CompBool
marshal__S_AS_E (CompObject *object,
		 CompBool   (*method) (CompObject    *,
				       char	     *,
				       CompListValue *,
				       char          **),
		 CompOption *in,
		 CompOption *out,
		 char       **error)
{
    return (*method) (object, in[0].value.s, &out[0].value.list, error);
}

#define INTERFACE_VERSION_objectType CORE_ABIVERSION

static const CommonMethod versionObjectMethod[] = {
    C_METHOD (get, "s", "i", CompVersionVTable, marshal_I_S)
};
#define INTERFACE_VERSION_versionObject CORE_ABIVERSION

static const CommonMethod propertiesObjectMethod[] = {
    C_METHOD (get, "ss", "v", CompPropertiesVTable, marshal__SS_V_E),
    C_METHOD (set, "ssv", "", CompPropertiesVTable, marshal__SSV__E)
};
#define INTERFACE_VERSION_propertiesObject CORE_ABIVERSION

static const CommonMethod metadataObjectMethod[] = {
    C_METHOD (get, "s", "as", CompMetadataVTable, marshal__S_AS_E)
};
#define INTERFACE_VERSION_metadataObject CORE_ABIVERSION

static const CommonInterface objectInterface[] = {
    C_INTERFACE (object,     Type,   CompObjectVTable, _, _, _, _),
    C_INTERFACE (version,    Object, CompObjectVTable, X, X, _, _),
    C_INTERFACE (properties, Object, CompObjectVTable, X, X, _, _),
    C_INTERFACE (metadata,   Object, CompObjectVTable, X, X, _, _)
};

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
noopForEachInterface (CompObject	    *object,
		      InterfaceCallBackProc proc,
		      void		    *closure)
{
    return TRUE;
}

static CompBool
noopForEachMethod (CompObject	      *object,
		   void		      *interface,
		   MethodCallBackProc proc,
		   void		      *closure)
{
    return TRUE;
}

static CompBool
noopForEachSignal (CompObject	      *object,
		   void		      *interface,
		   SignalCallBackProc proc,
		   void		      *closure)
{
    return TRUE;
}

static CompBool
noopForEachProp (CompObject	  *object,
		 void		  *interface,
		 PropCallBackProc proc,
		 void		  *closure)
{
    return TRUE;
}

static const CompObjectType *
noopGetType (CompObject *object)
{
    return NULL;
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

typedef struct _GetVersionContext {
    const char *interface;
    int	       result;
} GetVersionContext;

static CompBool
baseObjectGetVersion (CompObject *object,
		      void       *closure)
{
    GetVersionContext *pCtx = (GetVersionContext *) closure;

    pCtx->result = (*object->vTable->version.get) (object, pCtx->interface);

    return TRUE;
}

static int
noopGetVersion (CompObject *object,
		const char *interface)
{
    GetVersionContext ctx;

    ctx.interface = interface;

    (*object->vTable->forBaseObject) (object,
				      baseObjectGetVersion,
				      (void *) &ctx);

    return ctx.result;
}

static CompBool
noopGetProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     CompOption *value,
	     char	**error)
{
    if (error)
	*error = strdup ("No such property");

    return FALSE;
}

static CompBool
noopSetProp (CompObject	      *object,
	     const char	      *interface,
	     const char	      *name,
	     const CompOption *value,
	     char	      **error)
{
    if (error)
	*error = strdup ("No such property");

    return FALSE;
}

typedef struct _GetMetadataContext {
    const char    *interface;
    CompListValue *list;
    char          **error;
} GetMetadataContext;

static CompBool
baseObjectGetMetadata (CompObject *object,
		       void       *closure)
{
    GetMetadataContext *pCtx = (GetMetadataContext *) closure;

    return (*object->vTable->metadata.get) (object,
					    pCtx->interface,
					    pCtx->list,
					    pCtx->error);
}

static CompBool
noopGetMetadata (CompObject    *object,
		 const char    *interface,
		 CompListValue *list,
		 char	       **error)
{
    GetMetadataContext ctx;

    ctx.interface = interface;
    ctx.list      = list;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectGetMetadata,
					     (void *) &ctx);
}

CompBool
handleForEachInterface (CompObject	      *object,
			const CommonInterface *interface,
			int		      nInterface,
			InterfaceCallBackProc proc,
			void		      *closure)
{
    int i;

    for (i = 0; i < nInterface; i++)
	if (!(*proc) (object,
		      interface[i].name,
		      (void *) &interface[i],
		      interface[i].offset,
		      closure))
	    return FALSE;

    return TRUE;
}

CompBool
handleForEachMethod (CompObject	        *object,
		     const CommonMethod *method,
		     int		nMethod,
		     MethodCallBackProc proc,
		     void		*closure)
{
    int i;

    for (i = 0; i < nMethod; i++)
	if (!(*proc) (object, method[i].name, method[i].in, method[i].out,
		      method[i].offset, method[i].marshal, closure))
	    return FALSE;

    return TRUE;
}

CompBool
commonForEachMethod (CompObject		*object,
		     void		*key,
		     MethodCallBackProc proc,
		     void	        *closure)
{
    CommonInterface *interface = (CommonInterface *) key;

    if (!handleForEachMethod (object,
			      interface->method, interface->nMethod,
			      proc, closure))
	return FALSE;

    return TRUE;
}

CompBool
handleForEachSignal (CompObject	        *object,
		     const CommonSignal *signal,
		     int		nSignal,
		     SignalCallBackProc proc,
		     void		*closure)
{
    int i;

    for (i = 0; i < nSignal; i++)
	if (!(*proc) (object, signal[i].name, signal[i].out, closure))
	    return FALSE;

    return TRUE;
}

CompBool
commonForEachSignal (CompObject		*object,
		     void	        *key,
		     SignalCallBackProc proc,
		     void		*closure)
{
    CommonInterface *interface = (CommonInterface *) key;

    if (!handleForEachSignal (object,
			      interface->signal, interface->nSignal,
			      proc, closure))
	return FALSE;

    return TRUE;
}

CompBool
handleForEachProp (CompObject	    *object,
		   const CommonProp *prop,
		   int		    nProp,
		   PropCallBackProc proc,
		   void		    *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].name, prop[i].type, closure))
	    return FALSE;

    return TRUE;
}

CompBool
commonForEachProp (CompObject	    *object,
		   void		    *key,
		   PropCallBackProc proc,
		   void		    *closure)
{
    CommonInterface *interface = (CommonInterface *) key;

    if (!handleForEachProp (object,
			    interface->prop, interface->nProp,
			    proc, closure))
	return FALSE;

    return TRUE;
}

static CompBool
handleGetInterfaceVersion (CompObject *object,
			   const char *name,
			   void       *key,
			   size_t     offset,
			   void       *closure)
{
    GetVersionContext *pCtx = (GetVersionContext *) closure;
    CommonInterface   *interface = (CommonInterface *) key;

    if (strcmp (name, pCtx->interface) == 0)
    {
	pCtx->result = interface->version;
	return FALSE;
    }

    return TRUE;
}

int
commonGetVersion (CompObject *object,
		  const char *interface)
{
    GetVersionContext ctx;

    ctx.interface = interface;

    if (!(*object->vTable->forEachInterface) (object,
					      handleGetInterfaceVersion,
					      (void *) &ctx))
	return ctx.result;

    return noopGetVersion (object, interface);
}

static CompBool
forBaseObject (CompObject	      *object,
	       BaseObjectCallBackProc proc,
	       void		      *closure)
{
    return TRUE;
}

static CompBool
forEachInterface (CompObject		*object,
		  InterfaceCallBackProc proc,
		  void			*closure)
{
    return handleForEachInterface (object,
				   objectInterface,
				   N_ELEMENTS (objectInterface),
				   proc, closure);
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
getProp (CompObject *object,
	 const char *interface,
	 const char *name,
	 CompOption *value,
	 char	    **error)
{
    if (error)
	*error = strdup ("No such property");

    return FALSE;
}

static CompBool
setProp (CompObject	  *object,
	 const char	  *interface,
	 const char	  *name,
	 const CompOption *value,
	 char		  **error)
{
    if (error)
	*error = strdup ("No such property");

    return FALSE;
}

static CompBool
getMetadata (CompObject    *object,
	     const char    *interface,
	     CompListValue *list,
	     char	   **error)
{
    if (error)
	*error = strdup ("No available metadata");

    return FALSE;
}

static CompObjectVTable objectVTable = {
    forBaseObject,
    forEachInterface,
    commonForEachMethod,
    commonForEachSignal,
    commonForEachProp,
    getType,
    queryObjectName,
    forEachChildObject,
    lookupChildObject,
    {
	commonGetVersion
    }, {
	getProp,
	setProp
    }, {
	getMetadata
    }
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

    ENSURE (vTable, getType,   noopGetType);
    ENSURE (vTable, queryName, noopQueryName);

    ENSURE (vTable, forEachChildObject, noopForEachChildObject);
    ENSURE (vTable, lookupChildObject,  noopLookupChildObject);

    ENSURE (vTable, version.get, noopGetVersion);

    ENSURE (vTable, properties.get, noopGetProp);
    ENSURE (vTable, properties.set, noopSetProp);

    ENSURE (vTable, metadata.get, noopGetMetadata);
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

static CompBool
baseObjectType (CompObject *object,
		void	   *closure)
{
    const CompObjectType *type = (*object->vTable->getType) (object);

    if (type)
    {
	*((const CompObjectType **) closure) = type;
	return TRUE;
    }

    return (*object->vTable->forBaseObject) (object,
					     baseObjectType,
					     closure);
}

typedef struct _InitObjectContext {
    CompObjectType  *type;
    CompObjectFuncs *funcs;
    CompChildObject *object;
} InitObjectContext;

static CompBool
initBaseObject (CompObject *object,
		void	   *closure)
{
    InitObjectContext    *pCtx = (InitObjectContext *) closure;
    const CompObjectType *type;

    baseObjectType (object, (void *) &type);

    if (type == pCtx->type)
	return (*pCtx->funcs->init) (object);
    else
	return (*object->vTable->forBaseObject) (object,
						 initBaseObject,
						 closure);
}

static CompBool
finiBaseObject (CompObject *object,
		void	   *closure)
{
    InitObjectContext    *pCtx = (InitObjectContext *) closure;
    const CompObjectType *type;

    baseObjectType (object, (void *) &type);

    if (type == pCtx->type)
	(*pCtx->funcs->fini) (object);
    else
	(*object->vTable->forBaseObject) (object,
					  finiBaseObject,
					  closure);

    return TRUE;
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

    if (private->vTable)
	(*type->initVTable) (private->vTable);

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

static CompBool
checkInterfaceProp (CompObject *object,
		    const char *name,
		    void       *interface,
		    size_t     offset,
		    void       *closure)
{
    return (*object->vTable->forEachProp) (object, interface,
					   checkSignalOrProp,
					   closure);
}

const char *
compObjectPropType (CompObject *object,
		    const char *interface,
		    const char *name)
{
    CheckContext ctx;

    ctx.name = name;
    ctx.type = NULL;

    compForInterface (object, interface, checkInterfaceProp, (void *) &ctx);

    return ctx.type;
}

static CompBool
checkInterfaceSignal (CompObject *object,
		      const char *name,
		      void       *interface,
		      size_t     offset,
		      void       *closure)
{
    return (*object->vTable->forEachSignal) (object, interface,
					     checkSignalOrProp,
					     closure);
}

const char *
compObjectSignalType (CompObject *object,
		      const char *interface,
		      const char *name)
{
    CheckContext ctx;

    ctx.name = name;
    ctx.type = NULL;

    compForInterface (object, interface, checkInterfaceSignal, (void *) &ctx);

    return ctx.type;
}

typedef struct _ForInterfaceContext {
    const char		  *interface;
    InterfaceCallBackProc proc;
    void		  *closure;
} ForInterfaceContext;

static CompBool
handleInterface (CompObject *object,
		 const char *name,
		 void	    *interface,
		 size_t     offset,
		 void	    *closure)
{
    ForInterfaceContext *pCtx = (ForInterfaceContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
	if (!(*pCtx->proc) (object, name, interface, offset, pCtx->closure))
	    return FALSE;

    return TRUE;
}

static CompBool
forInterface (CompObject *object,
	      void	 *closure)
{
    if (!(*object->vTable->forEachInterface) (object,
					      handleInterface,
					      closure))
	return FALSE;

    return (*object->vTable->forBaseObject) (object,
					     forInterface,
					     closure);
}

CompBool
compForInterface (CompObject		*object,
		  const char		*interface,
		  InterfaceCallBackProc proc,
		  void			*closure)
{
    ForInterfaceContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return forInterface (object, (void *) &ctx);
}

CompBool
compForEachInterface (CompObject	    *object,
		      InterfaceCallBackProc proc,
		      void		    *closure)
{
    return compForInterface (object, NULL, proc, closure);
}

static CompBool
getInterfaceVersion (CompObject *object,
		     const char *name,
		     void	*interface,
		     size_t     offset,
		     void       *closure)
{
    int *version = (int *) closure;

    *version = (object->vTable->version.get) (object, name);

    return FALSE;
}

CompBool
compObjectCheckVersion (CompObject *object,
			const char *interface,
			int	   version)
{
    int v;

    if (!compForInterface (object,
			   interface,
			   getInterfaceVersion,
			   (void *) &v))
    {
	if (v != version)
	{
	    compLogMessage (NULL, "core", CompLogLevelError,
			    "wrong '%s' interface version", interface);
	    return FALSE;
	}
    }
    else
    {
	compLogMessage (NULL, "core", CompLogLevelError,
			"no '%s' interface available", interface);

	return FALSE;
    }

    return TRUE;
}

void
compFreeMethodOutput (CompMethodOutput *output)
{
    if (output->signature)
    {
	CompOption *value = output->value;
	const char *signature = output->signature;

	while (*signature != COMP_TYPE_INVALID)
	{
	    signature = nextPropType (signature);
	    compFiniOption (value++);
	}
    }

    free (output);
}

typedef struct _MethodCallContext {
    const char	      *interface;
    const char	      *name;
    const char	      *signature;
    CompObjectVTable  *vTable;
    int		      offset;
    MethodMarshalProc marshal;
    CompMethodOutput  *output;
} MethodCallContext;

static CompBool
checkMethod (CompObject	       *object,
	     const char	       *name,
	     const char	       *in,
	     const char	       *out,
	     size_t	       offset,
	     MethodMarshalProc marshal,
	     void	       *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;
    CompMethodOutput  *output;
    int		      nOut = 0;
    const char	      *signature = out;

    if (strcmp (name, pCtx->name))
	return TRUE;

    if (strcmp (in, pCtx->signature))
	return TRUE;

    while (*signature != COMP_TYPE_INVALID)
    {
	signature = nextPropType (signature);
	nOut++;
    }

    output = malloc (sizeof (CompMethodOutput) +
		     nOut * sizeof (CompOption) +
		     (strlen (out) + 1) * sizeof (char));
    if (output)
    {
	output->value     = (CompOption *) (output + 1);
	output->signature = (char *) (output->value + nOut);

	strcpy (output->signature, out);
    }

    pCtx->offset  = offset;
    pCtx->marshal = marshal;
    pCtx->output  = output;

    return FALSE;
}

static CompBool
checkInterface (CompObject *object,
		const char *name,
		void       *interface,
		size_t	   offset,
		void       *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object,
					       interface,
					       checkMethod,
					       closure))
	{

	    /* need vTable if interface is not part of object type */
	    if (!(*object->vTable->getType) (object))
		pCtx->vTable = object->vTable;

	    /* add interface vTable offset to method offset set by
	       checkMethod function */
	    pCtx->offset += offset;

	    return FALSE;
	}
    }

    return TRUE;
}

static CompBool
checkInterfaces (CompObject *object,
		 void       *closure)
{
    if (!(*object->vTable->forEachInterface) (object,
					      checkInterface,
					      (void *) closure))
	return FALSE;

    return (*object->vTable->forBaseObject) (object,
					     checkInterfaces,
					     closure);
}

static CompBool
invokeMethod (CompObject        *o,
	      CompObjectVTable  *vTable,
	      int	        offset,
	      MethodMarshalProc marshal,
	      const CompOption	*in,
	      CompOption	*out,
	      char		**error)
{
    CompObjectVTableVec save = { o->vTable };
    CompObjectVTableVec interface = { vTable };
    CompBool		status;

    UNWRAP (&interface, o, vTable);
    status = (*marshal) (o,
			 *((void (**) (void)) (((char *) o->vTable) + offset)),
			 in, out,
			 error);
    WRAP (&interface, o, vTable, save.vTable);

    return status;
}

/* returns true if method exist */
CompBool
compInvokeMethodError (CompObject	*object,
		       const char	*interface,
		       const char	*name,
		       const char	*signature,
		       const CompOption *in,
		       CompBool		*status,
		       CompMethodOutput	**out,
		       char		**error)
{
    MethodCallContext ctx;

    ctx.interface = interface;
    ctx.name	  = name;
    ctx.signature = signature;
    ctx.vTable    = object->vTable;

    if (checkInterfaces (object, (void *) &ctx))
	return FALSE;

    if (!ctx.output)
    {
	if (error)
	    *error = strdup ("Failed to allocate memory for method output");

	*status = FALSE;
    }
    else
    {
	*status = invokeMethod (object,
				ctx.vTable,
				ctx.offset,
				ctx.marshal,
				in,
				ctx.output->value,
				error);

	if (*status)
	{
	    if (out)
		*out = ctx.output;
	    else
		compFreeMethodOutput (ctx.output);
	}
    }

    return TRUE;
}

CompBool
compInvokeMethod (CompObject	   *object,
		  const char	   *interface,
		  const char	   *name,
		  const char	   *signature,
		  const CompOption *in,
		  CompMethodOutput **out)
{
    CompBool status;

    if (compInvokeMethodError (object, interface, name, signature,
			       in, &status, out, NULL))
	return status;

    return FALSE;
}
