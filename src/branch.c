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

#include <compiz/branch.h>
#include <compiz/prop.h>
#include <compiz/error.h>
#include <compiz/marshal.h>
#include <compiz/c-object.h>

static CompBool
branchInitObject (const CompObjectInstantiator *instantiator,
		  CompObject		       *object,
		  const CompObjectFactory      *factory)
{
    BRANCH (object);

    if (!cObjectInit (instantiator, object, factory))
	return FALSE;

    b->factory.master        = factory;
    b->factory.instantiators = NULL;

    return TRUE;
}

static void
branchGetProp (CompObject   *object,
	       unsigned int what,
	       void	    *value)
{
    cGetObjectProp (&GET_BRANCH (object)->data.base,
		    getBranchObjectType (),
		    what, value);
}

static void
branchInsertObject (CompObject *object,
		    CompObject *parent,
		    const char *name)
{
    const CompObjectFactory *factory;

    BRANCH (object);

    cInsertObject (object, parent, name);

    for (factory = &b->factory; factory; factory = factory->master)
    {
	CompObjectInstantiatorNode *node;

	for (node = factory->instantiators; node; node = node->next)
	{
	    const CompObjectInstantiator *instantiator;
	    CompObject			 *type;

	    type = (*b->u.vTable->newObject) (b,
					      &b->data.types,
					      getStringPropObjectType (),
					      NULL,
					      NULL);
	    if (!type)
		continue;

	    (*type->vTable->setString) (type,
					NULL,
					"value", node->base.interface->name,
					NULL);

	    for (instantiator = node->instantiator;
		 instantiator;
		 instantiator = instantiator->base)
	    {
		if (instantiator->interface)
		{
		    const char *name = instantiator->interface->name;
		    CompObject *interface;

		    interface =
			(*b->u.vTable->newObject) (b,
						   type,
						   getStringPropObjectType (),
						   NULL,
						   NULL);
		    if (!interface)
			continue;

		    (*interface->vTable->setString) (interface,
						     NULL,
						     "value", name,
						     NULL);
		}

		if (instantiator == &node->base)
		    break;
	    }
	}
    }
}

static CompObject *
noopCreateObject (CompBranch	       *branch,
		  const CompObjectType *type,
		  char		       **error)
{
    CompObject *object;

    FOR_BASE (&branch->u.base,
	      object = (*branch->u.vTable->createObject) (branch,
							  type,
							  error));

    return object;
}

static CompObject *
createObject (CompBranch	   *branch,
	      const CompObjectType *type,
	      char		   **error)
{
    const CompObjectInstantiatorNode *node;
    CompObject			     *object;

    node = compGetObjectInstantiatorNode (&branch->factory, type);
    if (!node)
    {
	esprintf (error, "'%s' is not a registered object type", type->name);
	return NULL;
    }

    if (node->base.interface->instance.size <= 0)
    {
	esprintf (error, "'%s' has unknown instance size", type);
	return NULL;
    }

    object = (CompObject *) malloc (node->base.interface->instance.size);
    if (!object)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return NULL;
    }

    if (!compObjectInit (&branch->factory, object, node->instantiator))
    {
	esprintf (error, "Initialization of '%s' object failed", type);
	free (object);
	return NULL;
    }

    return object;
}

static void
noopDestroyObject (CompBranch *branch,
		   CompObject *object)
{
    FOR_BASE (&branch->u.base,
	      (*branch->u.vTable->destroyObject) (branch, object));
}

static void
destroyObject (CompBranch *branch,
	       CompObject *object)
{
    if (object->parent)
	(*object->parent->vTable->removeChild) (object->parent, object->name);

    (*object->vTable->finalize) (object);
    free (object);
}

static CompObject *
noopNewObject (CompBranch	    *branch,
	       CompObject	    *parent,
	       const CompObjectType *type,
	       const char	    *name,
	       char		    **error)
{
    CompObject *object;

    FOR_BASE (&branch->u.base,
	      object = (*branch->u.vTable->newObject) (branch,
						       parent,
						       type,
						       name,
						       error));

    return object;
}

static CompObject *
newObject (CompBranch		*branch,
	   CompObject		*parent,
	   const CompObjectType *type,
	   const char		*name,
	   char			**error)
{
    CompObject *object;
    char       tmp[256];

    object = (*branch->u.vTable->createObject) (branch, type, error);
    if (!object)
	return NULL;

    if (!name)
    {
	snprintf (tmp, sizeof (tmp), "%p", object);
	name = tmp;
    }

    if (!(*parent->vTable->addChild) (parent, object, name))
    {
	(*branch->u.vTable->destroyObject) (branch, object);
	esprintf (error, "Parent '%s' cannot hold object of type '%s'",
		  parent->name, type->name);
	return NULL;
    }

    return object;
}

typedef struct _MemberContext {
    const CompObjectInterface *interface;
    const char		      *name;
    int			      offset;
    char		      *signature;
} MemberContext;

static CompBool
getInterface (CompObject		*object,
	      const CompObjectInterface *interface,
	      void			*closure)
{
    MemberContext *pCtx = (MemberContext *) closure;

    pCtx->interface = interface;

    return FALSE;
}

static CompBool
checkSignal (CompObject	*object,
	     const char *name,
	     const char	*in,
	     size_t	offset,
	     void	*closure)
{
    MemberContext *pCtx = (MemberContext *) closure;

    if (strcmp (name, pCtx->name))
	return TRUE;

    if (!pCtx->interface)
	(*object->vTable->forEachInterface) (object, getInterface, closure);

    pCtx->signature = strdup (in);
    pCtx->offset    = offset;

    return FALSE;
}

static CompBool
checkMethod (CompObject	       *object,
	     const char	       *name,
	     const char	       *in,
	     const char	       *out,
	     size_t	       offset,
	     MethodMarshalProc marshal,
	     void	       *closure)
{
    MemberContext *pCtx = (MemberContext *) closure;

    if (strcmp (name, pCtx->name))
	return TRUE;

    if (!pCtx->interface)
	(*object->vTable->forEachInterface) (object, getInterface, closure);

    pCtx->signature = strdup (in);
    pCtx->offset    = offset;

    return FALSE;
}

static CompBool
noopConnectDescendants (CompBranch *b,
			CompObject *source,
			const char *signalInterfaceName,
			const char *signalName,
			CompObject *target,
			const char *methodInterfaceName,
			const char *methodName,
			int	   *index,
			char       **error)
{
    CompBool status;

    FOR_BASE (&b->u.base,
	      status = (*b->u.vTable->connectDescendants) (b,
							   source,
							   signalInterfaceName,
							   signalName,
							   target,
							   methodInterfaceName,
							   methodName,
							   index,
							   error));

    return status;
}

static CompBool
connectDescendants (CompBranch *b,
		    CompObject *source,
		    const char *signalInterfaceName,
		    const char *signalName,
		    CompObject *target,
		    const char *methodInterfaceName,
		    const char *methodName,
		    int	       *index,
		    char       **error)
{
    CompObject	  *ancestor;
    MemberContext signal, method;
    int		  i;

    signal.interface = NULL;
    signal.name      = signalName;

    if (signalInterfaceName)
    {
	signal.interface = compLookupObjectInterface (&b->factory,
						      signalInterfaceName);
	if (!signal.interface)
	{
	    esprintf (error, "'%s' is not a registered object type",
		      signalInterfaceName);
	    return FALSE;
	}
    }

    if ((*source->vTable->forEachSignal) (source,
					  signal.interface,
					  checkSignal,
					  (void *) &signal))
    {
	esprintf (error, "Signal '%s' doesn't exist", signalName);
	return FALSE;
    }

    if (!signal.signature)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    method.interface = NULL;
    method.name      = methodName;

    if (methodInterfaceName)
    {
	method.interface = compLookupObjectInterface (&b->factory,
						      methodInterfaceName);
	if (!method.interface)
	{
	    esprintf (error, "'%s' is not a registered object type",
		      methodInterfaceName);
	    free (signal.signature);
	    return FALSE;
	}
    }

    if ((*target->vTable->forEachMethod) (target,
					  method.interface,
					  checkMethod,
					  (void *) &method))
    {
	esprintf (error, "Method '%s' doesn't exist", methodName);
	free (signal.signature);
	return FALSE;
    }

    if (!method.signature)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	free (signal.signature);
	return FALSE;
    }

    if (strcmp (signal.signature, method.signature))
    {
	esprintf (error, "Method signature '%s' doesn't match signal "
		  "signature '%s'", method.signature, signal.signature);
	free (method.signature);
	free (signal.signature);
	return FALSE;
    }

    for (ancestor = target; ancestor; ancestor = ancestor->parent)
	if (ancestor == source)
	    break;

    if (ancestor == source)
    {
	i = (*source->vTable->connect) (source,
					signal.interface,
					signal.offset,
					target,
					method.interface,
					method.offset,
					NULL,
					(va_list) 0);
    }
    else
    {
	i = (*b->u.base.vTable->connectAsync) (&b->u.base,
					       source,
					       signal.interface,
					       signal.offset,
					       target,
					       method.interface,
					       method.offset);
    }

    free (method.signature);
    free (signal.signature);

    if (i < 0)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    if (index)
	*index = i;

    return TRUE;
}

static CompBool
noopAddNewObject (CompBranch *branch,
		  const char *parent,
		  const char *type,
		  char	     **name,
		  char	     **error)
{
    CompBool status;

    FOR_BASE (&branch->u.base,
	      status = (*branch->u.vTable->addNewObject) (branch,
							  parent,
							  type,
							  name,
							  error));

    return status;
}

static CompBool
addNewObject (CompBranch *branch,
	      const char *parent,
	      const char *type,
	      char	 **name,
	      char	 **error)
{
    const CompObjectType *t;
    CompObject		 *object, *p;
    char		 *objectName;

    p = compLookupDescendant (&branch->u.base, parent);
    if (!p)
    {
	esprintf (error, "Parent object '%s' doesn't exist", parent);
	return FALSE;
    }

    t = compLookupObjectType (&branch->factory, type);
    if (!t)
    {
	esprintf (error, "'%s' is not a registered object type", type);
	return FALSE;
    }

    object = (*branch->u.vTable->newObject) (branch, p, t, NULL, error);
    if (!object)
	return FALSE;

    objectName = malloc (strlen (parent) + strlen (object->name) + 2);
    if (!objectName)
    {
	(*branch->u.vTable->destroyObject) (branch, object);
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    sprintf (objectName, "%s/%s", parent, object->name);
    *name = objectName;

    return TRUE;
}

static CompBool
noopConnectObjects (CompBranch *b,
		    const char *source,
		    const char *signalInterface,
		    const char *signal,
		    const char *target,
		    const char *methodInterface,
		    const char *method,
		    int	       *index,
		    char       **error)
{
    CompBool status;

    FOR_BASE (&b->u.base,
	      status = (*b->u.vTable->connectObjects) (b,
						       source,
						       signalInterface,
						       signal,
						       target,
						       methodInterface,
						       method,
						       index,
						       error));

    return status;
}

static CompBool
connectObjects (CompBranch *b,
		const char *source,
		const char *signal,
		const char *signalInterface,
		const char *target,
		const char *method,
		const char *methodInterface,
		int	   *index,
		char       **error)
{
    CompObject *s, *t;

    s = compLookupDescendant (&b->u.base, source);
    if (!s)
    {
	esprintf (error, "Source object '%s' doesn't exist", source);
	return FALSE;
    }

    t = compLookupDescendant (&b->u.base, target);
    if (!t)
    {
	esprintf (error, "Target object '%s' doesn't exist", target);
	return FALSE;
    }

    if (signalInterface == '\0')
	signalInterface = NULL;

    if (methodInterface == '\0')
	methodInterface = NULL;

    return (*b->u.vTable->connectDescendants) (b,
					       s,
					       signalInterface,
					       signal,
					       t,
					       methodInterface,
					       method,
					       index,
					       error);
}

static CompBranchVTable branchObjectVTable = {
    .base.getProp      = branchGetProp,
    .base.insertObject = branchInsertObject,

    .createObject	= createObject,
    .destroyObject	= destroyObject,
    .newObject		= newObject,
    .addNewObject	= addNewObject,
    .connectDescendants = connectDescendants,
    .connectObjects	= connectObjects
};

static const CompBranchVTable noopBranchObjectVTable = {
    .createObject	= noopCreateObject,
    .destroyObject	= noopDestroyObject,
    .newObject		= noopNewObject,
    .addNewObject	= noopAddNewObject,
    .connectDescendants = noopConnectDescendants,
    .connectObjects	= noopConnectObjects
};

static const CMethod branchTypeMethod[] = {
    C_METHOD (addNewObject,   "os", "o", CompBranchVTable, marshal__SS_S_E),
    C_METHOD (connectObjects, "ossoss", "i", CompBranchVTable,
	      marshal__SSSSSS_I_E)
};

static const CChildObject branchTypeChildObject[] = {
    C_CHILD (types,   CompBranchData, COMPIZ_OBJECT_TYPE_NAME),
    C_CHILD (plugins, CompBranchData, COMPIZ_OBJECT_TYPE_NAME)
};

const CompObjectType *
getBranchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_BRANCH_TYPE_NAME,
	    .i.version	     = COMPIZ_BRANCH_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &branchObjectVTable.base,
	    .i.vTable.noop   = &noopBranchObjectVTable.base,
	    .i.vTable.size   = sizeof (branchObjectVTable),
	    .i.instance.init = branchInitObject,

	    .method  = branchTypeMethod,
	    .nMethod = N_ELEMENTS (branchTypeMethod),

	    .child  = branchTypeChildObject,
	    .nChild = N_ELEMENTS (branchTypeChildObject)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
