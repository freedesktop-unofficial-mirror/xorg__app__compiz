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
#include <compiz/c-object.h>

static CInterface branchInterface[] = {
    C_INTERFACE (branch, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

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
    static const CMetadata template = {
	.interface  = branchInterface,
	.nInterface = N_ELEMENTS (branchInterface),
	.version    = COMPIZ_BRANCH_VERSION
    };

    cGetObjectProp (&GET_BRANCH (object)->data, &template, what, value);
}

static CompBool
noopRegisterType (CompBranch	       *branch,
		  const char           *interface,
		  const CompObjectType *type)
{
    CompBool status;

    FOR_BASE (&branch->u.base.base,
	      status = (*branch->u.vTable->registerType) (branch,
							  interface,
							  type));

    return status;
}

static CompBool
registerType (CompBranch	   *branch,
	      const char	   *interface,
	      const CompObjectType *type)
{
    return compFactoryRegisterType (&branch->factory, interface, type);
}

static CompObject *
noopCreateObject (CompBranch *branch,
		  const char *type,
		  char	     **error)
{
    CompObject *object;

    FOR_BASE (&branch->u.base.base,
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

    if (node->type->instance.size <= 0)
    {
	esprintf (error, "'%s' has unknown instance size", type);
	return NULL;
    }

    object = (CompObject *) malloc (node->type->instance.size);
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
    FOR_BASE (&branch->u.base.base,
	      (*branch->u.vTable->destroyObject) (branch, object));
}

static void
destroyObject (CompBranch *branch,
	       CompObject *object)
{
    (*object->vTable->finalize) (object);
    free (object);
}

static CompBranchVTable branchObjectVTable = {
    .base.getProp  = branchGetProp,
    .registerType  = registerType,
    .createObject  = createObject,
    .destroyObject = destroyObject
};

static const CompBranchVTable noopBranchObjectVTable = {
    .registerType  = noopRegisterType,
    .createObject  = noopCreateObject,
    .destroyObject = noopDestroyObject
};

const CompObjectType *
getBranchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CompObjectType template = {
	    .name.name     = BRANCH_TYPE_NAME,
	    .name.base     = CONTAINER_TYPE_NAME,
	    .vTable.impl   = &branchObjectVTable.base,
	    .vTable.noop   = &noopBranchObjectVTable.base,
	    .vTable.size   = sizeof (branchObjectVTable),
	    .instance.init = branchInitObject
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
