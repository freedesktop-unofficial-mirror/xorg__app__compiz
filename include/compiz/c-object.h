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

#ifndef _COMPIZ_C_OBJECT_H
#define _COMPIZ_C_OBJECT_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CMethod {
    const char	      *name;
    const char	      *in;
    const char	      *out;
    size_t	      offset;
    MethodMarshalProc marshal;
} CMethod;

#define C_METHOD(name, in, out, vtable, marshal)			      \
    { # name, in, out, offsetof (vtable, name), (MethodMarshalProc) marshal }

typedef struct _CSignal {
    const char *name;
    const char *out;
    size_t     offset;
} CSignal;

#define C_SIGNAL(name, out, vtable)	     \
    { # name, out, offsetof (vtable, name) }

typedef struct _CProp {
    const char *name;
    size_t     offset;
} CProp;

#define C_PROP(name, object, ...)			 \
    { { # name, offsetof (object, name) }, __VA_ARGS__ }

typedef struct _CBoolProp {
    CProp base;

    CompBool defaultValue;

    SetBoolPropProc     set;
    BoolPropChangedProc changed;
} CBoolProp;

typedef struct _CIntProp {
    CProp base;

    CompBool restriction;
    int32_t  min;
    int32_t  max;

    int32_t defaultValue;

    SetIntPropProc     set;
    IntPropChangedProc changed;
} CIntProp;

#define C_INT_PROP(name, object, min, max, ...)				 \
    { { # name, offsetof (object, name) }, TRUE, min, max, __VA_ARGS__ }

typedef struct _CDoubleProp {
    CProp base;

    CompBool restriction;
    double   min;
    double   max;
    double   precision;

    double defaultValue;

    SetDoublePropProc     set;
    DoublePropChangedProc changed;
} CDoubleProp;

#define C_DOUBLE_PROP(name, object, min, max, precision, ...) \
    { { # name, offsetof (object, name) },		      \
	    TRUE, min, max, precision,  __VA_ARGS__ }

typedef struct _CStringProp {
    CProp base;

    const char *defaultValue;
    char       *data;

    SetStringPropProc     set;
    StringPropChangedProc changed;
} CStringProp;

typedef struct _CChildObject {
    const char		 *name;
    size_t		 offset;
    const char		 *type;
    const CompObjectType *objectType;
} CChildObject;

#define C_CHILD(name, object, type)	      \
    { # name, offsetof (object, name), type }

typedef struct _CInterface {
    const char	  *name;
    size_t	  offset;
    const CMethod *method;
    int		  nMethod;
    const CSignal *signal;
    int		  nSignal;
    CBoolProp     *boolProp;
    int		  nBoolProp;
    CIntProp      *intProp;
    int		  nIntProp;
    CDoubleProp   *doubleProp;
    int		  nDoubleProp;
    CStringProp   *stringProp;
    int		  nStringProp;
    CChildObject  *child;
    int		  nChild;
} CInterface;

#define C_OFFSET__(vtable, name) 0
#define C_OFFSET_X(vtable, name) offsetof (vtable, name)

#define C_MEMBER__(name, type, member) NULL, 0
#define C_MEMBER_X(name, type, member)				\
    name ## type ## member, N_ELEMENTS (name ## type ## member)

#define C_INTERFACE(name, type, vtable, offset, method, signal,		\
		    bool, int, double, string, child)			\
    { # name,								\
	    C_OFFSET_ ## offset (vtable, name),				\
	    C_MEMBER_ ## method (name, type, Method),			\
	    C_MEMBER_ ## signal (name, type, Signal),			\
	    C_MEMBER_ ## bool   (name, type, BoolProp),			\
	    C_MEMBER_ ## int    (name, type, IntProp),			\
	    C_MEMBER_ ## double (name, type, DoubleProp),		\
	    C_MEMBER_ ## string (name, type, StringProp),		\
	    C_MEMBER_ ## child  (name, type, ChildObject) }

typedef struct _CContext {
    const CInterface     *interface;
    int			 nInterface;
    const CompObjectType *type;
    char		 *data;
    CompObjectVTableVec	 *vtStore;
    int			 version;
} CContext;

typedef void (*GetCContextProc) (CompObject *object,
				 CContext   *ctx);

typedef struct _CObjectPrivate {
    const char	    *name;
    int		    *pIndex;
    int		    size;
    void	    *vTable;
    GetCContextProc proc;
    CInterface      *interface;
    int		    nInterface;
    InitObjectProc  init;
    FiniObjectProc  fini;
} CObjectPrivate;

#define C_INDEX__(name, type, struct) 0, 0
#define C_INDEX_X(name, type, struct)		    \
    & name ## type ## PrivateIndex, sizeof (struct)

#define C_VTABLE__(name, type) 0, 0, 0, 0
#define C_VTABLE_X(name, type)		       \
    & name ## type ## ObjectVTable,	       \
	name ## type ## GetCContext,	       \
	name ## type ## Interface,	       \
	N_ELEMENTS (name ## type ## Interface)

#define C_OBJECT_PRIVATE(str, name, type, struct, index, vtable) \
    {	str,							 \
	    C_INDEX_ ## index (name, type, struct),		 \
	    C_VTABLE_ ## vtable (name, type),			 \
	    (InitObjectProc) name ## Init ## type,		 \
	    (FiniObjectProc) name ## Fini ## type }


void
cInitObjectVTable (CompObjectVTable *vTable,
		   GetCContextProc  getCContext,
		   InitVTableProc   initVTable);

CompBool
cForBaseObject (CompObject	       *object,
		BaseObjectCallBackProc proc,
		void		       *closure);

CompBool
cForEachInterface (CompObject	         *object,
		   InterfaceCallBackProc proc,
		   void		         *closure);

CompBool
cForEachMethod (CompObject	   *object,
		const char	   *interface,
		MethodCallBackProc proc,
		void	           *closure);

CompBool
cForEachSignal (CompObject	   *object,
		const char	   *interface,
		SignalCallBackProc proc,
		void		   *closure);

CompBool
cForEachProp (CompObject       *object,
	      const char       *interface,
	      PropCallBackProc proc,
	      void	       *closure);

void
cInterfacesAdded (CompObject	   *object,
		  const CInterface *interface,
		  int		   nInterface);

void
cInterfacesRemoved (CompObject	     *object,
		    const CInterface *interface,
		    int		     nInterface);

CompBool
cForEachType (CompObject       *object,
	      TypeCallBackProc proc,
	      void	       *closure);

CompBool
cForEachChildObject (CompObject		     *object,
		     ChildObjectCallBackProc proc,
		     void		     *closure);

CompBool
cGetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   *value,
	      char	 **error);

CompBool
cSetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   value,
	      char	 **error);

void
cBoolPropChanged (CompObject *object,
		  const char *interface,
		  const char *name,
		  CompBool   value);

CompBool
cGetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    *value,
	     char	**error);

CompBool
cSetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    value,
	     char	**error);

void
cIntPropChanged (CompObject *object,
		 const char *interface,
		 const char *name,
		 int32_t    value);

CompBool
cGetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double     *value,
		char	   **error);

CompBool
cSetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double     value,
		char	   **error);

void
cDoublePropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    double     value);

CompBool
cGetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		char       **value,
		char	   **error);

CompBool
cSetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		const char *value,
		char	   **error);

void
cStringPropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    const char *value);

int
cGetVersion (CompObject *object,
	     const char *interface);

CompBool
cGetMetadata (CompObject *object,
	      const char *interface,
	      char	 **data,
	      char	 **error);

void
cDefaultValuesFromFile (CInterface *interface,
			int	   nInterface,
			const char *name);

CompBool
cInterfaceInit (CInterface *interface,
		int	   nInterface);

void
cInterfaceFini (CInterface *interface,
		int	   nInterface);

CompBool
cObjectPropertiesInit (CompObject	*object,
		       char	        *data,
		       const CInterface *interface,
		       int		nInterface);

void
cObjectPropertiesFini (CompObject	*object,
		       char	        *data,
		       const CInterface *interface,
		       int		nInterface);

CompBool
cObjectChildrenInit (CompObject	      *object,
		     char	      *data,
		     const CInterface *interface,
		     int	      nInterface);

void
cObjectChildrenFini (CompObject	      *object,
		     char	      *data,
		     const CInterface *interface,
		     int	      nInterface);

CompBool
cObjectInterfaceInit (CompObject	     *object,
		      const CompObjectVTable *vTable);

void
cObjectInterfaceFini (CompObject *object);

CompBool
cObjectInit (CompObject	            *object,
	     const CompObjectType   *baseType,
	     const CompObjectVTable *vTable);

void
cObjectFini (CompObject	          *object,
	     const CompObjectType *baseType);

CompBool
cObjectInitPrivates (CObjectPrivate *privates,
		     int	    nPrivates);

void
cObjectFiniPrivates (CObjectPrivate *privates,
		     int	    nPrivates);

COMPIZ_END_DECLS

#endif
