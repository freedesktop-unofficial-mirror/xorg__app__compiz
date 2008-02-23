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

#ifndef _COMPIZ_OBJECT_H
#define _COMPIZ_OBJECT_H

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

#include <compiz/types.h>
#include <compiz/macros.h>
#include <compiz/privates.h>

COMPIZ_BEGIN_DECLS

/*
  object-type, object, interface and member names

  - must only contain the ASCII characters "[A-Z][a-z][0-9]_"
  - may not be the empty string
  - may not be exceed the maximum length of 127 characters

  object-type, interface and member names

  - must also not begin with a digit
*/

typedef struct _CompObject	       CompObject;
typedef struct _CompObjectType	       CompObjectType;
typedef struct _CompObjectType	       CompObjectInterface;
typedef struct _CompObjectFactory      CompObjectFactory;
typedef struct _CompObjectVTable       CompObjectVTable;
typedef struct _CompObjectInstantiator CompObjectInstantiator;

typedef CompBool (*InitObjectProc) (const CompObjectInstantiator *i,
				    CompObject			 *object,
				    const CompObjectFactory	 *factory);

typedef struct _CompObjectPrivates {
    int	len;
    int	*sizes;
    int	totalSize;
} CompObjectPrivates;

struct _CompObjectInstantiator {
    const CompObjectInstantiator *base;
    const CompObjectInterface	 *interface;
    InitObjectProc		 init;
    CompObjectVTable		 *vTable;
};

typedef struct _CompObjectInstantiatorNode {
    CompObjectInstantiator	       base;
    struct _CompObjectInstantiatorNode *next;
    const CompObjectInstantiator       *instantiator;
    CompObjectPrivates		       privates;
} CompObjectInstantiatorNode;

struct _CompObjectFactory {
    const CompObjectFactory    *master;
    CompObjectInstantiatorNode *instantiators;
};

typedef struct _CompObjectPrivatesNode {
    struct _CompObjectPrivatesNode *next;
    const char			   *name;
    CompObjectPrivates		   privates;
} CompObjectPrivatesNode;

typedef struct _CompFactory CompFactory;

typedef int (*AllocatePrivateIndexProc) (CompFactory *factory,
					 const char  *name,
					 int	     size);

typedef void (*FreePrivateIndexProc) (CompFactory *factory,
				      const char  *name,
				      int	  index);

struct _CompFactory {
    CompObjectFactory base;

    AllocatePrivateIndexProc allocatePrivateIndex;
    FreePrivateIndexProc     freePrivateIndex;

    CompObjectPrivatesNode *privates;
};

typedef CompBool (*InstallProc)   (const CompObjectInterface *interface,
				   CompFactory		      *factory);
typedef void     (*UninstallProc) (const CompObjectInterface *interface,
				   CompFactory	             *factory);

typedef CompBool (*InitInterfaceProc) (CompObject	       *object,
				       const CompObjectVTable  *vTable,
				       const CompObjectFactory *factory);
typedef void     (*FiniInterfaceProc) (CompObject *object);

struct _CompObjectType {
    struct {
	const char *name;
	const char *base;
    } name;

    struct {
	const CompObjectVTable *impl;
	const CompObjectVTable *noop;
	size_t		       size;
    } vTable;

    struct {
	InitObjectProc init;
	size_t	       size;
    } instance;

    struct {
	InstallProc   install;
	UninstallProc uninstall;
    } factory;

    struct {
	InitInterfaceProc init;
	FiniInterfaceProc fini;
    } interface;
};

typedef unsigned int CompObjectTypeID;

#define COMP_OBJECT_TYPE_CORE    0
#define COMP_OBJECT_TYPE_DISPLAY 1
#define COMP_OBJECT_TYPE_SCREEN  2
#define COMP_OBJECT_TYPE_WINDOW  3

/* compiz uses a sub-set of the type-codes in the dbus specification

   BOOLEAN	98  (ASCII 'b')	Boolean value, 0 is FALSE and 1 is TRUE.
   INT32	105 (ASCII 'i')	32-bit signed integer.
   DOUBLE	100 (ASCII 'd')	IEEE 754 double.
   STRING	115 (ASCII 's')	Nul terminated UTF-8 string.
   OBJECT	111 (ASCII 'o')	Object path
*/

#define COMP_TYPE_INVALID ((int) '\0')

#define COMP_TYPE_BOOLEAN ((int) 'b')
#define COMP_TYPE_INT32   ((int) 'i')
#define COMP_TYPE_DOUBLE  ((int) 'd')
#define COMP_TYPE_STRING  ((int) 's')
#define COMP_TYPE_OBJECT  ((int) 'o')

typedef union {
    CompBool b;
    int32_t  i;
    double   d;
    char     *s;
} CompAnyValue;

typedef struct _CompArgs CompArgs;

typedef void (*ArgsLoadProc) (CompArgs *args,
			      int      type,
			      void     *value);

typedef void (*ArgsStoreProc) (CompArgs	*args,
			       int      type,
			       void	*value);

typedef void (*ArgsErrorProc) (CompArgs	*args,
			       char	*error);

struct _CompArgs {
    ArgsLoadProc  load;
    ArgsStoreProc store;
    ArgsErrorProc error;
};

typedef void (*FinalizeObjectProc) (CompObject *object);

#define COMP_PROP_BASE_VTABLE 0
#define COMP_PROP_PRIVATES    1

typedef void (*GetPropProc) (CompObject   *object,
			     unsigned int what,
			     void	  *value);

typedef void (*SetPropProc) (CompObject   *object,
			     unsigned int what,
			     void	  *value);

typedef void (*InsertObjectProc) (CompObject *object,
				  CompObject *parent,
				  const char *name);

typedef void (*RemoveObjectProc) (CompObject *object);

typedef void (*InsertedProc) (CompObject *object);

typedef void (*RemovedProc)  (CompObject *object);

typedef CompBool (*InterfaceCallBackProc) (CompObject		*object,
					   const char		*name,
					   size_t		offset,
					   const CompObjectType *type,
					   void			*closure);

typedef CompBool (*ForEachInterfaceProc) (CompObject		*object,
					  InterfaceCallBackProc proc,
					  void		        *closure);

typedef void (*MethodMarshalProc) (CompObject *object,
				   void	      (*method) (void),
				   CompArgs   *args);

typedef CompBool (*MethodCallBackProc) (CompObject	  *object,
					const char	  *name,
					const char	  *in,
					const char	  *out,
					size_t		  offset,
					MethodMarshalProc marshal,
					void		  *closure);

typedef CompBool (*ForEachMethodProc) (CompObject	  *object,
				       const char	  *interface,
				       MethodCallBackProc proc,
				       void		  *closure);

typedef CompBool (*SignalCallBackProc) (CompObject *object,
					const char *name,
					const char *out,
					size_t	   offset,
					void	   *closure);

typedef CompBool (*ForEachSignalProc) (CompObject	  *object,
				       const char	  *interface,
				       SignalCallBackProc proc,
				       void		  *closure);

typedef CompBool (*PropCallBackProc) (CompObject *object,
				      const char *name,
				      int	 type,
				      void	 *closure);

typedef CompBool (*ForEachPropProc) (CompObject	      *object,
				     const char	      *interface,
				     PropCallBackProc proc,
				     void	      *closure);

typedef void (*InterfaceAddedProc) (CompObject *object,
				    const char *interface);

typedef void (*InterfaceRemovedProc) (CompObject *object,
				      const char *interface);

typedef CompBool (*AddChildObjectProc) (CompObject *object,
					CompObject *child,
					const char *name);

typedef void (*RemoveChildObjectProc) (CompObject *object,
				       CompObject *child);

typedef CompBool (*ChildObjectCallBackProc) (CompObject *object,
					     void	*closure);

typedef CompBool (*ForEachChildObjectProc) (CompObject		    *object,
					    ChildObjectCallBackProc proc,
					    void		    *closure);

typedef int (*ConnectProc) (CompObject *object,
			    const char *interface,
			    size_t     offset,
			    CompObject *descendant,
			    const char *descendantInterface,
			    size_t     descendantOffset,
			    const char *details,
			    va_list    args);

typedef void (*DisconnectProc) (CompObject *object,
				const char *interface,
				size_t     offset,
				int	   index);

typedef void (*SignalProc) (CompObject   *object,
			    const char   *path,
			    const char   *interface,
			    const char   *name,
			    const char   *signature,
			    CompAnyValue *value,
			    int	         nValue);

typedef int (*GetVersionProc) (CompObject *object,
			       const char *interface);

typedef CompBool (*GetBoolPropProc) (CompObject *object,
				     const char *interface,
				     const char *name,
				     CompBool   *value,
				     char	**error);

typedef CompBool (*SetBoolPropProc) (CompObject *object,
				     const char *interface,
				     const char *name,
				     CompBool   value,
				     char	**error);

typedef void (*BoolPropChangedProc) (CompObject *object,
				     const char *interface,
				     const char *name,
				     CompBool   value);

typedef CompBool (*GetIntPropProc) (CompObject *object,
				    const char *interface,
				    const char *name,
				    int32_t    *value,
				    char       **error);

typedef CompBool (*SetIntPropProc) (CompObject *object,
				    const char *interface,
				    const char *name,
				    int32_t    value,
				    char       **error);

typedef void (*IntPropChangedProc) (CompObject *object,
				    const char *interface,
				    const char *name,
				    int32_t    value);

typedef CompBool (*GetDoublePropProc) (CompObject *object,
				       const char *interface,
				       const char *name,
				       double	  *value,
				       char	  **error);

typedef CompBool (*SetDoublePropProc) (CompObject *object,
				       const char *interface,
				       const char *name,
				       double	  value,
				       char	  **error);

typedef void (*DoublePropChangedProc) (CompObject *object,
				       const char *interface,
				       const char *name,
				       double     value);

typedef CompBool (*GetStringPropProc) (CompObject *object,
				       const char *interface,
				       const char *name,
				       char	  **value,
				       char	  **error);

typedef CompBool (*SetStringPropProc) (CompObject *object,
				       const char *interface,
				       const char *name,
				       const char *value,
				       char	  **error);

typedef void (*StringPropChangedProc) (CompObject *object,
				       const char *interface,
				       const char *name,
				       const char *value);

struct _CompObjectVTable {

    /* finalize function*/
    FinalizeObjectProc finalize;

    /* abstract property functions */
    GetPropProc getProp;
    SetPropProc setProp;

    /* object tree functions

       used when inserting and removing objects from an object tree
    */
    InsertObjectProc insertObject;
    RemoveObjectProc removeObject;

    /* interface functions

       object interfaces are provided by implementing some of these
    */
    ForEachInterfaceProc forEachInterface;
    ForEachMethodProc    forEachMethod;
    ForEachSignalProc    forEachSignal;
    ForEachPropProc      forEachProp;

    /* child object functions

       child objects are provided by implementing these
     */
    AddChildObjectProc     addChild;
    RemoveChildObjectProc  removeChild;
    ForEachChildObjectProc forEachChildObject;

    ConnectProc    connect;
    DisconnectProc disconnect;

    GetVersionProc getVersion;

    GetBoolPropProc getBool;
    SetBoolPropProc setBool;

    GetIntPropProc getInt;
    SetIntPropProc setInt;

    GetDoublePropProc getDouble;
    SetDoublePropProc setDouble;

    GetStringPropProc getString;
    SetStringPropProc setString;


    /* signals */

    SignalProc signal;

    InsertedProc inserted;
    RemovedProc  removed;

    InterfaceAddedProc   interfaceAdded;
    InterfaceRemovedProc interfaceRemoved;

    BoolPropChangedProc   boolChanged;
    IntPropChangedProc    intChanged;
    DoublePropChangedProc doubleChanged;
    StringPropChangedProc stringChanged;
};

typedef struct _CompObjectVTableVec {
    const CompObjectVTable *vTable;
} CompObjectVTableVec;

CompBool
compObjectInit (const CompObjectFactory	     *factory,
		CompObject		     *object,
		const CompObjectInstantiator *instantiator);

CompBool
compObjectInitByType (const CompObjectFactory *factory,
		      CompObject	      *object,
		      const CompObjectType    *type);

CompBool
compObjectInitByTypeName (const CompObjectFactory *factory,
			  CompObject		  *object,
			  const char		  *name);

CompObjectInstantiatorNode *
compObjectInstantiatorNode (const CompObjectFactory *factory,
			    const char		    *name);

void
compVTableInit (CompObjectVTable       *vTable,
		const CompObjectVTable *noopVTable,
		int		       size);

CompBool
compFactoryInstallType (CompObjectFactory    *factory,
			const CompObjectType *type);

const CompObjectType *
compFactoryUninstallType (CompObjectFactory *factory);

CompBool
compFactoryInstallInterface (CompObjectFactory	       *factory,
			     const CompObjectType      *type,
			     const CompObjectInterface *interface);

const CompObjectInterface *
compFactoryUninstallInterface (CompObjectFactory    *factory,
			       const CompObjectType *type);

CompBool
compInsertTopInterface (CompObject	     *root,
			CompObjectFactory    *factory,
			const CompObjectType *type);

void
compRemoveTopInterface (CompObject	     *root,
			CompObjectFactory    *factory,
			const CompObjectType *type);

typedef struct _CompSerializedMethodCallHeader {
    char	 *path;
    char	 *interface;
    char	 *name;
    char	 *signature;
    CompAnyValue *value;
    int		 nValue;
} CompSerializedMethodCallHeader;

typedef struct _CompSignalHandler {
    struct _CompSignalHandler	   *next;
    int				   id;
    CompObject			   *object;
    const CompObjectVTable	   *vTable;
    size_t			   offset;
    MethodMarshalProc		   marshal;
    CompSerializedMethodCallHeader *header;
} CompSignalHandler;

struct _CompObject {
    const CompObjectVTable *vTable;
    const char		   *name;
    CompObject		   *parent;
    CompSignalHandler      **signalVec;
    CompPrivate		   *privates;

    CompObjectTypeID id;
};

#define GET_OBJECT(object) ((CompObject *) (object))
#define OBJECT(object) CompObject *o = GET_OBJECT (object)

#define COMPIZ_OBJECT_VERSION   20080221
#define COMPIZ_OBJECT_TYPE_NAME COMPIZ_NAME_PREFIX "object"

/* useful structures for object and interface implementations */

typedef struct _CompInterfaceData {
    const CompObjectVTable *vTable;
    int		           signalVecOffset;
} CompInterfaceData;

typedef struct _CompObjectData {
    CompInterfaceData base;
    CompPrivate       *privates;
} CompObjectData;

#define INVOKE_HANDLER_PROC(object, offset, prototype, ...)		\
    (*(*((prototype *)							\
	 (((char *) (object)->vTable) +					\
	  (offset))))) ((void *) object, ##__VA_ARGS__)

#define MATCHING_DETAILS(handler, ...)					\
    ((!(handler)->header->signature) ||					\
     compCheckEqualityOfValuesAndArgs ((handler)->header->signature,	\
				       (handler)->header->value,	\
				       ##__VA_ARGS__))

#define EMIT_SIGNAL(source, type, vOffset, ...)				\
    do									\
    {									\
	CompSignalHandler   *handler = (source)->signalVec[(vOffset)];	\
	CompObjectVTableVec save;					\
									\
	while (handler)							\
	{								\
	    if (MATCHING_DETAILS (handler, ##__VA_ARGS__))		\
	    {								\
		if (handler->vTable)					\
		{							\
		    save.vTable = handler->object->vTable;		\
									\
		    UNWRAP (handler, handler->object, vTable);		\
		    INVOKE_HANDLER_PROC (handler->object,		\
					 handler->offset,		\
					 type, ##__VA_ARGS__);		\
		    WRAP (handler, handler->object, vTable,		\
			  save.vTable);					\
		}							\
		else							\
		{							\
		    INVOKE_HANDLER_PROC (handler->object,		\
					 handler->offset,		\
					 type, ##__VA_ARGS__);		\
		}							\
	    }								\
									\
	    handler = handler->next;					\
	}								\
    } while (0)

#define FOR_BASE(object, ...)						\
    do {								\
	const CompObjectVTable *__selfVTable = (object)->vTable;	\
									\
	(*(object)->vTable->getProp) (object,				\
				      COMP_PROP_BASE_VTABLE,		\
				      (void *) &(object)->vTable);	\
									\
	__VA_ARGS__;							\
									\
	(object)->vTable = __selfVTable;				\
    } while (0)


const CompObjectType *
getObjectType (void);

CompBool
compForInterface (CompObject		*object,
		  const char		*interface,
		  InterfaceCallBackProc proc,
		  void			*closure);

CompObject *
compLookupObject (CompObject *root,
		  const char *path);

CompBool
compObjectCheckVersion (CompObject *object,
			const char *interface,
			int	   version);

CompBool
compObjectInterfaceIsPartOfType (CompObject *object,
				 const char *interface);

CompBool
compInvokeMethodWithArgs (CompObject *object,
			  const char *interface,
			  const char *name,
			  const char *in,
			  const char *out,
			  CompArgs   *args);

CompBool
compInvokeMethod (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *in,
		  const char *out,
		  ...);

int
compSerializeMethodCall (CompObject *observer,
			 CompObject *subject,
			 const char *interface,
			 const char *name,
			 const char *signature,
			 va_list    args,
			 void	    *data,
			 int	    size);

CompSignalHandler **
compGetSignalVecRange (CompObject *object,
		       int	  size,
		       int	  *offset);

void
compFreeSignalVecRange (CompObject *object,
			int	   size,
			int	   offset);

CompBool
compCheckEqualityOfValuesAndArgs (const char   *signature,
				  CompAnyValue *value,
				  ...);

int
compConnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     CompObject *descendant,
	     const char *descendantInterface,
	     size_t     descendantOffset,
	     const char *details,
	     ...);

void
compDisconnect (CompObject *object,
		const char *interface,
		size_t     offset,
		int	   index);

const char *
compTranslateObjectPath (CompObject *ancestor,
			 CompObject *descendant,
			 const char *path);

COMPIZ_END_DECLS

#endif
