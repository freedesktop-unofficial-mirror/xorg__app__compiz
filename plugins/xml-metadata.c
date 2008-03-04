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

#include <string.h>
#include <poll.h>
#include <sys/stat.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <compiz/branch.h>
#include <compiz/plugin.h>
#include <compiz/delegate.h>
#include <compiz/error.h>
#include <compiz/c-object.h>
#include <compiz/p-object.h>
#include <compiz/marshal.h>

#define COMPIZ_XML_METADATA_PRIVATE_INTERFACE_NAME \
    "org.compiz.metadata.xml.private"

const CompObjectInterface *
getXmlmObjectInterface (void);

#define COMPIZ_XML_METADATA_VERSION        20080220
#define COMPIZ_XML_METADATA_INTERFACE_NAME "org.compiz.metadata.xml"

const CompObjectInterface *
getXmlmBranchObjectInterface (void);

#define XML_DATADIR      METADATADIR
#define HOME_XML_DATADIR ".compiz-0/metadata"
#define XML_EXTENSION    ".xml"
#define XML_ROOT         "/compiz"

static int xmlmObjectPrivateIndex;
static int xmlmBranchPrivateIndex;

typedef CompBool (*XmlmLoadMetadataFileProc) (CompBranch *branch,
					      const char *filename,
					      char	 **error);

typedef CompBool (*XmlmUnloadMetadataFileProc) (CompBranch *branch,
						const char *filename,
						char	   **error);

typedef CompBool (*XmlmLoadInterfaceFromDefaultFileProc) (CompBranch *branch,
							  const char *name,
							  char	     **error);

typedef CompBool (*XmlmApplyInterfaceDefaultsProc) (CompBranch *branch,
						    const char *path,
						    const char *name,
						    char       **error);

typedef void (*XmlmInterfaceAddedProc) (CompBranch *branch,
					const char *path,
					const char *name);

typedef struct _XmlmBranchVTable {
    CompBranchVTable base;

    XmlmLoadMetadataFileProc		 loadMetadataFile;
    XmlmUnloadMetadataFileProc		 unloadMetadataFile;
    XmlmLoadInterfaceFromDefaultFileProc loadInterfaceFromDefaultFile;
    XmlmApplyInterfaceDefaultsProc	 applyInterfaceDefaults;

    XmlmInterfaceAddedProc interfaceAdded;

    SignalProc interfaceAddedDelegate;
} XmlmBranchVTable;

typedef struct _XmlmInterface {
    xmlXPathObject *props;
    xmlXPathObject *nodes;
} XmlmInterface;

typedef struct _XmlmBranch {
    CompInterfaceData base;

    xmlDocPtr *doc;
    int	      nDoc;
} XmlmBranch;

#define GET_XMLM_BRANCH(b)						 \
    ((XmlmBranch *) (b)->data.base.privates[xmlmBranchPrivateIndex].ptr)

#define XMLM_BRANCH(b)			 \
    XmlmBranch *xb = GET_XMLM_BRANCH (b)


static void
xmlmObjectInit (CompObject *object)
{
    object->privates[xmlmObjectPrivateIndex].ptr = NULL;
}

const CompObjectInterface *
getXmlmObjectInterface (void)
{
    static CompObjectInterface *interface = NULL;

    if (!interface)
    {
	const CompObjectInterface template = {
	    .name	  = COMPIZ_XML_METADATA_PRIVATE_INTERFACE_NAME,
	    .base.name    = COMPIZ_OBJECT_TYPE_NAME,
	    .base.version = COMPIZ_OBJECT_VERSION
	};

	interface = pObjectInterfaceFromTemplate (&template,
						  xmlmObjectInit,
						  &xmlmObjectPrivateIndex,
						  0);
    }

    return interface;
}

static CompBool
xmlmForEachObject (CompObject *object,
		   void	      *closure)
{
    if (object->privates[xmlmObjectPrivateIndex].ptr == closure)
	object->privates[xmlmObjectPrivateIndex].ptr = NULL;

    return (*object->vTable->forEachChildObject) (object,
						  xmlmForEachObject,
						  closure);
}

static void
xmlmFreeInterfaceNode (CompBranch *b,
		       xmlNodePtr node)
{
    XmlmInterface *interface = (XmlmInterface *) node->_private;

    if (!interface)
	return;

    if (interface->nodes)
    {
	if (interface->nodes->nodesetval)
	{
	    xmlNodePtr *nodeTab;
	    int	       nodeNr, i;

	    nodeTab = interface->nodes->nodesetval->nodeTab;
	    nodeNr  = interface->nodes->nodesetval->nodeNr;

	    for (i = 0; i < nodeNr; i++)
	    {
		xmlXPathObject *ifaces = (xmlXPathObject *)
		    nodeTab[i]->_private;

		if (!ifaces)
		    continue;

		if (b)
		    (*b->u.base.vTable->forEachChildObject) (&b->u.base,
							     xmlmForEachObject,
							     (void *) ifaces);

		if (ifaces->nodesetval)
		{
		    int j;

		    for (j = 0; j < ifaces->nodesetval->nodeNr; j++)
		    {
			xmlNodePtr interfaceNode =
			    ifaces->nodesetval->nodeTab[j];

			xmlmFreeInterfaceNode (b, interfaceNode);
		    }
		}

		xmlXPathFreeObject (ifaces);
	    }
	}

	xmlXPathFreeObject (interface->nodes);
    }

    if (interface->props)
	xmlXPathFreeObject (interface->props);

    free (interface);

    node->_private = NULL;
}

static void
xmlmBuildInterfaceNode (xmlNodePtr	   node,
			xmlXPathContextPtr ctx,
			const char	   *path,
			int		   index)
{
    XmlmInterface *interface;
    char	  expr[2048], propExpr[2048], nodesExpr[2048];

    node->_private = NULL;

    if (!xmlGetProp (node, BAD_CAST "name"))
	return;

    interface = malloc (sizeof (XmlmInterface));
    if (!interface)
	return;

    interface->props = NULL;
    interface->nodes = NULL;

    node->_private = interface;

    if (snprintf (expr, sizeof (expr),
		  "%s[%d]", path, index + 1) >= sizeof (expr))
	return;

    if (snprintf (propExpr, sizeof (propExpr),
		  "%s"
		  "/property[@name and @type and "
		  "(@type='b' or @type='i' or @type='d' or @type='s')]"
		  "/default[1]/text()", expr) >= sizeof (propExpr))
	return;

    interface->props = xmlXPathEvalExpression (BAD_CAST propExpr, ctx);

    if (snprintf (nodesExpr, sizeof (nodesExpr),
		  "%s"
		  "/node", expr) >= sizeof (nodesExpr))
	return;

    interface->nodes = xmlXPathEvalExpression (BAD_CAST nodesExpr, ctx);

    if (interface->nodes && interface->nodes->nodesetval)
    {
	xmlXPathObjectPtr ifaces;
	char	          interfacesExpr[2048];
	int		  i;

	for (i = 0; i < interface->nodes->nodesetval->nodeNr; i++)
	{
	    interface->nodes->nodesetval->nodeTab[i]->_private = NULL;

	    if (snprintf (interfacesExpr, sizeof (interfacesExpr),
			  "%s"
			  "[%d]/interface[@name]",
			  nodesExpr, i + 1) >= sizeof (interfacesExpr))
		continue;

	    ifaces = xmlXPathEvalExpression (BAD_CAST interfacesExpr, ctx);
	    if (ifaces && ifaces->nodesetval)
	    {
		int j;

		for (j = 0; j < ifaces->nodesetval->nodeNr; j++)
		    xmlmBuildInterfaceNode (ifaces->nodesetval->nodeTab[j],
					    ctx,
					    interfacesExpr,
					    j);
	    }

	    interface->nodes->nodesetval->nodeTab[i]->_private = ifaces;
	}
    }
}

static CompBool
xmlmInitBranch (CompObject *object)
{
    BRANCH (object);
    XMLM_BRANCH (b);

    xb->doc  = NULL;
    xb->nDoc = 0;

    return TRUE;
}

static void
xmlmFiniBranch (CompObject *object)
{
    BRANCH (object);
    XMLM_BRANCH (b);

    if (xb->doc)
    {
	int i;

	for (i = 0; i < xb->nDoc; i++)
	{
	    xmlXPathObject *ifaces = (xmlXPathObject *) xb->doc[i]->_private;

	    if (ifaces)
	    {
		int j;

		for (j = 0; j < ifaces->nodesetval->nodeNr; j++)
		    xmlmFreeInterfaceNode (0, ifaces->nodesetval->nodeTab[j]);

		xmlXPathFreeObject (ifaces);
	    }

	    xmlFreeDoc (xb->doc[i]);
	}

	free (xb->doc);
    }
}

static CompBool
xmlmLoadMetadataFile (CompBranch *b,
		      const char *filename,
		      char	 **error)
{
    xmlDocPtr	       *doc;
    xmlXPathContextPtr ctx;

    XMLM_BRANCH (b);

    doc = realloc (xb->doc, sizeof (xmlDocPtr) * (xb->nDoc + 1));
    if (!doc)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    doc[xb->nDoc] = xmlReadFile (filename, NULL, 0);
    if (!doc[xb->nDoc])
    {
	esprintf (error, "Failed to read XML data from '%s'", filename);
	return FALSE;
    }

    doc[xb->nDoc]->_private = NULL;

    ctx = xmlXPathNewContext (doc[xb->nDoc]);
    if (ctx)
    {
	xmlXPathObjectPtr ifaces;

	ifaces = xmlXPathEvalExpression (BAD_CAST XML_ROOT "/interface[@name]",
					 ctx);
	if (ifaces && ifaces->nodesetval)
	{
	    int i;

	    for (i = 0; i < ifaces->nodesetval->nodeNr; i++)
		xmlmBuildInterfaceNode (ifaces->nodesetval->nodeTab[i],
					ctx,
					XML_ROOT "/interface[@name]",
					i);
	}

	doc[xb->nDoc]->_private = ifaces;

	xmlXPathFreeContext (ctx);
    }

    xb->doc = doc;
    xb->nDoc++;

    return TRUE;
}

static CompBool
xmlmUnloadMetadataFile (CompBranch *b,
			const char *filename,
			char       **error)
{
    xmlDocPtr      *doc;
    xmlXPathObject *ifaces;
    int		   i;

    XMLM_BRANCH (b);

    for (i = 0; i < xb->nDoc; i++)
	if (strcmp (xb->doc[i]->name, filename) == 0)
	    break;

    if (i == xb->nDoc)
    {
	esprintf (error, "File '%s' is not currently loaded", filename);
	return FALSE;
    }

    ifaces = (xmlXPathObject *) xb->doc[i]->_private;
    if (ifaces)
    {
	int j;

	for (j = 0; j < ifaces->nodesetval->nodeNr; j++)
	    xmlmFreeInterfaceNode (b, ifaces->nodesetval->nodeTab[i]);

	xmlXPathFreeObject (ifaces);
    }

    xmlFreeDoc (xb->doc[i]);

    xb->nDoc--;

    if (i < xb->nDoc)
	memmove (xb->doc, &xb->doc[i + 1],
		 sizeof (xmlDocPtr) * (xb->nDoc - i));

    doc = realloc (xb->doc, sizeof (xmlDocPtr) * xb->nDoc);
    if (doc || !xb->nDoc)
	xb->doc = doc;

    return TRUE;
}

static CompBool
xmlmLoadInterfaceFromDefaultFile (CompBranch *b,
				  const char *name,
				  char	     **error)
{
    CompBool    status;
    const char	*home;
    char	*path;
    struct stat buf;

    home = getenv ("HOME");
    if (home)
    {
	path = malloc (strlen (home) + strlen (HOME_XML_DATADIR) +
		       strlen (name) + strlen (XML_EXTENSION) + 3);
	if (!path)
	{
	    esprintf (error, NO_MEMORY_ERROR_STRING);
	    return FALSE;
	}

	sprintf (path, "%s/%s/%s%s", home, HOME_XML_DATADIR, name,
		 XML_EXTENSION);

	if (stat (path, &buf) == 0)
	{
	    status = xmlmLoadMetadataFile (b, path, error);
	    free (path);
	    return status;
	}

	free (path);
    }

    path = malloc (strlen (XML_DATADIR) + strlen (name) +
		   strlen (XML_EXTENSION) + 2);
    if (!path)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    sprintf (path, "%s/%s%s", XML_DATADIR, name, XML_EXTENSION);

    if (stat (path, &buf) == 0)
    {
	status = xmlmLoadMetadataFile (b, path, error);
	free (path);
	return status;
    }

    free (path);
    return FALSE;
}

static XmlmInterface *
xmlmGetMetadataInterfaceFromNodeSet (xmlXPathObject *interfaces,
				     const char     *name)
{
    xmlNodePtr *nodeTab;
    int	       nodeNr, i;

    if (!interfaces->nodesetval)
	return NULL;

    nodeTab = interfaces->nodesetval->nodeTab;
    nodeNr  = interfaces->nodesetval->nodeNr;

    for (i = 0; i < nodeNr; i++)
	if (!strcmp ((char *) xmlGetProp (nodeTab[i], BAD_CAST "name"), name))
	    return (XmlmInterface *) nodeTab[i]->_private;

    return NULL;
}

static XmlmInterface *
xmlmGetMetadataInterface (CompBranch *b,
			  CompObject *object,
			  const char *name)
{
    XmlmInterface  *interface;
    xmlXPathObject *ifaces;
    int		   i;

    XMLM_BRANCH (b);

    ifaces = (xmlXPathObject *) object->privates[xmlmObjectPrivateIndex].ptr;
    if (ifaces)
    {
	interface = xmlmGetMetadataInterfaceFromNodeSet (ifaces, name);
	if (interface)
	    return interface;
    }

    for (i = 0; i < xb->nDoc; i++)
    {
	ifaces = (xmlXPathObject *) xb->doc[i]->_private;
	if (ifaces)
	{
	    interface = xmlmGetMetadataInterfaceFromNodeSet (ifaces, name);
	    if (interface)
		return interface;
	}
    }

    return NULL;
}

static void
xmlmSetupNode (CompBranch *b,
	       CompObject *parent,
	       CompObject *node)
{
    CompDelegate *d;

    d = COMP_TYPE_CAST (node, getDelegateObjectType (), CompDelegate);
    if (d)
	if (compConnect (parent,
			 getObjectType (),
			 offsetof (CompObjectVTable, signal),
			 &d->u.base,
			 getDelegateObjectType (),
			 offsetof (CompDelegateVTable, processSignal),
			 NULL) < 0)
	    compLog (&b->u.base,
		     getXmlmBranchObjectInterface (),
		     offsetof (XmlmBranchVTable,
			       applyInterfaceDefaults),
		     "Failed to connect '%s' signal to '%s' delegate",
		     parent->name, node->name);
}

static CompBool
xmlmApplyInterfaceDefaults (CompBranch *b,
			    const char *path,
			    const char *name,
			    char       **error)
{
    CompObject    *object;
    XmlmInterface *interface;
    xmlNodePtr    *nodeTab;
    int	          nodeNr, i;

    object = compLookupDescendant (&b->u.base, path);
    if (!object)
	return TRUE;

    interface = xmlmGetMetadataInterface (b, object, name);
    if (!interface)
	return TRUE;

    if (interface->props && interface->props->nodesetval)
    {
	nodeTab = interface->props->nodesetval->nodeTab;
	nodeNr  = interface->props->nodesetval->nodeNr;

	for (i = 0; i < nodeNr; i++)
	{
	    const char *value = (const char *) nodeTab[i]->content;
	    xmlChar    *prop, *type;

	    prop = xmlGetProp (nodeTab[i]->parent->parent, BAD_CAST "name");
	    type = xmlGetProp (nodeTab[i]->parent->parent, BAD_CAST "type");

	    switch (*type) {
	    case 'b':
		(*object->vTable->setBool) (object, name,
					    (const char *) prop,
					    *value == 't' ? TRUE : FALSE,
					    NULL);
		break;
	    case 'i':
		(*object->vTable->setInt) (object, name,
					   (const char *) prop,
					   strtol (value, NULL, 0),
					   NULL);
		break;
	    case 'd':
		(*object->vTable->setDouble) (object, name,
					      (const char *) prop,
					      strtod (value, NULL),
					      NULL);
		break;
	    case 's':
		(*object->vTable->setString) (object, name,
					      (const char *) prop,
					      value,
					      NULL);
	    default:
		break;
	    }
	}
    }

    if (interface->nodes && interface->nodes->nodesetval)
    {
	CompObject *node;

	nodeTab = interface->nodes->nodesetval->nodeTab;
	nodeNr  = interface->nodes->nodesetval->nodeNr;

	for (i = 0; i < nodeNr; i++)
	{
	    const char *nodeName;

	    nodeName = (const char *) xmlGetProp (nodeTab[i], BAD_CAST "name");
	    if (nodeName)
	    {
		node = compLookupDescendantVa (object, nodeName, NULL);
	    }
	    else
	    {
		char childName[256];

		sprintf (childName, "%p", nodeTab[i]);

		node = compLookupDescendantVa (object, childName, NULL);
		if (!node)
		{
		    const char *nodeType;

		    nodeType = (const char *) xmlGetProp (nodeTab[i],
							  BAD_CAST "type");
		    if (nodeType)
		    {
			const CompObjectType *type;

			type = compLookupObjectType (&b->factory, nodeType);
			if (type)
			{
			    node = (*b->u.vTable->newObject) (b,
							      object,
							      type,
							      childName,
							      NULL);
			    if (node)
				xmlmSetupNode (b, object, node);
			}
			else
			{
			    compLog (&b->u.base,
				     getXmlmBranchObjectInterface (),
				     offsetof (XmlmBranchVTable,
					       applyInterfaceDefaults),
				     "'%s' is not a registered object type",
				     nodeType);
			}
		    }
		    else
		    {
			node = (*b->u.vTable->newObject) (b,
							  object,
							  getObjectType (),
							  childName,
							  NULL);
		    }
		}
	    }

	    if (node)
		node->privates[xmlmObjectPrivateIndex].ptr =
		    nodeTab[i]->_private;
	}
    }

    return TRUE;
}
static void
xmlmInterfaceAdded (CompBranch *b,
		    const char *path,
		    const char *name)
{
    C_EMIT_SIGNAL (&b->u.base, XmlmInterfaceAddedProc,
		   offsetof (XmlmBranchVTable, interfaceAdded),
		   path, name);
}

static void
xmlmInterfaceAddedDelegate (CompObject   *object,
			    const char   *path,
			    const char   *interface,
			    const char   *name,
			    const char   *signature,
			    CompAnyValue *value,
			    int	         nValue)
{
    const char *decendantPath;

    BRANCH (object);

    decendantPath = compTranslateObjectPath (object->parent, object, path);
    if (decendantPath)
	xmlmInterfaceAdded (b, decendantPath, value[0].s);
}

static void
xmlmInsertBranch (CompObject *object,
		  CompObject *parent)
{
    int i, j;

    BRANCH (object);

    compConnect (parent,
		 getObjectType (),
		 offsetof (CompObjectVTable, signal),
		 &b->u.base,
		 getXmlmBranchObjectInterface (),
		 offsetof (XmlmBranchVTable, interfaceAddedDelegate),
		 "osss", "//*", getObjectType ()->name, "interfaceAdded", "s");

    (*parent->vTable->connectAsync) (parent,
				     &b->u.base,
				     getXmlmBranchObjectInterface (),
				     offsetof (XmlmBranchVTable,
					       interfaceAdded),
				     &b->u.base,
				     getXmlmBranchObjectInterface (),
				     offsetof (XmlmBranchVTable,
					       applyInterfaceDefaults));

    for (i = 0; i < b->data.types.nChild; i++)
    {
	CompObject *type = b->data.types.child[i].ref;

	for (j = 0; j < type->nChild; j++)
	{
	    char *v;

	    if ((*type->child[j].ref->vTable->getString) (type->child[j].ref,
							  0, "value", &v, 0))
	    {
		xmlmLoadInterfaceFromDefaultFile (b, v, 0);
		free (v);
	    }
	}
    }
}

static void
xmlmGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    cGetInterfaceProp (&GET_XMLM_BRANCH (GET_BRANCH (object))->base,
		       getXmlmBranchObjectInterface (),
		       what, value);
}

static const XmlmBranchVTable xmlmBranchObjectVTable = {
    .base.base.getProp = xmlmGetProp,

    .loadMetadataFile		  = xmlmLoadMetadataFile,
    .unloadMetadataFile		  = xmlmUnloadMetadataFile,
    .loadInterfaceFromDefaultFile = xmlmLoadInterfaceFromDefaultFile,
    .applyInterfaceDefaults       = xmlmApplyInterfaceDefaults,

    .interfaceAdded = xmlmInterfaceAdded,

    .interfaceAddedDelegate = xmlmInterfaceAddedDelegate
};

static const CMethod xmlMetadataBranchMethod[] = {
    C_METHOD (loadMetadataFile, "s", "", XmlmBranchVTable, marshal__S__E),
    C_METHOD (unloadMetadataFile, "s", "", XmlmBranchVTable, marshal__S__E),
    C_METHOD (loadInterfaceFromDefaultFile, "s", "", XmlmBranchVTable,
	      marshal__S__E),
    C_METHOD (applyInterfaceDefaults, "os", "", XmlmBranchVTable,
	      marshal__SS__E)
};

static const CSignal xmlMetadataBranchSignal[] = {
    C_SIGNAL (interfaceAdded, "os", XmlmBranchVTable)
};

const CompObjectInterface *
getXmlmBranchObjectInterface (void)
{
    static CompObjectInterface *interface = NULL;

    if (!interface)
    {
	static const CObjectInterface template = {
	    .i.name	    = COMPIZ_XML_METADATA_INTERFACE_NAME,
	    .i.version	    = COMPIZ_XML_METADATA_VERSION,
	    .i.base.name    = COMPIZ_BRANCH_TYPE_NAME,
	    .i.base.version = COMPIZ_BRANCH_VERSION,
	    .i.vTable.impl  = &xmlmBranchObjectVTable.base.base,
	    .i.vTable.size  = sizeof (xmlmBranchObjectVTable),

	    .method  = xmlMetadataBranchMethod,
	    .nMethod = N_ELEMENTS (xmlMetadataBranchMethod),

	    .signal  = xmlMetadataBranchSignal,
	    .nSignal = N_ELEMENTS (xmlMetadataBranchSignal),

	    .init = xmlmInitBranch,
	    .fini = xmlmFiniBranch,

	    .insert = xmlmInsertBranch
	};

	interface = cObjectInterfaceFromTemplate (&template,
						  &xmlmBranchPrivateIndex,
						  sizeof (XmlmBranch));
    }

    return interface;
}

const GetInterfaceProc *
getCompizObjectInterfaces20080220 (int *n)
{
    static const GetInterfaceProc interfaces[] = {
	getXmlmObjectInterface,
	getXmlmBranchObjectInterface
    };

    *n = N_ELEMENTS (interfaces);
    return interfaces;
}
