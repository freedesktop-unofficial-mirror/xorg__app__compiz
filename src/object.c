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
#include <compiz/core.h>

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

static CompBool
allocateObjectPrivates (CompPrivate		 **pPrivates,
			const CompObjectPrivates *size)
{
    CompPrivate *privates;

    privates = allocatePrivates (size->len, size->sizes, size->totalSize);
    if (!privates)
	return FALSE;

    *pPrivates = privates;

    return TRUE;
}

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

static void
vTableInit (CompObjectVTable	   *vTable,
	    const CompObjectVTable *noopVTable,
	    int			   size)
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
    node->base.init	     = type->init;
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
		vTableInit (node->base.vTable,
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
noopInsertObject (CompObject *object,
		  CompObject *parent,
		  const char *name)
{
    FOR_BASE (object, (*object->vTable->insertObject) (object, parent, name));
}

static void
noopRemoveObject (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->removeObject) (object));
}

static void
noopInserted (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->inserted) (object));
}

static void
noopRemoved (CompObject *object)
{
    FOR_BASE (object, (*object->vTable->removed) (object));
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
noopInterfaceAdded (CompObject *object,
		    const char *interface)
{
    FOR_BASE (object, (*object->vTable->interfaceAdded) (object, interface));
}

static void
noopInterfaceRemoved (CompObject *object,
		      const char *interface)
{
    FOR_BASE (object, (*object->vTable->interfaceRemoved) (object, interface));
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
noopRemoveChild (CompObject *object,
		 CompObject *child)
{
    FOR_BASE (object, (*object->vTable->removeChild) (object, child));
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
noopGetVersion (CompObject *object,
		const char *interface)
{
    int result;

    FOR_BASE (object,
	      result = (*object->vTable->version.get) (object, interface));

    return result;
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

#define CHILD(data, child)				   \
    ((CompObject *) (((char *) (data)) + (child)->offset))

void
cInsertObject (CompObject *object,
	       CompObject *parent,
	       const char *name)
{
    CompObject *child;
    CMetadata  m;
    char       *data;
    int        i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    FOR_BASE (object, (*object->vTable->insertObject) (object, parent, name));

    for (i = 0; i < m.nInterface; i++)
    {
	for (j = 0; j < m.interface[i].nChild; j++)
	{
	    if (m.interface[i].child[j].type)
	    {
		child = CHILD (data, &m.interface[i].child[j]);
		(*child->vTable->insertObject) (child, object,
						m.interface[i].child[j].name);
	    }
	}
    }
}

void
cRemoveObject (CompObject *object)
{
    CompObject *child;
    CMetadata  m;
    char       *data;
    int        i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    for (i = 0; i < m.nInterface; i++)
    {
	for (j = 0; j < m.interface[i].nChild; j++)
	{
	    if (m.interface[i].child[j].type)
	    {
		child = CHILD (data, &m.interface[i].child[j]);
		(*child->vTable->removeObject) (child);
	    }
	}
    }

    FOR_BASE (object, (*object->vTable->removeObject) (object));
}

#define PROP_VALUE(data, prop, type)			  \
    (*((type *) (((char *) data) + (prop)->base.offset)))

void
cInserted (CompObject *object)
{
    CompObject *child;
    CMetadata  m;
    char       *data;
    int        i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    FOR_BASE (object, (*object->vTable->inserted) (object));

    for (i = 0; i < m.nInterface; i++)
    {
	(*object->vTable->interfaceAdded) (object, m.interface[i].name);

	for (j = 0; j < m.interface[i].nBoolProp; j++)
	{
	    CBoolProp *prop = &m.interface[i].boolProp[j];

	    (*object->vTable->properties.boolChanged) (object,
						       m.interface[i].name,
						       prop->base.name,
						       PROP_VALUE (data, prop,
								   CompBool));
	}

	for (j = 0; j < m.interface[i].nIntProp; j++)
	{
	    CIntProp *prop = &m.interface[i].intProp[j];

	    (*object->vTable->properties.intChanged) (object,
						      m.interface[i].name,
						      prop->base.name,
						      PROP_VALUE (data, prop,
								  int32_t));
	}

	for (j = 0; j < m.interface[i].nDoubleProp; j++)
	{
	    CDoubleProp *prop = &m.interface[i].doubleProp[j];

	    (*object->vTable->properties.doubleChanged) (object,
							 m.interface[i].name,
							 prop->base.name,
							 PROP_VALUE (data,
								     prop,
								     double));
	}

	for (j = 0; j < m.interface[i].nStringProp; j++)
	{
	    CStringProp *prop = &m.interface[i].stringProp[j];

	    (*object->vTable->properties.stringChanged) (object,
							 m.interface[i].name,
							 prop->base.name,
							 PROP_VALUE (data,
								     prop,
								     char *));
	}

	for (j = 0; j < m.interface[i].nChild; j++)
	{
	    if (m.interface[i].child[j].type)
	    {
		child = CHILD (data, &m.interface[i].child[j]);
		(*child->vTable->inserted) (child);
	    }
	}
    }
}

void
cRemoved (CompObject *object)
{
    CompObject *child;
    CMetadata  m;
    char       *data;
    int        i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    for (i = 0; i < m.nInterface; i++)
    {
	for (j = 0; j < m.interface[i].nChild; j++)
	{
	    if (m.interface[i].child[j].type)
	    {
		child = CHILD (data, &m.interface[i].child[j]);
		(*child->vTable->removed) (child);
	    }
	}

	(*object->vTable->interfaceRemoved) (object, m.interface[i].name);
    }

    FOR_BASE (object, (*object->vTable->removed) (object));
}

CompBool
cForEachInterface (CompObject	         *object,
		   InterfaceCallBackProc proc,
		   void		         *closure)
{
    CompBool  status;
    CMetadata m;
    int       i;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!(*proc) (object,
		      m.interface[i].name,
		      m.interface[i].offset,
		      m.interface[i].type,
		      closure))
	    return FALSE;

    FOR_BASE (object,
	      status = (*object->vTable->forEachInterface) (object,
							    proc,
							    closure));

    return status;
}

CompBool
cForEachChildObject (CompObject		     *object,
		     ChildObjectCallBackProc proc,
		     void		     *closure)
{
    CompBool   status;
    CMetadata  m;
    char       *data;
    int        i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    for (i = 0; i < m.nInterface; i++)
	for (j = 0; j < m.interface[i].nChild; j++)
	    if (!(*proc) (CHILD (data, &m.interface[i].child[j]), closure))
		return FALSE;

    FOR_BASE (object,
	      status = (*object->vTable->forEachChildObject) (object,
							      proc,
							      closure));

    return status;
}

CompBool
cForEachMethod (CompObject	   *object,
		const char	   *interface,
		MethodCallBackProc proc,
		void	           *closure)
{
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, m.interface[i].name))
		continue;

	for (j = 0; j < m.interface[i].nMethod; j++)
	    if (!(*proc) (object,
			  m.interface[i].method[j].name,
			  m.interface[i].method[j].in,
			  m.interface[i].method[j].out,
			  m.interface[i].method[j].offset,
			  m.interface[i].method[j].marshal,
			  closure))
		return FALSE;
    }

    FOR_BASE (object,
	      status = (*object->vTable->forEachMethod) (object,
							 interface,
							 proc,
							 closure));

    return status;
}

CompBool
cForEachSignal (CompObject	   *object,
		const char	   *interface,
		SignalCallBackProc proc,
		void		   *closure)
{
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, m.interface[i].name))
		continue;

	for (j = 0; j < m.interface[i].nSignal; j++)
	    if (m.interface[i].signal[j]->out)
		if (!(*proc) (object,
			      m.interface[i].signal[j]->name,
			      m.interface[i].signal[j]->out,
			      m.interface[i].signal[j]->offset,
			      closure))
		    return FALSE;
    }

    FOR_BASE (object,
	      status = (*object->vTable->forEachSignal) (object,
							 interface,
							 proc,
							 closure));

    return status;
}

static CompBool
handleForEachBoolProp (CompObject	*object,
		       const CBoolProp  *prop,
		       int		nProp,
		       PropCallBackProc	proc,
		       void		*closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_BOOLEAN, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachIntProp (CompObject       *object,
		      const CIntProp   *prop,
		      int	       nProp,
		      PropCallBackProc proc,
		      void	       *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_INT32, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachDoubleProp (CompObject	   *object,
			 const CDoubleProp *prop,
			 int		   nProp,
			 PropCallBackProc  proc,
			 void		   *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_DOUBLE, closure))
	    return FALSE;

    return TRUE;
}

static CompBool
handleForEachStringProp (CompObject	   *object,
			 const CStringProp *prop,
			 int		   nProp,
			 PropCallBackProc  proc,
			 void		   *closure)
{
    int i;

    for (i = 0; i < nProp; i++)
	if (!(*proc) (object, prop[i].base.name, COMP_TYPE_STRING, closure))
	    return FALSE;

    return TRUE;
}

CompBool
cForEachProp (CompObject       *object,
	      const char       *interface,
	      PropCallBackProc proc,
	      void	       *closure)
{
    CompBool  status;
    CMetadata m;
    int       i;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, m.interface[i].name))
		continue;

	if (!handleForEachBoolProp (object,
				    m.interface[i].boolProp,
				    m.interface[i].nBoolProp,
				    proc, closure))
	    return FALSE;

	if (!handleForEachIntProp (object,
				   m.interface[i].intProp,
				   m.interface[i].nIntProp,
				   proc, closure))
	    return FALSE;

	if (!handleForEachDoubleProp (object,
				      m.interface[i].doubleProp,
				      m.interface[i].nDoubleProp,
				      proc, closure))
	    return FALSE;

	if (!handleForEachStringProp (object,
				      m.interface[i].stringProp,
				      m.interface[i].nStringProp,
				      proc, closure))
	    return FALSE;
    }

    FOR_BASE (object,
	      status = (*object->vTable->forEachProp) (object,
						       interface,
						       proc,
						       closure));

    return status;
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

int
cGetVersion (CompObject *object,
	     const char *interface)
{
    CMetadata  m;
    int        version, i;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (strcmp (interface, m.interface[i].name) == 0)
	    return m.version;

    FOR_BASE (object,
	      version = (*object->vTable->version.get) (object, interface));

    return version;
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
removeObject (CompObject *object)
{
    object->parent = NULL;
    object->name   = NULL;
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
removed (CompObject *object)
{
    int i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
	(*object->vTable->interfaceRemoved) (object, objectInterface[i].name);

    C_EMIT_SIGNAL_INT (object, RemovedProc, 0, object->signalVec,
		       &removedSignal);
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
addChild (CompObject *object,
	  CompObject *child,
	  const char *name)
{
    return FALSE;
}

static void
removeChild (CompObject *object,
	     CompObject *child)
{
}

static CompBool
forEachChildObject (CompObject		     *object,
		    ChildObjectCallBackProc proc,
		    void		     *closure)
{
    return TRUE;
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
forEachProp (CompObject       *object,
	     const char       *interface,
	     PropCallBackProc proc,
	     void	       *closure)
{
    int        i;

    for (i = 0; i < N_ELEMENTS (objectInterface); i++)
    {
	if (interface)
	    if (*interface && strcmp (interface, objectInterface[i].name))
		continue;

	if (!handleForEachBoolProp (object,
				    objectInterface[i].boolProp,
				    objectInterface[i].nBoolProp,
				    proc, closure))
	    return FALSE;

	if (!handleForEachIntProp (object,
				   objectInterface[i].intProp,
				   objectInterface[i].nIntProp,
				   proc, closure))
	    return FALSE;

	if (!handleForEachDoubleProp (object,
				      objectInterface[i].doubleProp,
				      objectInterface[i].nDoubleProp,
				      proc, closure))
	    return FALSE;

	if (!handleForEachStringProp (object,
				      objectInterface[i].stringProp,
				      objectInterface[i].nStringProp,
				      proc, closure))
	    return FALSE;
    }

    return TRUE;
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

static void
interfaceAdded (CompObject *object,
		const char *interface)
{
    C_EMIT_SIGNAL_INT (object, InterfaceAddedProc, 0, object->signalVec,
		       &interfaceAddedSignal,
		       interface);
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
			if (strcmp (handler->header->interface, interface) == 0)
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

static CompBool
signalIndex (const char	      *name,
	     const CInterface *interface,
	     int	      nInterface,
	     size_t	      offset,
	     int	      *index,
	     const char	      **signature)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	if (strcmp (name, interface[i].name) == 0)
	{
	    for (j = 0; j < interface[i].nSignal; j++)
	    {
		if (offset == interface[i].signal[j]->offset)
		{
		    if (signature)
			*signature = interface[i].signal[j]->out;

		    *index = interface[i].signal[j]->index;

		    return TRUE;
		}
	    }

	    *index = -1;

	    return TRUE;
	}
    }

    return FALSE;
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

typedef struct _HandleConnectContext {
    const char		   *interface;
    size_t		   offset;
    char		   *in;
    CompBool		   out;
    const CompObjectVTable *vTable;
    MethodMarshalProc	   marshal;
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
    {
	pCtx->out = TRUE;
	return FALSE;
    }

    pCtx->out     = FALSE;
    pCtx->in      = strdup (in);
    pCtx->offset  = offset;
    pCtx->marshal = marshal;

    return FALSE;
}

static CompBool
connectInterface (CompObject	       *object,
		  const char	       *name,
		  size_t	       offset,
		  const CompObjectType *type,
		  void		       *closure)
{
    HandleConnectContext *pCtx = (HandleConnectContext *) closure;

    if (strcmp (name, pCtx->interface) == 0)
    {
	if (!(*object->vTable->forEachMethod) (object, name, connectMethod,
					       closure))
	{
	    /* method must not have any output arguments */
	    if (pCtx->out)
		return TRUE;
	}

	/* need vTable if interface is not part of object type */
	if (!type)
	    pCtx->vTable = object->vTable;

	/* add interface vTable offset to method offset set by
	   connectMethod function */
	pCtx->offset += offset;

	return FALSE;
    }

    return TRUE;
}

static CompBool
handleConnect (CompObject	 *object,
	       const CInterface  *interface,
	       int		 nInterface,
	       int		 *signalVecOffset,
	       const char	 *name,
	       size_t		 offset,
	       CompObject	 *descendant,
	       const char	 *descendantInterface,
	       size_t		 descendantOffset,
	       const char	 *details,
	       va_list		 args,
	       int		 *id)
{
    const char *in;
    int	       index;

    if (signalIndex (name, interface, nInterface, offset, &index, &in))
    {
	HandleConnectContext ctx;
	CompSignalHandler    *handler;
	int		     size;
	CompSignalHandler    **vec;

	if (index < 0)
	    return -1;

	/* make sure details match if signal got a signature */
	if (in && details && strncmp (in, details, strlen (details)))
	    return -1;

	if (!details)
	    details = "";

	ctx.interface = descendantInterface;
	ctx.in	      = NULL;
	ctx.vTable    = NULL;
	ctx.offset    = descendantOffset;
	ctx.marshal   = NULL;

	if ((*descendant->vTable->forEachInterface) (descendant,
						     connectInterface,
						     (void *) &ctx))
	    return -1;

	/* make sure signatures match */
	if (ctx.in && in && strcmp (ctx.in, in))
	{
	    free (ctx.in);
	    return -1;
	}

	if (ctx.in)
	    free (ctx.in);

	vec = compGetSignalVecRange (object,
				     getSignalVecSize (interface, nInterface),
				     signalVecOffset);
	if (!vec)
	    return -1;

	size = compSerializeMethodCall (object,
					descendant,
					descendantInterface,
					name,
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
	handler->offset  = ctx.offset;
	handler->marshal = ctx.marshal;
	handler->header  = (CompSerializedMethodCallHeader *) (handler + 1);

	compSerializeMethodCall (object,
				 descendant,
				 descendantInterface,
				 name,
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

	*id = handler->id;
	return TRUE;
    }

    return FALSE;
}

static CompBool
handleDisconnect (CompObject	   *object,
		  const CInterface *interface,
		  int		   nInterface,
		  int		   *signalVecOffset,
		  const char	   *name,
		  size_t	   offset,
		  int		   id)
{
    int	index;

    if (signalIndex (name, interface, nInterface, offset, &index, NULL))
    {
	CompSignalHandler *handler, *prev = NULL;
	CompSignalHandler **vec = object->signalVec;

	if (index < 0)
	    return TRUE;

	if (signalVecOffset)
	{
	    if (!*signalVecOffset)
		return TRUE;

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

	return TRUE;
    }

    return FALSE;
}

int
cConnect (CompObject *object,
	  const char *interface,
	  size_t     offset,
	  CompObject *descendant,
	  const char *descendantInterface,
	  size_t     descendantOffset,
	  const char *details,
	  va_list    args)
{
    CompInterfaceData *data;
    CMetadata	      m;
    int		      id;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    if (handleConnect (object,
		       m.interface, m.nInterface,
		       &data->signalVecOffset,
		       interface,
		       offset,
		       descendant,
		       descendantInterface,
		       descendantOffset,
		       details,
		       args,
		       &id))
	return id;

    FOR_BASE (object,
	      id = (*object->vTable->signal.connect) (object,
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
cDisconnect (CompObject *object,
	     const char *interface,
	     size_t     offset,
	     int	id)
{
    CompInterfaceData *data;
    CMetadata	      m;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);
    (*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

    if (handleDisconnect (object,
			  m.interface, m.nInterface,
			  &data->signalVecOffset,
			  interface,
			  offset,
			  id))
	return;

    FOR_BASE (object, (*object->vTable->signal.disconnect) (object,
							    interface,
							    offset,
							    id));
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
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nBoolProp; j++)
		if (strcmp (name, m.interface[i].boolProp[j].base.name) == 0)
		    return handleGetBoolProp (object,
					      &m.interface[i].boolProp[j],
					      value);

    FOR_BASE (object,
	      status = (*object->vTable->properties.getBool) (object,
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
	if (!(*prop->set) (object, interface, name, value, error))
	    return FALSE;
    }
    else
    {
	*ptr = value;
    }

    if (!*ptr != !oldValue)
	(*object->vTable->properties.boolChanged) (object,
						   interface, name,
						   *ptr);

    return TRUE;
}

CompBool
cSetBoolProp (CompObject *object,
	      const char *interface,
	      const char *name,
	      CompBool   value,
	      char	 **error)
{
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nBoolProp; j++)
		if (strcmp (name, m.interface[i].boolProp[j].base.name) == 0)
		    return handleSetBoolProp (object,
					      &m.interface[i].boolProp[j],
					      interface, name,
					      value, error);

    FOR_BASE (object,
	      status = (*object->vTable->properties.setBool) (object,
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
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nBoolProp; j++)
		if (strcmp (name, m.interface[i].boolProp[j].base.name) == 0)
		    if (m.interface[i].boolProp[j].changed)
			(*m.interface[i].boolProp[j].changed) (object,
							       interface,
							       name,
							       value);

    FOR_BASE (object, (*object->vTable->properties.boolChanged) (object,
								 interface,
								 name,
								 value));
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
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nIntProp; j++)
		if (strcmp (name, m.interface[i].intProp[j].base.name) == 0)
		    return handleGetIntProp (object,
					     &m.interface[i].intProp[j],
					     value);

    FOR_BASE (object,
	      status = (*object->vTable->properties.getInt) (object,
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
	if (!(*prop->set) (object, interface, name, value, error))
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
	(*object->vTable->properties.intChanged) (object,
						  interface, name,
						  *ptr);

    return TRUE;
}

CompBool
cSetIntProp (CompObject *object,
	     const char *interface,
	     const char *name,
	     int32_t    value,
	     char	**error)
{
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nIntProp; j++)
		if (strcmp (name, m.interface[i].intProp[j].base.name) == 0)
		    return handleSetIntProp (object,
					     &m.interface[i].intProp[j],
					     interface, name,
					     value, error);

    FOR_BASE (object,
	      status = (*object->vTable->properties.setInt) (object,
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
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nIntProp; j++)
		if (strcmp (name, m.interface[i].intProp[j].base.name) == 0)
		    if (m.interface[i].intProp[j].changed)
			(*m.interface[i].intProp[j].changed) (object,
							      interface,
							      name,
							      value);

    FOR_BASE (object, (*object->vTable->properties.intChanged) (object,
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
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nDoubleProp; j++)
		if (strcmp (name, m.interface[i].doubleProp[j].base.name) == 0)
		    return handleGetDoubleProp (object,
						&m.interface[i].doubleProp[j],
						value);

    FOR_BASE (object,
	      status = (*object->vTable->properties.getDouble) (object,
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
	if (!(*prop->set) (object, interface, name, value, error))
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
	(*object->vTable->properties.doubleChanged) (object,
						     interface, name,
						     *ptr);

    return TRUE;
}

CompBool
cSetDoubleProp (CompObject *object,
		const char *interface,
		const char *name,
		double	   value,
		char	   **error)
{
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nDoubleProp; j++)
		if (strcmp (name, m.interface[i].doubleProp[j].base.name) == 0)
		    return handleSetDoubleProp (object,
						&m.interface[i].doubleProp[j],
						interface, name,
						value, error);

    FOR_BASE (object,
	      status = (*object->vTable->properties.setDouble) (object,
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
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nDoubleProp; j++)
		if (strcmp (name, m.interface[i].doubleProp[j].base.name) == 0)
		    if (m.interface[i].doubleProp[j].changed)
			(*m.interface[i].doubleProp[j].changed) (object,
								 interface,
								 name,
								 value);

    FOR_BASE (object, (*object->vTable->properties.doubleChanged) (object,
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
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nStringProp; j++)
		if (strcmp (name, m.interface[i].stringProp[j].base.name) == 0)
		    return handleGetStringProp (object,
						&m.interface[i].stringProp[j],
						value, error);

    FOR_BASE (object,
	      status = (*object->vTable->properties.getString) (object,
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
	if (!(*prop->set) (object, interface, name, value, error))
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
	(*object->vTable->properties.stringChanged) (object,
						     interface, name,
						     *ptr);

    return TRUE;
}

CompBool
cSetStringProp (CompObject *object,
		const char *interface,
		const char *name,
		const char *value,
		char	   **error)
{
    CompBool  status;
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nStringProp; j++)
		if (strcmp (name, m.interface[i].stringProp[j].base.name) == 0)
		    return handleSetStringProp (object,
						&m.interface[i].stringProp[j],
						interface, name,
						value, error);

    FOR_BASE (object,
	      status = (*object->vTable->properties.setString) (object,
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
    CMetadata m;
    int       i, j;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (!interface || strcmp (interface, m.interface[i].name) == 0)
	    for (j = 0; j < m.interface[i].nStringProp; j++)
		if (strcmp (name, m.interface[i].stringProp[j].base.name) == 0)
		    if (m.interface[i].stringProp[j].changed)
			(*m.interface[i].stringProp[j].changed) (object,
								 interface,
								 name,
								 value);

    FOR_BASE (object, (*object->vTable->properties.stringChanged) (object,
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

#define HOME_DATADIR   ".compiz-0/data"
#define XML_EXTENSION ".xml"

typedef CompBool (*ForMetadataFileProc) (FILE	     *fp,
					 const char  *path,
					 struct stat *buf,
					 void	     *closure);

static CompBool
forMetadataFile (const char	     *path,
		 ForMetadataFileProc proc,
		 void		     *closure)
{
    FILE	*fp;
    struct stat buf;
    CompBool	status = TRUE;

    if (stat (path, &buf) == 0)
    {
	fp = fopen (path, "r");
	if (fp)
	{
	    status = (*proc) (fp, path, &buf, closure);
	    fclose (fp);
	}
    }

    return status;
}

static CompBool
forEachMetadataFile (const char	         *file,
		     ForMetadataFileProc proc,
		     void		 *closure)
{
    CompBool status = TRUE;
    char     *home;
    char     *path;

    home = getenv ("HOME");
    if (home)
    {
	path = malloc (strlen (home) + strlen (HOME_DATADIR) +
		       strlen (file) + strlen (XML_EXTENSION) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s%s%s", home, HOME_DATADIR, file,
		     XML_EXTENSION);

	    status = forMetadataFile (path, proc, closure);

	    free (path);
	}
    }

    if (status)
    {
	path = malloc (strlen (METADATADIR) + strlen (file) +
		       strlen (XML_EXTENSION) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s%s", METADATADIR, file, XML_EXTENSION);

	    status = forMetadataFile (path, proc, closure);

	    free (path);
	}
    }

    return status;
}

static CompBool
readMetadata (FILE	  *fp,
	      const char  *path,
	      struct stat *buf,
	      void	  *closure)
{
    char *data;

    data = malloc (buf->st_size + 1);
    if (data)
    {
	data[fread (data, 1, buf->st_size, fp)] = '\0';
	*((char **) closure) = data;

	return FALSE;
    }

    return TRUE;
}

static CompBool
handleGetMetadata (CompObject *object,
		   const char *interface,
		   char       **data,
		   char       **error)
{
    if (forEachMetadataFile (interface, readMetadata, data))
    {
	char *str;

	str = strdup ("<compiz/>");
	if (str)
	{
	    *data = str;
	}
	else
	{
	    esprintf (error, NO_MEMORY_ERROR_STRING);
	    return FALSE;
	}
    }

    return TRUE;
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

CompBool
cGetMetadata (CompObject *object,
	      const char *interface,
	      char	 **data,
	      char	 **error)
{
    CompBool  status;
    CMetadata m;
    int       i;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	if (strcmp (interface, m.interface[i].name) == 0)
	    return handleGetMetadata (object, interface, data, error);


    FOR_BASE (object, status = (*object->vTable->metadata.get) (object,
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

static CompBool
initObject (const CompObjectInstantiator *instantiator,
	    CompObject			 *object,
	    const CompObjectFactory	 *factory)
{
    const CompObjectInstantiatorNode *node =
	(const CompObjectInstantiatorNode *) instantiator;

    if (!allocateObjectPrivates (&object->privates, &node->privates))
	return FALSE;

    object->vTable    = instantiator->vTable;
    object->parent    = NULL;
    object->name      = NULL;
    object->signalVec = NULL;

    object->id = ~0; /* XXX: remove id asap */

    return TRUE;
}

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
    .name.name   = OBJECT_TYPE_NAME,
    .vTable.impl = &objectVTable,
    .vTable.noop = &noopObjectVTable,
    .vTable.size = sizeof (objectVTable),
    .init	 = initObject
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

int
compObjectAllocatePrivateIndex (const CompObjectType *type,
				int	             size)
{
    const CompObjectFactory *f;
    CompFactory		    *factory;

    for (f = &core.u.base.factory; f->master; f = f->master);
    factory = (CompFactory *) f;

    return (*factory->allocatePrivateIndex) (factory, type->name.name, size);
}

void
compObjectFreePrivateIndex (const CompObjectType *type,
			    int	                 index)
{
    const CompObjectFactory *f;
    CompFactory		    *factory;

    for (f = &core.u.base.factory; f->master; f = f->master);
    factory = (CompFactory *) f;

    return (*factory->freePrivateIndex) (factory, type->name.name, index);
}

static CompBool
cObjectAllocPrivateIndex (CompFactory    *factory,
			  CObjectPrivate *private)
{
    int	index;

    index = (*factory->allocatePrivateIndex) (factory,
					      private->name,
					      private->size);
    if (index < 0)
	return FALSE;

    *(private->pIndex) = index;

    return TRUE;
}

static void
cObjectFreePrivateIndex (CompFactory    *factory,
			 CObjectPrivate *private)
{
    (*factory->freePrivateIndex) (factory, private->name, *(private->pIndex));
}

CompBool
cObjectAllocPrivateIndices (CompFactory    *factory,
			    CObjectPrivate *private,
			    int	           nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
	if (!cObjectAllocPrivateIndex (factory, &private[i]))
	    break;

    if (i < nPrivate)
    {
	if (i)
	    cObjectFreePrivateIndices (factory, private, i - 1);

	return FALSE;
    }

    return TRUE;
}

void
cObjectFreePrivateIndices (CompFactory	  *factory,
			   CObjectPrivate *private,
			   int		  nPrivate)
{
    int	n = nPrivate;

    while (n--)
	cObjectFreePrivateIndex (factory, &private[n]);
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

    i = strlen (object->name);

    if (i != pCtx->size)
	return TRUE;

    pCtx->path += i;

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
    {
	if (v != version)
	{
	    compLogMessage (NULL, "core", CompLogLevelError,
			    "wrong '%s' interface version", interface);
	    return FALSE;
	}
    }
    else
    {
	compLogMessage (NULL, "core", CompLogLevelError,
			"no '%s' interface available", interface);

	return FALSE;
    }

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


static CompBool
setIndex (int *index,
	  int value)
{
    *index = value;
    return TRUE;
}

static CompBool
propertyIndex (CInterface *interface,
	       const char *name,
	       int	  type,
	       int	  *index)
{
    int i;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
	for (i = 0; i < interface->nBoolProp; i++)
	    if (strcmp (name, interface->boolProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    case COMP_TYPE_INT32:
	for (i = 0; i < interface->nIntProp; i++)
	    if (strcmp (name, interface->intProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    case COMP_TYPE_DOUBLE:
	for (i = 0; i < interface->nDoubleProp; i++)
	    if (strcmp (name, interface->doubleProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    case COMP_TYPE_STRING:
	for (i = 0; i < interface->nStringProp; i++)
	    if (strcmp (name, interface->stringProp[i].base.name) == 0)
		return setIndex (index, i);
	break;
    }

    return FALSE;
}

static const char *
attrValue (const xmlChar **atts,
	   const char    *name)
{
    int i;

    for (i = 0; atts[i]; i += 2)
	if (strcmp ((const char *) atts[i], name) == 0)
	    return (const char *) atts[i + 1];

    return NULL;
}

typedef struct _DefaultValuesContext {
    CInterface *interface;
    int	       nInterface;
    int	       depth;
    int	       validDepth;
    CInterface *currentInterface;
    int	       propType;
    int	       propIndex;
} DefaultValuesContext;

static void
handleStartElement (void	  *data,
		    const xmlChar *name,
		    const xmlChar **atts)
{
    DefaultValuesContext *pCtx = (DefaultValuesContext *) data;

    if (pCtx->validDepth == pCtx->depth++)
    {
	switch (pCtx->validDepth) {
	case 0:
	    if (strcmp ((const char *) name, "compiz") == 0)
		pCtx->validDepth = pCtx->depth;
	    break;
	case 1:
	    if (strcmp ((const char *) name, "interface") == 0)
	    {
		const char *nameAttr = attrValue (atts, "name");

		if (nameAttr)
		{
		    int i;

		    for (i = 0; i < pCtx->nInterface; i++)
		    {
			if (strcmp (nameAttr, pCtx->interface[i].name) == 0)
			{
			    pCtx->validDepth       = pCtx->depth;
			    pCtx->currentInterface = &pCtx->interface[i];
			    break;
			}
		    }
		}
	    }
	    break;
	case 2:
	    if (strcmp ((const char *) name, "property") == 0)
	    {
		const char *typeAttr = attrValue (atts, "type");

		if (typeAttr && typeAttr[1] == '\0')
		{
		    static int types[] = {
			COMP_TYPE_BOOLEAN,
			COMP_TYPE_INT32,
			COMP_TYPE_DOUBLE,
			COMP_TYPE_STRING
		    };
		    int i;

		    for (i = 0; i < N_ELEMENTS (types); i++)
			if (typeAttr[0] == types[i])
			    break;

		    if (i < N_ELEMENTS (types))
		    {
			const char *nameAttr = attrValue (atts, "name");

			if (nameAttr)
			{
			    if (propertyIndex (pCtx->currentInterface,
					       nameAttr, types[i],
					       &pCtx->propIndex))
			    {
				pCtx->propType	 = types[i];
				pCtx->validDepth = pCtx->depth;
			    }
			}
		    }
		}
	    }
	    break;
	case 3:
	    if (strcmp ((const char *) name, "default") == 0)
		pCtx->validDepth = pCtx->depth;
	    break;
	}
    }
}

static void
handleEndElement (void		*data,
		  const xmlChar *name)
{
    DefaultValuesContext *pCtx = (DefaultValuesContext *) data;

    if (pCtx->validDepth == pCtx->depth--)
	pCtx->validDepth = pCtx->depth;
}

static void
handleCharacters (void		*data,
		  const xmlChar *ch,
		  int		len)
{
    DefaultValuesContext *pCtx = (DefaultValuesContext *) data;

    if (pCtx->validDepth == 4)
    {
	switch (pCtx->propType) {
	case COMP_TYPE_BOOLEAN: {
	    CBoolProp *prop =
		&pCtx->currentInterface->boolProp[pCtx->propIndex];

	    if (len == 4 && strncmp ((const char *) ch, "true", 4) == 0)
		prop->defaultValue = TRUE;
	    else
		prop->defaultValue = FALSE;
	} break;
	case COMP_TYPE_INT32: {
	    char tmp[256];

	    if (len < sizeof (tmp))
	    {
		CIntProp *prop =
		    &pCtx->currentInterface->intProp[pCtx->propIndex];

		strncpy (tmp, (const char *) ch, len);
		tmp[len] = '\0';

		prop->defaultValue = strtol (tmp, NULL, 0);
	    }
	} break;
	case COMP_TYPE_DOUBLE: {
	    char tmp[256];

	    if (len < sizeof (tmp))
	    {
		CDoubleProp *prop =
		    &pCtx->currentInterface->doubleProp[pCtx->propIndex];

		strncpy (tmp, (const char *) ch, len);
		tmp[len] = '\0';

		prop->defaultValue = strtod (tmp, NULL);
	    }
	} break;
	case COMP_TYPE_STRING: {
	    char *value;

	    value = malloc (len + 1);
	    if (value)
	    {
		CStringProp *prop =
		    &pCtx->currentInterface->stringProp[pCtx->propIndex];

		strncpy (value, (const char *) ch, len);
		value[len] = '\0';

		if (prop->data)
		    free (prop->data);

		prop->defaultValue = prop->data	= value;
	    }
	} break;
	}
    }
}

static void
cVerfiyDefaultValues (CInterface *interface,
		      int	 nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nIntProp; j++)
	    interface[i].intProp[j].defaultValue =
		MAX (MIN (interface[i].intProp[j].defaultValue,
			  interface[i].intProp[j].max),
		     interface[i].intProp[j].min);
	for (j = 0; j < interface[i].nDoubleProp; j++)
	    interface[i].doubleProp[j].defaultValue =
		MAX (MIN (interface[i].doubleProp[j].defaultValue,
			  interface[i].doubleProp[j].max),
		     interface[i].doubleProp[j].min);
    }
}

static CompBool
parseDefaultValues (FILE	*fp,
		    const char  *path,
		    struct stat *buf,
		    void	*closure)
{
    xmlSAXHandler saxHandler = {
	.initialized  = XML_SAX2_MAGIC,
	.startElement = handleStartElement,
	.endElement   = handleEndElement,
	.characters   = handleCharacters
    };

    if (!xmlSAXUserParseFile (&saxHandler, closure, path))
	return TRUE;

    return FALSE;
}

static void
cDefaultValuesFromFile (CInterface *interface,
			int	   nInterface,
			const char *name)
{
    DefaultValuesContext ctx = {
	.interface  = interface,
	.nInterface = nInterface
    };

    forEachMetadataFile (name, parseDefaultValues, (void *) &ctx);

    cVerfiyDefaultValues (interface, nInterface);
}

#define SET_DEFAULT_VALUE(data, prop, type)		 \
    PROP_VALUE (data, prop, type) = (prop)->defaultValue

CompBool
cObjectPropertiesInit (CompObject	*object,
		       char		*data,
		       const CInterface *interface,
		       int		nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nBoolProp; j++)
	    SET_DEFAULT_VALUE (data, &interface[i].boolProp[j], CompBool);

	for (j = 0; j < interface[i].nIntProp; j++)
	    SET_DEFAULT_VALUE (data, &interface[i].intProp[j], int32_t);

	for (j = 0; j < interface[i].nDoubleProp; j++)
	    SET_DEFAULT_VALUE (data, &interface[i].doubleProp[j], double);

	for (j = 0; j < interface[i].nStringProp; j++)
	{
	    if (interface[i].stringProp[j].defaultValue)
	    {
		char *str;

		str = strdup (interface[i].stringProp[j].defaultValue);
		if (!str)
		{
		    while (j--)
		    {
			str = PROP_VALUE (data,
					  &interface[i].stringProp[j],
					  char *);
			if (str)
			    free (str);
		    }

		    while (i--)
			cObjectPropertiesFini (object, data, &interface[i], 1);

		    return FALSE;
		}

		PROP_VALUE (data, &interface[i].stringProp[j], char *) = str;
	    }
	    else
	    {
		PROP_VALUE (data, &interface[i].stringProp[j], char *) = NULL;
	    }
	}
    }

    return TRUE;
}

void
cObjectPropertiesFini (CompObject	*object,
		       char		*data,
		       const CInterface *interface,
		       int		nInterface)
{
    int i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nStringProp; j++)
	{
	    char *str;

	    str = PROP_VALUE (data, &interface[i].stringProp[j], char *);
	    if (str)
		free (str);
	}
    }
}

CompBool
cObjectChildrenInit (CompObject		     *object,
		     char		     *data,
		     const CInterface	     *interface,
		     int		     nInterface,
		     const CompObjectFactory *factory)
{
    CompObject *child;
    int	       i, j;

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nChild; j++)
	{
	    CChildObject *cChild = &interface[i].child[j];

	    if (cChild->type)
	    {
		child = CHILD (data, cChild);

		if (!compObjectInitByTypeName (factory, child, cChild->type))
		{
		    while (j--)
		    {
			cChild = &interface[i].child[j];
			if (cChild->type)
			{
			    child = CHILD (data, cChild);
			    (*child->vTable->finalize) (child);
			}
		    }

		    while (i--)
			cObjectChildrenFini (object, data, &interface[i], 1);

		    return FALSE;
		}
	    }
	}
    }

    if (object->parent)
    {
	for (i = 0; i < nInterface; i++)
	{
	    for (j = 0; j < interface[i].nChild; j++)
	    {
		CChildObject *cChild = &interface[i].child[j];

		if (cChild->type)
		{
		    child = CHILD (data, cChild);

		    (*child->vTable->insertObject) (child, object,
						    cChild->name);
		    (*child->vTable->inserted) (child);
		}
	    }
	}
    }

    return TRUE;
}

void
cObjectChildrenFini (CompObject	      *object,
		     char	      *data,
		     const CInterface *interface,
		     int	      nInterface)
{
    CompObject *child;
    int	       i, j;

    if (object->parent)
    {
	for (i = 0; i < nInterface; i++)
	{
	    for (j = 0; j < interface[i].nChild; j++)
	    {
		CChildObject *cChild = &interface[i].child[j];

		if (cChild->type)
		{
		    child = CHILD (data, cChild);

		    (*child->vTable->removed) (child);
		    (*child->vTable->removeObject) (child);
		}
	    }
	}
    }

    for (i = 0; i < nInterface; i++)
    {
	for (j = 0; j < interface[i].nChild; j++)
	{
	    CChildObject *cChild = &interface[i].child[j];

	    if (cChild->type)
	    {
		child = CHILD (data, &interface[i].child[j]);
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
    CMetadata m;
    int	      i, j, signalIndex = 0;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
    {
	if (!m.interface[i].initialized)
	{
	    CInterface *interface = (CInterface *) &m.interface[i];

	    for (j = 0; j < interface->nSignal; j++)
	    {
		interface->signal[j]->index     = signalIndex + j;
		interface->signal[j]->interface = interface->name;
	    }

	    cDefaultValuesFromFile (interface, 1, interface->name);

	    interface->initialized = TRUE;
	}

	signalIndex += m.interface[i].nSignal;
    }

    data->signalVecOffset = 0;

    if (!cObjectPropertiesInit (object, (char *) data,
				m.interface, m.nInterface))
	return FALSE;

    if (!cObjectChildrenInit (object, (char *) data,
			      m.interface, m.nInterface,
			      factory))
    {
	cObjectPropertiesFini (object, (char *) data,
			       m.interface, m.nInterface);
	return FALSE;
    }

    if (m.init && !(*m.init) (object))
    {
	cObjectChildrenFini (object, (char *) data, m.interface, m.nInterface);
	cObjectPropertiesFini (object, (char *) data,
			       m.interface, m.nInterface);
	return FALSE;
    }

    return TRUE;
}

static void
cFiniObjectInterface (CompObject	*object,
		      CompInterfaceData	*data)
{
    CMetadata m;

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    if (m.fini)
	(*m.fini) (object);

    cObjectChildrenFini (object, (char *) data, m.interface, m.nInterface);
    cObjectPropertiesFini (object, (char *) data, m.interface, m.nInterface);

    if (data->signalVecOffset)
	compFreeSignalVecRange (object,
				getSignalVecSize (m.interface, m.nInterface),
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
    CMetadata			     m;
    int				     i;
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

    (*object->vTable->getProp) (object, COMP_PROP_C_METADATA, (void *) &m);

    for (i = 0; i < m.nInterface; i++)
	((CInterface *) m.interface)[i].type = node->type;

    if (!allocateObjectPrivates (&data->privates, &node->privates))
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
cGetInterfaceProp (CompInterfaceData *data,
		   const CMetadata   *template,
		   unsigned int	     what,
		   void		     *value)
{
    switch (what) {
    case COMP_PROP_BASE_VTABLE:
	*((const CompObjectVTable **) value) = data->vTable;
	break;
    case COMP_PROP_C_DATA:
	*((CompInterfaceData **) value) = data;
	break;
    case COMP_PROP_C_METADATA:
	*((CMetadata *) value) = *template;
	break;
    }
}

void
cGetObjectProp (CompObjectData	*data,
		const CMetadata *template,
		unsigned int	what,
		void		*value)
{
    switch (what) {
    case COMP_PROP_PRIVATES:
	*((CompPrivate **) value) = data->privates;
	break;
    default:
	cGetInterfaceProp (&data->base, template, what, value);
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

static const CompObjectVTable cVTable = {
    .setProp = cSetObjectProp,

    .insertObject = cInsertObject,
    .removeObject = cRemoveObject,
    .inserted     = cInserted,
    .removed      = cRemoved,

    .forEachInterface = cForEachInterface,
    .forEachMethod    = cForEachMethod,
    .forEachSignal    = cForEachSignal,
    .forEachProp      = cForEachProp,

    .forEachChildObject = cForEachChildObject,

    .signal.connect    = cConnect,
    .signal.disconnect = cDisconnect,

    .version.get = cGetVersion,

    .properties.getBool     = cGetBoolProp,
    .properties.setBool     = cSetBoolProp,
    .properties.boolChanged = cBoolPropChanged,

    .properties.getInt     = cGetIntProp,
    .properties.setInt     = cSetIntProp,
    .properties.intChanged = cIntPropChanged,

    .properties.getDouble     = cGetDoubleProp,
    .properties.setDouble     = cSetDoubleProp,
    .properties.doubleChanged = cDoublePropChanged,

    .properties.getString     = cGetStringProp,
    .properties.setString     = cSetStringProp,
    .properties.stringChanged = cStringPropChanged,

    .metadata.get = cGetMetadata
};

CompObjectType *
cObjectTypeFromTemplate (const CompObjectType *template)
{
    CompObjectType   *type;
    CompObjectVTable *vTable;
    int		     nameSize = strlen (template->name.name) + 1;
    int		     baseNameSize = 0;
    int		     vTableSize = template->vTable.size;
    int		     noopVTableSize = 0;

    if (template->name.base)
	baseNameSize = strlen (template->name.base) + 1;
    else
	baseNameSize = strlen (OBJECT_TYPE_NAME) + 1;

    if (!vTableSize)
	vTableSize = sizeof (CompObjectVTable);

    if (template->vTable.noop)
	noopVTableSize = vTableSize;

    type = malloc (sizeof (CompObjectType) +
		   nameSize +
		   baseNameSize +
		   vTableSize +
		   noopVTableSize);
    if (!type)
	return NULL;

    type->name.name = strcpy ((char *) (type + 1), template->name.name);
    type->name.base = NULL;

    type->vTable.size = vTableSize;
    type->vTable.impl = NULL;
    type->vTable.noop = NULL;

    type->init = cObjectInit;

    if (template->name.base)
	type->name.base = strcpy ((char *) (type + 1) + nameSize,
				  template->name.base);
    else
	type->name.base = strcpy ((char *) (type + 1) + nameSize,
				  OBJECT_TYPE_NAME);

    vTable = (CompObjectVTable *) ((char *) (type + 1) + nameSize +
				   baseNameSize);

    if (template->vTable.impl)
	type->vTable.impl = memcpy (vTable, template->vTable.impl, vTableSize);
    else
	type->vTable.impl = memset (vTable, 0, vTableSize);

    if (!vTable->finalize)
	vTable->finalize = cObjectFini;

    vTableInit (vTable, &cVTable, sizeof (CompObjectVTable));

    if (template->vTable.noop)
	type->vTable.noop = memcpy ((char *) vTable + vTableSize,
				    template->vTable.noop, vTableSize);

    if (template->init)
	type->init = template->init;

    return type;
}

static CompBool
topObjectType (CompObject	    *object,
	       const char	    *name,
	       size_t		    offset,
	       const CompObjectType *type,
	       void		    *closure)
{
    if (type)
    {
	*((const CompObjectType **) closure) = type;
	return FALSE;
    }

    return TRUE;
}

typedef struct _InitObjectContext {
    const CompObjectFactory *factory;
    const CompObjectType    *type;
    const CompObjectVTable  *vTable;
    CompObject		    *object;
} InitObjectContext;

static CompBool
initBaseObject (CompObject *object,
		void	   *closure)
{
    InitObjectContext    *pCtx = (InitObjectContext *) closure;
    const CompObjectType *type;

    (*object->vTable->forEachInterface) (object,
					 topObjectType,
					 (void *) &type);

    if (type == pCtx->type)
    {
	const CompObjectVTable *vTable = object->vTable;
	CompInterfaceData      *data;

	object->vTable = pCtx->vTable;

	(*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

	data->vTable = vTable;

	if (!cInitObjectInterface (object, pCtx->factory, data))
	{
	    object->vTable = vTable;
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

	    status = initBaseObject (object, closure);

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
finiBaseObject (CompObject *object,
		void	   *closure)
{
    InitObjectContext    *pCtx = (InitObjectContext *) closure;
    const CompObjectType *type;

    (*object->vTable->forEachInterface) (object,
					 topObjectType,
					 (void *) &type);

    if (type == pCtx->type)
    {
	CompInterfaceData *data;

	(*object->vTable->getProp) (object, COMP_PROP_C_DATA, (void *) &data);

	cFiniObjectInterface (object, data);

	object->vTable = data->vTable;
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

	    finiBaseObject (object, closure);

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
initTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type,
		  const CompObjectVTable  *vTable);

static CompBool
finiTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type);

static CompBool
initObjectTree (CompObject *object,
		void	   *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    pCtx->object = object;

    return initTypedObjects (pCtx->factory, object, pCtx->type,
			     pCtx->vTable);
}

static CompBool
finiObjectTree (CompObject *object,
		void	   *closure)
{
    InitObjectContext *pCtx = (InitObjectContext *) closure;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == object)
	return FALSE;

    return finiTypedObjects (pCtx->factory, object, pCtx->type);
}

static CompBool
initTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type,
		  const CompObjectVTable  *vTable)
{
    InitObjectContext ctx;

    ctx.factory = factory;
    ctx.type    = type;
    ctx.vTable  = vTable;
    ctx.object  = NULL;

    if (!initBaseObject (object, (void *) &ctx))
	return FALSE;

    if (!(*object->vTable->forEachChildObject) (object,
						initObjectTree,
						(void *) &ctx))
    {
	(*object->vTable->forEachChildObject) (object,
					       finiObjectTree,
					       (void *) &ctx);

	finiBaseObject (object, (void *) &ctx);

	return FALSE;
    }

    return TRUE;
}

static CompBool
finiTypedObjects (const CompObjectFactory *factory,
		  CompObject              *object,
		  const CompObjectType    *type)
{
    InitObjectContext ctx;

    ctx.factory = factory;
    ctx.type    = type;
    ctx.object  = NULL;

    (*object->vTable->forEachChildObject) (object,
					   finiObjectTree,
					   (void *) &ctx);

    finiBaseObject (object, (void *) &ctx);

    return TRUE;
}

static CompBool
cObjectInitPrivate (CompBranch	   *branch,
		    CObjectPrivate *private)
{
    CompObjectInstantiatorNode *node;
    CompObjectInstantiator	*instantiator;

    node = lookupObjectInstantiatorNode (&branch->factory, private->name);
    if (!node)
	return FALSE;

    instantiator = malloc (sizeof (CompObjectInstantiator) +
			   private->vTableSize);
    if (!instantiator)
	return FALSE;

    instantiator->base   = NULL;
    instantiator->init   = cObjectInterfaceInit;
    instantiator->vTable = (CompObjectVTable *) (instantiator + 1);

    if (private->vTableSize)
    {
	const CompObjectInstantiatorNode *n;
	const CompObjectInstantiator     *p;

	memcpy (instantiator->vTable, private->vTable, private->vTableSize);

	if (!instantiator->vTable->finalize)
	    instantiator->vTable->finalize = cObjectInterfaceFini;

	vTableInit (instantiator->vTable, &cVTable, sizeof (CompObjectVTable));

	for (p = &node->base; p; p = p->base)
	{
	    n = (const CompObjectInstantiatorNode *) p;

	    if (n->type->vTable.noop)
		vTableInit (instantiator->vTable, n->type->vTable.noop,
			    n->type->vTable.size);
	}
    }
    else
    {
	instantiator->vTable = NULL;
    }

    /* initialize all objects of this type */
    if (!initTypedObjects (&branch->factory, &branch->u.base, node->type,
			   instantiator->vTable))
	return FALSE;

    instantiator->base = node->instantiator;
    node->instantiator = instantiator;

    return TRUE;
}

static void
cObjectFiniPrivate (CompBranch	   *branch,
		    CObjectPrivate *private)
{
    CompObjectInstantiatorNode *node;
    CompObjectInstantiator     *instantiator;

    node = lookupObjectInstantiatorNode (&branch->factory, private->name);

    instantiator = (CompObjectInstantiator *) node->instantiator;
    node->instantiator = instantiator->base;

    /* finalize all objects of this type */
    finiTypedObjects (&branch->factory, &branch->u.base, node->type);

    free (instantiator);
}

CompBool
cObjectInitPrivates (CompBranch	    *branch,
		     CObjectPrivate *private,
		     int	    nPrivate)
{
    int	i;

    for (i = 0; i < nPrivate; i++)
	if (!cObjectInitPrivate (branch, &private[i]))
	    break;

    if (i < nPrivate)
    {
	if (i)
	    cObjectFiniPrivates (branch, private, i - 1);

	return FALSE;
    }

    return TRUE;
}

void
cObjectFiniPrivates (CompBranch	    *branch,
		     CObjectPrivate *private,
		     int	    nPrivate)
{
    int	n = nPrivate;

    while (n--)
	cObjectFiniPrivate (branch, &private[n]);
}
