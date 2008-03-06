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

#include <compiz/branch.h>

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

typedef void (*CPropChangedProc) (CompObject *object);

typedef struct _CProp {
    const char *name;
    size_t     offset;
} CProp;

#define C_PROP(name, object, ...)			 \
    { { # name, offsetof (object, name) }, __VA_ARGS__ }

typedef CompBool (*CSetBoolPropProc) (CompObject *object,
				      CompBool   value,
				      char	 **error);

typedef struct _CBoolProp {
    CProp base;

    CompBool defaultValue;

    CSetBoolPropProc set;
    CPropChangedProc changed;
} CBoolProp;

typedef CompBool (*CSetIntPropProc) (CompObject *object,
				     int32_t    value,
				     char	**error);

typedef struct _CIntProp {
    CProp base;

    CompBool restriction;
    int32_t  min;
    int32_t  max;

    int32_t defaultValue;

    CSetIntPropProc  set;
    CPropChangedProc changed;
} CIntProp;

#define C_INT_PROP(name, object, min, max, ...)				 \
    { { # name, offsetof (object, name) }, TRUE, min, max, min, __VA_ARGS__ }

typedef CompBool (*CSetDoublePropProc) (CompObject *object,
					double     value,
					char	   **error);

typedef struct _CDoubleProp {
    CProp base;

    CompBool restriction;
    double   min;
    double   max;
    double   precision;

    double defaultValue;

    CSetDoublePropProc set;
    CPropChangedProc   changed;
} CDoubleProp;

#define C_DOUBLE_PROP(name, object, min, max, precision, ...) \
    { { # name, offsetof (object, name) },		      \
	    TRUE, min, max, precision, min,  __VA_ARGS__ }

typedef CompBool (*CSetStringPropProc) (CompObject *object,
					const char *value,
					char	   **error);

typedef struct _CStringProp {
    CProp base;

    const char *defaultValue;

    CSetStringPropProc set;
    CPropChangedProc   changed;
} CStringProp;

typedef struct _CChildObject {
    const char *name;
    size_t     offset;
    const char *type;
} CChildObject;

#define C_CHILD(name, object, type, ...)       \
    { # name, offsetof (object, name), type  }

typedef CompBool (*CInitObjectProc) (CompObject *object);
typedef void     (*CFiniObjectProc) (CompObject	*object);

typedef void (*CInsertObjectProc) (CompObject *object,
				   CompObject *parent);
typedef void (*CRemoveObjectProc) (CompObject *object);

typedef struct _CObjectInterface {
    CompObjectInterface i;

    const CMethod      *method;
    int		       nMethod;
    const CSignal      *signal;
    int		       nSignal;
    const CBoolProp    *boolProp;
    int		       nBoolProp;
    const CIntProp     *intProp;
    int		       nIntProp;
    const CDoubleProp  *doubleProp;
    int		       nDoubleProp;
    const CStringProp  *stringProp;
    int		       nStringProp;
    const CChildObject *child;
    int		       nChild;

    CInitObjectProc init;
    CFiniObjectProc fini;

    CInsertObjectProc insert;
    CRemoveObjectProc remove;

    int	*index;
    int	size;
} CObjectInterface;

#define C_OFFSET__(vtable, name) 0
#define C_OFFSET_X(vtable, name) offsetof (vtable, name)

#define C_MEMBER__(name, type, member) NULL, 0
#define C_MEMBER_X(name, type, member)				\
    name ## type ## member, N_ELEMENTS (name ## type ## member)

#define C_MEMBER_INDEX_FROM_OFFSET(members, vTableOffset)		   \
    (((vTableOffset) - (members)[0].offset) / sizeof (FinalizeObjectProc))

#define COMP_PROP_C_BASE      1024
#define COMP_PROP_C_DATA      (COMP_PROP_C_BASE + 0)
#define COMP_PROP_C_INTERFACE (COMP_PROP_C_BASE + 2)

#define C_EMIT_SIGNAL_INT(object, prototype, offset, vec, \
			  interface, name, out, index, ...)	     \
    if (out)							     \
    {								     \
	if (vec)						     \
	{							     \
	    EMIT_SIGNAL (object, prototype,			     \
			 (offset) + (index), ##__VA_ARGS__);	     \
	}							     \
								     \
	compEmitSignedSignal (object,				     \
			      interface, name,			     \
			      out, ##__VA_ARGS__);		     \
    }								     \
    else if (vec)						     \
    {								     \
	EMIT_SIGNAL (object, prototype,				     \
		     (offset) + (index), ##__VA_ARGS__);	     \
    }

#define C_EMIT_SIGNAL(object, prototype, offset, ...)		        \
    do {								\
	CompInterfaceData      *_d;					\
	const CObjectInterface *_i;					\
	int		       _index;					\
									\
	(*GET_OBJECT (object)->vTable->getProp) (GET_OBJECT (object),	\
						 COMP_PROP_C_DATA,	\
						 (void *) &_d);		\
									\
	(*GET_OBJECT (object)->vTable->getProp) (GET_OBJECT (object),	\
						 COMP_PROP_C_INTERFACE, \
						 (void *) &_i);		\
									\
	_index = C_MEMBER_INDEX_FROM_OFFSET (_i->signal, offset);	\
									\
	C_EMIT_SIGNAL_INT (object, prototype,				\
			   _d->signalVecOffset, _d->signalVecOffset,	\
			   _i->i.name,					\
			   _i->signal[_index].name,			\
			   _i->signal[_index].out,			\
			   _index, ##__VA_ARGS__);			\
    } while (0)


void
cInsertObject (CompObject *object,
	       CompObject *parent,
	       const char *name);

void
cRemoveObject (CompObject *object);

CompBool
cForEachInterface (CompObject	         *object,
		   InterfaceCallBackProc proc,
		   void		         *closure);

int
cConnect (CompObject		    *object,
	  const CompObjectInterface *interface,
	  size_t		    offset,
	  CompObject		    *descendant,
	  const CompObjectInterface *descendantInterface,
	  size_t		    descendantOffset,
	  const char		    *details,
	  va_list		    args);

void
cDisconnect (CompObject		       *object,
	     const CompObjectInterface *interface,
	     size_t		       offset,
	     int		       id);

CompBool
cForEachMethod (CompObject		  *object,
		const CompObjectInterface *interface,
		MethodCallBackProc	  proc,
		void			  *closure);

CompBool
cForEachSignal (CompObject		  *object,
		const CompObjectInterface *interface,
		SignalCallBackProc	  proc,
		void			  *closure);

CompBool
cForEachProp (CompObject		*object,
	      const CompObjectInterface *interface,
	      PropCallBackProc	        proc,
	      void		        *closure);

CompBool
cForEachChildObject (CompObject		     *object,
		     ChildObjectCallBackProc proc,
		     void		     *closure);

CompObject *
cLookupChildObject (CompObject *object,
		    const char *name);

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

CompBool
cObjectPropertiesInit (CompObject	      *object,
		       char		      *data,
		       const CObjectInterface *interface);

void
cObjectPropertiesFini (CompObject	      *object,
		       char		      *data,
		       const CObjectInterface *interface);

CompBool
cObjectChildrenInit (CompObject		     *object,
		     char		     *data,
		     const CObjectInterface  *interface,
		     const CompObjectFactory *factory);

void
cObjectChildrenFini (CompObject	            *object,
		     char		    *data,
		     const CObjectInterface *interface);

CompBool
cObjectInterfaceInit (const CompObjectInstantiator *instantiator,
		      CompObject		   *object,
		      const CompObjectFactory      *factory);

void
cObjectInterfaceFini (CompObject *object);

CompBool
cObjectInit (const CompObjectInstantiator *instantiator,
	     CompObject			  *object,
	     const CompObjectFactory      *factory);

void
cObjectFini (CompObject *object);

void
cGetInterfaceProp (CompInterfaceData	     *data,
		   const CompObjectInterface *interface,
		   unsigned int		     what,
		   void			     *value);

void
cGetObjectProp (CompObjectData	          *data,
		const CompObjectInterface *interface,
		unsigned int	          what,
		void		          *value);

void
cSetInterfaceProp (CompObject   *object,
		   unsigned int what,
		   void	        *value);

void
cSetObjectProp (CompObject   *object,
		unsigned int what,
		void	     *value);

CompObjectType *
cObjectTypeFromTemplate (const CObjectInterface *tmpl);

CompObjectInterface *
cObjectInterfaceFromTemplate (const CObjectInterface *tmpl,
			      int		     *index,
			      int		     size);


int
handleConnectVa (CompObject		   *object,
		 const CObjectInterface	   *interface,
		 int			   *signalVecOffset,
		 size_t			   offset,
		 CompObject		   *descendant,
		 const CompObjectInterface *descendantInterface,
		 size_t			   descendantOffset,
		 const char		   *signature,
		 const char		   *details,
		 va_list		   args);

int
handleConnect (CompObject		 *object,
	       const CObjectInterface	 *interface,
	       int			 *signalVecOffset,
	       size_t			 offset,
	       CompObject		 *descendant,
	       const CompObjectInterface *descendantInterface,
	       size_t			 descendantOffset,
	       const char		 *signature,
	       const char		 *details,
	       ...);

void
handleDisconnect (CompObject		 *object,
		  const CObjectInterface *interface,
		  int			 *signalVecOffset,
		  size_t		 offset,
		  int			 id);

COMPIZ_END_DECLS

#endif
