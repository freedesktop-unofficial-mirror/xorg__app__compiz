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
#include <sys/stat.h>
#include <assert.h>

#include <compiz/object.h>
#include <compiz/c-object.h>
#include <compiz/marshal.h>
#include <compiz/error.h>

static CompSignalHandler lastSignalVecEntry  = { 0 };
static CompSignalHandler emptySignalVecEntry = { 0 };

CompBool
compObjectInit (const CompObjectFactory      *factory,
		CompObject		     *object,
		const CompObjectInstantiator *instantiator)
{
    return (*instantiator->init) (instantiator, object, factory);
}

static CompObjectInstantiatorNode *
findObjectInstantiatorNode (const CompObjectFactory *factory,
			    const CompObjectType    *type)
{
    CompObjectInstantiatorNode *node;

    for (node = factory->instantiators; node; node = node->next)
	if (type == node->base.interface)
	    return node;

    if (factory->master)
	return findObjectInstantiatorNode (factory->master, type);

    return NULL;
}

static const CompObjectInstantiator *
findObjectInstantiator (const CompObjectFactory *factory,
			const CompObjectType	*type)
{
    CompObjectInstantiatorNode *node;

    for (node = factory->instantiators; node; node = node->next)
	if (type == node->base.interface)
	    return node->instantiator;

    if (factory->master)
	return findObjectInstantiator (factory->master, type);

    return NULL;
}

static CompObjectInstantiatorNode *
lookupObjectInstantiatorNode (const CompObjectFactory *factory,
			      const char	      *name)
{
    CompObjectInstantiatorNode *node;

    for (node = factory->instantiators; node; node = node->next)
	if (strcmp (name, node->base.interface->name) == 0)
	    return node;

    if (factory->master)
	return lookupObjectInstantiatorNode (factory->master, name);

    return NULL;
}

static const CompObjectInstantiator *
lookupObjectInstantiator (const CompObjectFactory *factory,
			  const char		  *name)
{
    CompObjectInstantiatorNode *node;

    node = lookupObjectInstantiatorNode (factory, name);
    if (!node)
	return NULL;

    return node->instantiator;
}

CompBool
compObjectInitByType (const CompObjectFactory *factory,
		      CompObject	      *object,
		      const CompObjectType    *type)
{
    const CompObjectInstantiator *instantiator;

    instantiator = findObjectInstantiator (factory, type);
    if (!instantiator)
	return FALSE;

    return compObjectInit (factory, object, instantiator);
}

CompBool
compObjectInitByTypeName (const CompObjectFactory *factory,
			  CompObject		  *object,
			  const char		  *name)
{
    const CompObjectInstantiator *instantiator;

    instantiator = lookupObjectInstantiator (factory, name);
    if (!instantiator)
	return FALSE;

    return compObjectInit (factory, object, instantiator);
}

const CompObjectType *
compLookupObjectType (const CompObjectFactory *factory,
		      const char	      *name)
{
    CompObjectInstantiatorNode *node;

    node = lookupObjectInstantiatorNode (factory, name);
    if (!node)
	return NULL;

    return node->base.interface;
}

const CompObjectInterface *
compLookupObjectInterface (const CompObjectFactory *factory,
			   const char		   *name)
{
    for (; factory; factory = factory->master)
    {
	CompObjectInstantiatorNode *node;

	for (node = factory->instantiators; node; node = node->next)
	{
	    const CompObjectInstantiator *instantiator;

	    for (instantiator = node->instantiator;
		 instantiator;
		 instantiator = instantiator->base)
	    {
		if (instantiator->interface)
		    if (strcmp (instantiator->interface->name, name) == 0)
			return instantiator->interface;

		if (instantiator == &node->base)
		    break;
	    }
	}
    }

    return NULL;
}

const CompObjectInstantiatorNode *
compGetObjectInstantiatorNode (const CompObjectFactory *factory,
			       const CompObjectType    *type)
{
    return findObjectInstantiatorNode (factory, type);
}

void
compVTableInit (CompObjectVTable       *vTable,
		const CompObjectVTable *noopVTable,
		int		       size)
{
    char	 *dVTable = (char *) vTable;
    const char	 *sVTable = (const char *) noopVTable;
    unsigned int i;

    for (i = 0; i < size; i += sizeof (vTable->finalize))
    {
	GetPropProc *dst = (GetPropProc *) (dVTable + i);
	GetPropProc *src = (GetPropProc *) (sVTable + i);

	if (*src && !*dst)
	    *dst = *src;
    }
}

CompBool
compFactoryInstallType (CompObjectFactory    *factory,
			const CompObjectType *type,
			char		     **error)
{
    const CompObjectInstantiatorNode *base = NULL;
    CompObjectInstantiatorNode	     *node;
    CompFactory			     *master;
    const CompObjectFactory	     *f;
    CompObjectPrivatesNode	     *pNode;

    if (type->base.name)
    {
	base = lookupObjectInstantiatorNode (factory, type->base.name);
	if (!base)
	{
	    esprintf (error, "Base type '%s' is not yet registered",
		      type->base.name);
	    return FALSE;
	}

	if (base->base.interface->version < type->base.version)
	{
	    esprintf (error, "Version '%d' of base type '%s' is too old",
		      base->base.interface->version, type->base.name);
	    return FALSE;
	}
    }

    node = malloc (sizeof (CompObjectInstantiatorNode) + type->vTable.size);
    if (!node)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    for (f = factory; f->master; f = f->master);
    master = (CompFactory *) f;

    if (type->factory.install)
    {
	if (!(*type->factory.install) (type, master, error))
	{
	    free (node);
	    return FALSE;
	}
    }

    node->base.base	     = &base->base;
    node->base.interface     = type;
    node->base.vTable        = NULL;
    node->base.init	     = type->instance.init;
    node->next		     = factory->instantiators;
    node->instantiator	     = &node->base;
    node->privates.len	     = 0;
    node->privates.sizes     = NULL;
    node->privates.totalSize = 0;

    if (type->vTable.size)
    {
	const CompObjectInstantiatorNode *n;
	const CompObjectInstantiator     *p;

	assert (!base ||
		base->base.interface->vTable.size <= type->vTable.size);

	node->base.vTable = memcpy (node + 1,
				    type->vTable.impl,
				    type->vTable.size);

	for (p = &node->base; p; p = p->base)
	{
	    n = (const CompObjectInstantiatorNode *) p;

	    if (n->base.interface->vTable.noop)
		compVTableInit (node->base.vTable,
				n->base.interface->vTable.noop,
				n->base.interface->vTable.size);
	}
    }

    for (pNode = master->privates; pNode; pNode = pNode->next)
    {
	if (strcmp (pNode->name, type->name) == 0)
	{
	    node->privates = pNode->privates;
	    break;
	}
    }

    factory->instantiators = node;

    return TRUE;
}

const CompObjectType *
compFactoryUninstallType (CompObjectFactory *factory)
{
    CompObjectInstantiatorNode *node;
    CompFactory		       *master;
    const CompObjectFactory    *f;
    const CompObjectType       *type;

    node = factory->instantiators;
    if (!node)
	return NULL;

    type = node->base.interface;
    factory->instantiators = node->next;

    for (f = factory; f->master; f = f->master);
    master = (CompFactory *) f;

    if (type->factory.uninstall)
	(*type->factory.uninstall) (type, master);

    free (node);

    return type;
}

typedef struct _InitObjectInterfaceContext {
    const CompObjectFactory	 *factory;
    const CompObjectInstantiator *instantiator;
    CompObject			 *object;
    char			 **error;
} InitObjectInterfaceContext;

static CompBool
initObjectInterface (CompObject *object,
		     void	*closure)
{
    InitObjectInterfaceContext *pCtx = (InitObjectInterfaceContext *) closure;

    if (object->vTable == pCtx->instantiator->base->vTable)
    {
	const CompObjectInterface *interface = pCtx->instantiator->interface;

	if (!(*interface->interface.init) (object,
					   pCtx->instantiator->vTable,
					   pCtx->factory))
	{
	    esprintf (pCtx->error,
		      "Failed to initialize interface '%s' for '%s' object",
		      interface->name, object->name);
	    return FALSE;
	}

	return TRUE;
    }
    else
    {
	const CompObjectVTable *baseVTable, *vTable = object->vTable;
	CompBool	       status = TRUE;

	(*object->vTable->getProp) (object,
				    COMP_PROP_BASE_VTABLE,
				    (void *) &baseVTable);

	if (baseVTable)
	{
	    object->vTable = baseVTable;

	    status = initObjectInterface (object, closure);

	    baseVTable = object->vTable;
	    object->vTable = vTable;

	    (*object->vTable->setProp) (object,
					COMP_PROP_BASE_VTABLE,
					(void *) &baseVTable);
	}

	return status;
    }
}

static CompBool
finiObjectInterface (CompObject *object,
		     void	*closure)
{
    InitObjectInterfaceContext *pCtx = (InitObjectInterfaceContext *) closure;

    if (object->vTable == pCtx->instantiator->vTable)
    {
	const CompObjectInterface *interface = pCtx->instantiator->interface;

	(*interface->interface.fini) (object);
    }
    else
    {
	const CompObjectVTable *baseVTable, *vTable = object->vTable;

	(*object->vTable->getProp) (object,
				    COMP_PROP_BASE_VTABLE,
				    (void *) &baseVTable);

	if (baseVTable)
	{
	    object->vTable = baseVTable;

	    finiObjectInterface (object, closure);

	    baseVTable = object->vTable;
	    object->vTable = vTable;

	    (*object->vTable->setProp) (object,
					COMP_PROP_BASE_VTABLE,
					(void *) &baseVTable);
	}
    }

    return TRUE;
}

static CompBool
finiInterfaceForObjectTree (CompObject *object,
			    void       *closure)
{
    InitObjectInterfaceContext ctx, *pCtx = (InitObjectInterfaceContext *)
	closure;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == object)
	return FALSE;

    ctx = *pCtx;
    ctx.object = NULL;

    (*object->vTable->forEachChildObject) (object,
					   finiInterfaceForObjectTree,
					   (void *) &ctx);

    finiObjectInterface (object, (void *) &ctx);

    return TRUE;
}

static CompBool
initInterfaceForObjectTree (CompObject *object,
			    void	*closure)
{
    InitObjectInterfaceContext ctx, *pCtx = (InitObjectInterfaceContext *)
	closure;

    pCtx->object = object;

    if (!initObjectInterface (object, closure))
	return FALSE;

    ctx = *pCtx;
    ctx.object = NULL;

    if (!(*object->vTable->forEachChildObject) (object,
						initInterfaceForObjectTree,
						(void *) &ctx))
    {
	(*object->vTable->forEachChildObject) (object,
					       finiInterfaceForObjectTree,
					       (void *) &ctx);

	finiObjectInterface (object, (void *) &ctx);

	return FALSE;
    }

    return TRUE;
}

CompBool
compFactoryInstallInterface (CompObjectFactory	       *factory,
			     CompObject		       *root,
			     const CompObjectType      *type,
			     const CompObjectInterface *interface,
			     char		       **error)
{
    InitObjectInterfaceContext ctx;
    CompObjectInstantiatorNode *node;
    CompObjectInstantiator     *instantiator;
    CompFactory		       *master;
    const CompObjectFactory    *f;

    node = findObjectInstantiatorNode (factory, type);
    if (!node)
    {
	esprintf (error, "Base type '%s' is not yet registered",
		  interface->base.name);
	return FALSE;
    }

    instantiator = malloc (sizeof (CompObjectInstantiator) +
			   interface->vTable.size);
    if (!instantiator)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    for (f = factory; f->master; f = f->master);
    master = (CompFactory *) f;

    if (interface->factory.install)
    {
	if (!(*interface->factory.install) (interface, master, error))
	{
	    free (instantiator);
	    return FALSE;
	}
    }

    instantiator->interface = interface;
    instantiator->vTable    = NULL;
    instantiator->init	    = interface->instance.init;

    if (interface->vTable.size)
    {
	const CompObjectInstantiatorNode *n;
	const CompObjectInstantiator     *p;

	assert (node->base.interface->vTable.size <= type->vTable.size);

	instantiator->vTable = memcpy (instantiator + 1,
				       interface->vTable.impl,
				       interface->vTable.size);

	for (p = &node->base; p; p = p->base)
	{
	    n = (const CompObjectInstantiatorNode *) p;

	    if (n->base.interface->vTable.noop)
		compVTableInit (instantiator->vTable,
				n->base.interface->vTable.noop,
				n->base.interface->vTable.size);
	}
    }

    instantiator->base = node->instantiator;
    node->instantiator = instantiator;

    ctx.factory	     = factory;
    ctx.instantiator = node->instantiator;
    ctx.object	     = NULL;
    ctx.error	     = error;

    if (!initInterfaceForObjectTree (root, (void *) &ctx))
    {
	node->instantiator = instantiator->base;

	if (interface->factory.uninstall)
	    (*interface->factory.uninstall) (interface, master);

	free (instantiator);
    }

    return TRUE;
}

const CompObjectInterface *
compFactoryUninstallInterface (CompObjectFactory    *factory,
			       CompObject	    *root,
			       const CompObjectType *type)
{
    InitObjectInterfaceContext ctx;
    CompObjectInstantiatorNode *node;
    CompObjectInstantiator     *instantiator;
    CompFactory		       *master;
    const CompObjectFactory    *f;
    const CompObjectInterface  *interface;

    node = findObjectInstantiatorNode (factory, type);
    if (!node)
	return NULL;

    if (node->instantiator == &node->base)
	return NULL;

    interface = node->instantiator->interface;

    for (f = factory; f->master; f = f->master);
    master = (CompFactory *) f;

    instantiator = (CompObjectInstantiator *) node->instantiator;
    node->instantiator = instantiator->base;

    ctx.factory	     = factory;
    ctx.instantiator = instantiator;
    ctx.object	     = NULL;
    ctx.error	     = NULL;

    finiInterfaceForObjectTree (root, (void *) &ctx);

    free (instantiator);

    if (interface->factory.uninstall)
	(*interface->factory.uninstall) (interface, master);

    return interface;
}

static CompBool
initObject (const CompObjectInstantiator *instantiator,
	    CompObject			 *object,
	    const CompObjectFactory	 *factory)
{
    const CompObjectInstantiatorNode *node =
	(const CompObjectInstantiatorNode *) instantiator;

    object->privates = allocatePrivates (node->privates.len,
					 node->privates.sizes,
					 node->privates.totalSize);
    if (!object->privates)
	return FALSE;

    object->vTable    = instantiator->vTable;
    object->parent    = NULL;
    object->name      = NULL;
    object->signalVec = NULL;
    object->child     = NULL;
    object->nChild    = 0;

    object->id = ~0; /* XXX: remove id asap */

    return TRUE;
}

static void
finalize (CompObject *object)
{
    if (object->child)
    {
	int i;

	for (i = 0; i < object->nChild; i++)
	{
	    (*object->child[i].ref->vTable->finalize) (object->child[i].ref);

	    free (object->child[i].ref);
	    free (object->child[i].name);
	}

	free (object->child);
    }

    if (object->privates)
	free (object->privates);

    if (object->signalVec)
	free (object->signalVec);
}

static void
noopFinalize (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->finalize) (object));
}

static void
getProp (CompObject   *object,
	 unsigned int what,
	 void	      *value)
{
    switch (what) {
    case COMP_PROP_BASE_VTABLE:
	*((CompObjectVTable **) value) = NULL;
	break;
    case COMP_PROP_PRIVATES:
	*((CompPrivate **) value) = object->privates;
	break;
    case COMP_PROP_C_DATA:
	*((void **) value) = NULL;
	break;
    case COMP_PROP_C_INTERFACE:
	*((const CObjectInterface **) value) = (const CObjectInterface *)
	    getObjectType ();
	break;
    }
}

static void
setProp (CompObject   *object,
	 unsigned int what,
	 void	      *value)
{
    switch (what) {
    case COMP_PROP_PRIVATES:
	object->privates = *((CompPrivate **) value);
	break;
    }
}

static void
insertObject (CompObject *object,
	      CompObject *parent,
	      const char *name)
{
    int i;

    object->parent = parent;
    object->name   = name;

    (*object->vTable->inserted) (object);
    (*object->vTable->interfaceAdded) (object, getObjectType ()->name);

    for (i = 0; i < object->nChild; i++)
	(*object->child[i].ref->vTable->insertObject) (object->child[i].ref,
						       object,
						       object->child[i].name);
}

static void
noopInsertObject (CompObject *object,
		  CompObject *parent,
		  const char *name)
{
    FOR_BASE (object, (*object->vTable->insertObject) (object, parent, name));
}

static void
removeObject (CompObject *object)
{
    int i;

    i = object->nChild;
    while (i--)
	(*object->child[i].ref->vTable->removeObject) (object->child[i].ref);

    (*object->vTable->interfaceRemoved) (object, getObjectType ()->name);
    (*object->vTable->removed) (object);

    object->parent = NULL;
    object->name   = NULL;
}

static void
noopRemoveObject (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->removeObject) (object));
}

static void
inserted (CompObject *object)
{
    const CObjectInterface *interface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (interface->signal,
				    offsetof (CompObjectVTable, inserted));
    const CSignal	   *signal = &interface->signal[index];

    C_EMIT_SIGNAL_INT (object, InsertedProc, 0, object->signalVec,
		       interface->i.name, signal->name, signal->out,
		       index);
}

static void
noopInserted (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->inserted) (object));
}

static void
removed (CompObject *object)
{
    const CObjectInterface *interface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (interface->signal,
				    offsetof (CompObjectVTable, removed));
    const CSignal	   *signal = &interface->signal[index];

    C_EMIT_SIGNAL_INT (object, RemovedProc, 0, object->signalVec,
		       interface->i.name, signal->name, signal->out,
		       index);
}

static void
noopRemoved (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->removed) (object));
}

static CompBool
forEachInterface (CompObject	        *object,
		  InterfaceCallBackProc proc,
		  void		        *closure)
{
    if (!(*proc) (object, getObjectType (), closure))
	return FALSE;

    return TRUE;
}

static CompBool
noopForEachInterface (CompObject	    *object,
		      InterfaceCallBackProc proc,
		      void		    *closure)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->forEachInterface) (object,
							    proc,
							    closure));

    return status;
}

static CompBool
forEachMethod (CompObject		 *object,
	       const CompObjectInterface *interface,
	       MethodCallBackProc	 proc,
	       void			 *closure)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();

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
    }

    return TRUE;
}

static CompBool
noopForEachMethod (CompObject		     *object,
		   const CompObjectInterface *interface,
		   MethodCallBackProc	     proc,
		   void			     *closure)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->forEachMethod) (object,
							 interface,
							 proc,
							 closure));

    return status;
}

static CompBool
forEachSignal (CompObject		 *object,
	       const CompObjectInterface *interface,
	       SignalCallBackProc	 proc,
	       void			 *closure)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();

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
    }

    return TRUE;
}

static CompBool
noopForEachSignal (CompObject		     *object,
		   const CompObjectInterface *interface,
		   SignalCallBackProc	     proc,
		   void			     *closure)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->forEachSignal) (object,
							 interface,
							 proc,
							 closure));

    return status;
}

static CompBool
forEachProp (CompObject		       *object,
	     const CompObjectInterface *interface,
	     PropCallBackProc	       proc,
	     void		       *closure)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();

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
    }

    return TRUE;
}

static CompBool
noopForEachProp (CompObject		   *object,
		 const CompObjectInterface *interface,
		 PropCallBackProc	   proc,
		 void		           *closure)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->forEachProp) (object,
						       interface,
						       proc,
						       closure));

    return status;
}

static void
interfaceAdded (CompObject *object,
		const char *interface)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable,
					      interfaceAdded));
    const CSignal	   *signal = &cInterface->signal[index];

    C_EMIT_SIGNAL_INT (object, InterfaceAddedProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface);
}

static void
noopInterfaceAdded (CompObject *object,
		    const char *interface)
{
    FOR_BASE (object, (*object->vTable->interfaceAdded) (object, interface));
}

static void
interfaceRemoved (CompObject *object,
		  const char *interface)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable,
					      interfaceRemoved));
    const CSignal	   *signal = &cInterface->signal[index];
    CompObject		   *node;

    for (node = object; node; node = node->parent)
    {
	if (node->signalVec)
	{
	    int i;

	    for (i = 0; node->signalVec[i] != &lastSignalVecEntry; i++)
	    {
		CompSignalHandler *handler, *prev;

		if (node->signalVec[i] == &emptySignalVecEntry)
		    continue;

		prev    = NULL;
		handler = node->signalVec[i];

		while (handler)
		{
		    if (handler->object == object)
		    {
			if (strcmp (handler->header->interface,
				    interface) == 0)
			{
			    if (prev)
			    {
				prev->next = handler->next;
				free (handler);
				handler = prev->next;
			    }
			    else
			    {
				node->signalVec[i] = handler->next;
				free (handler);
				handler = node->signalVec[i];
			    }

			    continue;
			}
		    }

		    prev    = handler;
		    handler = handler->next;
		}
	    }
	}
    }

    C_EMIT_SIGNAL_INT (object, InterfaceRemovedProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface);
}

static void
noopInterfaceRemoved (CompObject *object,
		      const char *interface)
{
    FOR_BASE (object, (*object->vTable->interfaceRemoved) (object, interface));
}

static CompBool
addChild (CompObject *object,
	  CompObject *child,
	  const char *name)
{
    CompChild *children;
    char      *childName;

    childName = strdup (name);
    if (!childName)
	return FALSE;

    children = realloc (object->child, sizeof (CompChild) *
			(object->nChild + 1));
    if (!children)
    {
	free (childName);
	return FALSE;
    }

    children[object->nChild].ref  = child;
    children[object->nChild].name = childName;

    object->child = children;
    object->nChild++;

    if (object->parent)
	(*child->vTable->insertObject) (child, object, childName);

    return TRUE;
}

static CompBool
noopAddChild (CompObject *object,
	      CompObject *child,
	      const char *name)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->addChild) (object, child, name));

    return status;
}

static CompObject *
removeChild (CompObject *object,
	     const char *name)
{
    CompObject *child;
    CompChild  *children;
    int	       i;

    for (i = 0; i < object->nChild; i++)
	if (strcmp (object->child[i].name, name) == 0)
	    break;

    if (i == object->nChild)
	return NULL;

    child = object->child[i].ref;

    if (object->parent)
	(*child->vTable->removeObject) (child);

    free (object->child[i].name);

    object->nChild--;

    for (; i < object->nChild; i++)
	object->child[i] = object->child[i + 1];

    children = realloc (object->child, sizeof (CompChild) * object->nChild);
    if (children || !object->nChild)
	object->child = children;

    return child;
}

static CompObject *
noopRemoveChild (CompObject *object,
		 const char *name)
{
    CompObject *child;

    FOR_BASE (object, child = (*object->vTable->removeChild) (object, name));

    return child;
}

static CompBool
forEachChildObject (CompObject		    *object,
		    ChildObjectCallBackProc proc,
		    void		    *closure)
{
    int	i;

    for (i = 0; i < object->nChild; i++)
	if (!(*proc) (object->child[i].ref, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
noopForEachChildObject (CompObject		*object,
			ChildObjectCallBackProc proc,
			void		        *closure)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->forEachChildObject) (object,
							      proc,
							      closure));

    return status;
}

static CompObject *
lookupChildObject (CompObject *object,
		   const char *name)
{
    int	i;

    for (i = 0; i < object->nChild; i++)
	if (strcmp (object->child[i].name, name) == 0)
	    return object->child[i].ref;

    return NULL;
}

static CompObject *
noopLookupChildObject (CompObject *object,
		       const char *name)
{
    CompObject *child;

    FOR_BASE (object,
	      child = (*object->vTable->lookupChildObject) (object, name));

    return child;
}

CompSignalHandler **
compGetSignalVecRange (CompObject *object,
		       int	  size,
		       int	  *offset)
{
    CompSignalHandler **vec;
    int		      start = 0;

    if (object->signalVec)
    {
	int i, empty = 0;

	if (!offset)
	    return object->signalVec;

	if (*offset)
	    return &object->signalVec[*offset];

	if (!size)
	    return NULL;

	/* find empty slot */
	for (i = 0; object->signalVec[i] != &lastSignalVecEntry; i++)
	{
	    if (object->signalVec[i] == &emptySignalVecEntry)
	    {
		empty++;

		if (empty == size)
		{
		    start = i - size + 1;

		    memset (&object->signalVec[start], 0,
			    sizeof (CompSignalHandler **) * size);

		    *offset = start;

		    return &object->signalVec[start];
		}
	    }
	    else
	    {
		empty = 0;
	    }
	}

	start = i;
    }
    else
    {
	const CObjectInterface *cInterface = (const CObjectInterface *)
	    getObjectType ();

	if (offset)
	    start = cInterface->nSignal;
    }

    vec = realloc (object->signalVec, sizeof (CompSignalHandler **) *
		   (start + size + 1));
    if (!vec)
	return NULL;

    if (object->signalVec)
	memset (&vec[start], 0, sizeof (CompSignalHandler **) * size);
    else
	memset (vec, 0, sizeof (CompSignalHandler **) * (size + start));

    vec[start + size] = &lastSignalVecEntry;
    object->signalVec = vec;

    if (offset)
	*offset = start;

    return &object->signalVec[start];
}

void
compFreeSignalVecRange (CompObject *object,
			int	   size,
			int	   offset)
{
    while (size--)
	object->signalVec[offset + size] = &emptySignalVecEntry;
}

static int
connect (CompObject		   *object,
	 const CompObjectInterface *interface,
	 size_t			   offset,
	 CompObject		   *descendant,
	 const CompObjectInterface *descendantInterface,
	 size_t			   descendantOffset,
	 const char		   *details,
	 va_list		   args)
{
    if (getObjectType () == interface)
	return handleConnectVa (object,
				(const CObjectInterface *) interface, NULL,
				offset,
				descendant,
				descendantInterface,
				descendantOffset,
				NULL,
				details,
				args);

    return -1;
}

static int
noopConnect (CompObject		       *object,
	     const CompObjectInterface *interface,
	     size_t		       offset,
	     CompObject		       *descendant,
	     const CompObjectInterface *descendantInterface,
	     size_t		       descendantOffset,
	     const char		       *details,
	     va_list		       args)
{
    int index;

    FOR_BASE (object, index = (*object->vTable->connect) (object,
							  interface,
							  offset,
							  descendant,
							  descendantInterface,
							  descendantOffset,
							  details,
							  args));

    return index;
}

typedef struct _CheckSignalContext {
    size_t offset;
    char   *name;
    char   *signature;
} CheckSignalContext;

static CompBool
checkSignal (CompObject	 *object,
	     const char	 *name,
	     const char	 *out,
	     size_t	 offset,
	     void	 *closure)
{
    CheckSignalContext *pCtx = (CheckSignalContext *) closure;

    if (offset != pCtx->offset || !out)
	return TRUE;

    pCtx->name = strdup (name);
    if (!pCtx->name)
	return TRUE;

    pCtx->signature = strdup (out);
    if (!pCtx->signature)
    {
	free (pCtx->name);
	return TRUE;
    }

    return FALSE;
}

static int
connectAsync (CompObject		*object,
	      CompObject		*descendant,
	      const CompObjectInterface *descendantInterface,
	      size_t			descendantOffset,
	      CompObject		*target,
	      const CompObjectInterface *targetInterface,
	      size_t			targetOffset)
{
    CheckSignalContext ctx;
    char	       *path;
    int		       index, pathSize = 0;

    ctx.offset = descendantOffset;

    if ((*descendant->vTable->forEachSignal) (descendant,
					      descendantInterface,
					      checkSignal,
					      (void *) &ctx))
	return -1;

    pathSize = compGetObjectPath (object, descendant, NULL, 0);
    if (!pathSize)
    {
	free (ctx.name);
	free (ctx.signature);
	return -1;
    }

    path = malloc (pathSize);
    if (!path)
    {
	free (ctx.name);
	free (ctx.signature);
	return -1;
    }

    compGetObjectPath (object, descendant, path, pathSize);

    index = handleConnect (object,
			   (const CObjectInterface *) getObjectType (), NULL,
			   offsetof (CompObjectVTable, signal),
			   target,
			   targetInterface,
			   targetOffset,
			   ctx.signature,
			   "osss",
			   path,
			   descendantInterface->name,
			   ctx.name,
			   ctx.signature);

    free (path);
    free (ctx.name);
    free (ctx.signature);

    return index;
}

static int
noopConnectAsync (CompObject		    *object,
		  CompObject		    *descendant,
		  const CompObjectInterface *descendantInterface,
		  size_t		    descendantOffset,
		  CompObject		    *target,
		  const CompObjectInterface *targetInterface,
		  size_t		    targetOffset)
{
    int index;

    FOR_BASE (object,
	      index = (*object->vTable->connectAsync) (object,
						       descendant,
						       descendantInterface,
						       descendantOffset,
						       target,
						       targetInterface,
						       targetOffset));

    return index;
}

static void
disconnect (CompObject		      *object,
	    const CompObjectInterface *interface,
	    size_t		      offset,
	    int			      index)
{
    if (getObjectType () == interface)
	handleDisconnect (object,
			  (const CObjectInterface *) interface, NULL,
			  offset,
			  index);
}

static void
noopDisconnect (CompObject		  *object,
		const CompObjectInterface *interface,
		size_t			  offset,
		int			  index)
{
    FOR_BASE (object, (*object->vTable->disconnect) (object,
						     interface,
						     offset,
						     index));
}

typedef struct _SignalArgs {
    CompArgs base;

    CompAnyValue *value;
    int		 nValue;
    int		 i;
} SignalArgs;

static void
signalArgsLoad (CompArgs *args,
		int      type,
		void     *value)
{
    SignalArgs *sa = (SignalArgs *) args;

    assert (sa->nValue > sa->i);

    switch (type) {
    case COMP_TYPE_BOOLEAN:
	*((CompBool *) value) = sa->value[sa->i].b;
	break;
    case COMP_TYPE_INT32:
	*((int32_t *) value) = sa->value[sa->i].i;
	break;
    case COMP_TYPE_DOUBLE:
	*((double *) value) = sa->value[sa->i].d;
	break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT:
	*((char **) value) = sa->value[sa->i].s;
	break;
    }

    sa->i++;
}

static void
signalArgsStore (CompArgs *args,
		 int      type,
		 void     *value)
{
    switch (type) {
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT: {
	if (*((char **) value))
	    free (*((char **) value));
    } break;
    }
}

static void
signalArgsError (CompArgs *args,
		 char     *error)
{
    if (error)
	free (error);
}

static void
signalInitArgs (SignalArgs   *sa,
		CompAnyValue *value,
		int	     nValue)
{
    sa->base.load  = signalArgsLoad;
    sa->base.store = signalArgsStore;
    sa->base.error = signalArgsError;

    sa->value  = value;
    sa->nValue = nValue;
    sa->i      = 0;
}

static void
invokeSignalHandler (CompObject        *object,
		     size_t	       offset,
		     MethodMarshalProc marshal,
		     const char        *path,
		     const char        *interface,
		     const char        *name,
		     const char        *signature,
		     CompAnyValue      *value,
		     int               nValue)
{
    if (marshal)
    {
	SignalArgs args;

	signalInitArgs (&args, value, nValue);

	(*marshal) (object,
		    *((void (**) (void)) (((char *) object->vTable) + offset)),
		    &args.base);
    }
    else
    {
	INVOKE_HANDLER_PROC (object,
			     offset,
			     SignalProc,
			     path,
			     interface,
			     name,
			     signature,
			     value,
			     nValue);
    }
}

static void
signal (CompObject   *object,
	const char   *path,
	const char   *interface,
	const char   *name,
	const char   *signature,
	CompAnyValue *value,
	int	     nValue)
{
    if (object->signalVec)
    {
	const CObjectInterface *cInterface = (const CObjectInterface *)
	    getObjectType ();
	int		       index =
	    C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
					offsetof (CompObjectVTable, signal));
	CompSignalHandler      *handler = object->signalVec[index];
	CompObjectVTableVec    save;

	while (handler)
	{
	    if (MATCHING_DETAILS (handler,
				  path,
				  interface,
				  name,
				  signature))
	    {
		if (handler->vTable)
		{
		    save.vTable = handler->object->vTable;
		    UNWRAP (handler, handler->object, vTable);

		    invokeSignalHandler (handler->object,
					 handler->offset,
					 handler->marshal,
					 path,
					 interface,
					 name,
					 signature,
					 value,
					 nValue);

		    WRAP (handler, handler->object, vTable, save.vTable);

		}
		else
		{
		    invokeSignalHandler (handler->object,
					 handler->offset,
					 handler->marshal,
					 path,
					 interface,
					 name,
					 signature,
					 value,
					 nValue);
		}
	    }

	    handler = handler->next;
	}
    }
}

static void
noopSignal (CompObject   *object,
	    const char   *path,
	    const char   *interface,
	    const char   *name,
	    const char   *signature,
	    CompAnyValue *value,
	    int	         nValue)
{
    FOR_BASE (object, (*object->vTable->signal) (object,
						 path,
						 interface,
						 name,
						 signature,
						 value,
						 nValue));
}

static CompBool
getBoolProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     CompBool   *value,
	     char	**error)
{
    if (error)
	*error = strdup ("No such boolean property");

    return FALSE;
}

static CompBool
noopGetBoolProp (CompObject *object,
		 const char *interface,
		 const char *name,
		 CompBool   *value,
		 char	    **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->getBool) (object,
							   interface,
							   name,
							   value,
							   error));

    return status;
}

static CompBool
setBoolProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     CompBool   value,
	     char       **error)
{
    if (error)
	*error = strdup ("No such boolean property");

    return FALSE;
}

static CompBool
noopSetBoolProp (CompObject *object,
		 const char *interface,
		 const char *name,
		 CompBool   value,
		 char	    **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->setBool) (object,
							   interface,
							   name,
							   value,
							   error));

    return status;
}

static void
boolPropChanged (CompObject *object,
		 const char *interface,
		 const char *name,
		 CompBool   value)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable, boolChanged));
    const CSignal	   *signal = &cInterface->signal[index];

    C_EMIT_SIGNAL_INT (object, BoolPropChangedProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface, name, value);
}

static void
noopBoolPropChanged (CompObject *object,
		     const char *interface,
		     const char *name,
		     CompBool   value)
{
    FOR_BASE (object, (*object->vTable->boolChanged) (object,
						      interface,
						      name,
						      value));
}

static CompBool
getIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    *value,
	     char	**error)
{
    if (error)
	*error = strdup ("No such int32 property");

    return FALSE;
}

static CompBool
noopGetIntProp (CompObject *object,
		const char *interface,
		const char *name,
		int32_t    *value,
		char	   **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->getInt) (object,
							  interface,
							  name,
							  value,
							  error));

    return status;
}

static CompBool
setIntProp (CompObject *object,
	    const char *interface,
	    const char *name,
	    int32_t    value,
	    char       **error)
{
    if (error)
	*error = strdup ("No such int32 property");

    return FALSE;
}

static CompBool
noopSetIntProp (CompObject *object,
		const char *interface,
		const char *name,
		int32_t    value,
		char	   **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->setInt) (object,
							  interface,
							  name,
							  value,
							  error));

    return status;
}

static void
intPropChanged (CompObject *object,
		const char *interface,
		const char *name,
		int32_t    value)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable, intChanged));
    const CSignal	   *signal = &cInterface->signal[index];

    C_EMIT_SIGNAL_INT (object, IntPropChangedProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface, name, value);
}

static void
noopIntPropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    int32_t    value)
{
    FOR_BASE (object, (*object->vTable->intChanged) (object,
						     interface,
						     name,
						     value));
}

static CompBool
getDoubleProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       double     *value,
	       char	  **error)
{
    if (error)
	*error = strdup ("No such double property");

    return FALSE;
}

static CompBool
noopGetDoubleProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   double     *value,
		   char	      **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->getDouble) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

static CompBool
setDoubleProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       double     value,
	       char       **error)
{
    if (error)
	*error = strdup ("No such double property");

    return FALSE;
}

static CompBool
noopSetDoubleProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   double     value,
		   char	      **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->setDouble) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

static void
doublePropChanged (CompObject *object,
		   const char *interface,
		   const char *name,
		   double     value)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable,
					      doubleChanged));
    const CSignal	   *signal = &cInterface->signal[index];

    C_EMIT_SIGNAL_INT (object, DoublePropChangedProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface, name, value);
}

static void
noopDoublePropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       double     value)
{
    FOR_BASE (object, (*object->vTable->doubleChanged) (object,
							interface,
							name,
							value));
}

static CompBool
getStringProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       char       **value,
	       char	  **error)
{
    if (error)
	*error = strdup ("No such string property");

    return FALSE;
}

static CompBool
noopGetStringProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   char       **value,
		   char	      **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->getString) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

static CompBool
setStringProp (CompObject *object,
	       const char *interface,
	       const char *name,
	       const char *value,
	       char       **error)
{
    if (error)
	*error = strdup ("No such string property");

    return FALSE;
}

static CompBool
noopSetStringProp (CompObject *object,
		   const char *interface,
		   const char *name,
		   const char *value,
		   char	      **error)
{
    CompBool status;

    FOR_BASE (object, status = (*object->vTable->setString) (object,
							     interface,
							     name,
							     value,
							     error));

    return status;
}

static void
stringPropChanged (CompObject *object,
		   const char *interface,
		   const char *name,
		   const char *value)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable,
					      stringChanged));
    const CSignal	   *signal = &cInterface->signal[index];

    C_EMIT_SIGNAL_INT (object, StringPropChangedProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface, name, value);
}

static void
noopStringPropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       const char *value)
{
    FOR_BASE (object, (*object->vTable->stringChanged) (object,
							interface,
							name,
							value));
}

static void
logImpl (CompObject *object,
	 const char *interface,
	 const char *message)
{
    const CObjectInterface *cInterface = (const CObjectInterface *)
	getObjectType ();
    int			   index =
	C_MEMBER_INDEX_FROM_OFFSET (cInterface->signal,
				    offsetof (CompObjectVTable, log));
    const CSignal	   *signal = &cInterface->signal[index];

    C_EMIT_SIGNAL_INT (object, LogProc, 0, object->signalVec,
		       cInterface->i.name, signal->name, signal->out,
		       index,
		       interface, message);
}

static void
noopLog (CompObject *object,
	 const char *interface,
	 const char *message)
{
    FOR_BASE (object, (*object->vTable->log) (object, interface, message));
}

static const CompObjectVTable objectVTable = {
    .finalize = finalize,

    .getProp = getProp,
    .setProp = setProp,

    .insertObject = insertObject,
    .removeObject = removeObject,
    .inserted     = inserted,
    .removed      = removed,

    .forEachInterface = forEachInterface,
    .forEachMethod    = forEachMethod,
    .forEachSignal    = forEachSignal,
    .forEachProp      = forEachProp,

    .addChild		= addChild,
    .removeChild	= removeChild,
    .forEachChildObject = forEachChildObject,
    .lookupChildObject  = lookupChildObject,

    .interfaceAdded   = interfaceAdded,
    .interfaceRemoved = interfaceRemoved,

    .connect      = connect,
    .connectAsync = connectAsync,
    .disconnect   = disconnect,
    .signal       = signal,

    .getBool     = getBoolProp,
    .setBool     = setBoolProp,
    .boolChanged = boolPropChanged,

    .getInt     = getIntProp,
    .setInt     = setIntProp,
    .intChanged = intPropChanged,

    .getDouble     = getDoubleProp,
    .setDouble     = setDoubleProp,
    .doubleChanged = doublePropChanged,

    .getString     = getStringProp,
    .setString     = setStringProp,
    .stringChanged = stringPropChanged,

    .log = logImpl
};

static const CompObjectVTable noopObjectVTable = {
    .finalize = noopFinalize,

    .insertObject = noopInsertObject,
    .removeObject = noopRemoveObject,
    .inserted     = noopInserted,
    .removed      = noopRemoved,

    .forEachInterface = noopForEachInterface,
    .forEachMethod    = noopForEachMethod,
    .forEachSignal    = noopForEachSignal,
    .forEachProp      = noopForEachProp,

    .interfaceAdded   = noopInterfaceAdded,
    .interfaceRemoved = noopInterfaceRemoved,

    .addChild		= noopAddChild,
    .removeChild	= noopRemoveChild,
    .forEachChildObject = noopForEachChildObject,
    .lookupChildObject  = noopLookupChildObject,

    .connect      = noopConnect,
    .connectAsync = noopConnectAsync,
    .disconnect   = noopDisconnect,
    .signal       = noopSignal,

    .getBool     = noopGetBoolProp,
    .setBool     = noopSetBoolProp,
    .boolChanged = noopBoolPropChanged,

    .getInt     = noopGetIntProp,
    .setInt     = noopSetIntProp,
    .intChanged = noopIntPropChanged,

    .getDouble     = noopGetDoubleProp,
    .setDouble     = noopSetDoubleProp,
    .doubleChanged = noopDoublePropChanged,

    .getString     = noopGetStringProp,
    .setString     = noopSetStringProp,
    .stringChanged = noopStringPropChanged,

    .log = noopLog
};

static const CMethod objectTypeMethod[] = {
    C_METHOD (getBool,    "ss",  "b", CompObjectVTable, marshal__SS_B_E),
    C_METHOD (setBool,    "ssb", "",  CompObjectVTable, marshal__SSB__E),
    C_METHOD (getInt,     "ss",  "i", CompObjectVTable, marshal__SS_I_E),
    C_METHOD (setInt,     "ssi", "",  CompObjectVTable, marshal__SSI__E),
    C_METHOD (getDouble,  "ss",  "d", CompObjectVTable, marshal__SS_D_E),
    C_METHOD (setDouble,  "ssd", "",  CompObjectVTable, marshal__SSD__E),
    C_METHOD (getString,  "ss",  "s", CompObjectVTable, marshal__SS_S_E),
    C_METHOD (setString,  "sss", "",  CompObjectVTable, marshal__SSS__E)
};

static const CSignal objectTypeSignal[] = {
    C_SIGNAL (signal, 0, CompObjectVTable),

    C_SIGNAL (inserted, "", CompObjectVTable),
    C_SIGNAL (removed,  "", CompObjectVTable),

    C_SIGNAL (interfaceAdded,   "s", CompObjectVTable),
    C_SIGNAL (interfaceRemoved, "s", CompObjectVTable),

    C_SIGNAL (boolChanged,   "ssb", CompObjectVTable),
    C_SIGNAL (intChanged,    "ssi", CompObjectVTable),
    C_SIGNAL (doubleChanged, "ssd", CompObjectVTable),
    C_SIGNAL (stringChanged, "sss", CompObjectVTable),

    C_SIGNAL (log, "ss", CompObjectVTable)
};

static const CObjectInterface objectType = {
    .i.name	     = COMPIZ_OBJECT_TYPE_NAME,
    .i.version	     = COMPIZ_OBJECT_VERSION,
    .i.vTable.impl   = &objectVTable,
    .i.vTable.noop   = &noopObjectVTable,
    .i.vTable.size   = sizeof (objectVTable),
    .i.instance.init = initObject,
    .i.instance.size = sizeof (CompObject),

    .method  = objectTypeMethod,
    .nMethod = N_ELEMENTS (objectTypeMethod),

    .signal  = objectTypeSignal,
    .nSignal = N_ELEMENTS (objectTypeSignal)
};

const CompObjectType *
getObjectType (void)
{
    return &objectType.i;
}

typedef struct _ForInterfaceContext {
    const char		  *interface;
    InterfaceCallBackProc proc;
    void		  *closure;
} ForInterfaceContext;

static CompBool
handleInterface (CompObject		   *object,
		 const CompObjectInterface *interface,
		 void			   *closure)
{
    ForInterfaceContext *pCtx = (ForInterfaceContext *) closure;

    if (!pCtx->interface || strcmp (interface->name, pCtx->interface) == 0)
	if (!(*pCtx->proc) (object, interface, pCtx->closure))
	    return FALSE;

    return TRUE;
}

CompBool
compForInterface (CompObject		*object,
		  const char		*interface,
		  InterfaceCallBackProc proc,
		  void			*closure)
{
    ForInterfaceContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return (*object->vTable->forEachInterface) (object,
						handleInterface,
						(void *) &ctx);
}

CompObject *
compLookupDescendant (CompObject *object,
		      const char *path)
{
    char name[128];
    int  i, j = 0;

    do {
	for (i = 0; path[j] != '\0' && path[j] != '/'; j++)
	    name[i++] = path[j];

	name[i] = '\0';

	object = (*object->vTable->lookupChildObject) (object, name);
    } while (object && path[j++]);

    return object;
}

CompObject *
compLookupDescendantVa (CompObject *object,
			const char *name,
			...)
{
    va_list args;

    va_start (args, name);

    do {
	object = (*object->vTable->lookupChildObject) (object, name);
	name = va_arg (args, const char *);
    } while (object && name);

    va_end (args);

    return object;
}

typedef struct _MethodCallContext {
    const char		   *interface;
    const char		   *name;
    const char		   *in;
    const char		   *out;
    const CompObjectVTable *vTable;
    int			   offset;
    MethodMarshalProc	   marshal;
} MethodCallContext;

static CompBool
checkMethod (CompObject	       *object,
	     const char	       *name,
	     const char	       *in,
	     const char	       *out,
	     size_t	       offset,
	     MethodMarshalProc marshal,
	     void	       *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;

    if (strcmp (name, pCtx->name))
	return TRUE;

    if (strcmp (in, pCtx->in))
	return TRUE;

    if (pCtx->out)
	if (strcmp (out, pCtx->out))
	    return TRUE;

    pCtx->offset  = offset;
    pCtx->marshal = marshal;

    return FALSE;
}

static CompBool
checkInterface (CompObject		  *object,
		const CompObjectInterface *interface,
		void		          *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;

    if (!pCtx->interface || strcmp (interface->name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object,
					       interface,
					       checkMethod,
					       closure))
	{
	    if (!interface->vTable.noop)
		pCtx->vTable = object->vTable;

	    return FALSE;
	}
    }

    return TRUE;
}

static void
invokeMethod (CompObject	     *object,
	      const CompObjectVTable *vTable,
	      int		     offset,
	      MethodMarshalProc	     marshal,
	      CompArgs		     *args)
{
    CompObjectVTableVec save = { object->vTable };
    CompObjectVTableVec interface = { vTable };

    UNWRAP (&interface, object, vTable);
    (*marshal) (object,
		*((void (**) (void)) (((char *) object->vTable) + offset)),
		args);
    WRAP (&interface, object, vTable, save.vTable);
}

CompBool
compInvokeMethodWithArgs (CompObject *object,
			  const char *interface,
			  const char *name,
			  const char *in,
			  const char *out,
			  CompArgs   *args)
{
    MethodCallContext ctx;

    ctx.interface = interface;
    ctx.name	  = name;
    ctx.in	  = in;
    ctx.out       = out;
    ctx.vTable    = object->vTable;

    if ((*object->vTable->forEachInterface) (object,
					     checkInterface,
					     (void *) &ctx))
	return FALSE;

    invokeMethod (object, ctx.vTable, ctx.offset, ctx.marshal, args);

    return TRUE;
}

typedef struct _VaArgs {
    CompArgs base;

    va_list    args;
    const char *in;
    int	       nIn;
    const char *out;
    int	       nOut;
    CompBool   status;
} VaArgs;

static void
vaArgsStep (VaArgs *va,
	    int	   type)
{
    switch (type) {
    case COMP_TYPE_BOOLEAN:
	va_arg (va->args, CompBool);
	break;
    case COMP_TYPE_INT32:
	va_arg (va->args, int32_t);
	break;
    case COMP_TYPE_DOUBLE:
	va_arg (va->args, double);
	break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT:
	va_arg (va->args, char *);
	break;
    }
}

static void
vaArgsLoad (CompArgs *args,
	    int      type,
	    void     *value)
{
    VaArgs *va = (VaArgs *) args;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
	*((CompBool *) value) = va_arg (va->args, CompBool);
	break;
    case COMP_TYPE_INT32:
	*((int32_t *) value) = va_arg (va->args, int32_t);
	break;
    case COMP_TYPE_DOUBLE:
	*((double *) value) = va_arg (va->args, double);
	break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT:
	*((char **) value) = va_arg (va->args, char *);
	break;
    }

    va->in++;
}

static void
vaArgsStore (CompArgs *args,
	     int      type,
	     void     *value)
{
    VaArgs *va = (VaArgs *) args;

    while (*va->in != COMP_TYPE_INVALID)
	vaArgsStep (va, *va->in++);

    switch (type) {
    case COMP_TYPE_BOOLEAN: {
	CompBool *boolOut;

	boolOut = va_arg (va->args, CompBool *);
	if (boolOut)
	    *boolOut = *((CompBool *) value);
    } break;
    case COMP_TYPE_INT32: {
	int32_t *intOut;

	intOut = va_arg (va->args, int32_t *);
	if (intOut)
	    *intOut = *((int32_t *) value);
    } break;
    case COMP_TYPE_DOUBLE: {
	double *doubleOut;

	doubleOut = va_arg (va->args, double *);
	if (doubleOut)
	    *doubleOut = *((double *) value);
    } break;
    case COMP_TYPE_STRING:
    case COMP_TYPE_OBJECT: {
	char **stringOut;

	stringOut = va_arg (va->args, char **);
	if (stringOut)
	{
	    *stringOut = *((char **) value);
	}
	else
	{
	    if (*((char **) value))
		free (*((char **) value));
	}
    } break;
    }

    va->out++;
}

static void
vaArgsError (CompArgs *args,
	     char     *error)
{
    VaArgs *va = (VaArgs *) args;
    char   **errorOut;

    while (*va->in != COMP_TYPE_INVALID)
	vaArgsStep (va, *va->in++);

    while (*va->out != COMP_TYPE_INVALID)
	vaArgsStep (va, *va->out++);

    errorOut = va_arg (va->args, char **);
    if (errorOut)
    {
	*errorOut = error;
    }
    else
    {
	if (error)
	    free (error);
    }

    va->status = FALSE;
}

static CompBool
vaInitArgs (VaArgs     *va,
	    const char *in,
	    const char *out,
	    va_list    args)
{
    va->base.load  = vaArgsLoad;
    va->base.store = vaArgsStore;
    va->base.error = vaArgsError;

    va->status = TRUE;
    va->args   = args;
    va->in     = in;
    va->out    = out;

    return TRUE;
}

static CompBool
vaFiniArgs (VaArgs *va)
{
    return va->status;
}

CompBool
compInvokeMethod (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *in,
		  const char *out,
		  ...)
{
    VaArgs   args;
    CompBool status;
    va_list  ap;

    va_start (ap, out);

    vaInitArgs (&args, in, out, ap);

    if (!compInvokeMethodWithArgs (object,
				   interface, name, in, out,
				   &args.base))
    {
	char *error;

	esprintf (&error, "Method \"%s\" with input signature \"%s\" and "
		  "output signature \"%s\" on interface "
		  "\"%s\" doesn't exist", name, in, out, interface);

	(*args.base.error) (&args.base, error);
    }

    status = vaFiniArgs (&args);

    va_end (ap);

    return status;
}

int
compGetObjectPath (CompObject *ancestor,
		   CompObject *descendant,
		   char	      *path,
		   int	      size)
{
    CompObject *node;
    int	       pathSize = 0;

    for (node = descendant; node; node = node->parent)
    {
	if (node == ancestor)
	    break;

	pathSize += strlen (node->name) + 1;
    }

    assert (node);

    if (!pathSize)
	pathSize = 1;

    if (pathSize <= size)
    {
	int offset = pathSize;

	path[offset - 1] = '\0';

	for (node = descendant; node; node = node->parent)
	{
	    int len;

	    if (node == ancestor)
		break;

	    if (offset-- < pathSize)
		path[offset] = '/';

	    len = strlen (node->name);
	    offset -= len;
	    memcpy (&path[offset], node->name, len);
	}
    }

    return pathSize;
}

int
compSerializeMethodCall (CompObject *observer,
			 CompObject *subject,
			 const char *interface,
			 const char *name,
			 const char *signature,
			 va_list    args,
			 void	    *data,
			 int	    size)
{
    char    *str, *ptr;
    int	    pathSize;
    int	    interfaceSize = strlen (interface) + 1;
    int	    nameSize = strlen (name) + 1;
    int	    signatureSize = strlen (signature) + 1;
    int	    valueSize = 0;
    int	    nValue = 0;
    int	    requiredSize, i;
    va_list ap;

    pathSize = compGetObjectPath (observer, subject, NULL, 0);

    va_copy (ap, args);

    for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
    {
	switch (signature[i]) {
	case COMP_TYPE_BOOLEAN:
	    va_arg (ap, CompBool);
	    break;
	case COMP_TYPE_INT32:
	    va_arg (ap, int32_t);
	    break;
	case COMP_TYPE_DOUBLE:
	    va_arg (ap, double);
	    break;
	case COMP_TYPE_STRING:
	case COMP_TYPE_OBJECT:
	    str = va_arg (ap, char *);
	    if (str)
		valueSize += strlen (str) + 1;
	    break;
	}

	nValue++;
    }

    va_end (ap);

    requiredSize = sizeof (CompSerializedMethodCallHeader) +
	pathSize + interfaceSize + nameSize + signatureSize +
	sizeof (CompAnyValue) * nValue + valueSize;

    if (requiredSize <= size)
    {
	CompSerializedMethodCallHeader *header = data;

	header->path = (char *) (header + 1);
	compGetObjectPath (observer, subject, header->path, pathSize);

	header->interface = (char *) (header->path + pathSize);
	strcpy (header->interface, interface);

	header->name = (char *) (header->interface + interfaceSize);
	strcpy (header->name, name);

	header->signature = (char *) (header->name + nameSize);
	strcpy (header->signature, signature);

	header->value  = (CompAnyValue *) (header->signature + signatureSize);
	header->nValue = nValue;

	ptr = (char *) (header->value + nValue);

	va_copy (ap, args);

	for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
	{
	    switch (signature[i]) {
	    case COMP_TYPE_BOOLEAN:
		header->value[i].b = va_arg (ap, CompBool);
		break;
	    case COMP_TYPE_INT32:
		header->value[i].i = va_arg (ap, int32_t);
		break;
	    case COMP_TYPE_DOUBLE:
		header->value[i].d = va_arg (ap, double);
		break;
	    case COMP_TYPE_STRING:
	    case COMP_TYPE_OBJECT:
		str = va_arg (ap, char *);
		if (str)
		{
		    header->value[i].s = ptr;
		    strcpy (header->value[i].s, str);
		    ptr += strlen (str) + 1;
		}
		else
		{
		    header->value[i].s = NULL;
		}
		break;
	    }
	}

	va_end (ap);
    }

    return requiredSize;
}

CompBool
compCheckEqualityOfValuesAndArgs (const char   *signature,
				  CompAnyValue *value,
				  ...)
{
    CompBool equal = TRUE;
    va_list  args;
    int      i;

    va_start (args, value);

    for (i = 0; equal && signature[i] != COMP_TYPE_INVALID; i++)
    {
	switch (signature[i]) {
	case COMP_TYPE_BOOLEAN:
	    equal = (!va_arg (args, CompBool) == !value[i].b);
	    break;
	case COMP_TYPE_INT32:
	    equal = (va_arg (args, int32_t) == value[i].i);
	    break;
	case COMP_TYPE_DOUBLE:
	    equal = (va_arg (args, double) == value[i].d);
	    break;
	case COMP_TYPE_STRING: {
	    char *s = va_arg (args, char *);

	    if (s && value[i].s)
		equal = (strcmp (s, value[i].s) == 0);
	    else
		equal = (s == value[i].s);
	} break;
	case COMP_TYPE_OBJECT: {
	    char *s = va_arg (args, char *);

	    /* complete XPath expression support could be added later */
	    if (strcmp (value[i].s, "//*") != 0)
	    {
		int j;

		for (j = 0; s[j]; j++)
		    if (value[i].s[j] != s[j])
			break;

		if (s[j])
		    equal = (value[i].s[j] == '*');
		else
		    equal = (value[i].s[j] == '\0');
	    }
	} break;
	}
    }

    va_end (args);

    return equal;
}

int
compConnect (CompObject		       *object,
	     const CompObjectInterface *interface,
	     size_t		       offset,
	     CompObject		       *descendant,
	     const CompObjectInterface *descendantInterface,
	     size_t		       descendantOffset,
	     const char		       *details,
	     ...)
{
    va_list args;
    int     index;

    va_start (args, details);

    index = (*object->vTable->connect) (object,
					interface,
					offset,
					descendant,
					descendantInterface,
					descendantOffset,
					details,
					args);

    va_end (args);

    return index;
}

void
compDisconnect (CompObject		  *object,
		const CompObjectInterface *interface,
		size_t			  offset,
		int			  index)
{
    (*object->vTable->disconnect) (object,
				   interface,
				   offset,
				   index);
}

const char *
compTranslateObjectPath (CompObject *ancestor,
			 CompObject *descendant,
			 const char *path)
{
    CompObject *node;
    int	       n = 0;

    for (node = descendant; node != ancestor; node = node->parent)
	n += strlen (node->name) + 1;

    if (n && path[n - 1] != '/')
	return path + (n - 1);

    return path + n;
}
