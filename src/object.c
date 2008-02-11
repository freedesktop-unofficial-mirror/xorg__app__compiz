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

static CSignal insertedSignal = C_SIGNAL (inserted, "", CompObjectVTable);
static CSignal removedSignal = C_SIGNAL (removed, "", CompObjectVTable);
static CSignal interfaceAddedSignal =
    C_SIGNAL (interfaceAdded, "s", CompObjectVTable);
static CSignal interfaceRemovedSignal =
    C_SIGNAL (interfaceRemoved, "s", CompObjectVTable);

static CSignal *objectTypeSignal[] = {
    &insertedSignal,
    &removedSignal,
    &interfaceAddedSignal,
    &interfaceRemovedSignal
};

static CSignal signalSignal = C_SIGNAL (signal, NULL, CompSignalVTable);

static CSignal *signalObjectSignal[] = {
    &signalSignal
};

static const CMethod propertiesObjectMethod[] = {
    C_METHOD (getBool,   "ss",  "b", CompPropertiesVTable, marshal__SS_B_E),
    C_METHOD (setBool,   "ssb", "",  CompPropertiesVTable, marshal__SSB__E),
    C_METHOD (getInt,    "ss",  "i", CompPropertiesVTable, marshal__SS_I_E),
    C_METHOD (setInt,    "ssi", "",  CompPropertiesVTable, marshal__SSI__E),
    C_METHOD (getDouble, "ss",  "d", CompPropertiesVTable, marshal__SS_D_E),
    C_METHOD (setDouble, "ssd", "",  CompPropertiesVTable, marshal__SSD__E),
    C_METHOD (getString, "ss",  "s", CompPropertiesVTable, marshal__SS_S_E),
    C_METHOD (setString, "sss", "",  CompPropertiesVTable, marshal__SSS__E)
};

static CSignal boolChangedSignal =
    C_SIGNAL (boolChanged,"ssb", CompPropertiesVTable);
static CSignal intChangedSignal =
    C_SIGNAL (intChanged,"ssi", CompPropertiesVTable);
static CSignal doubleChangedSignal =
    C_SIGNAL (doubleChanged,"ssd", CompPropertiesVTable);
static CSignal stringChangedSignal =
    C_SIGNAL (stringChanged,"sss", CompPropertiesVTable);

static CSignal *propertiesObjectSignal[] = {
    &boolChangedSignal,
    &intChangedSignal,
    &doubleChangedSignal,
    &stringChangedSignal
};

static const CMethod metadataObjectMethod[] = {
    C_METHOD (get, "s", "s", CompMetadataVTable, marshal__S_S_E)
};

static const CMethod versionObjectMethod[] = {
    C_METHOD (get, "s", "i", CompVersionVTable, marshal_I_S)
};

static CInterface objectInterface[] = {
    C_INTERFACE (object,     Type,   CompObjectVTable, _, _, X, _, _, _, _, _),
    C_INTERFACE (signal,     Object, CompObjectVTable, X, _, X, _, _, _, _, _),
    C_INTERFACE (properties, Object, CompObjectVTable, X, X, X, _, _, _, _, _),
    C_INTERFACE (metadata,   Object, CompObjectVTable, X, X, _, _, _, _, _, _),
    C_INTERFACE (version,    Object, CompObjectVTable, X, X, _, _, _, _, _, _)
};

static CompSignalHandler lastSignalVecEntry  = { 0 };
static CompSignalHandler emptySignalVecEntry = { 0 };

CompBool
compObjectInit (const CompObjectFactory      *factory,
		CompObject		     *object,
		const CompObjectInstantiator *instantiator)
{
    return (*instantiator->init) (instantiator, object, factory);
}

static const CompObjectInstantiator *
findObjectInstantiator (const CompObjectFactory *factory,
			const CompObjectType	*type)
{
    CompObjectInstantiatorNode *node;

    for (node = factory->instantiators; node; node = node->next)
	if (type == node->type)
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
	if (strcmp (name, node->type->name.name) == 0)
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

CompObjectInstantiatorNode *
compObjectInstantiatorNode (const CompObjectFactory *factory,
			    const char		    *name)
{
    return lookupObjectInstantiatorNode (factory, name);
}

void
compVTableInit (CompObjectVTable       *vTable,
		const CompObjectVTable *noopVTable,
		int		       size)
{
    char		      *dVTable = (char *) vTable;
    const char		      *sVTable = (const char *) noopVTable;
    unsigned int              i;
    static const unsigned int offset =
	offsetof (CompObjectVTable, removed) -
	offsetof (CompObjectVTable, inserted);

    for (i = 0; i < size; i += offset)
    {
	GetPropProc *dst = (GetPropProc *) (dVTable + i);
	GetPropProc *src = (GetPropProc *) (sVTable + i);

	if (*src && !*dst)
	    *dst = *src;
    }
}

CompBool
compFactoryRegisterType (CompObjectFactory    *factory,
			 const char	      *interface,
			 const CompObjectType *type)
{
    const CompObjectInstantiatorNode *base = NULL;
    CompObjectInstantiatorNode	     *node;
    const CompFactory		     *master;
    const CompObjectFactory	     *f;
    CompObjectPrivatesNode	     *pNode;
    int				     interfaceSize = 0;

    if (type->name.base)
    {
	base = lookupObjectInstantiatorNode (factory, type->name.base);
	if (!base)
	    return FALSE;
    }

    if (interface)
	interfaceSize = strlen (interface) + 1;

    node = malloc (sizeof (CompObjectInstantiatorNode) + type->vTable.size +
		   interfaceSize);
    if (!node)
	return FALSE;

    node->base.base	     = &base->base;
    node->base.vTable        = NULL;
    node->base.init	     = type->instance.init;
    node->next		     = factory->instantiators;
    node->instantiator	     = &node->base;
    node->type		     = type;
    node->privates.len	     = 0;
    node->privates.sizes     = NULL;
    node->privates.totalSize = 0;
    node->interface	     = NULL;

    if (type->vTable.size)
    {
	const CompObjectInstantiatorNode *n;
	const CompObjectInstantiator     *p;

	node->base.vTable = memcpy (node + 1,
				    type->vTable.impl,
				    type->vTable.size);

	for (p = &node->base; p; p = p->base)
	{
	    n = (const CompObjectInstantiatorNode *) p;

	    if (n->type->vTable.noop)
		compVTableInit (node->base.vTable,
				n->type->vTable.noop,
				n->type->vTable.size);
	}
    }

    if (interfaceSize)
	node->interface	= strcpy (((char *) (node + 1)) + type->vTable.size,
				  interface);

    for (f = factory; f->master; f = f->master);
    master = (const CompFactory *) f;

    for (pNode = master->privates; pNode; pNode = pNode->next)
    {
	if (strcmp (pNode->name, type->name.name) == 0)
	{
	    node->privates = pNode->privates;
	    break;
	}
    }

    factory->instantiators = node;

    return TRUE;
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

    object->id = ~0; /* XXX: remove id asap */

    return TRUE;
}

static void
finalize (CompObject *object)
{
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
    static const CMetadata template = {
	.interface  = objectInterface,
	.nInterface = N_ELEMENTS (objectInterface),
	.version    = COMPIZ_OBJECT_VERSION
    };

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
    case COMP_PROP_C_METADATA:
	*((CMetadata *) value) = template;
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
    object->parent = parent;
    object->name   = name;
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
    int i;

    C_EMIT_SIGNAL_INT (object, InsertedProc, 0, object->signalVec,
		       &insertedSignal);

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	(*object->vTable->interfaceAdded) (object, objectInterface[i].name);
}

static void
noopInserted (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->inserted) (object));
}

static void
removed (CompObject *object)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	(*object->vTable->interfaceRemoved) (object, objectInterface[i].name);

    C_EMIT_SIGNAL_INT (object, RemovedProc, 0, object->signalVec,
		       &removedSignal);
}

static void
noopRemoved (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->removed) (object));
}

static CompBool
forEachInterface (CompObject	         *object,
		  InterfaceCallBackProc proc,
		  void		         *closure)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	if (!(*proc) (object,
		      objectInterface[i].name,
		      objectInterface[i].offset,
		      objectInterface[i].type,
		      closure))
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
forEachMethod (CompObject	   *object,
	       const char	   *interface,
	       MethodCallBackProc proc,
	       void	           *closure)
{
    int        i, j;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, objectInterface[i].name))
		continue;

	for (j = 0; j < objectInterface[i].nMethod; j++)
	    if (!(*proc) (object,
			  objectInterface[i].method[j].name,
			  objectInterface[i].method[j].in,
			  objectInterface[i].method[j].out,
			  objectInterface[i].method[j].offset,
			  objectInterface[i].method[j].marshal,
			  closure))
		return FALSE;
    }

    return TRUE;
}

static CompBool
noopForEachMethod (CompObject	      *object,
		   const char	      *interface,
		   MethodCallBackProc proc,
		   void		      *closure)
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
forEachSignal (CompObject	   *object,
	       const char	   *interface,
	       SignalCallBackProc proc,
	       void		   *closure)
{
    int        i, j;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, objectInterface[i].name))
		continue;

	for (j = 0; j < objectInterface[i].nSignal; j++)
	    if (objectInterface[i].signal[j]->out)
		if (!(*proc) (object,
			      objectInterface[i].signal[j]->name,
			      objectInterface[i].signal[j]->out,
			      objectInterface[i].signal[j]->offset,
			      closure))
		    return FALSE;
    }

    return TRUE;
}

static CompBool
noopForEachSignal (CompObject	      *object,
		   const char	      *interface,
		   SignalCallBackProc proc,
		   void		      *closure)
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
forEachProp (CompObject       *object,
	     const char       *interface,
	     PropCallBackProc proc,
	     void	       *closure)
{
    int i, j;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, objectInterface[i].name))
		continue;

	for (j = 0; j < objectInterface[i].nBoolProp; j++)
	    if (!(*proc) (object, objectInterface[i].boolProp[j].base.name,
			  COMP_TYPE_BOOLEAN, closure))
		return FALSE;

	for (j = 0; j < objectInterface[i].nIntProp; j++)
	    if (!(*proc) (object, objectInterface[i].intProp[j].base.name,
			  COMP_TYPE_INT32, closure))
		return FALSE;

	for (j = 0; j < objectInterface[i].nDoubleProp; j++)
	    if (!(*proc) (object, objectInterface[i].doubleProp[j].base.name,
			  COMP_TYPE_DOUBLE, closure))
		return FALSE;

	for (j = 0; j < objectInterface[i].nStringProp; j++)
	    if (!(*proc) (object, objectInterface[i].stringProp[j].base.name,
			  COMP_TYPE_STRING, closure))
		return FALSE;
    }

    return TRUE;
}

static CompBool
noopForEachProp (CompObject	  *object,
		 const char	  *interface,
		 PropCallBackProc proc,
		 void		  *closure)
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
    C_EMIT_SIGNAL_INT (object, InterfaceAddedProc, 0, object->signalVec,
		       &interfaceAddedSignal,
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
    CompObject *node;

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
		       &interfaceRemovedSignal,
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
    return FALSE;
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

static void
removeChild (CompObject *object,
	     CompObject *child)
{
}

static void
noopRemoveChild (CompObject *object,
		 CompObject *child)
{
    FOR_BASE (object, (*object->vTable->removeChild) (object, child));
}

static CompBool
forEachChildObject (CompObject		    *object,
		    ChildObjectCallBackProc proc,
		    void		    *closure)
{
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

static int
getSignalVecSize (const CInterface *interface,
		  int		   nInterface)
{
    int	i, size = 0;

    for (i = 0; i < nInterface; i++)
	size += interface[i].nSignal;

    return size;
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
	if (offset)
	    start = getSignalVecSize (objectInterface,
				      N_ELEMENTS (objectInterface));
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
connect (CompObject *object,
	 const char *interface,
	 size_t     offset,
	 CompObject *descendant,
	 const char *descendantInterface,
	 size_t     descendantOffset,
	 const char *details,
	 va_list    args)
{
    int id;

    if (handleConnect (object,
		       objectInterface, N_ELEMENTS (objectInterface), NULL,
		       interface,
		       offset,
		       descendant,
		       descendantInterface,
		       descendantOffset,
		       details,
		       args,
		       &id))
	return id;

    return -1;
}

static int
noopConnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     CompObject *descendant,
	     const char *descendantInterface,
	     size_t     descendantOffset,
	     const char *details,
	     va_list	args)
{
    int index;

    FOR_BASE (object,
	      index = (*object->vTable->signal.connect) (object,
							 interface,
							 offset,
							 descendant,
							 descendantInterface,
							 descendantOffset,
							 details,
							 args));

    return index;
}

static void
disconnect (CompObject *object,
	    const char *interface,
	    size_t     offset,
	    int	       id)
{
    handleDisconnect (object,
		      objectInterface, N_ELEMENTS (objectInterface), NULL,
		      interface,
		      offset,
		      id);
}

static void
noopDisconnect (CompObject *object,
		const char *interface,
		size_t     offset,
		int	   index)
{
    FOR_BASE (object, (*object->vTable->signal.disconnect) (object,
							    interface,
							    offset,
							    index));
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
    C_EMIT_SIGNAL_INT (object, SignalProc, 0, object->signalVec, &signalSignal,
		       path, interface, name, signature, value, nValue);
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
    FOR_BASE (object, (*object->vTable->signal.signal) (object,
							path,
							interface,
							name,
							signature,
							value,
							nValue));
}

static int
getVersion (CompObject *object,
	    const char *interface)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	if (strcmp (interface, objectInterface[i].name) == 0)
	    return COMPIZ_OBJECT_VERSION;

    return 0;
}

static int
noopGetVersion (CompObject *object,
		const char *interface)
{
    int result;

    FOR_BASE (object,
	      result = (*object->vTable->version.get) (object, interface));

    return result;
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.getBool) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.setBool) (object,
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
    C_EMIT_SIGNAL_INT (object, BoolPropChangedProc, 0, object->signalVec,
		       &boolChangedSignal,
		       interface, name, value);
}

static void
noopBoolPropChanged (CompObject *object,
		     const char *interface,
		     const char *name,
		     CompBool   value)
{
    FOR_BASE (object, (*object->vTable->properties.boolChanged) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.getInt) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.setInt) (object,
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
    C_EMIT_SIGNAL_INT (object, IntPropChangedProc, 0, object->signalVec,
		       &intChangedSignal,
		       interface, name, value);
}

static void
noopIntPropChanged (CompObject *object,
		    const char *interface,
		    const char *name,
		    int32_t    value)
{
    FOR_BASE (object, (*object->vTable->properties.intChanged) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.getDouble) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.setDouble) (object,
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
    C_EMIT_SIGNAL_INT (object, DoublePropChangedProc, 0, object->signalVec,
		       &doubleChangedSignal,
		       interface, name, value);
}

static void
noopDoublePropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       double     value)
{
    FOR_BASE (object, (*object->vTable->properties.doubleChanged) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.getString) (object,
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

    FOR_BASE (object,
	      status = (*object->vTable->properties.setString) (object,
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
    C_EMIT_SIGNAL_INT (object, StringPropChangedProc, 0, object->signalVec,
		       &stringChangedSignal,
		       interface, name, value);
}

static void
noopStringPropChanged (CompObject *object,
		       const char *interface,
		       const char *name,
		       const char *value)
{
    FOR_BASE (object, (*object->vTable->properties.stringChanged) (object,
								   interface,
								   name,
								   value));
}

static CompBool
getMetadata (CompObject *object,
	     const char *interface,
	     char	**data,
	     char	**error)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	if (strcmp (interface, objectInterface[i].name) == 0)
	    return handleGetMetadata (object, interface, data, error);

    esprintf (error, "No \"%s\" interface", interface);
    return FALSE;
}

static CompBool
noopGetMetadata (CompObject *object,
		 const char *interface,
		 char       **data,
		 char	    **error)
{
    CompBool status;

    FOR_BASE (object,
	      status = (*object->vTable->metadata.get) (object,
							interface,
							data,
							error));

    return status;
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

    .interfaceAdded   = interfaceAdded,
    .interfaceRemoved = interfaceRemoved,

    .signal.connect    = connect,
    .signal.disconnect = disconnect,
    .signal.signal     = signal,

    .version.get = getVersion,

    .properties.getBool     = getBoolProp,
    .properties.setBool     = setBoolProp,
    .properties.boolChanged = boolPropChanged,

    .properties.getInt     = getIntProp,
    .properties.setInt     = setIntProp,
    .properties.intChanged = intPropChanged,

    .properties.getDouble     = getDoubleProp,
    .properties.setDouble     = setDoubleProp,
    .properties.doubleChanged = doublePropChanged,

    .properties.getString     = getStringProp,
    .properties.setString     = setStringProp,
    .properties.stringChanged = stringPropChanged,

    .metadata.get = getMetadata
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

    .signal.connect    = noopConnect,
    .signal.disconnect = noopDisconnect,
    .signal.signal     = noopSignal,

    .version.get = noopGetVersion,

    .properties.getBool     = noopGetBoolProp,
    .properties.setBool     = noopSetBoolProp,
    .properties.boolChanged = noopBoolPropChanged,

    .properties.getInt     = noopGetIntProp,
    .properties.setInt     = noopSetIntProp,
    .properties.intChanged = noopIntPropChanged,

    .properties.getDouble     = noopGetDoubleProp,
    .properties.setDouble     = noopSetDoubleProp,
    .properties.doubleChanged = noopDoublePropChanged,

    .properties.getString     = noopGetStringProp,
    .properties.setString     = noopSetStringProp,
    .properties.stringChanged = noopStringPropChanged,

    .metadata.get = noopGetMetadata
};

static const CompObjectType objectType = {
    .name.name     = OBJECT_TYPE_NAME,
    .vTable.impl   = &objectVTable,
    .vTable.noop   = &noopObjectVTable,
    .vTable.size   = sizeof (objectVTable),
    .instance.init = initObject,
    .instance.size = sizeof (CompObject)
};

const CompObjectType *
getObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	int i, j, index = 0;

	for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	{
	    for (j = 0; j < objectInterface[i].nSignal; j++)
	    {
		objectInterface[i].signal[j]->index     = index++;
		objectInterface[i].signal[j]->interface =
		    objectInterface[i].name;
	    }

	    objectInterface[i].type = &objectType;
	}

	init = TRUE;
    }

    return &objectType;
}

typedef struct _ForInterfaceContext {
    const char		  *interface;
    InterfaceCallBackProc proc;
    void		  *closure;
} ForInterfaceContext;

static CompBool
handleInterface (CompObject	      *object,
		 const char	      *name,
		 size_t		      offset,
		 const CompObjectType *type,
		 void		      *closure)
{
    ForInterfaceContext *pCtx = (ForInterfaceContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
	if (!(*pCtx->proc) (object, name, offset, type, pCtx->closure))
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

static CompBool
getInterfaceVersion (CompObject		  *object,
		     const char	          *name,
		     size_t		  offset,
		     const CompObjectType *type,
		     void		  *closure)
{
    int *version = (int *) closure;

    *version = (object->vTable->version.get) (object, name);

    return FALSE;
}

typedef struct _LookupObjectContext {
    const char *path;
    int	       size;
    CompObject *object;
} LookupObjectContext;

static CompBool
checkChildObject (CompObject *object,
		  void	     *closure)
{
    LookupObjectContext *pCtx = (LookupObjectContext *) closure;
    int			i;

    if (strncmp (object->name, pCtx->path, pCtx->size))
	return TRUE;

    if (object->name[pCtx->size] != '\0')
	return TRUE;

    pCtx->path += pCtx->size;

    if (*pCtx->path++)
    {
	for (i = 0; pCtx->path[i] != '\0' && pCtx->path[i] != '/'; i++);

	pCtx->size = i;

	return (*object->vTable->forEachChildObject) (object,
						      checkChildObject,
						      closure);
    }

    pCtx->object = object;

    return FALSE;
}

CompObject *
compLookupObject (CompObject *root,
		  const char *path)
{
    LookupObjectContext ctx;
    int			i;

    for (i = 0; path[i] != '\0' && path[i] != '/'; i++);

    ctx.path = path;
    ctx.size = i;

    if ((*root->vTable->forEachChildObject) (root,
					     checkChildObject,
					     (void *) &ctx))
	return NULL;

    return ctx.object;
}

CompBool
compObjectCheckVersion (CompObject *object,
			const char *interface,
			int	   version)
{
    int v;

    if (!compForInterface (object,
			   interface,
			   getInterfaceVersion,
			   (void *) &v))
	if (v == version)
	    return TRUE;

    return TRUE;
}

static CompBool
interfaceIsPartOfType (CompObject	    *object,
		       const char	    *name,
		       size_t		    offset,
		       const CompObjectType *type,
		       void		    *closure)
{
    CompBool *partOfType = (CompBool *) closure;

    *partOfType = (type) ? TRUE : FALSE;

    return FALSE;
}

CompBool
compObjectInterfaceIsPartOfType (CompObject *object,
				 const char *interface)
{
    CompBool partOfType;

    if (compForInterface (object,
			  interface,
			  interfaceIsPartOfType,
			  &partOfType))
	return FALSE;

    return partOfType;
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
checkInterface (CompObject	     *object,
		const char	     *name,
		size_t		     offset,
		const CompObjectType *type,
		void		     *closure)
{
    MethodCallContext *pCtx = (MethodCallContext *) closure;

    if (!pCtx->interface || strcmp (name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object, name, checkMethod,
					       closure))
	{

	    /* need vTable if interface is not part of object type */
	    if (!type)
		pCtx->vTable = object->vTable;

	    /* add interface vTable offset to method offset set by
	       checkMethod function */
	    pCtx->offset += offset;

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
compSerializeMethodCall (CompObject *observer,
			 CompObject *subject,
			 const char *interface,
			 const char *name,
			 const char *signature,
			 va_list    args,
			 void	    *data,
			 int	    size)
{
    CompObject *node;
    char       *str, *ptr;
    int	       pathSize = 0;
    int	       interfaceSize = strlen (interface) + 1;
    int	       nameSize = strlen (name) + 1;
    int	       signatureSize = strlen (signature) + 1;
    int	       valueSize = 0;
    int	       nValue = 0;
    int	       requiredSize, i;
    va_list    ap;

    for (node = subject; node; node = node->parent)
    {
	if (node == observer)
	    break;

	pathSize += strlen (node->name) + 1;
    }

    assert (node);

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
	int			       offset;

	header->path = (char *) (header + 1);

	offset = pathSize;
	header->path[offset - 1] = '\0';

	for (node = subject; node; node = node->parent)
	{
	    int len;

	    if (node == observer)
		break;

	    if (offset-- < pathSize)
		header->path[offset] = '/';

	    len = strlen (node->name);
	    offset -= len;
	    memcpy (&header->path[offset], node->name, len);
	}

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
compConnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     CompObject *descendant,
	     const char *descendantInterface,
	     size_t     descendantOffset,
	     const char *details,
	     ...)
{
    va_list args;
    int     index;

    va_start (args, details);

    index = (*object->vTable->signal.connect) (object,
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
compDisconnect (CompObject *object,
		const char *interface,
		size_t     offset,
		int	   index)
{
    (*object->vTable->signal.disconnect) (object,
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
