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
