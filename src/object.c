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

static const CommonSignal objectTypeSignal[] = {
    C_SIGNAL (interfaceAdded,     "s", CompObjectVTable),
    C_SIGNAL (interfaceRemoved,   "s", CompObjectVTable),
    C_SIGNAL (childObjectAdded,   "o", CompObjectVTable),
    C_SIGNAL (childObjectRemoved, "o", CompObjectVTable)
};
#define INTERFACE_VERSION_objectType CORE_ABIVERSION

static const CommonSignal signalObjectSignal[] = {
    C_SIGNAL (signal, NULL, CompSignalVTable)
};
#define INTERFACE_VERSION_signalObject CORE_ABIVERSION

static const CommonMethod propertiesObjectMethod[] = {
    C_METHOD (getBool,   "ss",  "b", CompPropertiesVTable, marshal__SS_B_E),
    C_METHOD (setBool,   "ssb", "",  CompPropertiesVTable, marshal__SSB__E),
    C_METHOD (getInt,    "ss",  "i", CompPropertiesVTable, marshal__SS_I_E),
    C_METHOD (setInt,    "ssi", "",  CompPropertiesVTable, marshal__SSI__E),
    C_METHOD (getDouble, "ss",  "d", CompPropertiesVTable, marshal__SS_D_E),
    C_METHOD (setDouble, "ssd", "",  CompPropertiesVTable, marshal__SSD__E),
    C_METHOD (getString, "ss",  "s", CompPropertiesVTable, marshal__SS_S_E),
    C_METHOD (setString, "sss", "",  CompPropertiesVTable, marshal__SSS__E)
};
static const CommonSignal propertiesObjectSignal[] = {
    C_SIGNAL (boolChanged,   "ssb", CompPropertiesVTable),
    C_SIGNAL (intChanged,    "ssi", CompPropertiesVTable),
    C_SIGNAL (doubleChanged, "ssd", CompPropertiesVTable),
    C_SIGNAL (stringChanged, "sss", CompPropertiesVTable)
};
#define INTERFACE_VERSION_propertiesObject CORE_ABIVERSION

static const CommonMethod metadataObjectMethod[] = {
    C_METHOD (get, "s", "s", CompMetadataVTable, marshal__S_S_E)
};
#define INTERFACE_VERSION_metadataObject CORE_ABIVERSION

static const CommonMethod versionObjectMethod[] = {
    C_METHOD (get, "s", "i", CompVersionVTable, marshal_I_S)
};
#define INTERFACE_VERSION_versionObject CORE_ABIVERSION

static const CommonInterface objectInterface[] = {
    C_INTERFACE (object,     Type,   CompObjectVTable, _, _, _, X, _, _, _, _),
    C_INTERFACE (signal,     Object, CompObjectVTable, X, _, _, X, _, _, _, _),
    C_INTERFACE (properties, Object, CompObjectVTable, X, _, X, X, _, _, _, _),
    C_INTERFACE (metadata,   Object, CompObjectVTable, X, _, X, _, _, _, _, _),
    C_INTERFACE (version,    Object, CompObjectVTable, X, _, X, _, _, _, _, _)
};

CompBool
compObjectInit (CompObject     *object,
		CompObjectType *type)
{
    int i;

    if (!(*type->funcs.init) (object))
	return FALSE;

    if (!type->privates)
	return TRUE;

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
    if (type->privates)
    {
	int i = type->privates->nFuncs;

	while (--i >= 0)
	    (*type->privates->funcs[i].fini) (object);
    }

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

typedef struct _ForEachTypeContext {
    TypeCallBackProc proc;
    void	     *closure;
} ForEachTypeContext;

static CompBool
baseObjectForEachType (CompObject *object,
		       void       *closure)
{
    ForEachTypeContext *pCtx = (ForEachTypeContext *) closure;

    return (*object->vTable->forEachType) (object, pCtx->proc, pCtx->closure);
}

static CompBool
noopForEachType (CompObject	  *object,
		 TypeCallBackProc proc,
		 void	          *closure)
{
    ForEachTypeContext ctx;

    ctx.proc    = proc;
    ctx.closure = closure;

    return (*object->vTable->forBaseObject) (object,
					     baseObjectForEachType,
					     (void *) &ctx);
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

static CompBool
baseObjectChildObjectAdded (CompObject *object,
			    void       *closure)
{
    (*object->vTable->childObjectAdded) (object, (CompObject *) closure);
    return TRUE;
}

static void
noopChildObjectAdded (CompObject *object,
		      CompObject *child)
{
    (*object->vTable->forBaseObject) (object,
				      baseObjectChildObjectAdded,
				      (void *) child);
}

static CompBool
baseObjectChildObjectRemoved (CompObject *object,
			      void       *closure)
{
    (*object->vTable->childObjectRemoved) (object, (CompObject *) closure);
    return TRUE;
}

static void
noopChildObjectRemoved (CompObject *object,
			CompObject *child)
{
    (*object->vTable->forBaseObject) (object,
				      baseObjectChildObjectRemoved,
				      (void *) child);
}

typedef struct _ConnectContext {
    const char        *interface;
    size_t	      offset;
    SignalHandlerProc proc;
    void	      *data;
    int		      result;
} ConnectContext;

static CompBool
baseObjectConnect (CompObject *object,
		   void       *closure)
{
    ConnectContext *pCtx = (ConnectContext *) closure;

    pCtx->result = (*object->vTable->signal.connect) (object,
						      pCtx->interface,
						      pCtx->offset,
						      pCtx->proc,
						      pCtx->data);

    return TRUE;
}

static int
noopConnect (CompObject	       *object,
	     const char        *interface,
	     size_t	       offset,
	     SignalHandlerProc proc,
	     void	       *data)
{
    ConnectContext ctx;

    ctx.interface = interface;
    ctx.offset    = offset;
    ctx.proc      = proc;
    ctx.data      = data;

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
    CompObject *source;
    const char *interface;
    const char *name;
    const char *signature;
    va_list    args;
} SignalContext;

static CompBool
baseObjectSignal (CompObject *object,
		  void       *closure)
{
    SignalContext *pCtx = (SignalContext *) closure;

    (*object->vTable->signal.signal) (object,
				      pCtx->source,
				      pCtx->interface,
				      pCtx->name,
				      pCtx->signature,
				      pCtx->args);

    return TRUE;
}

static void
noopSignal (CompObject *object,
	    CompObject *source,
	    const char *interface,
	    const char *name,
	    const char *signature,
	    va_list    args)
{
    SignalContext ctx;

    ctx.source    = source;
    ctx.interface = interface;
    ctx.name      = name;
    ctx.signature = signature;
    ctx.args      = args;

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

CompBool
handleForEachInterface (CompObject	      *object,
			const CommonInterface *interface,
			int		      nInterface,
			const CompObjectType  *type,
			InterfaceCallBackProc proc,
			void		      *closure)
{
    int i;

    for (i = 0; i < nInterface; i++)
	if (!(*proc) (object,
		      interface[i].name,
		      (void *) &interface[i],
		      interface[i].offset,
		      type,
		      closure))
	    return FALSE;

    return noopForEachInterface (object, proc, closure);
}

typedef struct _IsCommonInterfaceContext {
    CompObjectVTable *vTable;
    const char	     *name;
    void	     *key;
} IsCommonInterfaceContext;

static CompBool
isNotCommonInterface (CompObject	   *object,
		      const char	   *name,
		      void		   *key,
		      size_t		   offset,
		      const CompObjectType *type,
		      void		   *closure)
{
    IsCommonInterfaceContext *pCtx = (IsCommonInterfaceContext *) closure;

    if (pCtx->key)
    {
	if (key != pCtx->key)
	    return TRUE;
    }
    else
    {
	if (strcmp (name, pCtx->name))
	    return TRUE;
    }

    if (pCtx->vTable == object->vTable)
    {
	pCtx->key = key;

	return FALSE;
    }

    return TRUE;
}

static CommonInterface *
getCommonInterface (CompObject *object,
		    const char *name,
		    void       *key)
{
    IsCommonInterfaceContext ctx;

    ctx.vTable = object->vTable;
    ctx.name   = name;
    ctx.key    = key;

    if ((*object->vTable->forEachInterface) (object,
					     isNotCommonInterface,
					     (void *) &ctx))
	return NULL;

    return (CommonInterface *) ctx.key;
}

static CompBool
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
    CommonInterface *interface;

    interface = getCommonInterface (object, NULL, key);
    if (interface)
	if (!handleForEachMethod (object,
				  interface->method, interface->nMethod,
				  proc, closure))
	    return FALSE;

    return noopForEachMethod (object, key, proc, closure);
}

static CompBool
handleForEachSignal (CompObject	        *object,
		     const CommonSignal *signal,
		     int		nSignal,
		     SignalCallBackProc proc,
		     void		*closure)
{
    int i;

    for (i = 0; i < nSignal; i++)
	if (signal[i].out)
	    if (!(*proc) (object,
			  signal[i].name,
			  signal[i].out,
			  signal[i].offset,
			  closure))
		return FALSE;

    return TRUE;
}

CompBool
commonForEachSignal (CompObject		*object,
		     void	        *key,
		     SignalCallBackProc proc,
		     void		*closure)
{
    CommonInterface *interface;

    interface = getCommonInterface (object, NULL, key);
    if (interface)
	if (!handleForEachSignal (object,
				  interface->signal, interface->nSignal,
				  proc, closure))
	    return FALSE;

    return noopForEachSignal (object, key, proc, closure);
}

static CompBool
handleForEachBoolProp (CompObject	    *object,
		       const CommonBoolProp *prop,
		       int		    nProp,
		       PropCallBackProc	    proc,
		       void		    *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_BOOLEAN, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachIntProp (CompObject	  *object,
		      const CommonIntProp *prop,
		      int		  nProp,
		      PropCallBackProc	  proc,
		      void		  *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_INT32, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachDoubleProp (CompObject	        *object,
			 const CommonDoubleProp *prop,
			 int		        nProp,
			 PropCallBackProc       proc,
			 void		        *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_DOUBLE, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachStringProp (CompObject	        *object,
			 const CommonStringProp *prop,
			 int		        nProp,
			 PropCallBackProc       proc,
			 void		        *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_STRING, closure))
	    return FALSE;

    return TRUE;
}

CompBool
commonForEachProp (CompObject	    *object,
		   void		    *key,
		   PropCallBackProc proc,
		   void		    *closure)
{
    CommonInterface *interface;

    interface = getCommonInterface (object, NULL, key);
    if (interface)
    {
	if (!handleForEachBoolProp (object,
				    interface->boolProp,
				    interface->nBoolProp,
				    proc, closure))
	    return FALSE;

	if (!handleForEachIntProp (object,
				   interface->intProp,
				   interface->nIntProp,
				   proc, closure))
	    return FALSE;

	if (!handleForEachDoubleProp (object,
				      interface->doubleProp,
				      interface->nDoubleProp,
				      proc, closure))
	    return FALSE;

	if (!handleForEachStringProp (object,
				      interface->stringProp,
				      interface->nStringProp,
				      proc, closure))
	    return FALSE;
    }

    return noopForEachProp (object, key, proc, closure);
}

static CompBool
handleGetInterfaceVersion (CompObject		*object,
			   const char	        *name,
			   void			*key,
			   size_t		offset,
			   const CompObjectType *type,
			   void			*closure)
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
				   getObjectType (),
				   proc, closure);
}

static void
interfaceAdded (CompObject *object,
		const char *interface)
{
    EMIT_EXT_SIGNAL (object,
		     object->signal[COMP_OBJECT_SIGNAL_INTERFACE_ADDED],
		     "object", "interfaceAdded", "s", interface);
}

void
commonInterfacesAdded (CompObject	     *object,
		       const CommonInterface *interface,
		       int		     nInterface)
{
    while (nInterface--)
    {
	(*object->vTable->interfaceAdded) (object, interface->name);

	interface++;
    }
}

static void
interfaceRemoved (CompObject *object,
		  const char *interface)
{
    EMIT_EXT_SIGNAL (object,
		     object->signal[COMP_OBJECT_SIGNAL_INTERFACE_REMOVED],
		     "object", "interfaceRemoved", "s", interface);
}

void
commonInterfacesRemoved (CompObject	       *object,
			 const CommonInterface *interface,
			 int		       nInterface)
{
    interface += (nInterface - 1);

    while (nInterface--)
    {
	(*object->vTable->interfaceAdded) (object, interface->name);

	interface--;
    }
}

static CompBool
forEachType (CompObject	      *object,
	     TypeCallBackProc proc,
	     void	      *closure)
{
    return (*proc) (object, getObjectType (), closure);
}

static void
childObjectAdded (CompObject *object,
		  CompObject *child)
{
    EMIT_EXT_SIGNAL (object, object->signal[COMP_OBJECT_SIGNAL_CHILD_ADDED],
		     "object", "childObjectAdded", "o", child);
}

static void
childObjectRemoved (CompObject *object,
		    CompObject *child)
{
    EMIT_EXT_SIGNAL (object, object->signal[COMP_OBJECT_SIGNAL_CHILD_REMOVED],
		     "object", "childObjectRemoved", "o", child);
}

static CompBool
forEachChildObject (CompObject		    *object,
		    ChildObjectCallBackProc proc,
		    void		    *closure)
{
    return TRUE;
}

static CompBool
signalIndex (const char		   *name,
	     const CommonInterface *interface,
	     int		   nInterface,
	     size_t		   offset,
	     int		   *index)
{
    int i, j, count = 0;

    for (i = 0; i < nInterface; i++)
    {
	if (strcmp (name, interface[i].name) == 0)
	{
	    for (j = 0; j < interface[i].nSignal; j++)
	    {
		if (offset == interface[i].signal[j].offset)
		{
		    *index = count;
		    return TRUE;
		}

		count++;
	    }

	    *index = -1;
	    return TRUE;
	}

	count += interface[i].nSignal;
    }

    return FALSE;
}

static int
handleConnect (CompObject	     *object,
	       const CommonInterface *interface,
	       int		     nInterface,
	       CompSignalHandler     **vec,
	       const char	     *name,
	       size_t		     offset,
	       SignalHandlerProc     proc,
	       void		     *data)
{
    int	index;

    if (signalIndex (name, interface, nInterface, offset, &index))
    {
	CompSignalHandler *handler;

	if (index < 0)
	    return -1;

	handler = malloc (sizeof (CompSignalHandler));
	if (!handler)
	    return -1;

	handler->proc = proc;
	handler->data = data;

	if (vec[index])
	{
	    handler->next = vec[index];
	    handler->id   = vec[index]->id + 1;
	}
	else
	{
	    handler->next = NULL;
	    handler->id   = 1;
	}

	vec[index] = handler;

	return handler->id;
    }
    else
    {
	return noopConnect (object, name, offset, proc, data);
    }
}

static void
handleDisconnect (CompObject	        *object,
		  const CommonInterface *interface,
		  int		        nInterface,
		  CompSignalHandler     **vec,
		  const char		*name,
		  size_t		offset,
		  int			id)
{
    int	index;

    if (signalIndex (name, interface, nInterface, offset, &index))
    {
	CompSignalHandler *handler, *prev = NULL;

	if (index < 0)
	    return;

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
    }
    else
    {
	return noopDisconnect (object, name, offset, id);
    }
}

static int
connect (CompObject	   *object,
	 const char        *interface,
	 size_t		   offset,
	 SignalHandlerProc proc,
	 void		   *data)
{
    return handleConnect (object,
			  objectInterface, N_ELEMENTS (objectInterface),
			  object->signal,
			  interface,
			  offset,
			  proc,
			  data);
}

static void
disconnect (CompObject *object,
	    const char *interface,
	    size_t     offset,
	    int	       id)
{
    handleDisconnect (object,
		      objectInterface, N_ELEMENTS (objectInterface),
		      object->signal,
		      interface,
		      offset,
		      id);
}

static void
signal (CompObject *object,
	CompObject *source,
	const char *interface,
	const char *name,
	const char *signature,
	va_list    args)
{
    EMIT_SIGNAL (object,
		 object->signal[COMP_OBJECT_SIGNAL_SIGNAL],
		 source, interface, name, signature, args);

    if (object->parent)
	(*object->parent->vTable->signal.signal) (object->parent, source,
						  interface, name, signature,
						  args);
}

void
emitSignalSignal (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *signature,
		  ...)
{
    va_list args;

    va_start (args, signature);

    (*object->vTable->signal.signal) (object, object,
				      interface, name, signature,
				      args);

    va_end (args);
}

#define INTERFACE_DATA(object, interface) \
    ((interface)->data ? (*(interface)->data) (object) : (char *) object)

static CompBool
handleGetBoolProp (CompObject		*object,
		   const CommonBoolProp *prop,
		   int			nProp,
		   void			*data,
		   const char		*interface,
		   const char		*name,
		   CompBool		*value,
		   char			**error)
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
commonGetBoolProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   CompBool   *value,
		   char	      **error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleGetBoolProp (object,
				  commonInterface->boolProp,
				  commonInterface->nBoolProp,
				  INTERFACE_DATA (object, commonInterface),
				  interface,
				  name,
				  value,
				  error);

    return noopGetBoolProp (object, interface, name, value, error);
}

static CompBool
handleSetBoolProp (CompObject		*object,
		   const CommonBoolProp *prop,
		   int			nProp,
		   void			*data,
		   const char		*interface,
		   const char		*name,
		   CompBool		value,
		   char			**error)
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
commonSetBoolProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   CompBool   value,
		   char	      **error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleSetBoolProp (object,
				  commonInterface->boolProp,
				  commonInterface->nBoolProp,
				  INTERFACE_DATA (object, commonInterface),
				  interface,
				  name,
				  value,
				  error);

    return noopSetBoolProp (object, interface, name, value, error);
}

static void
handleBoolPropChanged (CompObject	    *object,
		       const CommonBoolProp *prop,
		       int		    nProp,
		       const char	    *interface,
		       const char	    *name,
		       CompBool		    value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
commonBoolPropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       CompBool   value)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	handleBoolPropChanged (object,
			       commonInterface->boolProp,
			       commonInterface->nBoolProp,
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
    EMIT_EXT_SIGNAL (object, object->signal[COMP_OBJECT_SIGNAL_BOOL_CHANGED],
		     "properties", "boolChanged", "ssb",
		     interface, name, value);
}

static CompBool
handleGetIntProp (CompObject	      *object,
		  const CommonIntProp *prop,
		  int		      nProp,
		  void		      *data,
		  const char	      *interface,
		  const char	      *name,
		  int32_t	      *value,
		  char		      **error)
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
commonGetIntProp (CompObject *object,
		  const char *interface,
		  const char *name,
		  int32_t    *value,
		  char	     **error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleGetIntProp (object,
				 commonInterface->intProp,
				 commonInterface->nIntProp,
				 INTERFACE_DATA (object, commonInterface),
				 interface,
				 name,
				 value,
				 error);

    return noopGetIntProp (object, interface, name, value, error);
}

static CompBool
handleSetIntProp (CompObject	      *object,
		  const CommonIntProp *prop,
		  int		      nProp,
		  void		      *data,
		  const char	      *interface,
		  const char	      *name,
		  int32_t	      value,
		  char		      **error)
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
commonSetIntProp (CompObject *object,
		  const char *interface,
		  const char *name,
		  int32_t    value,
		  char	     **error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleSetIntProp (object,
				 commonInterface->intProp,
				 commonInterface->nIntProp,
				 INTERFACE_DATA (object, commonInterface),
				 interface,
				 name,
				 value,
				 error);

    return noopSetIntProp (object, interface, name, value, error);
}

static void
handleIntPropChanged (CompObject	  *object,
		      const CommonIntProp *prop,
		      int		  nProp,
		      const char	  *interface,
		      const char	  *name,
		      int32_t		  value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
commonIntPropChanged (CompObject *object,
		      const char *interface,
		      const char *name,
		      int32_t    value)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	handleIntPropChanged (object,
			      commonInterface->intProp,
			      commonInterface->nIntProp,
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
    EMIT_EXT_SIGNAL (object, object->signal[COMP_OBJECT_SIGNAL_INT_CHANGED],
		     "properties", "intChanged", "ssi",
		     interface, name, value);
}

static CompBool
handleGetDoubleProp (CompObject		    *object,
		     const CommonDoubleProp *prop,
		     int		    nProp,
		     void		    *data,
		     const char		    *interface,
		     const char		    *name,
		     double		    *value,
		     char		    **error)
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
commonGetDoubleProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     double     *value,
		     char	**error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleGetDoubleProp (object,
				    commonInterface->doubleProp,
				    commonInterface->nDoubleProp,
				    INTERFACE_DATA (object, commonInterface),
				    interface,
				    name,
				    value,
				    error);

    return noopGetDoubleProp (object, interface, name, value, error);
}

static CompBool
handleSetDoubleProp (CompObject		    *object,
		     const CommonDoubleProp *prop,
		     int		    nProp,
		     void		    *data,
		     const char		    *interface,
		     const char		    *name,
		     double		    value,
		     char		    **error)
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
commonSetDoubleProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     double     value,
		     char	**error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleSetDoubleProp (object,
				    commonInterface->doubleProp,
				    commonInterface->nDoubleProp,
				    INTERFACE_DATA (object, commonInterface),
				    interface,
				    name,
				    value,
				    error);

    return noopSetDoubleProp (object, interface, name, value, error);
}

static void
handleDoublePropChanged (CompObject		*object,
			 const CommonDoubleProp	*prop,
			 int			nProp,
			 const char		*interface,
			 const char		*name,
			 double			value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
commonDoublePropChanged (CompObject *object,
			 const char *interface,
			 const char *name,
			 double     value)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	handleDoublePropChanged (object,
				 commonInterface->doubleProp,
				 commonInterface->nDoubleProp,
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
    EMIT_EXT_SIGNAL (object, object->signal[COMP_OBJECT_SIGNAL_DOUBLE_CHANGED],
		     "properties", "doubleChanged", "ssd",
		     interface, name, value);
}

static CompBool
handleGetStringProp (CompObject		    *object,
		     const CommonStringProp *prop,
		     int		    nProp,
		     void		    *data,
		     const char		    *interface,
		     const char		    *name,
		     char		    **value,
		     char		    **error)
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
commonGetStringProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     char       **value,
		     char	**error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleGetStringProp (object,
				    commonInterface->stringProp,
				    commonInterface->nStringProp,
				    INTERFACE_DATA (object, commonInterface),
				    interface,
				    name,
				    value,
				    error);

    return noopGetStringProp (object, interface, name, value, error);
}

static CompBool
handleSetStringProp (CompObject		    *object,
		     const CommonStringProp *prop,
		     int		    nProp,
		     void		    *data,
		     const char		    *interface,
		     const char		    *name,
		     const char		    *value,
		     char		    **error)
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
commonSetStringProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     const char *value,
		     char	**error)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	return handleSetStringProp (object,
				    commonInterface->stringProp,
				    commonInterface->nStringProp,
				    INTERFACE_DATA (object, commonInterface),
				    interface,
				    name,
				    value,
				    error);

    return noopSetStringProp (object, interface, name, value, error);
}

static void
handleStringPropChanged (CompObject		*object,
			 const CommonStringProp	*prop,
			 int			nProp,
			 const char		*interface,
			 const char		*name,
			 const char		*value)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (prop[i].changed && strcmp (name, prop[i].base.name) == 0)
	    (*prop[i].changed) (object, interface, name, value);
}

void
commonStringPropChanged (CompObject *object,
			 const char *interface,
			 const char *name,
			 const char *value)
{
    CommonInterface *commonInterface;

    commonInterface = getCommonInterface (object, interface, NULL);
    if (commonInterface)
	handleStringPropChanged (object,
				 commonInterface->stringProp,
				 commonInterface->nStringProp,
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
    EMIT_EXT_SIGNAL (object, object->signal[COMP_OBJECT_SIGNAL_STRING_CHANGED],
		     "properties", "stringChanged", "sss",
		     interface, name, value);
}

static CompBool
getMetadata (CompObject *object,
	     const char *interface,
	     char	**data,
	     char	**error)
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
    interfaceAdded,
    interfaceRemoved,
    forEachType,
    forEachChildObject,
    childObjectAdded,
    childObjectRemoved,
    {
	connect,
	disconnect,
	signal
    }, {
	commonGetVersion
    }, {
	getBoolProp,
	setBoolProp,
	boolPropChanged,
	getIntProp,
	setIntProp,
	intPropChanged,
	getDoubleProp,
	setDoubleProp,
	doublePropChanged,
	getStringProp,
	setStringProp,
	stringPropChanged
    }, {
	getMetadata
    }
};

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
    object->parent = NULL;

    if (!allocateObjectPrivates (object, &objectPrivates))
	return FALSE;

    memset (object->signal, 0, sizeof (object->signal));

    object->parent = NULL;

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
    ENSURE (vTable, interfaceAdded,   noopInterfaceAdded);
    ENSURE (vTable, interfaceRemoved, noopInterfaceRemoved);

    ENSURE (vTable, forEachType, noopForEachType);

    ENSURE (vTable, forEachChildObject, noopForEachChildObject);
    ENSURE (vTable, childObjectAdded,   noopChildObjectAdded);
    ENSURE (vTable, childObjectRemoved, noopChildObjectRemoved);

    ENSURE (vTable, signal.connect,    noopConnect);
    ENSURE (vTable, signal.disconnect, noopDisconnect);
    ENSURE (vTable, signal.signal,     noopSignal);

    ENSURE (vTable, version.get, noopGetVersion);

    ENSURE (vTable, properties.getBool,     noopGetBoolProp);
    ENSURE (vTable, properties.setBool,     noopSetBoolProp);
    ENSURE (vTable, properties.boolChanged, noopBoolPropChanged);

    ENSURE (vTable, properties.getInt,     noopGetIntProp);
    ENSURE (vTable, properties.setInt,     noopSetIntProp);
    ENSURE (vTable, properties.intChanged, noopIntPropChanged);

    ENSURE (vTable, properties.getDouble,     noopGetDoubleProp);
    ENSURE (vTable, properties.setDouble,     noopSetDoubleProp);
    ENSURE (vTable, properties.doubleChanged, noopDoublePropChanged);

    ENSURE (vTable, properties.getString,     noopGetStringProp);
    ENSURE (vTable, properties.setString,     noopSetStringProp);
    ENSURE (vTable, properties.stringChanged, noopStringPropChanged);

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
forEachTypePrivates (CompObject		  *object,
		     const CompObjectType *type,
		     void		  *closure)
{
    ForEachObjectPrivatesContext *pCtx =
	(ForEachObjectPrivatesContext *) closure;

    if (type == pCtx->type)
    {
	CompObjectPrivates *objectPrivates = pCtx->type->privates;
	CompPrivate	   **pPrivates = (CompPrivate **)
	    (((char *) object) + objectPrivates->offset);

	return (*pCtx->proc) (pPrivates, pCtx->data);
    }

    return TRUE;
}

static CompBool
forEachObjectPrivatesTree (CompObject *object,
			   void	      *closure)
{
    if (!(*object->vTable->forEachType) (object,
					 forEachTypePrivates,
					 closure))
	return FALSE;

    return (*object->vTable->forEachChildObject) (object,
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

    if (!(*core.u.base.vTable->forEachType) (&core.u.base,
					     forEachTypePrivates,
					     (void *) &ctx))
	return FALSE;

    return (*core.u.base.vTable->forEachChildObject) (&core.u.base,
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
topObjectType (CompObject	    *object,
	       const CompObjectType *type,
	       void		    *closure)
{
    *((const CompObjectType **) closure) = type;

    return FALSE;
}

typedef struct _InitObjectContext {
    CompObjectType  *type;
    CompObjectFuncs *funcs;
    CompObject      *object;
} InitObjectContext;

static CompBool
initBaseObject (CompObject *object,
		void	   *closure)
{
    InitObjectContext    *pCtx = (InitObjectContext *) closure;
    const CompObjectType *type;

    (*object->vTable->forEachType) (object, topObjectType, (void *) &type);

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

    (*object->vTable->forEachType) (object, topObjectType, (void *) &type);

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
initObjectTree (CompObject *object,
		void	   *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    pCtx->object = object;

    return initTypedObjects (object, pCtx->type, pCtx->funcs);
}

static CompBool
finiObjectTree (CompObject *object,
		void	   *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == object)
	return FALSE;

    return finiTypedObjects (object, pCtx->type, pCtx->funcs);
}

static CompBool
initTypedObjects (CompObject	  *object,
		  CompObjectType  *type,
		  CompObjectFuncs *funcs)
{
    InitObjectContext ctx;

    ctx.type   = type;
    ctx.funcs  = funcs;
    ctx.object = NULL;

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
finiTypedObjects (CompObject	  *object,
		  CompObjectType  *type,
		  CompObjectFuncs *funcs)
{
    InitObjectContext ctx;

    ctx.type   = type;
    ctx.funcs  = funcs;
    ctx.object = NULL;

    (*object->vTable->forEachChildObject) (object,
					   finiObjectTree,
					   (void *) &ctx);

    finiBaseObject (object, (void *) &ctx);

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
    if (!initTypedObjects (&core.u.base, type, &funcs[type->privates->nFuncs]))
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
    finiTypedObjects (&core.u.base, type,
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

typedef struct _ForInterfaceContext {
    const char		  *interface;
    InterfaceCallBackProc proc;
    void		  *closure;
} ForInterfaceContext;

static CompBool
handleInterface (CompObject	      *object,
		 const char	      *name,
		 void		      *key,
		 size_t		      offset,
		 const CompObjectType *type,
		 void		      *closure)
{
    ForInterfaceContext *pCtx = (ForInterfaceContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
	if (!(*pCtx->proc) (object, name, key, offset, type, pCtx->closure))
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
		     void		  *key,
		     size_t		  offset,
		     const CompObjectType *type,
		     void		  *closure)
{
    int *version = (int *) closure;

    *version = (object->vTable->version.get) (object, name);

    return FALSE;
}

typedef struct _LookupObjectContext {
    char       **path;
    CompObject *object;
} LookupObjectContext;

static CompBool
checkChildObject (CompObject *object,
		  void	     *closure)
{
    LookupObjectContext *pCtx = (LookupObjectContext *) closure;

    if (strcmp (object->name, pCtx->path[0]))
	return TRUE;

    if (pCtx->path[1])
    {
	pCtx->path = &pCtx->path[1];

	return (*object->vTable->forEachChildObject) (object,
						      checkChildObject,
						      closure);
    }

    pCtx->object = object;

    return FALSE;
}

CompObject *
compLookupObject (CompObject *root,
		  char	     **path)
{
    LookupObjectContext ctx;

    ctx.path = path;

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

static void
basicArgsLoad (CompArgs   *args,
	       const char *type,
	       void       *value)
{
    CompBasicArgs *b = (CompBasicArgs *) args;

    switch (type[0]) {
    case COMP_TYPE_BOOLEAN:
	*((CompBool *) value) = b->in[b->inPos++].b;
	break;
    case COMP_TYPE_INT32:
	*((int32_t *) value) = b->in[b->inPos++].i;
	break;
    case COMP_TYPE_DOUBLE:
	*((double *) value) = b->in[b->inPos++].d;
	break;
    case COMP_TYPE_STRING:
	*((char **) value) = b->in[b->inPos++].s;
	break;
    case COMP_TYPE_OBJECT:
	*((CompObject **) value) = b->in[b->inPos++].o;
	break;
    }
}

static void
basicArgsStore (CompArgs   *args,
		const char *type,
		void       *value)
{
    CompBasicArgs *b = (CompBasicArgs *) args;

    switch (type[0]) {
    case COMP_TYPE_BOOLEAN:
	b->out[b->outPos++].b = *((CompBool *) value);
	break;
    case COMP_TYPE_INT32:
	b->out[b->outPos++].i = *((int32_t *) value);
	break;
    case COMP_TYPE_DOUBLE:
	b->out[b->outPos++].d = *((double *) value);
	break;
    case COMP_TYPE_STRING:
	b->out[b->outPos++].s = *((char **) value);
	break;
    case COMP_TYPE_OBJECT:
	b->out[b->outPos++].o = *((CompObject **) value);
	break;
    }
}

static void
basicArgsError (CompArgs *args,
		char     *error)
{
    CompBasicArgs *b = (CompBasicArgs *) args;

    b->error = error;
}

void
compInitBasicArgs (CompBasicArgs *args,
		   CompAnyValue  *in,
		   CompAnyValue  *out)
{
    args->base.load  = basicArgsLoad;
    args->base.store = basicArgsStore;
    args->base.error = basicArgsError;

    args->in  = in;
    args->out = out;

    args->inPos  = 0;
    args->outPos = 0;

    args->error = NULL;
}

void
compFiniBasicArgs (CompBasicArgs *args)
{
    if (args->error)
	free (args->error);
}

typedef struct _MethodCallContext {
    const char	      *interface;
    const char	      *name;
    const char	      *in;
    const char	      *out;
    CompObjectVTable  *vTable;
    int		      offset;
    MethodMarshalProc marshal;
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
		void		     *key,
		size_t		     offset,
		const CompObjectType *type,
		void		     *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object, key, checkMethod,
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
invokeMethod (CompObject        *object,
	      CompObjectVTable  *vTable,
	      int	        offset,
	      MethodMarshalProc marshal,
	      CompArgs		*args)
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
compInvokeMethod (CompObject *object,
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

static CompBool
setIndex (int *index,
	  int value)
{
    *index = value;
    return TRUE;
}

static CompBool
propertyIndex (CommonInterface *interface,
	       const char      *name,
	       int	       type,
	       int	       *index)
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
    CommonInterface *interface;
    int		    nInterface;
    int		    depth;
    int		    validDepth;
    CommonInterface *currentInterface;
    int		    propType;
    int		    propIndex;
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
	    CommonBoolProp *prop =
		&pCtx->currentInterface->boolProp[pCtx->propIndex];

	    if (len == 4 && strncmp ((const char *) ch, "true", 4) == 1)
		prop->defaultValue = TRUE;
	    else
		prop->defaultValue = FALSE;
	} break;
	case COMP_TYPE_INT32: {
	    char tmp[256];

	    if (len < sizeof (tmp))
	    {
		CommonIntProp *prop =
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
		CommonDoubleProp *prop =
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
		CommonStringProp *prop =
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

#define HOME_DATADIR   ".compiz/data"
#define XML_EXTENSION ".xml"

static CompBool
defaultValuesFromFile (CommonInterface *interface,
		       int	       nInterface,
		       const char      *path,
		       const char      *name)
{
    xmlSAXHandler saxHandler = {
	.initialized  = XML_SAX2_MAGIC,
	.startElement = handleStartElement,
	.endElement   = handleEndElement,
	.characters   = handleCharacters
    };
    DefaultValuesContext ctx = {
	.interface  = interface,
	.nInterface = nInterface
    };
    char *file;
    int  length = strlen (name) + strlen (XML_EXTENSION) + 1;
    FILE *fp;
    int  status;

    if (path)
	length += strlen (path) + 1;

    file = malloc (length);
    if (!file)
	return FALSE;

    if (path)
	sprintf (file, "%s/%s%s", path, name, XML_EXTENSION);
    else
	sprintf (file, "%s%s", name, XML_EXTENSION);

    fp = fopen (file, "r");
    if (!fp)
    {
	free (file);
	return FALSE;
    }

    fclose (fp);

    status = xmlSAXUserParseFile (&saxHandler, &ctx, file);

    free (file);

    return (status == 0) ? TRUE : FALSE;
}

void
commonDefaultValuesFromFile (CommonInterface *interface,
			     int	     nInterface,
			     const char      *name)
{
    CompBool status = FALSE;
    char     *home;

    home = getenv ("HOME");
    if (home)
    {
	char *path;

	path = malloc (strlen (home) + strlen (HOME_DATADIR) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s", home, HOME_DATADIR);
	    status = defaultValuesFromFile (interface, nInterface, path, name);
	    free (path);
	}
    }

    if (!status)
	defaultValuesFromFile (interface, nInterface, METADATADIR, name);
}

#define PROP_VALUE(data, prop, type)	       \
    (*((type *) (data + (prop)->base.offset)))

#define SET_DEFAULT_VALUE(data, prop, type)		 \
    PROP_VALUE (data, prop, type) = (prop)->defaultValue

CompBool
initCommonObjectProperties (CompObject		  *object,
			    const CommonInterface *interface,
			    int			  nInterface)
{
    char *data;
    int  i, j;

    for (i = 0; i < nInterface; i++)
    {
	data = INTERFACE_DATA (object, &interface[i]);

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
			finiCommonObjectProperties (object, &interface[i], 1);
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
finiCommonObjectProperties (CompObject		  *object,
			    const CommonInterface *interface,
			    int			  nInterface)
{
    char *data;
    int  i, j;

    for (i = 0; i < nInterface; i++)
    {
	data = INTERFACE_DATA (object, &interface[i]);

	for (j = 0; j < interface[i].nStringProp; j++)
	{
	    char *str;

	    str = PROP_VALUE (data, &interface[i].stringProp[j], char *);
	    if (str)
		free (str);
	}
    }
}
