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
    size_t	    offset;
    CompObjectFuncs *funcs;
    int		    nFuncs;
} CompObjectPrivates;

typedef void (*InitVTableProc) (void *vTable);

struct _CompObjectType {
    const char	       *name;
    CompObjectFuncs    funcs;
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
   OBJECT	111 (ASCII 'o')	Object instance
*/

#define COMP_TYPE_INVALID ((int) '\0')

#define COMP_TYPE_BOOLEAN ((int) 'b')
#define COMP_TYPE_INT32   ((int) 'i')
#define COMP_TYPE_DOUBLE  ((int) 'd')
#define COMP_TYPE_STRING  ((int) 's')
#define COMP_TYPE_OBJECT  ((int) 'o')

typedef union {
    CompBool   b;
    int	       i;
    double     d;
    char       *s;
    CompObject *o;
} CompAnyValue;

typedef struct _CompArgs CompArgs;

typedef void (*ArgsLoadProc) (CompArgs   *args,
			      const char *type,
			      void       *value);

typedef void (*ArgsStoreProc) (CompArgs	  *args,
			       const char *type,
			       void	  *value);

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

typedef CompBool (*InterfaceCallBackProc) (CompObject		*object,
					   const char		*name,
					   void			*key,
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
				       void		  *interface,
				       MethodCallBackProc proc,
				       void		  *closure);

typedef CompBool (*SignalCallBackProc) (CompObject *object,
					const char *name,
					const char *out,
					size_t	   offset,
					void	   *closure);

typedef CompBool (*ForEachSignalProc) (CompObject	  *object,
				       void		  *interface,
				       SignalCallBackProc proc,
				       void		  *closure);

typedef CompBool (*PropCallBackProc) (CompObject *object,
				      const char *name,
				      int	 type,
				      void	 *closure);

typedef CompBool (*ForEachPropProc) (CompObject	      *object,
				     void	      *interface,
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
				      CompObject *child);

typedef void (*ChildObjectRemovedProc) (CompObject *object,
					CompObject *child);

typedef void (*SignalHandlerProc) (CompObject *object,
				   void	      *data,
				   ...);

typedef int (*ConnectProc) (CompObject	      *object,
			    const char	      *interface,
			    size_t	      offset,
			    SignalHandlerProc proc,
			    void	      *data);

typedef void (*DisconnectProc) (CompObject *object,
				const char *interface,
				size_t     offset,
				int	   index);

/* the signal signal should be emitted for each normal signal
   with a name, signature and named interface. signal signals
   propogate to a root objects. source should be set to the
   object that is emitting the original signal and args should
   be the list of arguments. any signal handler connecting to
   this signal must use the va_copy macro before using args. */
typedef void (*SignalSignalProc) (CompObject *object,
				  CompObject *source,
				  const char *interface,
				  const char *name,
				  const char *signature,
				  va_list    args);

typedef struct _CompSignalVTable {
    ConnectProc      connect;
    DisconnectProc   disconnect;
    SignalSignalProc signal;
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

    /* interface functions

       object interfaces are provided by implementing some of these
    */
    ForEachInterfaceProc forEachInterface;
    ForEachMethodProc    forEachMethod;
    ForEachSignalProc    forEachSignal;
    ForEachPropProc      forEachProp;
    InterfaceAddedProc   interfaceAdded;
    InterfaceRemovedProc interfaceRemoved;

    /* type function

       must be implemented by every subtype
     */
    ForEachTypeProc forEachType;

    /* child object functions

       child objects are provided by implementing forEachChildObject
     */
    ForEachChildObjectProc forEachChildObject;
    ChildObjectAddedProc   childObjectAdded;
    ChildObjectRemovedProc childObjectRemoved;

    CompSignalVTable     signal;
    CompVersionVTable    version;
    CompPropertiesVTable properties;
    CompMetadataVTable   metadata;
} CompObjectVTable;

typedef struct _CompObjectVTableVec {
    CompObjectVTable *vTable;
} CompObjectVTableVec;

CompBool
compObjectInit (CompObject           *object,
		const CompObjectType *type);

void
compObjectFini (CompObject           *object,
		const CompObjectType *type);

typedef struct _CompSignalHandler {
    struct _CompSignalHandler *next;
    int			      id;
    SignalHandlerProc	      proc;
    void		      *data;
} CompSignalHandler;

#define COMP_OBJECT_SIGNAL_INTERFACE_ADDED   0
#define COMP_OBJECT_SIGNAL_INTERFACE_REMOVED 1
#define COMP_OBJECT_SIGNAL_CHILD_ADDED       2
#define COMP_OBJECT_SIGNAL_CHILD_REMOVED     3
#define COMP_OBJECT_SIGNAL_SIGNAL            4
#define COMP_OBJECT_SIGNAL_BOOL_CHANGED      5
#define COMP_OBJECT_SIGNAL_INT_CHANGED       6
#define COMP_OBJECT_SIGNAL_DOUBLE_CHANGED    7
#define COMP_OBJECT_SIGNAL_STRING_CHANGED    8
#define COMP_OBJECT_SIGNAL_NUM               9

struct _CompObject {
    CompObjectVTable *vTable;
    CompObject       *parent;
    const char	     *name;

    CompSignalHandler *signal[COMP_OBJECT_SIGNAL_NUM];

    CompPrivate	*privates;

    CompObjectTypeID id;
};

void
emitSignalSignal (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *signature,
		  ...);

#define EMIT_SIGNAL(object, handlers, ...)			   \
    do {							   \
	CompSignalHandler *handler = handlers;			   \
								   \
	while (handler)						   \
	{							   \
	    (*handler->proc) (object, handler->data, __VA_ARGS__); \
	    handler = handler->next;				   \
	}							   \
    } while (0)

#define EMIT_EXT_SIGNAL(object, handlers, interface, name, signature, ...) \
    EMIT_SIGNAL (object, handlers, __VA_ARGS__);			   \
    emitSignalSignal (object, interface, name, signature, __VA_ARGS__)

CompObjectType *
getObjectType (void);

CompBool
allocateObjectPrivates (CompObject	   *object,
			CompObjectPrivates *objectPrivates);

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
		  char	     **path);

typedef struct _CompObjectPrivate {
    const char	   *name;
    int		   *pIndex;
    int		   size;
    void	   *vTable;
    InitObjectProc init;
    FiniObjectProc fini;
} CompObjectPrivate;

CompBool
compObjectInitPrivates (CompObjectPrivate *privates,
			int		  nPrivates);

void
compObjectFiniPrivates (CompObjectPrivate *privates,
			int		  nPrivates);

CompBool
compObjectCheckVersion (CompObject *object,
			const char *interface,
			int	   version);

typedef char *(*GetDataProc) (CompObject *object);

typedef struct _CommonMethod {
    const char	      *name;
    const char	      *in;
    const char	      *out;
    size_t	      offset;
    MethodMarshalProc marshal;
} CommonMethod;

#define C_METHOD(name, in, out, vtable, marshal)			      \
    { # name, in, out, offsetof (vtable, name), (MethodMarshalProc) marshal }

typedef struct _CommonSignal {
    const char	      *name;
    const char	      *out;
    size_t	      offset;
} CommonSignal;

#define C_SIGNAL(name, out, vtable)	     \
    { # name, out, offsetof (vtable, name) }

typedef struct _CommonProp {
    const char *name;
    size_t     offset;
} CommonProp;

#define C_PROP(name, object, ...)			 \
    { { # name, offsetof (object, name) }, __VA_ARGS__ }

typedef struct _CommonBoolProp {
    CommonProp base;

    CompBool defaultValue;

    SetBoolPropProc     set;
    BoolPropChangedProc changed;
} CommonBoolProp;

typedef struct _CommonIntProp {
    CommonProp base;

    CompBool restriction;
    int32_t  min;
    int32_t  max;

    int32_t defaultValue;

    SetIntPropProc     set;
    IntPropChangedProc changed;
} CommonIntProp;

#define C_INT_PROP(name, object, min, max, ...)				 \
    { { # name, offsetof (object, name) }, TRUE, min, max, __VA_ARGS__ }

typedef struct _CommonDoubleProp {
    CommonProp base;

    CompBool restriction;
    double   min;
    double   max;
    double   precision;

    double defaultValue;

    SetDoublePropProc     set;
    DoublePropChangedProc changed;
} CommonDoubleProp;

#define C_DOUBLE_PROP(name, object, min, max, precision, ...) \
    { { # name, offsetof (object, name) },		      \
	    TRUE, min, max, precision,  __VA_ARGS__ }

typedef struct _CommonStringProp {
    CommonProp base;

    const char *defaultValue;
    char       *data;

    SetStringPropProc     set;
    StringPropChangedProc changed;
} CommonStringProp;

typedef struct _CommonChildObject {
    const char		 *name;
    size_t		 offset;
    const char		 *type;
    const CompObjectType *objectType;
} CommonChildObject;

#define C_CHILD(name, object, type)	      \
    { # name, offsetof (object, name), type }

typedef struct _CommonInterface {
    const char	       *name;
    int		       version;
    size_t	       offset;
    GetDataProc	       data;
    const CommonMethod *method;
    int		       nMethod;
    const CommonSignal *signal;
    int		       nSignal;
    CommonBoolProp     *boolProp;
    int		       nBoolProp;
    CommonIntProp      *intProp;
    int		       nIntProp;
    CommonDoubleProp   *doubleProp;
    int		       nDoubleProp;
    CommonStringProp   *stringProp;
    int		       nStringProp;
    CommonChildObject  *child;
    int		       nChild;
} CommonInterface;

#define C_OFFSET__(vtable, name) 0
#define C_OFFSET_X(vtable, name) offsetof (vtable, name)

#define C_DATA__(type) NULL
#define C_DATA_X(type) get ## type ## Data

#define C_MEMBER__(name, type, member) NULL, 0
#define C_MEMBER_X(name, type, member)				\
    name ## type ## member, N_ELEMENTS (name ## type ## member)

#define C_INTERFACE(name, type, vtable, offset, data, method, signal,	\
		    bool, int, double, string, child)			\
    { # name, INTERFACE_VERSION_ ## name ## type,			\
	    C_OFFSET_ ## offset (vtable, name),				\
	    C_DATA_   ## data   (type),					\
	    C_MEMBER_ ## method (name, type, Method),			\
	    C_MEMBER_ ## signal (name, type, Signal),			\
	    C_MEMBER_ ## bool   (name, type, BoolProp),			\
	    C_MEMBER_ ## int    (name, type, IntProp),			\
	    C_MEMBER_ ## double (name, type, DoubleProp),		\
	    C_MEMBER_ ## string (name, type, StringProp),		\
	    C_MEMBER_ ## child  (name, type, ChildObject) }

CompBool
handleForEachInterface (CompObject	      *object,
			const CommonInterface *interface,
			int		      nInterface,
			const CompObjectType  *type,
			InterfaceCallBackProc proc,
			void		      *closure);

CompBool
handleForEachChildObject (CompObject		  *object,
			  const CommonInterface   *interface,
			  int		          nInterface,
			  ChildObjectCallBackProc proc,
			  void			  *closure);

CompBool
commonForEachMethod (CompObject		*object,
		     void		*key,
		     MethodCallBackProc proc,
		     void	        *closure);

CompBool
commonForEachSignal (CompObject		*object,
		     void	        *interface,
		     SignalCallBackProc proc,
		     void		*closure);

CompBool
commonForEachProp (CompObject	    *object,
		   void		    *interface,
		   PropCallBackProc proc,
		   void		    *closure);

void
commonInterfacesAdded (CompObject	     *object,
		       const CommonInterface *interface,
		       int		     nInterface);
void
commonInterfacesRemoved (CompObject	       *object,
			 const CommonInterface *interface,
			 int		       nInterface);

CompBool
commonGetBoolProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   CompBool   *value,
		   char	      **error);

CompBool
commonSetBoolProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   CompBool   value,
		   char	      **error);

void
commonBoolPropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       CompBool   value);

CompBool
commonGetIntProp (CompObject *object,
		  const char *interface,
		  const char *name,
		  int32_t    *value,
		  char	     **error);

CompBool
commonSetIntProp (CompObject *object,
		  const char *interface,
		  const char *name,
		  int32_t    value,
		  char	     **error);

void
commonIntPropChanged (CompObject *object,
		      const char *interface,
		      const char *name,
		      int32_t    value);

CompBool
commonGetDoubleProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     double     *value,
		     char	**error);

CompBool
commonSetDoubleProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     double     value,
		     char	**error);

void
commonDoublePropChanged (CompObject *object,
			 const char *interface,
			 const char *name,
			 double     value);

CompBool
commonGetStringProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     char       **value,
		     char	**error);

CompBool
commonSetStringProp (CompObject *object,
		     const char *interface,
		     const char *name,
		     const char *value,
		     char	**error);

void
commonStringPropChanged (CompObject *object,
			 const char *interface,
			 const char *name,
			 const char *value);

int
commonGetVersion (CompObject *object,
		  const char *interface);

CompBool
commonGetMetadata (CompObject *object,
		   const char *interface,
		   char	      **data,
		   char	      **error);

typedef struct _CompBasicArgs {
    CompArgs base;

    CompAnyValue *in;
    CompAnyValue *out;

    int inPos;
    int outPos;

    char *error;
} CompBasicArgs;

void
compInitBasicArgs (CompBasicArgs *args,
		   CompAnyValue  *in,
		   CompAnyValue  *out);

void
compFiniBasicArgs (CompBasicArgs *args);

CompBool
compInvokeMethod (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *in,
		  const char *out,
		  CompArgs   *args);

void
commonDefaultValuesFromFile (CommonInterface *interface,
			     int	     nInterface,
			     const char      *name);

CompBool
commonInterfaceInit (CommonInterface *interface,
		     int	     nInterface);

void
commonInterfaceFini (CommonInterface *interface,
		     int	     nInterface);

CompBool
commonObjectPropertiesInit (CompObject		  *object,
			    const CommonInterface *interface,
			    int			  nInterface);

void
commonObjectPropertiesFini (CompObject		  *object,
			    const CommonInterface *interface,
			    int			  nInterface);

CompBool
commonObjectChildrenInit (CompObject	        *object,
			  const CommonInterface *interface,
			  int		        nInterface);

void
commonObjectChildrenFini (CompObject	        *object,
			  const CommonInterface *interface,
			  int		        nInterface);

CompBool
commonObjectInterfaceInit (CompObject	         *object,
			   const CommonInterface *interface,
			   int		         nInterface);

void
commonObjectInterfaceFini (CompObject	         *object,
			   const CommonInterface *interface,
			   int		         nInterface);

CompBool
commonObjectInit (CompObject	        *object,
		  const CompObjectType  *baseType,
		  const CommonInterface *interface,
		  int		        nInterface);

void
commonObjectFini (CompObject	        *object,
		  const CompObjectType  *baseType,
		  const CommonInterface *interface,
		  int		        nInterface);

COMPIZ_END_DECLS

#endif
