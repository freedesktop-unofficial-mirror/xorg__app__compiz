/*
 * Copyright © 2008 Novell, Inc.
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

#include <compiz/error.h>
#include <compiz/c-object.h>

#define CHILD(data, child)				   \
    ((CompObject *) (((char *) (data)) + (child)->offset))

#define PROP_VALUE(data, prop, type)			  \
    (*((type *) (((char *) data) + (prop)->base.offset)))

static void
cInsertObjectInterface (CompObject *object,
			CompObject *parent)
{
    CompObject	           *child;
    const CObjectInterface *cInterface;
    char	           *data;
    int		           i;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    if (cInterface->insert)
	(*cInterface->insert) (object, parent);

    (*object->vTable->interfaceAdded) (object, cInterface->i.name);

    for (i = 0; i < cInterface->nBoolProp; i++)
    {
	const CBoolProp *prop = &cInterface->boolProp[i];

	(*object->vTable->boolChanged) (object,
					cInterface->i.name,
					prop->base.name,
					PROP_VALUE (data, prop, CompBool));
    }

    for (i = 0; i < cInterface->nIntProp; i++)
    {
	const CIntProp *prop = &cInterface->intProp[i];

	(*object->vTable->intChanged) (object,
				       cInterface->i.name,
				       prop->base.name,
				       PROP_VALUE (data, prop, int32_t));
    }

    for (i = 0; i < cInterface->nDoubleProp; i++)
    {
	const CDoubleProp *prop = &cInterface->doubleProp[i];

	(*object->vTable->doubleChanged) (object,
					  cInterface->i.name,
					  prop->base.name,
					  PROP_VALUE (data, prop, double));
    }

    for (i = 0; i < cInterface->nStringProp; i++)
    {
	const CStringProp *prop = &cInterface->stringProp[i];

	(*object->vTable->stringChanged) (object,
					  cInterface->i.name,
					  prop->base.name,
					  PROP_VALUE (data, prop, char *));
    }

    for (i = 0; i < cInterface->nChild; i++)
    {
	if (cInterface->child[i].type)
	{
	    child = CHILD (data, &cInterface->child[i]);
	    (*child->vTable->insertObject) (child, object,
					    cInterface->child[i].name);
	}
    }
}

void
cInsertObject (CompObject *object,
	       CompObject *parent,
	       const char *name)
{
    FOR_BASE (object, (*object->vTable->insertObject) (object, parent, name));

    cInsertObjectInterface (object, parent);
}

static void
cRemoveObjectInterface (CompObject *object)
{
    CompObject	           *child;
    const CObjectInterface *cInterface;
    char	           *data;
    int		           i;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    i = cInterface->nChild;
    while (i--)
    {
	if (cInterface->child[i].type)
	{
	    child = CHILD (data, &cInterface->child[i]);
	    (*child->vTable->removeObject) (child);
	}
    }

    (*object->vTable->interfaceRemoved) (object, cInterface->i.name);

    if (cInterface->remove)
	(*cInterface->remove) (object);
}

void
cRemoveObject (CompObject *object)
{
    cRemoveObjectInterface (object);

    FOR_BASE (object, (*object->vTable->removeObject) (object));
}

CompBool
cForEachInterface (CompObject	         *object,
		   InterfaceCallBackProc proc,
		   void		         *closure)
{
    CompBool	           status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!(*proc) (object, &cInterface->i, closure))
	return FALSE;

    FOR_BASE (object,
	      status = (*object->vTable->forEachInterface) (object,
							    proc,
							    closure));

    return status;
}

CompBool
cForEachMethod (CompObject		  *object,
		const CompObjectInterface *interface,
		MethodCallBackProc	  proc,
		void			  *closure)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || interface == &cInterface->i)
    {
	int i;

	for (i = 0; i < cInterface->nMethod; i++)
	    if (!(*proc) (object,
			  cInterface->method[i].name,
			  cInterface->method[i].in,
			  cInterface->method[i].out,
			  cInterface->method[i].offset,
			  cInterface->method[i].marshal,
			  closure))
		return FALSE;

	if (interface)
	    return TRUE;
    }

    FOR_BASE (object,
	      status = (*object->vTable->forEachMethod) (object,
							 interface,
							 proc,
							 closure));

    return status;
}

CompBool
cForEachSignal (CompObject		  *object,
		const CompObjectInterface *interface,
		SignalCallBackProc	  proc,
		void			  *closure)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || interface == &cInterface->i)
    {
	int i;

	for (i = 0; i < cInterface->nSignal; i++)
	    if (cInterface->signal[i].out)
		if (!(*proc) (object,
			      cInterface->signal[i].name,
			      cInterface->signal[i].out,
			      cInterface->signal[i].offset,
			      closure))
		    return FALSE;

	if (interface)
	    return TRUE;
    }

    FOR_BASE (object,
	      status = (*object->vTable->forEachSignal) (object,
							 interface,
							 proc,
							 closure));

    return status;
}

CompBool
cForEachProp (CompObject		*object,
	      const CompObjectInterface *interface,
	      PropCallBackProc	        proc,
	      void		        *closure)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || interface == &cInterface->i)
    {
	int i;

	for (i = 0; i < cInterface->nBoolProp; i++)
	    if (!(*proc) (object, cInterface->boolProp[i].base.name,
			  COMP_TYPE_BOOLEAN, closure))
		return FALSE;

	for (i = 0; i < cInterface->nIntProp; i++)
	    if (!(*proc) (object, cInterface->intProp[i].base.name,
			  COMP_TYPE_INT32, closure))
		return FALSE;

	for (i = 0; i < cInterface->nDoubleProp; i++)
	    if (!(*proc) (object, cInterface->doubleProp[i].base.name,
			  COMP_TYPE_DOUBLE, closure))
		return FALSE;

	for (i = 0; i < cInterface->nStringProp; i++)
	    if (!(*proc) (object, cInterface->stringProp[i].base.name,
			  COMP_TYPE_STRING, closure))
		return FALSE;

	if (interface)
	    return TRUE;
    }

    FOR_BASE (object,
	      status = (*object->vTable->forEachProp) (object,
						       interface,
						       proc,
						       closure));

    return status;
}

CompBool
cForEachChildObject (CompObject		     *object,
		     ChildObjectCallBackProc proc,
		     void		     *closure)
{
    CompBool               status;
    const CObjectInterface *cInterface;
    char                   *data;
    int		           i;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    for (i = 0; i < cInterface->nChild; i++)
	if (CHILD (data, &cInterface->child[i])->parent)
	    if (!(*proc) (CHILD (data, &cInterface->child[i]), closure))
		return FALSE;

    FOR_BASE (object,
	      status = (*object->vTable->forEachChildObject) (object,
							      proc,
							      closure));

    return status;
}

CompObject *
cLookupChildObject (CompObject *object,
		    const char *name)
{
    CompObject	           *child;
    const CObjectInterface *cInterface;
    char                   *data;
    int		           i;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    for (i = 0; i < cInterface->nChild; i++)
	if (strcmp (CHILD (data, &cInterface->child[i])->name, name) == 0)
	    return CHILD (data, &cInterface->child[i]);

    FOR_BASE (object,
	      child = (*object->vTable->lookupChildObject) (object, name));

    return child;
}

typedef struct _HandleConnectContext {
    const CompObjectInterface *interface;
    size_t		      offset;
    const char		      *in;
    const CompObjectVTable    *vTable;
    MethodMarshalProc	      marshal;
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
	return TRUE;

    if (!marshal)
	return TRUE;

    if (strcmp (pCtx->in, in))
	return TRUE;

    pCtx->marshal = marshal;

    return FALSE;
}

static CompBool
connectInterface (CompObject		    *object,
		  const CompObjectInterface *interface,
		  void			    *closure)
{
    HandleConnectContext *pCtx = (HandleConnectContext *) closure;

    if (interface == pCtx->interface)
    {
	if (pCtx->in)
	    if ((*object->vTable->forEachMethod) (object,
						  interface,
						  connectMethod,
						  closure))
		return TRUE;

	if (!interface->vTable.noop)
	    pCtx->vTable = object->vTable;

	return FALSE;
    }

    return TRUE;
}

static int
signalIndex (const CObjectInterface *interface,
	     size_t		    offset,
	     const char		    **signature)
{
    if (interface->nSignal &&
	(offset % sizeof (FinalizeObjectProc)) == 0 &&
	offset >= interface->signal[0].offset &&
	offset <= interface->signal[interface->nSignal - 1].offset)
    {
	int i;

	i = C_MEMBER_INDEX_FROM_OFFSET (interface->signal, offset);

	if (signature)
	    *signature = interface->signal[i].out;

	return i;
    }

    return -1;
}

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
		 va_list		   args)
{
    HandleConnectContext ctx;
    CompSignalHandler    *handler;
    int			 size;
    CompSignalHandler    **vec;
    const char		 *in;
    int			 index;

    index = signalIndex (interface, offset, &in);
    if (index < 0)
	return -1;

    /* make sure details match if signal got a signature */
    if (in && details && strncmp (in, details, strlen (details)))
	return -1;

    if (!details)
	details = "";

    ctx.interface = descendantInterface;
    ctx.in	  = signature;
    ctx.vTable    = NULL;
    ctx.offset    = descendantOffset;
    ctx.marshal   = NULL;

    if (signature || !descendantInterface->vTable.noop)
	if ((*descendant->vTable->forEachInterface) (descendant,
						     connectInterface,
						     (void *) &ctx))
	    return -1;

    vec = compGetSignalVecRange (object,
				 interface->nSignal,
				 signalVecOffset);
    if (!vec)
	return -1;

    size = compSerializeMethodCall (object,
				    descendant,
				    descendantInterface->name,
				    interface->i.name,
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
    handler->offset  = descendantOffset;
    handler->marshal = ctx.marshal;
    handler->header  = (CompSerializedMethodCallHeader *) (handler + 1);

    compSerializeMethodCall (object,
			     descendant,
			     descendantInterface->name,
			     interface->i.name,
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

    return handler->id;
}

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
	       ...)
{
    va_list args;
    int     index;

    va_start (args, details);

    index = handleConnectVa (object,
			     interface,
			     signalVecOffset,
			     offset,
			     descendant,
			     descendantInterface,
			     descendantOffset,
			     signature,
			     details,
			     args);

    va_end (args);

    return index;
}

void
handleDisconnect (CompObject		 *object,
		  const CObjectInterface *interface,
		  int			 *signalVecOffset,
		  size_t		 offset,
		  int			 id)
{
    CompSignalHandler *handler, *prev = NULL;
    CompSignalHandler **vec = object->signalVec;
    int		      index;

    index = signalIndex (interface, offset, NULL);
    if (index < 0)
	return;

    if (signalVecOffset)
    {
	if (!*signalVecOffset)
	    return;

	object->signalVec += *signalVecOffset;
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
}

int
cConnect (CompObject		    *object,
	  const CompObjectInterface *interface,
	  size_t		    offset,
	  CompObject		    *descendant,
	  const CompObjectInterface *descendantInterface,
	  size_t		    descendantOffset,
	  const char		    *details,
	  va_list		    args)
{
    const CObjectInterface *cInterface;
    int			   id;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (interface == &cInterface->i)
    {
	CompInterfaceData *data;

	(*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

	return handleConnectVa (object,
				cInterface,
				&data->signalVecOffset,
				offset,
				descendant,
				descendantInterface,
				descendantOffset,
				NULL,
				details,
				args);
    }

    FOR_BASE (object, id = (*object->vTable->connect) (object,
						       interface,
						       offset,
						       descendant,
						       descendantInterface,
						       descendantOffset,
						       details,
						       args));

    return id;
}

void
cDisconnect (CompObject		       *object,
	     const CompObjectInterface *interface,
	     size_t		       offset,
	     int		       id)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (interface == &cInterface->i)
    {
	CompInterfaceData *data;

	(*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

	handleDisconnect (object,
			  cInterface,
			  &data->signalVecOffset,
			  offset,
			  id);
    }
    else
    {
	FOR_BASE (object, (*object->vTable->disconnect) (object,
							 interface,
							 offset,
							 id));
    }
}

static CompBool
handleGetBoolProp (CompObject	   *object,
		   const CBoolProp *prop,
		   CompBool	   *value)
{
    char *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    *value = *((CompBool *) (data + prop->base.offset));

    return TRUE;
}

CompBool
cGetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   *value,
	      char	 **error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nBoolProp; i++)
	    if (strcmp (name, cInterface->boolProp[i].base.name) == 0)
		return handleGetBoolProp (object,
					  &cInterface->boolProp[i],
					  value);
    }

    FOR_BASE (object, status = (*object->vTable->getBool) (object,
							   interface,
							   name,
							   value,
							   error));

    return status;
}

static CompBool
handleSetBoolProp (CompObject	   *object,
		   const CBoolProp *prop,
		   const char	   *interface,
		   const char	   *name,
		   CompBool	   value,
		   char		   **error)
{
    CompBool *ptr, oldValue;
    char     *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    ptr = (CompBool *) (data + prop->base.offset);
    oldValue = *ptr;

    if (prop->set)
    {
	if (!(*prop->set) (object, value, error))
	    return FALSE;
    }
    else
    {
	*ptr = value;
    }

    if (!*ptr != !oldValue)
	(*object->vTable->boolChanged) (object, interface, name, *ptr);

    return TRUE;
}

CompBool
cSetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   value,
	      char	 **error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nBoolProp; i++)
	    if (strcmp (name, cInterface->boolProp[i].base.name) == 0)
		return handleSetBoolProp (object,
					  &cInterface->boolProp[i],
					  interface, name,
					  value, error);
    }

    FOR_BASE (object, status = (*object->vTable->setBool) (object,
							   interface,
							   name,
							   value,
							   error));

    return status;
}

void
cBoolPropChanged (CompObject *object,
		  const char *interface,
		  const char *name,
		  CompBool   value)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nBoolProp; i++)
	    if (strcmp (name, cInterface->boolProp[i].base.name) == 0)
		if (cInterface->boolProp[i].changed)
		    (*cInterface->boolProp[i].changed) (object);
    }

    FOR_BASE (object, (*object->vTable->boolChanged) (object,
						      interface,
						      name,
						      value));
}

static CompBool
handleGetIntProp (CompObject	 *object,
		  const CIntProp *prop,
		  int32_t	 *value)
{
    char *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    *value = *((int32_t *) (data + prop->base.offset));

    return TRUE;
}

CompBool
cGetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    *value,
	     char	**error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nIntProp; i++)
	    if (strcmp (name, cInterface->intProp[i].base.name) == 0)
		return handleGetIntProp (object,
					 &cInterface->intProp[i],
					 value);
    }

    FOR_BASE (object, status = (*object->vTable->getInt) (object,
							  interface,
							  name,
							  value,
							  error));

    return status;
}

static CompBool
handleSetIntProp (CompObject	 *object,
		  const CIntProp *prop,
		  const char	 *interface,
		  const char	 *name,
		  int32_t	 value,
		  char		 **error)
{
    int32_t *ptr, oldValue;
    char    *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    ptr = (int32_t *) (data + prop->base.offset);
    oldValue = *ptr;

    if (prop->set)
    {
	if (!(*prop->set) (object, value, error))
	    return FALSE;
    }
    else
    {
	if (prop->restriction)
	{
	    if (value > prop->max)
	    {
		if (error)
		    *error = strdup ("Value is greater than maximium "
				     "allowed value");

		return FALSE;
	    }
	    else if (value < prop->min)
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
	(*object->vTable->intChanged) (object, interface, name, *ptr);

    return TRUE;
}

CompBool
cSetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    value,
	     char	**error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nIntProp; i++)
	    if (strcmp (name, cInterface->intProp[i].base.name) == 0)
		return handleSetIntProp (object,
					 &cInterface->intProp[i],
					 interface, name,
					 value, error);
    }

    FOR_BASE (object, status = (*object->vTable->setInt) (object,
							  interface,
							  name,
							  value,
							  error));

    return status;
}

void
cIntPropChanged (CompObject *object,
		 const char *interface,
		 const char *name,
		 int32_t    value)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nIntProp; i++)
	    if (strcmp (name, cInterface->intProp[i].base.name) == 0)
		if (cInterface->intProp[i].changed)
		    (*cInterface->intProp[i].changed) (object);
    }

    FOR_BASE (object, (*object->vTable->intChanged) (object,
						     interface,
						     name,
						     value));
}

static CompBool
handleGetDoubleProp (CompObject	       *object,
		     const CDoubleProp *prop,
		     double	       *value)
{
    char *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    *value = *((double *) (data + prop->base.offset));

    return TRUE;
}

CompBool
cGetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double	   *value,
		char	   **error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nDoubleProp; i++)
	    if (strcmp (name, cInterface->doubleProp[i].base.name) == 0)
		return handleGetDoubleProp (object,
					    &cInterface->doubleProp[i],
					    value);
    }

    FOR_BASE (object, status = (*object->vTable->getDouble) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

static CompBool
handleSetDoubleProp (CompObject	       *object,
		     const CDoubleProp *prop,
		     const char	       *interface,
		     const char	       *name,
		     double	       value,
		     char	       **error)
{
    double *ptr, oldValue;
    char   *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    ptr = (double *) (data + prop->base.offset);
    oldValue = *ptr;

    if (prop->set)
    {
	if (!(*prop->set) (object, value, error))
	    return FALSE;
    }
    else
    {
	if (prop->restriction)
	{
	    if (value > prop->max)
	    {
		if (error)
		    *error = strdup ("Value is greater than maximium "
				     "allowed value");

		return FALSE;
	    }
	    else if (value < prop->min)
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
	(*object->vTable->doubleChanged) (object, interface, name, *ptr);

    return TRUE;
}

CompBool
cSetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double	   value,
		char	   **error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nDoubleProp; i++)
	    if (strcmp (name, cInterface->doubleProp[i].base.name) == 0)
		return handleSetDoubleProp (object,
					    &cInterface->doubleProp[i],
					    interface, name,
					    value, error);
    }

    FOR_BASE (object, status = (*object->vTable->setDouble) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

void
cDoublePropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    double     value)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nDoubleProp; i++)
	    if (strcmp (name, cInterface->doubleProp[i].base.name) == 0)
		if (cInterface->doubleProp[i].changed)
		    (*cInterface->doubleProp[i].changed) (object);
    }

    FOR_BASE (object, (*object->vTable->doubleChanged) (object,
							interface,
							name,
							value));
}

static CompBool
handleGetStringProp (CompObject	       *object,
		     const CStringProp *prop,
		     char	       **value,
		     char	       **error)
{
    char *data, *s;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    s = strdup (*((char **) (data + prop->base.offset)));
    if (!s)
    {
	if (error)
	    *error = strdup ("Failed to copy string value");

	return FALSE;
    }

    *value = s;

    return TRUE;
}

CompBool
cGetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		char	   **value,
		char	   **error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nStringProp; i++)
	    if (strcmp (name, cInterface->stringProp[i].base.name) == 0)
		return handleGetStringProp (object,
					    &cInterface->stringProp[i],
					    value,
					    error);
    }

    FOR_BASE (object, status = (*object->vTable->getString) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

static CompBool
handleSetStringProp (CompObject	       *object,
		     const CStringProp *prop,
		     const char	       *interface,
		     const char	       *name,
		     const char	       *value,
		     char	       **error)
{
    char **ptr, *oldValue;
    char *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    ptr = (char **) (data + prop->base.offset);
    oldValue = *ptr;

    if (prop->set)
    {
	if (!(*prop->set) (object, value, error))
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
	(*object->vTable->stringChanged) (object, interface, name, *ptr);

    return TRUE;
}

CompBool
cSetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		const char *value,
		char	   **error)
{
    CompBool               status;
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nStringProp; i++)
	    if (strcmp (name, cInterface->stringProp[i].base.name) == 0)
		return handleSetStringProp (object,
					    &cInterface->stringProp[i],
					    interface, name,
					    value, error);
    }

    FOR_BASE (object, status = (*object->vTable->setString) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

void
cStringPropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    const char *value)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (!interface || !*interface ||
	!strcmp (interface, cInterface->i.name))
    {
	int i;

	for (i = 0; i < cInterface->nStringProp; i++)
	    if (strcmp (name, cInterface->stringProp[i].base.name) == 0)
		if (cInterface->stringProp[i].changed)
		    (*cInterface->stringProp[i].changed) (object);
    }

    FOR_BASE (object, (*object->vTable->stringChanged) (object,
							interface,
							name,
							value));
}

static const CompObjectVTable cVTable = {
    .setProp = cSetObjectProp,

    .insertObject = cInsertObject,
    .removeObject = cRemoveObject,

    .forEachInterface = cForEachInterface,
    .forEachMethod    = cForEachMethod,
    .forEachSignal    = cForEachSignal,
    .forEachProp      = cForEachProp,

    .forEachChildObject = cForEachChildObject,
    .lookupChildObject  = cLookupChildObject,

    .connect    = cConnect,
    .disconnect = cDisconnect,

    .getBool     = cGetBoolProp,
    .setBool     = cSetBoolProp,
    .boolChanged = cBoolPropChanged,

    .getInt     = cGetIntProp,
    .setInt     = cSetIntProp,
    .intChanged = cIntPropChanged,

    .getDouble     = cGetDoubleProp,
    .setDouble     = cSetDoubleProp,
    .doubleChanged = cDoublePropChanged,

    .getString     = cGetStringProp,
    .setString     = cSetStringProp,
    .stringChanged = cStringPropChanged
};

#define SET_DEFAULT_VALUE(data, prop, type)			\
    PROP_VALUE (data, prop, type) = (prop)->defaultValue

CompBool
cObjectPropertiesInit (CompObject	      *object,
		       char		      *data,
		       const CObjectInterface *interface)
{
    if (interface)
    {
	int i;

	for (i = 0; i < interface->nBoolProp; i++)
	    SET_DEFAULT_VALUE (data, &interface->boolProp[i], CompBool);

	for (i = 0; i < interface->nIntProp; i++)
	    SET_DEFAULT_VALUE (data, &interface->intProp[i], int32_t);

	for (i = 0; i < interface->nDoubleProp; i++)
	    SET_DEFAULT_VALUE (data, &interface->doubleProp[i], double);

	for (i = 0; i < interface->nStringProp; i++)
	{
	    const char *defaultValue = interface->stringProp[i].defaultValue;
	    char       *str;

	    if (!defaultValue)
		defaultValue = "";

	    str = strdup (defaultValue);
	    if (!str)
	    {
		while (i--)
		{
		    str = PROP_VALUE (data, &interface->stringProp[i], char *);
		    free (str);
		}

		return FALSE;
	    }

	    PROP_VALUE (data, &interface->stringProp[i], char *) = str;
	}
    }

    return TRUE;
}

void
cObjectPropertiesFini (CompObject	      *object,
		       char		      *data,
		       const CObjectInterface *interface)
{
    if (interface)
    {
	int i;

	for (i = 0; i < interface->nStringProp; i++)
	{
	    char *str;

	    str = PROP_VALUE (data, &interface->stringProp[i], char *);
	    if (str)
		free (str);
	}
    }
}

CompBool
cObjectChildrenInit (CompObject		     *object,
		     char		     *data,
		     const CObjectInterface  *interface,
		     const CompObjectFactory *factory)
{
    CompObject *child;

    if (interface)
    {
	int i;

	for (i = 0; i < interface->nChild; i++)
	{
	    const CChildObject *cChild = &interface->child[i];

	    if (cChild->type)
	    {
		child = CHILD (data, cChild);

		if (!compObjectInitByTypeName (factory, child, cChild->type))
		{
		    while (i--)
		    {
			cChild = &interface->child[i];
			if (cChild->type)
			{
			    child = CHILD (data, cChild);
			    (*child->vTable->finalize) (child);
			}
		    }

		    return FALSE;
		}
	    }
	}
    }

    if (object->parent)
    {
	if (interface)
	{
	    int i;

	    for (i = 0; i < interface->nChild; i++)
	    {
		const CChildObject *cChild = &interface->child[i];

		if (cChild->type)
		{
		    child = CHILD (data, cChild);
		    (*child->vTable->insertObject) (child, object,
						    cChild->name);
		}
	    }
	}
    }

    return TRUE;
}

void
cObjectChildrenFini (CompObject		    *object,
		     char		    *data,
		     const CObjectInterface *interface)
{
    CompObject *child;

    if (interface)
    {
	int i;

	i = interface->nChild;
	while (i--)
	{
	    const CChildObject *cChild = &interface->child[i];

	    if (cChild->type)
	    {
		child = CHILD (data, cChild);

		if (child->parent)
		    (*child->vTable->removeObject) (child);

		(*child->vTable->finalize) (child);
	    }
	}
    }
}

static CompBool
cInitObjectInterface (CompObject	      *object,
		      const CompObjectFactory *factory,
		      CompInterfaceData	      *data)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    data->signalVecOffset = 0;

    if (!cObjectPropertiesInit (object, (char *) data, cInterface))
	return FALSE;

    if (!cObjectChildrenInit (object, (char *) data, cInterface, factory))
    {
	cObjectPropertiesFini (object, (char *) data, cInterface);
	return FALSE;
    }

    if (cInterface->init && !(*cInterface->init) (object))
    {
	cObjectChildrenFini (object, (char *) data, cInterface);
	cObjectPropertiesFini (object, (char *) data, cInterface);
	return FALSE;
    }

    return TRUE;
}

static void
cFiniObjectInterface (CompObject	*object,
		      CompInterfaceData	*data)
{
    const CObjectInterface *cInterface;

    (*object->vTable->getProp) (object, COMP_PROP_C_INTERFACE, (void *)
				&cInterface);

    if (cInterface->fini)
	(*cInterface->fini) (object);

    cObjectChildrenFini (object, (char *) data, cInterface);
    cObjectPropertiesFini (object, (char *) data, cInterface);

    if (data->signalVecOffset)
	compFreeSignalVecRange (object,
				cInterface->nSignal,
				data->signalVecOffset);
}

CompBool
cObjectInterfaceInit (const CompObjectInstantiator *instantiator,
		      CompObject		   *object,
		      const CompObjectFactory      *factory)
{
    const CompObjectInstantiator *base = instantiator->base;
    const CompObjectVTable	 *vTable;
    CompInterfaceData	         *data;

    if (!(*base->init) (base, object, factory))
	return FALSE;

    vTable = object->vTable;
    object->vTable = instantiator->vTable;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    data->vTable = vTable;

    if (!cInitObjectInterface (object, factory, data))
    {
	object->vTable = vTable;
	(*object->vTable->finalize) (object);
	return FALSE;
    }

    return TRUE;
}

void
cObjectInterfaceFini (CompObject *object)
{
    CompInterfaceData *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    cFiniObjectInterface (object, data);

    object->vTable = data->vTable;

    (*object->vTable->finalize) (object);
}

CompBool
cObjectInit (const CompObjectInstantiator *instantiator,
	     CompObject			  *object,
	     const CompObjectFactory      *factory)
{
    CompObjectData		     *data;
    const CompObjectInstantiatorNode *node =
	(const CompObjectInstantiatorNode *) instantiator;
    const CompObjectInstantiatorNode *baseNode =
	(const CompObjectInstantiatorNode *) node->base.base;
    const CompObjectInstantiator     *base = baseNode->instantiator;
    const CompObjectVTable	     *vTable;

    if (!(*base->init) (base, object, factory))
	return FALSE;

    vTable = object->vTable;
    object->vTable = instantiator->vTable;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    data->base.vTable = vTable;

    if (!cInitObjectInterface (object, factory, &data->base))
    {
	object->vTable = vTable;
	(*object->vTable->finalize) (object);
	return FALSE;
    }

    data->privates = allocatePrivates (node->privates.len,
				       node->privates.sizes,
				       node->privates.totalSize);
    if (!data->privates)
    {
	cFiniObjectInterface (object, &data->base);
	object->vTable = vTable;
	(*object->vTable->finalize) (object);
	return FALSE;
    }

    return TRUE;
}

void
cObjectFini (CompObject *object)
{
    CompObjectData *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    if (data->privates)
	free (data->privates);

    cFiniObjectInterface (object, &data->base);

    object->vTable = data->base.vTable;

    (*object->vTable->finalize) (object);
}

void
cGetInterfaceProp (CompInterfaceData	     *data,
		   const CompObjectInterface *interface,
		   unsigned int		     what,
		   void			     *value)
{
    switch (what) {
    case COMP_PROP_BASE_VTABLE:
	*((const CompObjectVTable **) value) = data->vTable;
	break;
    case COMP_PROP_C_DATA:
	*((CompInterfaceData **) value) = data;
	break;
    case COMP_PROP_C_INTERFACE:
	*((const CObjectInterface **) value) =
	    (const CObjectInterface *) interface;
	break;
    }
}

void
cGetObjectProp (CompObjectData	          *data,
		const CompObjectInterface *interface,
		unsigned int	          what,
		void		          *value)
{
    switch (what) {
    case COMP_PROP_PRIVATES:
	*((CompPrivate **) value) = data->privates;
	break;
    default:
	cGetInterfaceProp (&data->base, interface, what, value);
	break;
    }
}

void
cSetInterfaceProp (CompObject   *object,
		   unsigned int what,
		   void	        *value)
{
    CompInterfaceData *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    switch (what) {
    case COMP_PROP_BASE_VTABLE:
	data->vTable = *((const CompObjectVTable **) value);
	break;
    }
}

void
cSetObjectProp (CompObject   *object,
		unsigned int what,
		void	     *value)
{
    CompObjectData *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    switch (what) {
    case COMP_PROP_PRIVATES:
	data->privates = *((CompPrivate **) value);
	break;
    default:
	cSetInterfaceProp (object, what, value);
	break;
    }
}

CompObjectType *
cObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface *type;
    CompObjectVTable *vTable;
    int		     nameSize = strlen (template->i.name) + 1;
    int		     baseNameSize = 0;
    int		     vTableSize = template->i.vTable.size;
    int		     noopVTableSize = 0;

    if (template->i.base.name)
	baseNameSize = strlen (template->i.base.name) + 1;
    else
	baseNameSize = strlen (COMPIZ_OBJECT_TYPE_NAME) + 1;

    if (!vTableSize)
	vTableSize = sizeof (CompObjectVTable);

    if (template->i.vTable.noop)
	noopVTableSize = vTableSize;

    type = malloc (sizeof (CObjectInterface) +
		   nameSize +
		   baseNameSize +
		   vTableSize +
		   noopVTableSize);
    if (!type)
	return NULL;

    *type = *template;

    type->i.name = strcpy ((char *) (type + 1), template->i.name);

    if (!template->i.vTable.size)
	type->i.vTable.size = vTableSize;

    if (!template->i.instance.init)
	type->i.instance.init = cObjectInit;

    if (template->i.base.name)
	type->i.base.name = strcpy ((char *) (type + 1) + nameSize,
				    template->i.base.name);
    else
	type->i.base.name = strcpy ((char *) (type + 1) + nameSize,
				    COMPIZ_OBJECT_TYPE_NAME);

    vTable = (CompObjectVTable *) ((char *) (type + 1) + nameSize +
				   baseNameSize);

    if (template->i.vTable.impl)
	type->i.vTable.impl = memcpy (vTable, template->i.vTable.impl,
				      vTableSize);
    else
	type->i.vTable.impl = memset (vTable, 0, vTableSize);

    if (!vTable->finalize)
	vTable->finalize = cObjectFini;

    compVTableInit (vTable, &cVTable, sizeof (CompObjectVTable));

    if (template->i.vTable.noop)
	type->i.vTable.noop = memcpy ((char *) vTable + vTableSize,
				      template->i.vTable.noop, vTableSize);

    return &type->i;
}

static CompBool
cInstallObjectInterface (const CompObjectInterface *interface,
			 CompFactory		   *factory,
			 char			   **error)
{
    const CObjectInterface *cInterface = (const CObjectInterface *) interface;
    int			   index;

    index = (*factory->allocatePrivateIndex) (factory,
					      cInterface->i.base.name,
					      cInterface->size);
    if (index < 0)
    {
	esprintf (error, "Failed to allocated private index "
		  "and data block of size '%d' in base object type '%s'",
		  cInterface->size, cInterface->i.base.name);
	return FALSE;
    }

    *(cInterface->index) = index;

    return TRUE;
}

static void
cUninstallObjectInterface (const CompObjectInterface *interface,
			   CompFactory	             *factory)
{
    const CObjectInterface *cInterface = (const CObjectInterface *) interface;
    int			   index = *(cInterface->index);

    (*factory->freePrivateIndex) (factory,
				  cInterface->i.base.name,
				  index);
}

static CompBool
cInitInterface (CompObject	        *object,
		const CompObjectVTable  *vTable,
		const CompObjectFactory *factory)
{
    const CompObjectVTable *baseVTable;
    CompInterfaceData	   *data;

    baseVTable = object->vTable;
    object->vTable = vTable;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    data->vTable = baseVTable;

    if (!cInitObjectInterface (object, factory, data))
    {
	object->vTable = baseVTable;
	return FALSE;
    }

    if (object->parent)
	cInsertObjectInterface (object, object->parent);

    return TRUE;
}

static void
cFiniInterface (CompObject *object)
{
    CompInterfaceData *data;

    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    if (object->parent)
	cRemoveObjectInterface (object);

    cFiniObjectInterface (object, data);

    object->vTable = data->vTable;
}

CompObjectInterface *
cObjectInterfaceFromTemplate (const CObjectInterface *template,
			      int		     *index,
			      int		     size)
{
    CObjectInterface *interface;
    CompObjectVTable *vTable;
    int		     nameSize = strlen (template->i.name) + 1;
    int		     baseNameSize = 0;
    int		     vTableSize = template->i.vTable.size;
    int		     noopVTableSize = 0;

    if (template->i.base.name)
	baseNameSize = strlen (template->i.base.name) + 1;
    else
	baseNameSize = strlen (COMPIZ_OBJECT_TYPE_NAME) + 1;

    if (!vTableSize)
	vTableSize = sizeof (CompObjectVTable);

    if (template->i.vTable.noop)
	noopVTableSize = vTableSize;

    interface = malloc (sizeof (CObjectInterface) +
			nameSize +
			baseNameSize +
			vTableSize +
			noopVTableSize);
    if (!interface)
	return NULL;

    *interface = *template;

    interface->index = index;
    interface->size  = size;

    interface->i.name = strcpy ((char *) (interface + 1),
				template->i.name);

    if (!template->i.vTable.size)
	interface->i.vTable.size = vTableSize;

    if (!template->i.instance.init)
	interface->i.instance.init = cObjectInterfaceInit;

    if (template->i.base.name)
	interface->i.base.name =
	    strcpy ((char *) (interface + 1) + nameSize,
		    template->i.base.name);
    else
	interface->i.base.name =
	    strcpy ((char *) (interface + 1) + nameSize,
		    COMPIZ_OBJECT_TYPE_NAME);

    vTable = (CompObjectVTable *) ((char *) (interface + 1) + nameSize +
				   baseNameSize);

    if (template->i.vTable.impl)
	interface->i.vTable.impl = memcpy (vTable, template->i.vTable.impl,
					   vTableSize);
    else
	interface->i.vTable.impl = memset (vTable, 0, vTableSize);

    if (!vTable->finalize)
	vTable->finalize = cObjectInterfaceFini;

    compVTableInit (vTable, &cVTable, sizeof (CompObjectVTable));

    if (template->i.vTable.noop)
	interface->i.vTable.noop =
	    memcpy ((char *) vTable + vTableSize,
		    template->i.vTable.noop, vTableSize);

    if (!template->i.factory.install)
	interface->i.factory.install = cInstallObjectInterface;

    if (!template->i.factory.uninstall)
	interface->i.factory.uninstall = cUninstallObjectInterface;

    if (!template->i.interface.init)
	interface->i.interface.init = cInitInterface;

    if (!template->i.interface.fini)
	interface->i.interface.fini = cFiniInterface;

    return &interface->i;
}
