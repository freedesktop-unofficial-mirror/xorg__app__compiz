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

#define COMPIZ_OBJECT_VERSION 20071116

COMPIZ_BEGIN_DECLS

/*
  object-type, object, interface and member names

  - must only contain the ASCII characters "[A-Z][a-z][0-9]_"
  - may not be the empty string
  - may not be exceed the maximum length of 127 characters

  object-type, interface and member names

  - must also not begin with a digit
*/

typedef struct _CompObject     CompObject;
typedef struct _CompObjectType CompObjectType;

typedef CompBool (*InitObjectProc) (CompObject *object);
typedef void     (*FiniObjectProc) (CompObject *object);

typedef struct _CompObjectFuncs {
    InitObjectProc init;
    FiniObjectProc fini;
} CompObjectFuncs;

typedef struct _CompObjectPrivates {
    int		    len;
    int		    *sizes;
    int		    totalSize;
    CompObjectFuncs *funcs;
    int		    nFuncs;
} CompObjectPrivates;

typedef void (*InitVTableProc) (void *vTable);

struct _CompObjectType {
    const char	       *name;
    CompObjectFuncs    funcs;
    size_t	       privatesOffset;
    CompObjectPrivates *privates;
    InitVTableProc     initVTable;
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

typedef CompBool (*BaseObjectCallBackProc) (CompObject *object,
					    void       *closure);
typedef CompBool (*ForBaseObjectProc) (CompObject	      *object,
				       BaseObjectCallBackProc proc,
				       void		      *closure);

typedef void (*UnusedProc) (CompObject *object);

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

typedef CompBool (*TypeCallBackProc) (CompObject	   *object,
				      const CompObjectType *type,
				      void		   *closure);

typedef CompBool (*ForEachTypeProc) (CompObject	      *object,
				     TypeCallBackProc proc,
				     void	      *closure);

typedef CompBool (*ChildObjectCallBackProc) (CompObject *object,
					     void	*closure);

typedef CompBool (*ForEachChildObjectProc) (CompObject		    *object,
					    ChildObjectCallBackProc proc,
					    void		    *closure);

typedef void (*ChildObjectAddedProc) (CompObject *object,
				      const char *name);

typedef void (*ChildObjectRemovedProc) (CompObject *object,
					const char *name);

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

typedef struct _CompSignalVTable {
    ConnectProc    connect;
    DisconnectProc disconnect;
    SignalProc     signal;
} CompSignalVTable;

typedef int (*GetVersionProc) (CompObject *object,
			       const char *interface);

typedef struct _CompVersionVTable {
    GetVersionProc get;
} CompVersionVTable;

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

typedef struct _CompPropertiesVTable {
    GetBoolPropProc     getBool;
    SetBoolPropProc     setBool;
    BoolPropChangedProc boolChanged;

    GetIntPropProc     getInt;
    SetIntPropProc     setInt;
    IntPropChangedProc intChanged;

    GetDoublePropProc     getDouble;
    SetDoublePropProc     setDouble;
    DoublePropChangedProc doubleChanged;

    GetStringPropProc     getString;
    SetStringPropProc     setString;
    StringPropChangedProc stringChanged;
} CompPropertiesVTable;

typedef CompBool (*GetMetadataProc) (CompObject *object,
				     const char *interface,
				     char	**data,
				     char	**error);

typedef struct _CompMetadataVTable {
    GetMetadataProc get;
} CompMetadataVTable;

typedef struct _CompObjectVTable {

    /* base object function

       must be implemented by each subtype
    */
    ForBaseObjectProc forBaseObject;

    /* empty vtable entry that each implementer can
       use for its own purpose */
    UnusedProc unused;

    /* object tree functions

       used when inserting and removing objects from an object tree
    */
    InsertObjectProc insertObject;
    RemoveObjectProc removeObject;
    InsertedProc     inserted;
    RemovedProc      removed;

    /* interface functions

       object interfaces are provided by implementing some of these
    */
    ForEachInterfaceProc forEachInterface;
    ForEachMethodProc    forEachMethod;
    ForEachSignalProc    forEachSignal;
    ForEachPropProc      forEachProp;
    InterfaceAddedProc   interfaceAdded;
    InterfaceRemovedProc interfaceRemoved;

    /* child object functions

       child objects are provided by implementing forEachChildObject
     */
    ForEachChildObjectProc forEachChildObject;

    CompSignalVTable     signal;
    CompVersionVTable    version;
    CompPropertiesVTable properties;
    CompMetadataVTable   metadata;
} CompObjectVTable;

typedef struct _CompObjectVTableVec {
    const CompObjectVTable *vTable;
} CompObjectVTableVec;

CompBool
compObjectInit (CompObject           *object,
		const CompObjectType *type);

void
compObjectFini (CompObject           *object,
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

#define INVOKE_HANDLER_PROC(object, offset, prototype, ...)		\
    (*(*((prototype *)							\
	 (((char *) (object)->vTable) +					\
	  (offset))))) (object, ##__VA_ARGS__)

#define MATCHING_DETAILS(handler, ...)					\
    ((!(handler)->header->signature) ||					\
     compCheckEqualityOfValuesAndArgs ((handler)->header->signature,	\
				       (handler)->header->value,	\
				       ##__VA_ARGS__))

#define EMIT_SIGNAL(object, type, vOffset, ...)				\
    do									\
    {									\
	CompSignalHandler   *handler = object->signalVec[(vOffset)];	\
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


CompObjectType *
getObjectType (void);

int
compObjectAllocatePrivateIndex (CompObjectType *type,
				int	       size);

void
compObjectFreePrivateIndex (CompObjectType *type,
			    int	           index);

CompObjectType *
compObjectFindType (const char *name);

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
