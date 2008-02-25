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

	    (*b->u.vTable->newObject) (b,
				       b->data.types.name,
				       COMPIZ_OBJECT_TYPE_NAME,
				       node->base.interface->name,
				       NULL);

	    for (instantiator = node->instantiator;
		 instantiator;
		 instantiator = instantiator->base)
	    {
		if (instantiator->interface)
		{
		    const char *name = instantiator->interface->name;
		    char       path[257];

		    sprintf (path,
			     "%s/%s",
			     b->data.types.name,
			     node->base.interface->name);

		    (*b->u.vTable->newObject) (b,
					       path,
					       COMPIZ_OBJECT_TYPE_NAME,
					       name,
					       NULL);
		}

		if (instantiator == &node->base)
		    break;
	    }
	}
    }
}

static CompObject *
noopCreateObject (CompBranch *branch,
		  const char *type,
		  char	     **error)
{
    CompObject *object;

    FOR_BASE (&branch->u.base,
	      object = (*branch->u.vTable->createObject) (branch,
							  type,
							  error));

    return object;
}

static CompObject *
createObject (CompBranch *branch,
	      const char *type,
	      char	 **error)
{
    CompObjectInstantiatorNode *node;
    CompObject		       *object;

    node = compObjectInstantiatorNode (&branch->factory, type);
    if (!node)
    {
	esprintf (error, "'%s' is not a registered object type", type);
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
noopNewObject (CompBranch *branch,
	       const char *parent,
	       const char *type,
	       const char *name,
	       char	  **error)
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
newObject (CompBranch *branch,
	   const char *parent,
	   const char *type,
	   const char *name,
	   char	      **error)
{
    CompObject *object, *p;
    char       tmp[256];

    p = compLookupObject (&branch->u.base, parent);
    if (!p)
    {
	esprintf (error, "Parent object '%s' doesn't exist", parent);
	return NULL;
    }

    object = (*branch->u.vTable->createObject) (branch, type, error);
    if (!object)
	return NULL;

    if (!name)
    {
	snprintf (tmp, sizeof (tmp), "%p", object);
	name = tmp;
    }

    if (!(*p->vTable->addChild) (p, object, name))
    {
	(*branch->u.vTable->destroyObject) (branch, object);
	esprintf (error, "Parent '%s' cannot hold object of type '%s'", parent,
		  type);
	return NULL;
    }

    return object;
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
    CompObject *object;
    char       *objectName;

    object = (*branch->u.vTable->newObject) (branch, parent, type, NULL,
					     error);
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

static CompBranchVTable branchObjectVTable = {
    .base.getProp      = branchGetProp,
    .base.insertObject = branchInsertObject,

    .createObject  = createObject,
    .destroyObject = destroyObject,
    .newObject     = newObject,
    .addNewObject  = addNewObject
};

static const CompBranchVTable noopBranchObjectVTable = {
    .createObject  = noopCreateObject,
    .destroyObject = noopDestroyObject,
    .newObject     = noopNewObject,
    .addNewObject  = noopAddNewObject
};

static const CMethod branchTypeMethod[] = {
    C_METHOD (addNewObject, "os", "o", CompBranchVTable, marshal__SS_S_E)
};

static const CChildObject branchTypeChildObject[] = {
    C_CHILD (types, CompBranchData, COMPIZ_OBJECT_TYPE_NAME)
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
