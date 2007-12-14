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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#include <compiz/object.h>
#include <compiz/c-object.h>
#include <compiz/marshal.h>
#include <compiz/error.h>
#include <compiz/core.h>

static CSignal insertedSignal = C_SIGNAL (inserted, "", CompObjectVTable);
static CSignal removedSignal = C_SIGNAL (removed, "", CompObjectVTable);
static CSignal interfaceAddedSignal =
    C_SIGNAL (interfaceAdded, "s", CompObjectVTable);
static CSignal interfaceRemovedSignal =
    C_SIGNAL (interfaceRemoved, "s", CompObjectVTable);

static CSignal *objectTypeSignal[] = {
    &insertedSignal,
    &removedSignal,
    &interfaceAddedSignal,
    &interfaceRemovedSignal
};

static CSignal signalSignal = C_SIGNAL (signal, NULL, CompSignalVTable);

static CSignal *signalObjectSignal[] = {
    &signalSignal
};

static const CMethod propertiesObjectMethod[] = {
    C_METHOD (getBool,   "ss",  "b", CompPropertiesVTable, marshal__SS_B_E),
    C_METHOD (setBool,   "ssb", "",  CompPropertiesVTable, marshal__SSB__E),
    C_METHOD (getInt,    "ss",  "i", CompPropertiesVTable, marshal__SS_I_E),
    C_METHOD (setInt,    "ssi", "",  CompPropertiesVTable, marshal__SSI__E),
    C_METHOD (getDouble, "ss",  "d", CompPropertiesVTable, marshal__SS_D_E),
    C_METHOD (setDouble, "ssd", "",  CompPropertiesVTable, marshal__SSD__E),
    C_METHOD (getString, "ss",  "s", CompPropertiesVTable, marshal__SS_S_E),
    C_METHOD (setString, "sss", "",  CompPropertiesVTable, marshal__SSS__E)
};

static CSignal boolChangedSignal =
    C_SIGNAL (boolChanged,"ssb", CompPropertiesVTable);
static CSignal intChangedSignal =
    C_SIGNAL (intChanged,"ssi", CompPropertiesVTable);
static CSignal doubleChangedSignal =
    C_SIGNAL (doubleChanged,"ssd", CompPropertiesVTable);
static CSignal stringChangedSignal =
    C_SIGNAL (stringChanged,"sss", CompPropertiesVTable);

static CSignal *propertiesObjectSignal[] = {
    &boolChangedSignal,
    &intChangedSignal,
    &doubleChangedSignal,
    &stringChangedSignal
};

static const CMethod metadataObjectMethod[] = {
    C_METHOD (get, "s", "s", CompMetadataVTable, marshal__S_S_E)
};

static const CMethod versionObjectMethod[] = {
    C_METHOD (get, "s", "i", CompVersionVTable, marshal_I_S)
};

static CInterface objectInterface[] = {
    C_INTERFACE (object,     Type,   CompObjectVTable, _, _, X, _, _, _, _, _),
    C_INTERFACE (signal,     Object, CompObjectVTable, X, _, X, _, _, _, _, _),
    C_INTERFACE (properties, Object, CompObjectVTable, X, X, X, _, _, _, _, _),
    C_INTERFACE (metadata,   Object, CompObjectVTable, X, X, _, _, _, _, _, _),
    C_INTERFACE (version,    Object, CompObjectVTable, X, X, _, _, _, _, _, _)
};

static CompSignalHandler lastSignalVecEntry  = { 0 };
static CompSignalHandler emptySignalVecEntry = { 0 };

static CompBool
allocateObjectPrivates (CompObject		     *object,
			const CompObjectType         *type,
			const CompObjectPrivatesSize *size)
{
    CompPrivate *privates, **pPrivates = (CompPrivate **)
	(((char *) object) + type->privatesOffset);

    privates = allocatePrivates (size->len, size->sizes, size->totalSize);
    if (!privates)
	return FALSE;

    *pPrivates = privates;

    return TRUE;
}

static void
freeObjectPrivates (CompObject		         *object,
		    const CompObjectType         *type,
		    const CompObjectPrivatesSize *size)
{
    CompPrivate **pPrivates = (CompPrivate **)
	(((char *) object) + type->privatesOffset);

    if (*pPrivates)
	free (*pPrivates);
}

CompBool
compObjectInit (const CompObjectFactory      *factory,
		CompObject		     *object,
		const CompObjectInstantiator *instantiator)
{
    int i;

    if (instantiator->base)
	if (!compObjectInit (factory, object, instantiator->base))
	    return FALSE;

    if (!(*instantiator->type->funcs.init) (factory, object))
    {
	if (instantiator->base)
	    compObjectFini (factory, object, instantiator->base);

	return FALSE;
    }

    if (!instantiator->type->privatesOffset)
	return TRUE;

    if (!allocateObjectPrivates (object, instantiator->type,
				 &instantiator->size))
    {
	(*instantiator->type->funcs.fini) (factory, object);
	if (instantiator->base)
	    compObjectFini (factory, object, instantiator->base);

	return FALSE;
    }

    for (i = 0; i < instantiator->nPrivates; i++)
	if (!(*instantiator->privates[i].funcs.init) (factory, object))
	    break;

    if (i == instantiator->nPrivates)
	return TRUE;

    while (--i >= 0)
	(*instantiator->privates[i].funcs.fini) (factory, object);

    (*instantiator->type->funcs.fini) (factory, object);

    if (instantiator->base)
	compObjectFini (factory, object, instantiator->base);

    return FALSE;
}

void
compObjectFini (const CompObjectFactory      *factory,
		CompObject		     *object,
		const CompObjectInstantiator *instantiator)
{
    if (instantiator->type->privatesOffset)
    {
	int i = instantiator->nPrivates;

	while (--i >= 0)
	    (*instantiator->privates[i].funcs.fini) (factory, object);

	freeObjectPrivates (object, instantiator->type, &instantiator->size);
    }

    (*instantiator->type->funcs.fini) (factory, object);
}

static CompObjectInstantiator *
findObjectInstantiator (const CompObjectFactory *factory,
			const CompObjectType	*type)
{
    CompObjectInstantiator *i;

    for (i = factory->instantiator; i; i = i->next)
	if (type == i->type)
	    return i;

    if (factory->master)
	return findObjectInstantiator (factory->master, type);

    return NULL;
}

static CompObjectInstantiator *
lookupObjectInstantiator (const CompObjectFactory *factory,
			  const char		  *name)
{
    CompObjectInstantiator *i;

    for (i = factory->instantiator; i; i = i->next)
	if (strcmp (name, i->type->name) == 0)
	    return i;

    if (factory->master)
	return lookupObjectInstantiator (factory->master, name);

    return NULL;
}

CompBool
compObjectInitByType (const CompObjectFactory *factory,
		      CompObject	      *object,
		      const CompObjectType    *type)
{
    const CompObjectInstantiator *instantiator;

    instantiator = findObjectInstantiator (factory, type);
    if (!instantiator)
	return FALSE;

    return compObjectInit (factory, object, instantiator);
}

void
compObjectFiniByType (const CompObjectFactory *factory,
		      CompObject              *object,
		      const CompObjectType    *type)
{
    compObjectFini (factory, object, findObjectInstantiator (factory, type));
}

CompBool
compObjectInitByTypeName (const CompObjectFactory *factory,
			  CompObject		  *object,
			  const char		  *name)
{
    const CompObjectInstantiator *instantiator;

    instantiator = lookupObjectInstantiator (factory, name);
    if (!instantiator)
	return FALSE;

    return compObjectInit (factory, object, instantiator);
}

void
compObjectFiniByTypeName (const CompObjectFactory *factory,
			  CompObject              *object,
			  const char		  *name)
{
    compObjectFini (factory, object, lookupObjectInstantiator (factory, name));
}

CompBool
compFactoryRegisterType (CompObjectFactory    *factory,
			 const char	      *interface,
			 const CompObjectType *type)
{
    const CompObjectInstantiator *base = NULL;
    CompObjectInstantiator	 *instantiator;
    CompObjectVTable		 *vTable;
    const CompFactory		 *master;
    const CompObjectFactory	 *f;
    int				 i;

    if (type->baseName)
    {
	base = lookupObjectInstantiator (factory, type->baseName);
	if (!base)
	    return FALSE;
    }

    instantiator = malloc (sizeof (CompObjectInstantiator));
    if (!instantiator)
	return FALSE;

    vTable = malloc (type->vTableSize);
    if (!vTable)
    {
	free (instantiator);
	return FALSE;
    }

    if (interface)
    {
	instantiator->interface = strdup (interface);
	if (!instantiator->interface)
	{
	    free (vTable);
	    free (instantiator);
	    return FALSE;
	}
    }
    else
    {
	instantiator->interface = NULL;
    }

    memcpy (vTable, type->vTable, type->vTableSize);

    instantiator->base		 = base;
    instantiator->type		 = type;
    instantiator->size.len	 = 0;
    instantiator->size.sizes     = 0;
    instantiator->size.totalSize = 0;
    instantiator->privates	 = NULL;
    instantiator->nPrivates	 = 0;
    instantiator->vTable	 = vTable;

    for (f = factory; f->master; f = f->master);
    master = (const CompFactory *) f;

    for (i = 0; i < master->nEntry; i++)
    {
	if (strcmp (master->entry[i].name, type->name) == 0)
	{
	    instantiator->size = master->entry[i].size;
	    break;
	}
    }

    instantiator->next = factory->instantiator;
    factory->instantiator = instantiator;

    return TRUE;
}

static void
vTableInit (CompObjectVTable	   *vTable,
	    const CompObjectVTable *noopVTable,
	    int			   size)
{
    char		      *dVTable = (char *) vTable;
    const char		      *sVTable = (const char *) noopVTable;
    unsigned int              i;
    static const unsigned int offset =
	offsetof (CompObjectVTable, removed) -
	offsetof (CompObjectVTable, inserted);

    for (i = 0; i < size; i += offset)
    {
	ForBaseObjectProc *dst = (ForBaseObjectProc *) (dVTable + i);
	ForBaseObjectProc *src = (ForBaseObjectProc *) (sVTable + i);

	if (*src && !*dst)
	    *dst = *src;
    }
}

typedef struct _InsertObjectContext {
    CompObject *parent;
    const char *name;
} InsertObjectContext;

static CompBool
baseObjectInsertObject (CompObject *object,
			void       *closure)
{
    InsertObjectContext *pCtx = (InsertObjectContext *) closure;

    (*object->vTable->insertObject) (object, pCtx->parent, pCtx->name);

    return TRUE;
}

static void
noopInsertObject (CompObject *object,
		  CompObject *parent,
		  const char *name)
{
    InsertObjectContext ctx;

    ctx.parent = parent;
    ctx.name   = name;

    (*object->vTable->forBaseObject) (object,
				      baseObjectInsertObject,
				      (void *) &ctx);
}

static CompBool
baseObjectRemoveObject (CompObject *object,
			void       *closure)
{
    (*object->vTable->removeObject) (object);
    return TRUE;
}

static void
noopRemoveObject (CompObject *object)
{
    (*object->vTable->forBaseObject) (object, baseObjectRemoveObject, NULL);
}

static CompBool
baseObjectInserted (CompObject *object,
		    void       *closure)
{
    (*object->vTable->inserted) (object);
    return TRUE;
}

static void
noopInserted (CompObject *object)
{
    (*object->vTable->forBaseObject) (object, baseObjectInserted, NULL);
}

static CompBool
baseObjectRemoved (CompObject *object,
		    void       *closure)
{
    (*object->vTable->removed) (object);
    return TRUE;
}

static void
noopRemoved (CompObject *object)
{
    (*object->vTable->forBaseObject) (object, baseObjectRemoved, NULL);
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
    const char         *interface;
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
    const char         *interface;
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
    const char       *interface;
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

static CompBool
baseObjectInterfaceAdded (CompObject *object,
			  void       *closure)
{
    (*object->vTable->interfaceAdded) (object, (const char *) closure);
    return TRUE;
}

static void
noopInterfaceAdded (CompObject *object,
		    const char *interface)
{
    (*object->vTable->forBaseObject) (object,
				      baseObjectInterfaceAdded,
				      (void *) interface);
}

static CompBool
baseObjectInterfaceRemoved (CompObject *object,
			    void       *closure)
{
    (*object->vTable->interfaceRemoved) (object, (const char *) closure);
    return TRUE;
}

static void
noopInterfaceRemoved (CompObject *object,
		      const char *interface)
{
    (*object->vTable->forBaseObject) (object,
				      baseObjectInterfaceRemoved,
				      (void *) interface);
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

typedef struct _ConnectContext {
    const char *interface;
    size_t     offset;
    CompObject *descendant;
    const char *descendantInterface;
    size_t     descendantOffset;
    const char *details;
    va_list    args;
    int	       result;
} ConnectContext;

static CompBool
baseObjectConnect (CompObject *object,
		   void       *closure)
{
    ConnectContext *pCtx = (ConnectContext *) closure;

    pCtx->result =
	(*object->vTable->signal.connect) (object,
					   pCtx->interface,
					   pCtx->offset,
					   pCtx->descendant,
					   pCtx->descendantInterface,
					   pCtx->descendantOffset,
					   pCtx->details,
					   pCtx->args);

    return TRUE;
}

static int
noopConnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     CompObject *descendant,
	     const char *descendantInterface,
	     size_t     descendantOffset,
	     const char *details,
	     va_list	args)
{
    ConnectContext ctx;

    ctx.interface           = interface;
    ctx.offset              = offset;
    ctx.descendant          = descendant;
    ctx.descendantInterface = descendantInterface;
    ctx.descendantOffset    = descendantOffset;
    ctx.details             = details;
    ctx.args                = args;

    (*object->vTable->forBaseObject) (object,
				      baseObjectConnect,
				      (void *) &ctx);

    return ctx.result;
}

typedef struct _DisconnectContext {
    const char *interface;
    size_t     offset;
    int        index;
} DisconnectContext;

static CompBool
baseObjectDisconnect (CompObject *object,
		      void       *closure)
{
    DisconnectContext *pCtx = (DisconnectContext *) closure;

    (*object->vTable->signal.disconnect) (object,
					  pCtx->interface,
					  pCtx->offset,
					  pCtx->index);

    return TRUE;
}

static void
noopDisconnect (CompObject *object,
		const char *interface,
		size_t     offset,
		int	   index)
{
    DisconnectContext ctx;

    ctx.interface = interface;
    ctx.offset    = offset;
    ctx.index     = index;

    (*object->vTable->forBaseObject) (object,
				      baseObjectDisconnect,
				      (void *) &ctx);
}

typedef struct _SignalContext {
    const char   *path;
    const char   *interface;
    const char   *name;
    const char   *signature;
    CompAnyValue *value;
    int	         nValue;
} SignalContext;

static CompBool
baseObjectSignal (CompObject *object,
		  void       *closure)
{
    SignalContext *pCtx = (SignalContext *) closure;

    (*object->vTable->signal.signal) (object,
				      pCtx->path,
				      pCtx->interface,
				      pCtx->name,
				      pCtx->signature,
				      pCtx->value,
				      pCtx->nValue);

    return TRUE;
}

static void
noopSignal (CompObject   *object,
	    const char   *path,
	    const char   *interface,
	    const char   *name,
	    const char   *signature,
	    CompAnyValue *value,
	    int	         nValue)
{
    SignalContext ctx;

    ctx.path      = path;
    ctx.interface = interface;
    ctx.name      = name;
    ctx.signature = signature;
    ctx.value     = value;
    ctx.nValue    = nValue;

    (*object->vTable->forBaseObject) (object,
				      baseObjectSignal,
				      (void *) &ctx);
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

typedef struct _GetBoolPropContext {
    const char *interface;
    const char *name;
    CompBool   *value;
    char       **error;
} GetBoolPropContext;

static CompBool
baseObjectGetBoolProp (CompObject *object,
		       void       *closure)
{
    GetBoolPropContext *pCtx = (GetBoolPropContext *) closure;

    return (*object->vTable->properties.getBool) (object,
						  pCtx->interface,
						  pCtx->name,
						  pCtx->value,
						  pCtx->error);
}

static CompBool
noopGetBoolProp (CompObject *object,
		 const char *interface,
		 const char *name,
		 CompBool   *value,
		 char	    **error)
{
    GetBoolPropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectGetBoolProp,
					     (void *) &ctx);
}

typedef struct _SetBoolPropContext {
    const char *interface;
    const char *name;
    CompBool   value;
    char       **error;
} SetBoolPropContext;

static CompBool
baseObjectSetBoolProp (CompObject *object,
		       void       *closure)
{
    SetBoolPropContext *pCtx = (SetBoolPropContext *) closure;

    return (*object->vTable->properties.setBool) (object,
						  pCtx->interface,
						  pCtx->name,
						  pCtx->value,
						  pCtx->error);
}

static CompBool
noopSetBoolProp (CompObject *object,
		 const char *interface,
		 const char *name,
		 CompBool   value,
		 char	    **error)
{
    SetBoolPropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectSetBoolProp,
					     (void *) &ctx);
}

typedef struct _BoolPropChangedContext {
    const char *interface;
    const char *name;
    CompBool   value;
} BoolPropChangedContext;

static CompBool
baseObjectBoolPropChanged (CompObject *object,
			   void       *closure)
{
    BoolPropChangedContext *pCtx = (BoolPropChangedContext *) closure;

    (*object->vTable->properties.boolChanged) (object,
					       pCtx->interface,
					       pCtx->name,
					       pCtx->value);

    return TRUE;
}

static void
noopBoolPropChanged (CompObject *object,
		     const char *interface,
		     const char *name,
		     CompBool   value)
{
    BoolPropChangedContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;

    (*object->vTable->forBaseObject) (object,
				      baseObjectBoolPropChanged,
				      (void *) &ctx);
}

typedef struct _GetIntPropContext {
    const char *interface;
    const char *name;
    int32_t    *value;
    char       **error;
} GetIntPropContext;

static CompBool
baseObjectGetIntProp (CompObject *object,
		       void       *closure)
{
    GetIntPropContext *pCtx = (GetIntPropContext *) closure;

    return (*object->vTable->properties.getInt) (object,
						 pCtx->interface,
						 pCtx->name,
						 pCtx->value,
						 pCtx->error);
}

static CompBool
noopGetIntProp (CompObject *object,
		const char *interface,
		const char *name,
		int32_t    *value,
		char	   **error)
{
    GetIntPropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectGetIntProp,
					     (void *) &ctx);
}

typedef struct _SetIntPropContext {
    const char *interface;
    const char *name;
    int32_t    value;
    char       **error;
} SetIntPropContext;

static CompBool
baseObjectSetIntProp (CompObject *object,
		      void       *closure)
{
    SetIntPropContext *pCtx = (SetIntPropContext *) closure;

    return (*object->vTable->properties.setInt) (object,
						 pCtx->interface,
						 pCtx->name,
						 pCtx->value,
						 pCtx->error);
}

static CompBool
noopSetIntProp (CompObject *object,
		const char *interface,
		const char *name,
		int32_t    value,
		char	   **error)
{
    SetIntPropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectSetIntProp,
					     (void *) &ctx);
}

typedef struct _IntPropChangedContext {
    const char *interface;
    const char *name;
    int32_t    value;
} IntPropChangedContext;

static CompBool
baseObjectIntPropChanged (CompObject *object,
			  void       *closure)
{
    IntPropChangedContext *pCtx = (IntPropChangedContext *) closure;

    (*object->vTable->properties.intChanged) (object,
					      pCtx->interface,
					      pCtx->name,
					      pCtx->value);

    return TRUE;
}

static void
noopIntPropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    int32_t    value)
{
    IntPropChangedContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;

    (*object->vTable->forBaseObject) (object,
				      baseObjectIntPropChanged,
				      (void *) &ctx);
}

typedef struct _GetDoublePropContext {
    const char *interface;
    const char *name;
    double     *value;
    char       **error;
} GetDoublePropContext;

static CompBool
baseObjectGetDoubleProp (CompObject *object,
			 void       *closure)
{
    GetDoublePropContext *pCtx = (GetDoublePropContext *) closure;

    return (*object->vTable->properties.getDouble) (object,
						    pCtx->interface,
						    pCtx->name,
						    pCtx->value,
						    pCtx->error);
}

static CompBool
noopGetDoubleProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   double     *value,
		   char	      **error)
{
    GetDoublePropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectGetDoubleProp,
					     (void *) &ctx);
}

typedef struct _SetDoublePropContext {
    const char *interface;
    const char *name;
    double     value;
    char       **error;
} SetDoublePropContext;

static CompBool
baseObjectSetDoubleProp (CompObject *object,
			 void       *closure)
{
    SetDoublePropContext *pCtx = (SetDoublePropContext *) closure;

    return (*object->vTable->properties.setDouble) (object,
						    pCtx->interface,
						    pCtx->name,
						    pCtx->value,
						    pCtx->error);
}

static CompBool
noopSetDoubleProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   double     value,
		   char	      **error)
{
    SetDoublePropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectSetDoubleProp,
					     (void *) &ctx);
}

typedef struct _DoublePropChangedContext {
    const char *interface;
    const char *name;
    double     value;
} DoublePropChangedContext;

static CompBool
baseObjectDoublePropChanged (CompObject *object,
			     void       *closure)
{
    DoublePropChangedContext *pCtx = (DoublePropChangedContext *) closure;

    (*object->vTable->properties.doubleChanged) (object,
						 pCtx->interface,
						 pCtx->name,
						 pCtx->value);

    return TRUE;
}

static void
noopDoublePropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       double     value)
{
    DoublePropChangedContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;

    (*object->vTable->forBaseObject) (object,
				      baseObjectDoublePropChanged,
				      (void *) &ctx);
}

typedef struct _GetStringPropContext {
    const char *interface;
    const char *name;
    char       **value;
    char       **error;
} GetStringPropContext;

static CompBool
baseObjectGetStringProp (CompObject *object,
			 void       *closure)
{
    GetStringPropContext *pCtx = (GetStringPropContext *) closure;

    return (*object->vTable->properties.getString) (object,
						    pCtx->interface,
						    pCtx->name,
						    pCtx->value,
						    pCtx->error);
}

static CompBool
noopGetStringProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   char	      **value,
		   char	      **error)
{
    GetStringPropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectGetStringProp,
					     (void *) &ctx);
}

typedef struct _SetStringPropContext {
    const char *interface;
    const char *name;
    const char *value;
    char       **error;
} SetStringPropContext;

static CompBool
baseObjectSetStringProp (CompObject *object,
			 void       *closure)
{
    SetStringPropContext *pCtx = (SetStringPropContext *) closure;

    return (*object->vTable->properties.setString) (object,
						    pCtx->interface,
						    pCtx->name,
						    pCtx->value,
						    pCtx->error);
}

static CompBool
noopSetStringProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   const char *value,
		   char	      **error)
{
    SetStringPropContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectSetStringProp,
					     (void *) &ctx);
}

typedef struct _StringPropChangedContext {
    const char *interface;
    const char *name;
    const char *value;
} StringPropChangedContext;

static CompBool
baseObjectStringPropChanged (CompObject *object,
			     void       *closure)
{
    StringPropChangedContext *pCtx = (StringPropChangedContext *) closure;

    (*object->vTable->properties.stringChanged) (object,
						 pCtx->interface,
						 pCtx->name,
						 pCtx->value);

    return TRUE;
}

static void
noopStringPropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       const char *value)
{
    StringPropChangedContext ctx;

    ctx.interface = interface;
    ctx.name      = name;
    ctx.value     = value;

    (*object->vTable->forBaseObject) (object,
				      baseObjectStringPropChanged,
				      (void *) &ctx);
}

typedef struct _GetMetadataContext {
    const char *interface;
    char       **data;
    char       **error;
} GetMetadataContext;

static CompBool
baseObjectGetMetadata (CompObject *object,
		       void       *closure)
{
    GetMetadataContext *pCtx = (GetMetadataContext *) closure;

    return (*object->vTable->metadata.get) (object,
					    pCtx->interface,
					    pCtx->data,
					    pCtx->error);
}

static CompBool
noopGetMetadata (CompObject *object,
		 const char *interface,
		 char       **data,
		 char	    **error)
{
    GetMetadataContext ctx;

    ctx.interface = interface;
    ctx.data      = data;
    ctx.error     = error;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectGetMetadata,
					     (void *) &ctx);
}

#define CCONTEXT(vTable, pCtx)				   \
    (*((GetCContextProc) (vTable)->unused)) (object, pCtx)

CompBool
cForBaseObject (CompObject	       *object,
		BaseObjectCallBackProc proc,
		void		       *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;
    CContext		ctx;

    CCONTEXT (object->vTable, &ctx);

    UNWRAP (ctx.vtStore, object, vTable);
    status = (*proc) (object, closure);
    WRAP (ctx.vtStore, object, vTable, v.vTable);

    return status;
}

#define CHILD(data, child)		      \
    ((CompObject *) ((data) + (child)->offset))

void
cInsertObject (CompObject *object,
	       CompObject *parent,
	       const char *name)
{
    CompObject *child;
    CContext   ctx;
    int        i, j;

    CCONTEXT (object->vTable, &ctx);

    noopInsertObject (object, parent, name);

    for (i = 0; i < ctx.nInterface; i++)
    {
	for (j = 0; j < ctx.interface[i].nChild; j++)
	{
	    if (ctx.interface[i].child[j].type)
	    {
		child = CHILD (ctx.data, &ctx.interface[i].child[j]);
		(*child->vTable->insertObject) (child, object, child->name);
	    }
	}
    }
}

void
cRemoveObject (CompObject *object)
{
    CompObject *child;
    CContext   ctx;
    int        i, j;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
    {
	for (j = 0; j < ctx.interface[i].nChild; j++)
	{
	    if (ctx.interface[i].child[j].type)
	    {
		child = CHILD (ctx.data, &ctx.interface[i].child[j]);
		(*child->vTable->removeObject) (child);
	    }
	}
    }

    noopRemoveObject (object);
}

void
cInserted (CompObject *object)
{
    CompObject *child;
    CContext   ctx;
    int        i, j;

    CCONTEXT (object->vTable, &ctx);

    noopInserted (object);

    for (i = 0; i < ctx.nInterface; i++)
    {
	(*object->vTable->interfaceAdded) (object, ctx.interface[i].name);

	for (j = 0; j < ctx.interface[i].nChild; j++)
	{
	    if (ctx.interface[i].child[j].type)
	    {
		child = CHILD (ctx.data, &ctx.interface[i].child[j]);
		(*child->vTable->inserted) (child);
	    }
	}
    }
}

void
cRemoved (CompObject *object)
{
    CompObject *child;
    CContext   ctx;
    int        i, j;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
    {
	for (j = 0; j < ctx.interface[i].nChild; j++)
	{
	    if (ctx.interface[i].child[j].type)
	    {
		child = CHILD (ctx.data, &ctx.interface[i].child[j]);
		(*child->vTable->removed) (child);
	    }
	}

	(*object->vTable->interfaceRemoved) (object, ctx.interface[i].name);
    }

    noopRemoved (object);
}

CompBool
cForEachInterface (CompObject	         *object,
		   InterfaceCallBackProc proc,
		   void		         *closure)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (!(*proc) (object,
		      ctx.interface[i].name,
		      ctx.interface[i].offset,
		      ctx.type,
		      closure))
	    return FALSE;

    return noopForEachInterface (object, proc, closure);
}

CompBool
cForEachChildObject (CompObject		     *object,
		     ChildObjectCallBackProc proc,
		     void		     *closure)
{
    CContext ctx;
    int      i, j;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	for (j = 0; j < ctx.interface[i].nChild; j++)
	    if (!(*proc) (CHILD (ctx.data, &ctx.interface[i].child[j]),
			  closure))
		return FALSE;

    return noopForEachChildObject (object, proc, closure);
}

CompBool
cForEachMethod (CompObject	   *object,
		const char	   *interface,
		MethodCallBackProc proc,
		void	           *closure)
{
    CContext ctx;
    int      i, j;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, ctx.interface[i].name))
		continue;

	for (j = 0; j < ctx.interface[i].nMethod; j++)
	    if (!(*proc) (object,
			  ctx.interface[i].method[j].name,
			  ctx.interface[i].method[j].in,
			  ctx.interface[i].method[j].out,
			  ctx.interface[i].method[j].offset,
			  ctx.interface[i].method[j].marshal,
			  closure))
		return FALSE;
    }

    return noopForEachMethod (object, interface, proc, closure);
}

CompBool
cForEachSignal (CompObject	   *object,
		const char	   *interface,
		SignalCallBackProc proc,
		void		   *closure)
{
    CContext ctx;
    int      i, j;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, ctx.interface[i].name))
		continue;

	for (j = 0; j < ctx.interface[i].nSignal; j++)
	    if (ctx.interface[i].signal[j]->out)
		if (!(*proc) (object,
			      ctx.interface[i].signal[j]->name,
			      ctx.interface[i].signal[j]->out,
			      ctx.interface[i].signal[j]->offset,
			      closure))
		    return FALSE;
    }

    return noopForEachSignal (object, interface, proc, closure);
}

static CompBool
handleForEachBoolProp (CompObject	*object,
		       const CBoolProp  *prop,
		       int		nProp,
		       PropCallBackProc	proc,
		       void		*closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_BOOLEAN, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachIntProp (CompObject       *object,
		      const CIntProp   *prop,
		      int	       nProp,
		      PropCallBackProc proc,
		      void	       *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_INT32, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachDoubleProp (CompObject	   *object,
			 const CDoubleProp *prop,
			 int		   nProp,
			 PropCallBackProc  proc,
			 void		   *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_DOUBLE, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachStringProp (CompObject	   *object,
			 const CStringProp *prop,
			 int		   nProp,
			 PropCallBackProc  proc,
			 void		   *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_STRING, closure))
	    return FALSE;

    return TRUE;
}

CompBool
cForEachProp (CompObject       *object,
	      const char       *interface,
	      PropCallBackProc proc,
	      void	       *closure)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, ctx.interface[i].name))
		continue;

	if (!handleForEachBoolProp (object,
				    ctx.interface[i].boolProp,
				    ctx.interface[i].nBoolProp,
				    proc, closure))
	    return FALSE;

	if (!handleForEachIntProp (object,
				   ctx.interface[i].intProp,
				   ctx.interface[i].nIntProp,
				   proc, closure))
	    return FALSE;

	if (!handleForEachDoubleProp (object,
				      ctx.interface[i].doubleProp,
				      ctx.interface[i].nDoubleProp,
				      proc, closure))
	    return FALSE;

	if (!handleForEachStringProp (object,
				      ctx.interface[i].stringProp,
				      ctx.interface[i].nStringProp,
				      proc, closure))
	    return FALSE;
    }

    return noopForEachProp (object, interface, proc, closure);
}

static int
getVersion (CompObject *object,
	    const char *interface)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	if (strcmp (interface, objectInterface[i].name) == 0)
	    return COMPIZ_OBJECT_VERSION;

    return 0;
}

int
cGetVersion (CompObject *object,
	     const char *interface)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return ctx.version;

    return noopGetVersion (object, interface);
}

static CompBool
forBaseObject (CompObject	      *object,
	       BaseObjectCallBackProc proc,
	       void		      *closure)
{
    return TRUE;
}

static void
insertObject (CompObject *object,
	      CompObject *parent,
	      const char *name)
{
    object->parent = parent;
    object->name   = name;
}

static void
removeObject (CompObject *object)
{
    object->parent = NULL;
    object->name   = NULL;
}

static void
inserted (CompObject *object)
{
    int i;

    C_EMIT_SIGNAL_INT (object, InsertedProc, 0, object->signalVec,
		       &insertedSignal);

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	(*object->vTable->interfaceAdded) (object, objectInterface[i].name);
}

static void
removed (CompObject *object)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	(*object->vTable->interfaceRemoved) (object, objectInterface[i].name);

    C_EMIT_SIGNAL_INT (object, RemovedProc, 0, object->signalVec,
		       &removedSignal);
}

static int
getSignalVecSize (const CInterface *interface,
		  int		   nInterface)
{
    int	i, size = 0;

    for (i = 0; i < nInterface; i++)
	size += interface[i].nSignal;

    return size;
}

static void
interfaceAdded (CompObject *object,
		const char *interface)
{
    C_EMIT_SIGNAL_INT (object, InterfaceAddedProc, 0, object->signalVec,
		       &interfaceAddedSignal,
		       interface);
}

static void
interfaceRemoved (CompObject *object,
		  const char *interface)
{
    CompObject *node;

    for (node = object; node; node = node->parent)
    {
	if (node->signalVec)
	{
	    int i;

	    for (i = 0; node->signalVec[i] != &lastSignalVecEntry; i++)
	    {
		CompSignalHandler *handler, *prev;

		if (node->signalVec[i] == &emptySignalVecEntry)
		    continue;

		prev    = NULL;
		handler = node->signalVec[i];

		while (handler)
		{
		    if (handler->object == object)
		    {
			if (strcmp (handler->header->interface, interface) == 0)
			{
			    if (prev)
			    {
				prev->next = handler->next;
				free (handler);
				handler = prev->next;
			    }
			    else
			    {
				node->signalVec[i] = handler->next;
				free (handler);
				handler = node->signalVec[i];
			    }

			    continue;
			}
		    }

		    prev    = handler;
		    handler = handler->next;
		}
	    }
	}
    }

    C_EMIT_SIGNAL_INT (object, InterfaceRemovedProc, 0, object->signalVec,
		       &interfaceRemovedSignal,
		       interface);
}

static CompBool
signalIndex (const char	      *name,
	     const CInterface *interface,
	     int	      nInterface,
	     size_t	      offset,
	     int	      *index,
	     const char	      **signature)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	if (strcmp (name, interface[i].name) == 0)
	{
	    for (j = 0; j < interface[i].nSignal; j++)
	    {
		if (offset == interface[i].signal[j]->offset)
		{
		    if (signature)
			*signature = interface[i].signal[j]->out;

		    *index = interface[i].signal[j]->index;

		    return TRUE;
		}
	    }

	    *index = -1;

	    return TRUE;
	}
    }

    return FALSE;
}

CompSignalHandler **
compGetSignalVecRange (CompObject *object,
		       int	  size,
		       int	  *offset)
{
    CompSignalHandler **vec;
    int		      start = 0;

    if (object->signalVec)
    {
	int i, empty = 0;

	if (!offset)
	    return object->signalVec;

	if (*offset)
	    return &object->signalVec[*offset];

	if (!size)
	    return NULL;

	/* find empty slot */
	for (i = 0; object->signalVec[i] != &lastSignalVecEntry; i++)
	{
	    if (object->signalVec[i] == &emptySignalVecEntry)
	    {
		empty++;

		if (empty == size)
		{
		    start = i - size + 1;

		    memset (&object->signalVec[start], 0,
			    sizeof (CompSignalHandler **) * size);

		    *offset = start;

		    return &object->signalVec[start];
		}
	    }
	    else
	    {
		empty = 0;
	    }
	}

	start = i;
    }
    else
    {
	if (offset)
	    start = getSignalVecSize (objectInterface,
				      N_ELEMENTS (objectInterface));
    }

    vec = realloc (object->signalVec, sizeof (CompSignalHandler **) *
		   (start + size + 1));
    if (!vec)
	return NULL;

    if (object->signalVec)
	memset (&vec[start], 0, sizeof (CompSignalHandler **) * size);
    else
	memset (vec, 0, sizeof (CompSignalHandler **) * (size + start));

    vec[start + size] = &lastSignalVecEntry;
    object->signalVec = vec;

    if (offset)
	*offset = start;

    return &object->signalVec[start];
}

void
compFreeSignalVecRange (CompObject *object,
			int	   size,
			int	   offset)
{
    while (size--)
	object->signalVec[offset + size] = &emptySignalVecEntry;
}

typedef struct _HandleConnectContext {
    const char		   *interface;
    size_t		   offset;
    char		   *in;
    CompBool		   out;
    const CompObjectVTable *vTable;
    MethodMarshalProc	   marshal;
} HandleConnectContext;

static CompBool
connectMethod (CompObject	 *object,
	       const char	 *name,
	       const char	 *in,
	       const char	 *out,
	       size_t		 offset,
	       MethodMarshalProc marshal,
	       void		 *closure)
{
    HandleConnectContext *pCtx = (HandleConnectContext *) closure;

    if (offset != pCtx->offset)
	return TRUE;

    if (out[0] != '\0')
    {
	pCtx->out = TRUE;
	return FALSE;
    }

    pCtx->out     = FALSE;
    pCtx->in      = strdup (in);
    pCtx->offset  = offset;
    pCtx->marshal = marshal;

    return FALSE;
}

static CompBool
connectInterface (CompObject	       *object,
		  const char	       *name,
		  size_t	       offset,
		  const CompObjectType *type,
		  void		       *closure)
{
    HandleConnectContext *pCtx = (HandleConnectContext *) closure;

    if (strcmp (name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object, name, connectMethod,
					       closure))
	{
	    /* method must not have any output arguments */
	    if (pCtx->out)
		return TRUE;
	}

	/* need vTable if interface is not part of object type */
	if (!type)
	    pCtx->vTable = object->vTable;

	/* add interface vTable offset to method offset set by
	   connectMethod function */
	pCtx->offset += offset;

	return FALSE;
    }

    return TRUE;
}

static CompBool
handleConnect (CompObject	 *object,
	       const CInterface  *interface,
	       int		 nInterface,
	       char		 *data,
	       size_t		 svOffset,
	       const char	 *name,
	       size_t		 offset,
	       CompObject	 *descendant,
	       const char	 *descendantInterface,
	       size_t		 descendantOffset,
	       const char	 *details,
	       va_list		 args,
	       int		 *id)
{
    const char *in;
    int	       index;

    if (signalIndex (name, interface, nInterface, offset, &index, &in))
    {
	HandleConnectContext ctx;
	CompSignalHandler    *handler;
	int		     size;
	CompSignalHandler    **vec;

	if (index < 0)
	    return -1;

	/* make sure details match if signal got a signature */
	if (in && details && strncmp (in, details, strlen (details)))
	    return -1;

	if (!details)
	    details = "";

	ctx.interface = descendantInterface;
	ctx.in	      = NULL;
	ctx.vTable    = NULL;
	ctx.offset    = descendantOffset;
	ctx.marshal   = NULL;

	if ((*descendant->vTable->forEachInterface) (descendant,
						     connectInterface,
						     (void *) &ctx))
	    return -1;

	/* make sure signatures match */
	if (ctx.in && in && strcmp (ctx.in, in))
	{
	    free (ctx.in);
	    return -1;
	}

	if (ctx.in)
	    free (ctx.in);

	vec = compGetSignalVecRange (object,
				     getSignalVecSize (interface, nInterface),
				     (int *) (data + svOffset));
	if (!vec)
	    return -1;

	size = compSerializeMethodCall (object,
					descendant,
					descendantInterface,
					name,
					details,
					args,
					NULL,
					0);

	handler = malloc (sizeof (CompSignalHandler) + size);
	if (!handler)
	    return -1;

	handler->next    = NULL;
	handler->object  = descendant;
	handler->vTable  = ctx.vTable;
	handler->offset  = ctx.offset;
	handler->marshal = ctx.marshal;
	handler->header  = (CompSerializedMethodCallHeader *) (handler + 1);

	compSerializeMethodCall (object,
				 descendant,
				 descendantInterface,
				 name,
				 details,
				 args,
				 handler->header,
				 size);

	if (vec[index])
	{
	    CompSignalHandler *last;

	    for (last = vec[index]; last->next; last = last->next);

	    handler->id = last->id + 1;
	    last->next  = handler;
	}
	else
	{
	    handler->id = 1;
	    vec[index]  = handler;
	}

	*id = handler->id;
	return TRUE;
    }

    return FALSE;
}

static CompBool
handleDisconnect (CompObject	    *object,
		  const CInterface  *interface,
		  int		    nInterface,
		  char		    *data,
		  size_t	    svOffset,
		  const char	    *name,
		  size_t	    offset,
		  int		    id)
{
    int	index;

    if (signalIndex (name, interface, nInterface, offset, &index, NULL))
    {
	CompSignalHandler *handler, *prev = NULL;
	int		  *offset = (int *) (data + svOffset);
	CompSignalHandler **vec = object->signalVec;

	if (index < 0)
	    return TRUE;

	if (offset)
	{
	    if (!*offset)
		return TRUE;

	    object->signalVec += *offset;
	}

	for (handler = vec[index]; handler; handler = handler->next)
	{
	    if (handler->id == id)
	    {
		if (prev)
		    prev->next = handler->next;
		else
		    vec[index] = handler->next;

		break;
	    }

	    prev = handler;
	}

	if (handler)
	    free (handler);

	return TRUE;
    }

    return FALSE;
}

int
cConnect (CompObject *object,
	  const char *interface,
	  size_t     offset,
	  CompObject *descendant,
	  const char *descendantInterface,
	  size_t     descendantOffset,
	  const char *details,
	  va_list    args)
{
    CContext ctx;
    int      id;

    CCONTEXT (object->vTable, &ctx);

    if (handleConnect (object,
		       ctx.interface, ctx.nInterface,
		       ctx.data, ctx.svOffset,
		       interface,
		       offset,
		       descendant,
		       descendantInterface,
		       descendantOffset,
		       details,
		       args,
		       &id))
	return id;

    return noopConnect (object, interface, offset,
			descendant, descendantInterface, descendantOffset,
			details, args);
}

void
cDisconnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     int	id)
{
    CContext ctx;

    CCONTEXT (object->vTable, &ctx);

    if (handleDisconnect (object,
			  ctx.interface, ctx.nInterface,
			  ctx.data, ctx.svOffset,
			  interface,
			  offset,
			  id))
	return;

    noopDisconnect (object, interface, offset, id);
}

static int
connect (CompObject *object,
	 const char *interface,
	 size_t     offset,
	 CompObject *descendant,
	 const char *descendantInterface,
	 size_t     descendantOffset,
	 const char *details,
	 va_list    args)
{
    int id;

    if (handleConnect (object,
		       objectInterface, N_ELEMENTS (objectInterface),
		       NULL, 0,
		       interface,
		       offset,
		       descendant,
		       descendantInterface,
		       descendantOffset,
		       details,
		       args,
		       &id))
	return id;

    return -1;
}

static void
disconnect (CompObject *object,
	    const char *interface,
	    size_t     offset,
	    int	       id)
{
    handleDisconnect (object,
		      objectInterface, N_ELEMENTS (objectInterface),
		      NULL, 0,
		      interface,
		      offset,
		      id);
}

static void
signal (CompObject   *object,
	const char   *path,
	const char   *interface,
	const char   *name,
	const char   *signature,
	CompAnyValue *value,
	int	     nValue)
{
    C_EMIT_SIGNAL_INT (object, SignalProc, 0, object->signalVec, &signalSignal,
		       path, interface, name, signature, value, nValue);
}

static CompBool
handleGetBoolProp (CompObject	   *object,
		   const CBoolProp *prop,
		   int		   nProp,
		   void		   *data,
		   const char	   *interface,
		   const char	   *name,
		   CompBool	   *value,
		   char		   **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    *value = *((CompBool *) ((char *) data + prop[i].base.offset));
	    return TRUE;
	}
    }

    return noopGetBoolProp (object, interface, name, value, error);
}

CompBool
cGetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   *value,
	      char	 **error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleGetBoolProp (object,
				      ctx.interface[i].boolProp,
				      ctx.interface[i].nBoolProp,
				      ctx.data,
				      interface,
				      name,
				      value,
				      error);

    return noopGetBoolProp (object, interface, name, value, error);
}

static CompBool
handleSetBoolProp (CompObject	   *object,
		   const CBoolProp *prop,
		   int		   nProp,
		   void		   *data,
		   const char	   *interface,
		   const char	   *name,
		   CompBool	   value,
		   char		   **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    CompBool *ptr = (CompBool *) ((char *) data + prop[i].base.offset);
	    CompBool oldValue = *ptr;

	    if (prop[i].set)
	    {
		if (!(*prop[i].set) (object, interface, name, value, error))
		    return FALSE;
	    }
	    else
	    {
		*ptr = value;
	    }

	    if (*ptr != oldValue)
		(*object->vTable->properties.boolChanged) (object,
							   interface, name,
							   *ptr);

	    return TRUE;
	}
    }

    return noopSetBoolProp (object, interface, name, value, error);
}

CompBool
cSetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   value,
	      char	 **error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleSetBoolProp (object,
				      ctx.interface[i].boolProp,
				      ctx.interface[i].nBoolProp,
				      ctx.data,
				      interface,
				      name,
				      value,
				      error);

    return noopSetBoolProp (object, interface, name, value, error);
}

static void
handleBoolPropChanged (CompObject      *object,
		       const CBoolProp *prop,
		       int	       nProp,
		       const char      *interface,
		       const char      *name,
		       CompBool	       value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
cBoolPropChanged (CompObject *object,
		  const char *interface,
		  const char *name,
		  CompBool   value)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    handleBoolPropChanged (object,
				   ctx.interface[i].boolProp,
				   ctx.interface[i].nBoolProp,
				   interface,
				   name,
				   value);

    noopBoolPropChanged (object, interface, name, value);
}

static CompBool
getBoolProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     CompBool   *value,
	     char	**error)
{
    if (error)
	*error = strdup ("No such boolean property");

    return FALSE;
}

static CompBool
setBoolProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     CompBool   value,
	     char       **error)
{
    if (error)
	*error = strdup ("No such boolean property");

    return FALSE;
}

static void
boolPropChanged (CompObject *object,
		 const char *interface,
		 const char *name,
		 CompBool   value)
{
    C_EMIT_SIGNAL_INT (object, BoolPropChangedProc, 0, object->signalVec,
		       &boolChangedSignal,
		       interface, name, value);
}

static CompBool
handleGetIntProp (CompObject	 *object,
		  const CIntProp *prop,
		  int		 nProp,
		  void		 *data,
		  const char	 *interface,
		  const char	 *name,
		  int32_t	 *value,
		  char		 **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    *value = *((int32_t *) ((char *) data + prop[i].base.offset));
	    return TRUE;
	}
    }

    return noopGetIntProp (object, interface, name, value, error);
}

CompBool
cGetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    *value,
	     char	**error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleGetIntProp (object,
				     ctx.interface[i].intProp,
				     ctx.interface[i].nIntProp,
				     ctx.data,
				     interface,
				     name,
				     value,
				     error);

    return noopGetIntProp (object, interface, name, value, error);
}

static CompBool
handleSetIntProp (CompObject	 *object,
		  const CIntProp *prop,
		  int		 nProp,
		  void		 *data,
		  const char	 *interface,
		  const char	 *name,
		  int32_t	 value,
		  char		 **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    int32_t *ptr = (int32_t *) ((char *) data + prop[i].base.offset);
	    int32_t oldValue = *ptr;

	    if (prop[i].set)
	    {
		if (!(*prop[i].set) (object, interface, name, value, error))
		    return FALSE;
	    }
	    else
	    {
		if (prop[i].restriction)
		{
		    if (value > prop[i].max)
		    {
			if (error)
			    *error = strdup ("Value is greater than maximium "
					     "allowed value");

			return FALSE;
		    }
		    else if (value < prop[i].min)
		    {
			if (error)
			    *error = strdup ("Value is less than minimuim "
					     "allowed value");

			return FALSE;
		    }
		}

		*ptr = value;
	    }

	    if (*ptr != oldValue)
		(*object->vTable->properties.intChanged) (object,
							  interface, name,
							  *ptr);

	    return TRUE;
	}
    }

    return noopSetIntProp (object, interface, name, value, error);
}

CompBool
cSetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    value,
	     char	**error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleSetIntProp (object,
				     ctx.interface[i].intProp,
				     ctx.interface[i].nIntProp,
				     ctx.data,
				     interface,
				     name,
				     value,
				     error);

    return noopSetIntProp (object, interface, name, value, error);
}

static void
handleIntPropChanged (CompObject     *object,
		      const CIntProp *prop,
		      int	     nProp,
		      const char     *interface,
		      const char     *name,
		      int32_t	     value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
cIntPropChanged (CompObject *object,
		 const char *interface,
		 const char *name,
		 int32_t    value)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    handleIntPropChanged (object,
				  ctx.interface[i].intProp,
				  ctx.interface[i].nIntProp,
				  interface,
				  name,
				  value);

    noopIntPropChanged (object, interface, name, value);
}

static CompBool
getIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    *value,
	     char	**error)
{
    if (error)
	*error = strdup ("No such int32 property");

    return FALSE;
}

static CompBool
setIntProp (CompObject *object,
	    const char *interface,
	    const char *name,
	    int32_t    value,
	    char       **error)
{
    if (error)
	*error = strdup ("No such int32 property");

    return FALSE;
}

static void
intPropChanged (CompObject *object,
		const char *interface,
		const char *name,
		int32_t    value)
{
    C_EMIT_SIGNAL_INT (object, IntPropChangedProc, 0, object->signalVec,
		       &intChangedSignal,
		       interface, name, value);
}

static CompBool
handleGetDoubleProp (CompObject	       *object,
		     const CDoubleProp *prop,
		     int	       nProp,
		     void	       *data,
		     const char	       *interface,
		     const char	       *name,
		     double	       *value,
		     char	       **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    *value = *((double *) ((char *) data + prop[i].base.offset));
	    return TRUE;
	}
    }

    return noopGetDoubleProp (object, interface, name, value, error);
}

CompBool
cGetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double     *value,
		char	   **error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleGetDoubleProp (object,
					ctx.interface[i].doubleProp,
					ctx.interface[i].nDoubleProp,
					ctx.data,
					interface,
					name,
					value,
					error);

    return noopGetDoubleProp (object, interface, name, value, error);
}

static CompBool
handleSetDoubleProp (CompObject	       *object,
		     const CDoubleProp *prop,
		     int	       nProp,
		     void	       *data,
		     const char	       *interface,
		     const char	       *name,
		     double	       value,
		     char	       **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    double *ptr = (double *) ((char *) data + prop[i].base.offset);
	    double oldValue = *ptr;

	    if (prop[i].set)
	    {
		if (!(*prop[i].set) (object, interface, name, value, error))
		    return FALSE;
	    }
	    else
	    {
		if (prop[i].restriction)
		{
		    if (value > prop[i].max)
		    {
			if (error)
			    *error = strdup ("Value is greater than maximium "
					     "allowed value");

			return FALSE;
		    }
		    else if (value < prop[i].min)
		    {
			if (error)
			    *error = strdup ("Value is less than minimuim "
					     "allowed value");

			return FALSE;
		    }
		}

		*ptr = value;
	    }

	    if (*ptr != oldValue)
		(*object->vTable->properties.doubleChanged) (object,
							     interface, name,
							     *ptr);

	    return TRUE;
	}
    }

    return noopSetDoubleProp (object, interface, name, value, error);
}

CompBool
cSetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double     value,
		char	   **error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleSetDoubleProp (object,
					ctx.interface[i].doubleProp,
					ctx.interface[i].nDoubleProp,
					ctx.data,
					interface,
					name,
					value,
					error);

    return noopSetDoubleProp (object, interface, name, value, error);
}

static void
handleDoublePropChanged (CompObject	   *object,
			 const CDoubleProp *prop,
			 int		   nProp,
			 const char	   *interface,
			 const char	   *name,
			 double		   value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
cDoublePropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    double     value)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    handleDoublePropChanged (object,
				     ctx.interface[i].doubleProp,
				     ctx.interface[i].nDoubleProp,
				     interface,
				     name,
				     value);

    noopDoublePropChanged (object, interface, name, value);
}

static CompBool
getDoubleProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       double     *value,
	       char	  **error)
{
    if (error)
	*error = strdup ("No such double property");

    return FALSE;
}

static CompBool
setDoubleProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       double     value,
	       char       **error)
{
    if (error)
	*error = strdup ("No such double property");

    return FALSE;
}

static void
doublePropChanged (CompObject *object,
		   const char *interface,
		   const char *name,
		   double     value)
{
    C_EMIT_SIGNAL_INT (object, DoublePropChangedProc, 0, object->signalVec,
		       &doubleChangedSignal,
		       interface, name, value);
}

static CompBool
handleGetStringProp (CompObject	       *object,
		     const CStringProp *prop,
		     int		nProp,
		     void		*data,
		     const char		*interface,
		     const char		*name,
		     char		**value,
		     char		**error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    char *s;

	    s = strdup (*((char **) ((char *) data + prop[i].base.offset)));
	    if (!s)
	    {
		if (error)
		    *error = strdup ("Failed to copy string value");

		return FALSE;
	    }

	    *value = s;

	    return TRUE;
	}
    }

    return noopGetStringProp (object, interface, name, value, error);
}

CompBool
cGetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		char       **value,
		char	   **error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleGetStringProp (object,
					ctx.interface[i].stringProp,
					ctx.interface[i].nStringProp,
					ctx.data,
					interface,
					name,
					value,
					error);

    return noopGetStringProp (object, interface, name, value, error);
}

static CompBool
handleSetStringProp (CompObject	       *object,
		     const CStringProp *prop,
		     int	       nProp,
		     void	       *data,
		     const char	       *interface,
		     const char	       *name,
		     const char	       *value,
		     char	       **error)
{
    int i;

    for (i = 0; i < nProp; i++)
    {
	if (strcmp (name, prop[i].base.name) == 0)
	{
	    char **ptr = (char **) ((char *) data + prop[i].base.offset);
	    char *oldValue = *ptr;

	    if (prop[i].set)
	    {
		if (!(*prop[i].set) (object, interface, name, value, error))
		    return FALSE;
	    }
	    else
	    {
		if (strcmp (*ptr, value))
		{
		    char *s;

		    s = strdup (value);
		    if (!s)
		    {
			if (error)
			    *error = strdup ("Failed to copy string value");

			return FALSE;
		    }

		    free (*ptr);
		    *ptr = s;
		}
	    }

	    if (*ptr != oldValue)
		(*object->vTable->properties.stringChanged) (object,
							     interface, name,
							     *ptr);

	    return TRUE;
	}
    }

    return noopSetStringProp (object, interface, name, value, error);
}

CompBool
cSetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		const char *value,
		char	   **error)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleSetStringProp (object,
					ctx.interface[i].stringProp,
					ctx.interface[i].nStringProp,
					ctx.data,
					interface,
					name,
					value,
					error);

    return noopSetStringProp (object, interface, name, value, error);
}

static void
handleStringPropChanged (CompObject	   *object,
			 const CStringProp *prop,
			 int		   nProp,
			 const char	   *interface,
			 const char	   *name,
			 const char	   *value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
cStringPropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    const char *value)
{
    CContext ctx;
    int      i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    handleStringPropChanged (object,
				     ctx.interface[i].stringProp,
				     ctx.interface[i].nStringProp,
				     interface,
				     name,
				     value);

    noopStringPropChanged (object, interface, name, value);
}

static CompBool
getStringProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       char       **value,
	       char	  **error)
{
    if (error)
	*error = strdup ("No such string property");

    return FALSE;
}

static CompBool
setStringProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       const char *value,
	       char       **error)
{
    if (error)
	*error = strdup ("No such string property");

    return FALSE;
}

static void
stringPropChanged (CompObject *object,
		   const char *interface,
		   const char *name,
		   const char *value)
{
    C_EMIT_SIGNAL_INT (object, StringPropChangedProc, 0, object->signalVec,
		       &stringChangedSignal,
		       interface, name, value);
}

#define HOME_DATADIR   ".compiz-0/data"
#define XML_EXTENSION ".xml"

typedef CompBool (*ForMetadataFileProc) (FILE	     *fp,
					 const char  *path,
					 struct stat *buf,
					 void	     *closure);

static CompBool
forMetadataFile (const char	     *path,
		 ForMetadataFileProc proc,
		 void		     *closure)
{
    FILE	*fp;
    struct stat buf;
    CompBool	status = TRUE;

    if (stat (path, &buf) == 0)
    {
	fp = fopen (path, "r");
	if (fp)
	{
	    status = (*proc) (fp, path, &buf, closure);
	    fclose (fp);
	}
    }

    return status;
}

static CompBool
forEachMetadataFile (const char	         *file,
		     ForMetadataFileProc proc,
		     void		 *closure)
{
    CompBool status = TRUE;
    char     *home;
    char     *path;

    home = getenv ("HOME");
    if (home)
    {
	path = malloc (strlen (home) + strlen (HOME_DATADIR) +
		       strlen (file) + strlen (XML_EXTENSION) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s%s%s", home, HOME_DATADIR, file,
		     XML_EXTENSION);

	    status = forMetadataFile (path, proc, closure);

	    free (path);
	}
    }

    if (status)
    {
	path = malloc (strlen (METADATADIR) + strlen (file) +
		       strlen (XML_EXTENSION) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s%s", METADATADIR, file, XML_EXTENSION);

	    status = forMetadataFile (path, proc, closure);

	    free (path);
	}
    }

    return status;
}

static CompBool
readMetadata (FILE	  *fp,
	      const char  *path,
	      struct stat *buf,
	      void	  *closure)
{
    char *data;

    data = malloc (buf->st_size + 1);
    if (data)
    {
	data[fread (data, 1, buf->st_size, fp)] = '\0';
	*((char **) closure) = data;

	return FALSE;
    }

    return TRUE;
}

static CompBool
handleGetMetadata (CompObject *object,
		   const char *interface,
		   char       **data,
		   char       **error)
{
    if (forEachMetadataFile (interface, readMetadata, data))
    {
	char *str;

	str = strdup ("<compiz/>");
	if (str)
	{
	    *data = str;
	}
	else
	{
	    esprintf (error, NO_MEMORY_ERROR_STRING);
	    return FALSE;
	}
    }

    return TRUE;
}

static CompBool
getMetadata (CompObject *object,
	     const char *interface,
	     char	**data,
	     char	**error)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	if (strcmp (interface, objectInterface[i].name) == 0)
	    return handleGetMetadata (object, interface, data, error);

    esprintf (error, "No \"%s\" interface", interface);
    return FALSE;
}

CompBool
cGetMetadata (CompObject *object,
	      const char *interface,
	      char	 **data,
	      char	 **error)
{
    CContext ctx;
    int	     i;

    CCONTEXT (object->vTable, &ctx);

    for (i = 0; i < ctx.nInterface; i++)
	if (strcmp (interface, ctx.interface[i].name) == 0)
	    return handleGetMetadata (object, interface, data, error);

    return noopGetMetadata (object, interface, data, error);
}

static CompObjectVTable objectVTable = {
    .forBaseObject = forBaseObject,

    .insertObject = insertObject,
    .removeObject = removeObject,
    .inserted     = inserted,
    .removed      = removed,

    .interfaceAdded   = interfaceAdded,
    .interfaceRemoved = interfaceRemoved,

    .signal.connect    = connect,
    .signal.disconnect = disconnect,
    .signal.signal     = signal,

    .version.get = getVersion,

    .properties.getBool     = getBoolProp,
    .properties.setBool     = setBoolProp,
    .properties.boolChanged = boolPropChanged,

    .properties.getInt     = getIntProp,
    .properties.setInt     = setIntProp,
    .properties.intChanged = intPropChanged,

    .properties.getDouble     = getDoubleProp,
    .properties.setDouble     = setDoubleProp,
    .properties.doubleChanged = doublePropChanged,

    .properties.getString     = getStringProp,
    .properties.setString     = setStringProp,
    .properties.stringChanged = stringPropChanged,

    .metadata.get = getMetadata
};

static CompBool
initObject (const CompObjectFactory *factory,
	    CompObject		    *object)
{
    object->vTable    = &objectVTable;
    object->parent    = NULL;
    object->name      = NULL;
    object->signalVec = NULL;

    object->id = ~0; /* XXX: remove id asap */

    return TRUE;
}

static void
finiObject (const CompObjectFactory *factory,
	    CompObject		    *object)
{
    if (object->signalVec)
	free (object->signalVec);
}

static const CompObjectVTable noopVTable = {
    .insertObject = noopInsertObject,
    .removeObject = noopRemoveObject,
    .inserted     = noopInserted,
    .removed      = noopRemoved,

    .forEachInterface = noopForEachInterface,
    .forEachMethod    = noopForEachMethod,
    .forEachSignal    = noopForEachSignal,
    .forEachProp      = noopForEachProp,

    .interfaceAdded   = noopInterfaceAdded,
    .interfaceRemoved = noopInterfaceRemoved,

    .forEachChildObject = noopForEachChildObject,

    .signal.connect    = noopConnect,
    .signal.disconnect = noopDisconnect,
    .signal.signal     = noopSignal,

    .version.get = noopGetVersion,

    .properties.getBool     = noopGetBoolProp,
    .properties.setBool     = noopSetBoolProp,
    .properties.boolChanged = noopBoolPropChanged,

    .properties.getInt     = noopGetIntProp,
    .properties.setInt     = noopSetIntProp,
    .properties.intChanged = noopIntPropChanged,

    .properties.getDouble     = noopGetDoubleProp,
    .properties.setDouble     = noopSetDoubleProp,
    .properties.doubleChanged = noopDoublePropChanged,

    .properties.getString     = noopGetStringProp,
    .properties.setString     = noopSetStringProp,
    .properties.stringChanged = noopStringPropChanged,

    .metadata.get = noopGetMetadata
};

static void
initObjectVTable (CompObjectVTable *vTable)
{
    vTableInit (vTable, &noopVTable, sizeof (CompObjectVTable));
}

static CompObjectType objectType = {
    OBJECT_TYPE_NAME, NULL,
    {
	initObject,
	finiObject
    },
    offsetof (CompObject, privates),
    (InitVTableProc) initObjectVTable,
    &objectVTable,
    sizeof (CompObjectVTable)
};

static void
objectGetCContext (CompObject *object,
		   CContext   *ctx)
{
    ctx->interface  = objectInterface;
    ctx->nInterface = N_ELEMENTS (objectInterface);
    ctx->type	    = &objectType;
    ctx->data	    = (char *) object;
    ctx->svOffset   = 0;
    ctx->vtStore    = NULL;
    ctx->version    = COMPIZ_OBJECT_VERSION;
}

CompObjectType *
getObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInterfaceInit (objectInterface, N_ELEMENTS (objectInterface),
			&objectVTable, objectGetCContext, 0);
	init = TRUE;
    }

    return &objectType;
}

int
compObjectAllocatePrivateIndex (CompObjectType *type,
				int	       size)
{
    const CompObjectFactory *f;
    CompFactory		    *factory;

    for (f = &core.u.base.factory; f->master; f = f->master);
    factory = (CompFactory *) f;

    return (*factory->allocatePrivateIndex) (factory, type->name, size);
}

void
compObjectFreePrivateIndex (CompObjectType *type,
			    int	           index)
{
    const CompObjectFactory *f;
    CompFactory		    *factory;

    for (f = &core.u.base.factory; f->master; f = f->master);
    factory = (CompFactory *) f;

    return (*factory->freePrivateIndex) (factory, type->name, index);
}

static CompBool
cObjectAllocPrivateIndex (CompFactory    *factory,
			  CObjectPrivate *private)
{
    int	index;

    index = (*factory->allocatePrivateIndex) (factory,
					      private->name,
					      private->size);
    if (index < 0)
	return FALSE;

    *(private->pIndex) = index;

    return TRUE;
}

static void
cObjectFreePrivateIndex (CompFactory    *factory,
			 CObjectPrivate *private)
{
    (*factory->freePrivateIndex) (factory, private->name, *(private->pIndex));
}

CompBool
cObjectAllocPrivateIndices (CompFactory    *factory,
			    CObjectPrivate *private,
			    int	           nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
	if (!cObjectAllocPrivateIndex (factory, &private[i]))
	    break;

    if (i < nPrivate)
    {
	if (i)
	    cObjectFreePrivateIndices (factory, private, i - 1);

	return FALSE;
    }

    return TRUE;
}

void
cObjectFreePrivateIndices (CompFactory	  *factory,
			   CObjectPrivate *private,
			   int		  nPrivate)
{
    int	n = nPrivate;

    while (n--)
	cObjectFreePrivateIndex (factory, &private[n]);
}

static CompBool
topObjectType (CompObject	    *object,
	       const char	    *name,
	       size_t		    offset,
	       const CompObjectType *type,
	       void		    *closure)
{
    if (type)
    {
	*((const CompObjectType **) closure) = type;
	return FALSE;
    }

    return TRUE;
}

typedef struct _InitObjectContext {
    const CompObjectFactory *factory;
    const CompObjectType    *type;
    CompObjectPrivate	    *privates;
    CompObject		    *object;
} InitObjectContext;

static CompBool
initBaseObject (CompObject *object,
		void	   *closure)
{
    InitObjectContext    *pCtx = (InitObjectContext *) closure;
    const CompObjectType *type;

    (*object->vTable->forEachInterface) (object,
					 topObjectType,
					 (void *) &type);

    if (type == pCtx->type)
	return (*pCtx->privates->funcs.init) (pCtx->factory, object);
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

    (*object->vTable->forEachInterface) (object,
					 topObjectType,
					 (void *) &type);

    if (type == pCtx->type)
	(*pCtx->privates->funcs.fini) (pCtx->factory, object);
    else
	(*object->vTable->forBaseObject) (object,
					  finiBaseObject,
					  closure);

    return TRUE;
}

static CompBool
initTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type,
		  CompObjectPrivate	  *privates);

static CompBool
finiTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type,
		  CompObjectPrivate	  *privates);

static CompBool
initObjectTree (CompObject *object,
		void	   *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    pCtx->object = object;

    return initTypedObjects (pCtx->factory, object, pCtx->type,
			     pCtx->privates);
}

static CompBool
finiObjectTree (CompObject *object,
		void	   *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == object)
	return FALSE;

    return finiTypedObjects (pCtx->factory, object, pCtx->type,
			     pCtx->privates);
}

static CompBool
initTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type,
		  CompObjectPrivate	  *privates)
{
    InitObjectContext ctx;

    ctx.factory  = factory;
    ctx.type     = type;
    ctx.privates = privates;
    ctx.object   = NULL;

    if (!initBaseObject (object, (void *) &ctx))
	return FALSE;

    if (!(*object->vTable->forEachChildObject) (object,
						initObjectTree,
						(void *) &ctx))
    {
	(*object->vTable->forEachChildObject) (object,
					       finiObjectTree,
					       (void *) &ctx);

	finiBaseObject (object, (void *) &ctx);

	return FALSE;
    }

    return TRUE;
}

static CompBool
finiTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type,
		  CompObjectPrivate	  *privates)
{
    InitObjectContext ctx;

    ctx.factory  = factory;
    ctx.type     = type;
    ctx.privates = privates;
    ctx.object   = NULL;

    (*object->vTable->forEachChildObject) (object,
					   finiObjectTree,
					   (void *) &ctx);

    finiBaseObject (object, (void *) &ctx);

    return TRUE;
}

static CompBool
cObjectInitPrivate (CompBranch	   *branch,
		    CObjectPrivate *private)
{
    CompObjectInstantiator *instantiator;
    CompObjectPrivate	   *privates;

    instantiator = lookupObjectInstantiator (&branch->factory, private->name);
    if (!instantiator)
	return FALSE;

    privates = realloc (instantiator->privates, (instantiator->nPrivates + 1) *
			sizeof (CompObjectPrivate));
    if (!privates)
	return FALSE;

    instantiator->privates = privates;

    if (private->interface)
	cInterfaceInit (private->interface, private->nInterface,
			private->vTable, private->proc,
			instantiator->type->initVTable);

    privates[instantiator->nPrivates].funcs.init = private->init;
    privates[instantiator->nPrivates].funcs.fini = private->fini;
    privates[instantiator->nPrivates].vTable     = NULL;

    /* initialize all objects of this type */
    if (!initTypedObjects (&branch->factory, &branch->u.base,
			   instantiator->type,
			   &privates[instantiator->nPrivates]))
	return FALSE;

    instantiator->nPrivates++;

    return TRUE;
}

static void
cObjectFiniPrivate (CompBranch	   *branch,
		    CObjectPrivate *private)
{
    CompObjectInstantiator *instantiator;
    CompObjectPrivate	   *privates;

    instantiator = lookupObjectInstantiator (&branch->factory, private->name);
    instantiator->nPrivates--;

    privates = instantiator->privates;

    /* finalize all objects of this type */
    finiTypedObjects (&branch->factory, &branch->u.base,
		      instantiator->type,
		      &privates[instantiator->nPrivates]);

    privates = realloc (instantiator->privates, instantiator->nPrivates *
			sizeof (CompObjectPrivate));
    if (privates || !instantiator->nPrivates)
	instantiator->privates = privates;

    if (private->interface)
	cInterfaceFini (private->interface, private->nInterface);
}

CompBool
cObjectInitPrivates (CompBranch	    *branch,
		     CObjectPrivate *private,
		     int	    nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
	if (!cObjectInitPrivate (branch, &private[i]))
	    break;

    if (i < nPrivate)
    {
	if (i)
	    cObjectFiniPrivates (branch, private, i - 1);

	return FALSE;
    }

    return TRUE;
}

void
cObjectFiniPrivates (CompBranch	    *branch,
		     CObjectPrivate *private,
		     int	    nPrivate)
{
    int	n = nPrivate;

    while (n--)
	cObjectFiniPrivate (branch, &private[n]);
}

typedef struct _ForInterfaceContext {
    const char		  *interface;
    InterfaceCallBackProc proc;
    void		  *closure;
} ForInterfaceContext;

static CompBool
handleInterface (CompObject	      *object,
		 const char	      *name,
		 size_t		      offset,
		 const CompObjectType *type,
		 void		      *closure)
{
    ForInterfaceContext *pCtx = (ForInterfaceContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
	if (!(*pCtx->proc) (object, name, offset, type, pCtx->closure))
	    return FALSE;

    return TRUE;
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

    return (*object->vTable->forEachInterface) (object,
						handleInterface,
						(void *) &ctx);
}

static CompBool
getInterfaceVersion (CompObject		  *object,
		     const char	          *name,
		     size_t		  offset,
		     const CompObjectType *type,
		     void		  *closure)
{
    int *version = (int *) closure;

    *version = (object->vTable->version.get) (object, name);

    return FALSE;
}

typedef struct _LookupObjectContext {
    const char *path;
    int	       size;
    CompObject *object;
} LookupObjectContext;

static CompBool
checkChildObject (CompObject *object,
		  void	     *closure)
{
    LookupObjectContext *pCtx = (LookupObjectContext *) closure;
    int			i;

    if (strncmp (object->name, pCtx->path, pCtx->size))
	return TRUE;

    i = strlen (object->name);

    if (i != pCtx->size)
	return TRUE;

    pCtx->path += i;

    if (*pCtx->path++)
    {
	for (i = 0; pCtx->path[i] != '\0' && pCtx->path[i] != '/'; i++);

	pCtx->size = i;

	return (*object->vTable->forEachChildObject) (object,
						      checkChildObject,
						      closure);
    }

    pCtx->object = object;

    return FALSE;
}

CompObject *
compLookupObject (CompObject *root,
		  const char *path)
{
    LookupObjectContext ctx;
    int			i;

    for (i = 0; path[i] != '\0' && path[i] != '/'; i++);

    ctx.path = path;
    ctx.size = i;

    if ((*root->vTable->forEachChildObject) (root,
					     checkChildObject,
					     (void *) &ctx))
	return NULL;

    return ctx.object;
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

static CompBool
interfaceIsPartOfType (CompObject	    *object,
		       const char	    *name,
		       size_t		    offset,
		       const CompObjectType *type,
		       void		    *closure)
{
    CompBool *partOfType = (CompBool *) closure;

    *partOfType = (type) ? TRUE : FALSE;

    return FALSE;
}

CompBool
compObjectInterfaceIsPartOfType (CompObject *object,
				 const char *interface)
{
    CompBool partOfType;

    if (compForInterface (object,
			  interface,
			  interfaceIsPartOfType,
			  &partOfType))
	return FALSE;

    return partOfType;
}

typedef struct _MethodCallContext {
    const char		   *interface;
    const char		   *name;
    const char		   *in;
    const char		   *out;
    const CompObjectVTable *vTable;
    int			   offset;
    MethodMarshalProc	   marshal;
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

    if (strcmp (name, pCtx->name))
	return TRUE;

    if (strcmp (in, pCtx->in))
	return TRUE;

    if (pCtx->out)
	if (strcmp (out, pCtx->out))
	    return TRUE;

    pCtx->offset  = offset;
    pCtx->marshal = marshal;

    return FALSE;
}

static CompBool
checkInterface (CompObject	     *object,
		const char	     *name,
		size_t		     offset,
		const CompObjectType *type,
		void		     *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object, name, checkMethod,
					       closure))
	{

	    /* need vTable if interface is not part of object type */
	    if (!type)
		pCtx->vTable = object->vTable;

	    /* add interface vTable offset to method offset set by
	       checkMethod function */
	    pCtx->offset += offset;

	    return FALSE;
	}
    }

    return TRUE;
}

static void
invokeMethod (CompObject	     *object,
	      const CompObjectVTable *vTable,
	      int		     offset,
	      MethodMarshalProc	     marshal,
	      CompArgs		     *args)
{
    CompObjectVTableVec save = { object->vTable };
    CompObjectVTableVec interface = { vTable };

    UNWRAP (&interface, object, vTable);
    (*marshal) (object,
		*((void (**) (void)) (((char *) object->vTable) + offset)),
		args);
    WRAP (&interface, object, vTable, save.vTable);
}

CompBool
compInvokeMethodWithArgs (CompObject *object,
			  const char *interface,
			  const char *name,
			  const char *in,
			  const char *out,
			  CompArgs   *args)
{
    MethodCallContext ctx;

    ctx.interface = interface;
    ctx.name	  = name;
    ctx.in	  = in;
    ctx.out       = out;
    ctx.vTable    = object->vTable;

    if ((*object->vTable->forEachInterface) (object,
					     checkInterface,
					     (void *) &ctx))
	return FALSE;

    invokeMethod (object, ctx.vTable, ctx.offset, ctx.marshal, args);

    return TRUE;
}

typedef struct _VaArgs {
    CompArgs base;

    va_list    args;
    const char *in;
    int	       nIn;
    const char *out;
    int	       nOut;
    CompBool   status;
} VaArgs;

static void
vaArgsStep (VaArgs *va,
	    int	   type)
{
    switch (type) {
    case COMP_TYPE_BOOLEAN:
	va_arg (va->args, CompBool);
	break;
    case COMP_TYPE_INT32:
	va_arg (va->args, int32_t);
	break;
    case COMP_TYPE_DOUBLE:
	va_arg (va->args, double);
	break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT:
	va_arg (va->args, char *);
	break;
    }
}

static void
vaArgsLoad (CompArgs *args,
	    int      type,
	    void     *value)
{
    VaArgs *va = (VaArgs *) args;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
	*((CompBool *) value) = va_arg (va->args, CompBool);
	break;
    case COMP_TYPE_INT32:
	*((int32_t *) value) = va_arg (va->args, int32_t);
	break;
    case COMP_TYPE_DOUBLE:
	*((double *) value) = va_arg (va->args, double);
	break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT:
	*((char **) value) = va_arg (va->args, char *);
	break;
    }

    va->in++;
}

static void
vaArgsStore (CompArgs *args,
	     int      type,
	     void     *value)
{
    VaArgs *va = (VaArgs *) args;

    while (*va->in != COMP_TYPE_INVALID)
	vaArgsStep (va, *va->in++);

    switch (type) {
    case COMP_TYPE_BOOLEAN: {
	CompBool *boolOut;

	boolOut = va_arg (va->args, CompBool *);
	if (boolOut)
	    *boolOut = *((CompBool *) value);
    } break;
    case COMP_TYPE_INT32: {
	int32_t *intOut;

	intOut = va_arg (va->args, int32_t *);
	if (intOut)
	    *intOut = *((int32_t *) value);
    } break;
    case COMP_TYPE_DOUBLE: {
	double *doubleOut;

	doubleOut = va_arg (va->args, double *);
	if (doubleOut)
	    *doubleOut = *((double *) value);
    } break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT: {
	char **stringOut;

	stringOut = va_arg (va->args, char **);
	if (stringOut)
	{
	    *stringOut = *((char **) value);
	}
	else
	{
	    if (*((char **) value))
		free (*((char **) value));
	}
    } break;
    }

    va->out++;
}

static void
vaArgsError (CompArgs *args,
	     char     *error)
{
    VaArgs *va = (VaArgs *) args;
    char   **errorOut;

    while (*va->in != COMP_TYPE_INVALID)
	vaArgsStep (va, *va->in++);

    while (*va->out != COMP_TYPE_INVALID)
	vaArgsStep (va, *va->out++);

    errorOut = va_arg (va->args, char **);
    if (errorOut)
    {
	*errorOut = error;
    }
    else
    {
	if (error)
	    free (error);
    }

    va->status = FALSE;
}

static CompBool
vaInitArgs (VaArgs     *va,
	    const char *in,
	    const char *out,
	    va_list    args)
{
    va->base.load  = vaArgsLoad;
    va->base.store = vaArgsStore;
    va->base.error = vaArgsError;

    va->status = TRUE;
    va->args   = args;
    va->in     = in;
    va->out    = out;

    return TRUE;
}

static CompBool
vaFiniArgs (VaArgs *va)
{
    return va->status;
}

CompBool
compInvokeMethod (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *in,
		  const char *out,
		  ...)
{
    VaArgs   args;
    CompBool status;
    va_list  ap;

    va_start (ap, out);

    vaInitArgs (&args, in, out, ap);

    if (!compInvokeMethodWithArgs (object,
				   interface, name, in, out,
				   &args.base))
    {
	char *error;

	esprintf (&error, "Method \"%s\" with input signature \"%s\" and "
		  "output signature \"%s\" on interface "
		  "\"%s\" doesn't exist", name, in, out, interface);

	(*args.base.error) (&args.base, error);
    }

    status = vaFiniArgs (&args);

    va_end (ap);

    return status;
}

int
compSerializeMethodCall (CompObject *observer,
			 CompObject *subject,
			 const char *interface,
			 const char *name,
			 const char *signature,
			 va_list    args,
			 void	    *data,
			 int	    size)
{
    CompObject *node;
    char       *str, *ptr;
    int	       pathSize = 0;
    int	       interfaceSize = strlen (interface) + 1;
    int	       nameSize = strlen (name) + 1;
    int	       signatureSize = strlen (signature) + 1;
    int	       valueSize = 0;
    int	       nValue = 0;
    int	       requiredSize, i;
    va_list    ap;

    for (node = subject; node; node = node->parent)
    {
	if (node == observer)
	    break;

	pathSize += strlen (node->name) + 1;
    }

    assert (node);

    va_copy (ap, args);

    for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
    {
	switch (signature[i]) {
	case COMP_TYPE_BOOLEAN:
	    va_arg (ap, CompBool);
	    break;
	case COMP_TYPE_INT32:
	    va_arg (ap, int32_t);
	    break;
	case COMP_TYPE_DOUBLE:
	    va_arg (ap, double);
	    break;
	case COMP_TYPE_STRING:
	case COMP_TYPE_OBJECT:
	    str = va_arg (ap, char *);
	    if (str)
		valueSize += strlen (str) + 1;
	    break;
	}

	nValue++;
    }

    va_end (ap);

    requiredSize = sizeof (CompSerializedMethodCallHeader) +
	pathSize + interfaceSize + nameSize + signatureSize +
	sizeof (CompAnyValue) * nValue + valueSize;

    if (requiredSize <= size)
    {
	CompSerializedMethodCallHeader *header = data;
	int			       offset;

	header->path = (char *) (header + 1);

	offset = pathSize;
	header->path[offset - 1] = '\0';

	for (node = subject; node; node = node->parent)
	{
	    int len;

	    if (node == observer)
		break;

	    if (offset-- < pathSize)
		header->path[offset] = '/';

	    len = strlen (node->name);
	    offset -= len;
	    memcpy (&header->path[offset], node->name, len);
	}

	header->interface = (char *) (header->path + pathSize);
	strcpy (header->interface, interface);

	header->name = (char *) (header->interface + interfaceSize);
	strcpy (header->name, name);

	header->signature = (char *) (header->name + nameSize);
	strcpy (header->signature, signature);

	header->value  = (CompAnyValue *) (header->signature + signatureSize);
	header->nValue = nValue;

	ptr = (char *) (header->value + nValue);

	va_copy (ap, args);

	for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
	{
	    switch (signature[i]) {
	    case COMP_TYPE_BOOLEAN:
		header->value[i].b = va_arg (ap, CompBool);
		break;
	    case COMP_TYPE_INT32:
		header->value[i].i = va_arg (ap, int32_t);
		break;
	    case COMP_TYPE_DOUBLE:
		header->value[i].d = va_arg (ap, double);
		break;
	    case COMP_TYPE_STRING:
	    case COMP_TYPE_OBJECT:
		str = va_arg (ap, char *);
		if (str)
		{
		    header->value[i].s = ptr;
		    strcpy (header->value[i].s, str);
		    ptr += strlen (str) + 1;
		}
		else
		{
		    header->value[i].s = NULL;
		}
		break;
	    }
	}

	va_end (ap);
    }

    return requiredSize;
}

CompBool
compCheckEqualityOfValuesAndArgs (const char   *signature,
				  CompAnyValue *value,
				  ...)
{
    CompBool equal = TRUE;
    va_list  args;
    int      i;

    va_start (args, value);

    for (i = 0; equal && signature[i] != COMP_TYPE_INVALID; i++)
    {
	switch (signature[i]) {
	case COMP_TYPE_BOOLEAN:
	    equal = (!va_arg (args, CompBool) == !value[i].b);
	    break;
	case COMP_TYPE_INT32:
	    equal = (va_arg (args, int32_t) == value[i].i);
	    break;
	case COMP_TYPE_DOUBLE:
	    equal = (va_arg (args, double) == value[i].d);
	    break;
	case COMP_TYPE_STRING: {
	    char *s = va_arg (args, char *);

	    if (s && value[i].s)
		equal = (strcmp (s, value[i].s) == 0);
	    else
		equal = (s == value[i].s);
	} break;
	case COMP_TYPE_OBJECT: {
	    char *s = va_arg (args, char *);

	    /* complete XPath expression support could be added later */
	    if (strcmp (value[i].s, "//*") != 0)
		equal = (strcmp (s, value[i].s) == 0);
	} break;
	}
    }

    va_end (args);

    return equal;
}

int
compConnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     CompObject *descendant,
	     const char *descendantInterface,
	     size_t     descendantOffset,
	     const char *details,
	     ...)
{
    va_list args;
    int     index;

    va_start (args, details);

    index = (*object->vTable->signal.connect) (object,
					       interface,
					       offset,
					       descendant,
					       descendantInterface,
					       descendantOffset,
					       details,
					       args);

    va_end (args);

    return index;
}

void
compDisconnect (CompObject *object,
		const char *interface,
		size_t     offset,
		int	   index)
{
    (*object->vTable->signal.disconnect) (object,
					  interface,
					  offset,
					  index);
}

const char *
compTranslateObjectPath (CompObject *ancestor,
			 CompObject *descendant,
			 const char *path)
{
    CompObject *node;
    int	       n = 0;

    for (node = descendant; node != ancestor; node = node->parent)
	n += strlen (node->name) + 1;

    if (n && path[n - 1] != '/')
	return path + (n - 1);

    return path + n;
}

static CompBool
setIndex (int *index,
	  int value)
{
    *index = value;
    return TRUE;
}

static CompBool
propertyIndex (CInterface *interface,
	       const char *name,
	       int	  type,
	       int	  *index)
{
    int i;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
	for (i = 0; i < interface->nBoolProp; i++)
	    if (strcmp (name, interface->boolProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    case COMP_TYPE_INT32:
	for (i = 0; i < interface->nIntProp; i++)
	    if (strcmp (name, interface->intProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    case COMP_TYPE_DOUBLE:
	for (i = 0; i < interface->nDoubleProp; i++)
	    if (strcmp (name, interface->doubleProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    case COMP_TYPE_STRING:
	for (i = 0; i < interface->nStringProp; i++)
	    if (strcmp (name, interface->stringProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    }

    return FALSE;
}

static const char *
attrValue (const xmlChar **atts,
	   const char    *name)
{
    int i;

    for (i = 0; atts[i]; i += 2)
	if (strcmp ((const char *) atts[i], name) == 0)
	    return (const char *) atts[i + 1];

    return NULL;
}

typedef struct _DefaultValuesContext {
    CInterface *interface;
    int	       nInterface;
    int	       depth;
    int	       validDepth;
    CInterface *currentInterface;
    int	       propType;
    int	       propIndex;
} DefaultValuesContext;

static void
handleStartElement (void	  *data,
		    const xmlChar *name,
		    const xmlChar **atts)
{
    DefaultValuesContext *pCtx = (DefaultValuesContext *) data;

    if (pCtx->validDepth == pCtx->depth++)
    {
	switch (pCtx->validDepth) {
	case 0:
	    if (strcmp ((const char *) name, "compiz") == 0)
		pCtx->validDepth = pCtx->depth;
	    break;
	case 1:
	    if (strcmp ((const char *) name, "interface") == 0)
	    {
		const char *nameAttr = attrValue (atts, "name");

		if (nameAttr)
		{
		    int i;

		    for (i = 0; i < pCtx->nInterface; i++)
		    {
			if (strcmp (nameAttr, pCtx->interface[i].name) == 0)
			{
			    pCtx->validDepth       = pCtx->depth;
			    pCtx->currentInterface = &pCtx->interface[i];
			    break;
			}
		    }
		}
	    }
	    break;
	case 2:
	    if (strcmp ((const char *) name, "property") == 0)
	    {
		const char *typeAttr = attrValue (atts, "type");

		if (typeAttr && typeAttr[1] == '\0')
		{
		    static int types[] = {
			COMP_TYPE_BOOLEAN,
			COMP_TYPE_INT32,
			COMP_TYPE_DOUBLE,
			COMP_TYPE_STRING
		    };
		    int i;

		    for (i = 0; i < N_ELEMENTS (types); i++)
			if (typeAttr[0] == types[i])
			    break;

		    if (i < N_ELEMENTS (types))
		    {
			const char *nameAttr = attrValue (atts, "name");

			if (nameAttr)
			{
			    if (propertyIndex (pCtx->currentInterface,
					       nameAttr, types[i],
					       &pCtx->propIndex))
			    {
				pCtx->propType	 = types[i];
				pCtx->validDepth = pCtx->depth;
			    }
			}
		    }
		}
	    }
	    break;
	case 3:
	    if (strcmp ((const char *) name, "default") == 0)
		pCtx->validDepth = pCtx->depth;
	    break;
	}
    }
}

static void
handleEndElement (void		*data,
		  const xmlChar *name)
{
    DefaultValuesContext *pCtx = (DefaultValuesContext *) data;

    if (pCtx->validDepth == pCtx->depth--)
	pCtx->validDepth = pCtx->depth;
}

static void
handleCharacters (void		*data,
		  const xmlChar *ch,
		  int		len)
{
    DefaultValuesContext *pCtx = (DefaultValuesContext *) data;

    if (pCtx->validDepth == 4)
    {
	switch (pCtx->propType) {
	case COMP_TYPE_BOOLEAN: {
	    CBoolProp *prop =
		&pCtx->currentInterface->boolProp[pCtx->propIndex];

	    if (len == 4 && strncmp ((const char *) ch, "true", 4) == 0)
		prop->defaultValue = TRUE;
	    else
		prop->defaultValue = FALSE;
	} break;
	case COMP_TYPE_INT32: {
	    char tmp[256];

	    if (len < sizeof (tmp))
	    {
		CIntProp *prop =
		    &pCtx->currentInterface->intProp[pCtx->propIndex];

		strncpy (tmp, (const char *) ch, len);
		tmp[len] = '\0';

		prop->defaultValue = strtol (tmp, NULL, 0);
	    }
	} break;
	case COMP_TYPE_DOUBLE: {
	    char tmp[256];

	    if (len < sizeof (tmp))
	    {
		CDoubleProp *prop =
		    &pCtx->currentInterface->doubleProp[pCtx->propIndex];

		strncpy (tmp, (const char *) ch, len);
		tmp[len] = '\0';

		prop->defaultValue = strtod (tmp, NULL);
	    }
	} break;
	case COMP_TYPE_STRING: {
	    char *value;

	    value = malloc (len + 1);
	    if (value)
	    {
		CStringProp *prop =
		    &pCtx->currentInterface->stringProp[pCtx->propIndex];

		strncpy (value, (const char *) ch, len);
		value[len] = '\0';

		if (prop->data)
		    free (prop->data);

		prop->defaultValue = prop->data	= value;
	    }
	} break;
	}
    }
}

static void
cVerfiyDefaultValues (CInterface *interface,
		      int	 nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nIntProp; j++)
	    interface[i].intProp[j].defaultValue =
		MAX (MIN (interface[i].intProp[j].defaultValue,
			  interface[i].intProp[j].max),
		     interface[i].intProp[j].min);
	for (j = 0; j < interface[i].nDoubleProp; j++)
	    interface[i].doubleProp[j].defaultValue =
		MAX (MIN (interface[i].doubleProp[j].defaultValue,
			  interface[i].doubleProp[j].max),
		     interface[i].doubleProp[j].min);
    }
}

static CompBool
parseDefaultValues (FILE	*fp,
		    const char  *path,
		    struct stat *buf,
		    void	*closure)
{
    xmlSAXHandler saxHandler = {
	.initialized  = XML_SAX2_MAGIC,
	.startElement = handleStartElement,
	.endElement   = handleEndElement,
	.characters   = handleCharacters
    };

    if (!xmlSAXUserParseFile (&saxHandler, closure, path))
	return TRUE;

    return FALSE;
}

void
cDefaultValuesFromFile (CInterface *interface,
			int	   nInterface,
			const char *name)
{
    DefaultValuesContext ctx = {
	.interface  = interface,
	.nInterface = nInterface
    };

    forEachMetadataFile (name, parseDefaultValues, (void *) &ctx);

    cVerfiyDefaultValues (interface, nInterface);
}

static void
cInitSignals (CInterface *interface,
	      int	 nInterface)
{
    int i, j;
    int index = 0;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nSignal; j++)
	{
	    interface[i].signal[j]->index     = index++;
	    interface[i].signal[j]->interface = interface[i].name;
	}
    }
}

CompBool
cInterfaceInit (CInterface	 *interface,
		int		 nInterface,
		CompObjectVTable *vTable,
		GetCContextProc  getCContext,
		InitVTableProc   initVTable)
{
    int i;

    for (i = 0; i < nInterface; i++)
	cDefaultValuesFromFile (&interface[i], 1, interface->name);

    cInitSignals (interface, nInterface);

    cInitObjectVTable (vTable, getCContext, initVTable);

    return TRUE;
}

void
cInterfaceFini (CInterface *interface,
		int	   nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
	for (j = 0; j < interface[i].nStringProp; j++)
	    if (interface[i].stringProp[j].data)
		free (interface[i].stringProp[j].data);
}

#define PROP_VALUE(data, prop, type)	       \
    (*((type *) (data + (prop)->base.offset)))

#define SET_DEFAULT_VALUE(data, prop, type)		 \
    PROP_VALUE (data, prop, type) = (prop)->defaultValue

CompBool
cObjectPropertiesInit (CompObject	*object,
		       char		*data,
		       const CInterface *interface,
		       int		nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nBoolProp; j++)
	    SET_DEFAULT_VALUE (data, &interface[i].boolProp[j], CompBool);

	for (j = 0; j < interface[i].nIntProp; j++)
	    SET_DEFAULT_VALUE (data, &interface[i].intProp[j], int32_t);

	for (j = 0; j < interface[i].nDoubleProp; j++)
	    SET_DEFAULT_VALUE (data, &interface[i].doubleProp[j], double);

	for (j = 0; j < interface[i].nStringProp; j++)
	{
	    if (interface[i].stringProp[j].defaultValue)
	    {
		char *str;

		str = strdup (interface[i].stringProp[j].defaultValue);
		if (!str)
		{
		    while (j--)
		    {
			str = PROP_VALUE (data,
					  &interface[i].stringProp[j],
					  char *);
			if (str)
			    free (str);
		    }

		    while (i--)
			cObjectPropertiesFini (object, data, &interface[i], 1);

		    return FALSE;
		}

		PROP_VALUE (data, &interface[i].stringProp[j], char *) = str;
	    }
	    else
	    {
		PROP_VALUE (data, &interface[i].stringProp[j], char *) = NULL;
	    }
	}
    }

    return TRUE;
}

void
cObjectPropertiesFini (CompObject	*object,
		       char		*data,
		       const CInterface *interface,
		       int		nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nStringProp; j++)
	{
	    char *str;

	    str = PROP_VALUE (data, &interface[i].stringProp[j], char *);
	    if (str)
		free (str);
	}
    }
}

CompBool
cObjectChildrenInit (const CompObjectFactory *factory,
		     CompObject		     *object,
		     char		     *data,
		     const CInterface	     *interface,
		     int		     nInterface)
{
    CompObject *child;
    int	       i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nChild; j++)
	{
	    CChildObject *cChild = &interface[i].child[j];

	    if (cChild->type)
	    {
		child = CHILD (data, cChild);

		if (!compObjectInitByTypeName (factory, child, cChild->type))
		{
		    while (j--)
		    {
			cChild = &interface[i].child[j];
			if (cChild->type)
			    compObjectFiniByTypeName (factory,
						      CHILD (data, cChild),
						      cChild->type);
		    }

		    while (i--)
			cObjectChildrenFini (factory, object, data,
					     &interface[i], 1);

		    return FALSE;
		}

		child->name = cChild->name;
	    }
	}
    }

    return TRUE;
}

void
cObjectChildrenFini (const CompObjectFactory *factory,
		     CompObject		     *object,
		     char		     *data,
		     const CInterface	     *interface,
		     int		     nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
	for (j = 0; j < interface[i].nChild; j++)
	    if (interface[i].child[j].type)
		compObjectFiniByTypeName (factory,
					  CHILD (data, &interface[i].child[j]),
					  interface[i].child[j].type);
}

CompBool
cObjectInterfaceInit (const CompObjectFactory *factory,
		      CompObject	      *object,
		      const CompObjectVTable  *vTable)
{
    CContext ctx;

    CCONTEXT (vTable, &ctx);

    if (ctx.svOffset)
	*((int *) (ctx.data + ctx.svOffset)) = 0;

    if (!cObjectPropertiesInit (object,
				ctx.data,
				ctx.interface,
				ctx.nInterface))
	return FALSE;

    if (!cObjectChildrenInit (factory, object,
			      ctx.data,
			      ctx.interface,
			      ctx.nInterface))
    {
	cObjectPropertiesFini (object,
			       ctx.data,
			       ctx.interface,
			       ctx.nInterface);
	return FALSE;
    }

    WRAP (ctx.vtStore, object, vTable, vTable);

    return TRUE;
}

void
cObjectInterfaceFini (const CompObjectFactory *factory,
		      CompObject	      *object)
{
    CContext ctx;

    CCONTEXT (object->vTable, &ctx);

    UNWRAP (ctx.vtStore, object, vTable);

    cObjectChildrenFini (factory, object,
			 ctx.data,
			 ctx.interface,
			 ctx.nInterface);
    cObjectPropertiesFini (object,
			   ctx.data,
			   ctx.interface,
			   ctx.nInterface);

    if (ctx.svOffset)
    {
	int offset = *((int *) (ctx.data + ctx.svOffset));

	if (offset)
	    compFreeSignalVecRange (object,
				    getSignalVecSize (ctx.interface,
						      ctx.nInterface),
				    offset);
    }
}

static const CompObjectVTable cVTable = {
    .forBaseObject = cForBaseObject,

    .insertObject = cInsertObject,
    .removeObject = cRemoveObject,
    .inserted     = cInserted,
    .removed      = cRemoved,

    .forEachInterface = cForEachInterface,
    .forEachMethod    = cForEachMethod,
    .forEachSignal    = cForEachSignal,
    .forEachProp      = cForEachProp,

    .forEachChildObject = cForEachChildObject,

    .signal.connect    = cConnect,
    .signal.disconnect = cDisconnect,

    .version.get = cGetVersion,

    .properties.getBool     = cGetBoolProp,
    .properties.setBool     = cSetBoolProp,
    .properties.boolChanged = cBoolPropChanged,

    .properties.getInt     = cGetIntProp,
    .properties.setInt     = cSetIntProp,
    .properties.intChanged = cIntPropChanged,

    .properties.getDouble     = cGetDoubleProp,
    .properties.setDouble     = cSetDoubleProp,
    .properties.doubleChanged = cDoublePropChanged,

    .properties.getString     = cGetStringProp,
    .properties.setString     = cSetStringProp,
    .properties.stringChanged = cStringPropChanged,

    .metadata.get = cGetMetadata
};

void
cInitObjectVTable (CompObjectVTable *vTable,
		   GetCContextProc  getCContext,
		   InitVTableProc   initVTable)
{
    vTable->unused = (UnusedProc) getCContext;

    vTableInit (vTable, &cVTable, sizeof (CompObjectVTable));

    if (initVTable)
	(*initVTable) (vTable);
}
