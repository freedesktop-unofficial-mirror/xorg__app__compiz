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

typedef struct _XmlmInterface XmlmInterface;

typedef void (*XmlmApplyInterfaceMetadataProc) (CompBranch	    *branch,
						CompObject	    *object,
						const XmlmInterface *interface,
						const char	    *name);

typedef CompBool (*XmlmLoadMetadataFileProc) (CompBranch *branch,
					      const char *filename,
					      char	 **error);

typedef CompBool (*XmlmUnloadMetadataFileProc) (CompBranch *branch,
						const char *filename,
						char	   **error);

typedef CompBool (*XmlmLoadInterfaceFromDefaultFileProc) (CompBranch *branch,
							  const char *name,
							  char	     **error);

typedef void (*XmlmEnsureInterfaceMetadataProc) (CompBranch *branch,
						 const char *path,
						 const char *name);

typedef void (*XmlmInterfaceAddedProc) (CompBranch *branch,
					const char *path,
					const char *name);

typedef struct _XmlmBranchVTable {
    CompBranchVTable base;

    XmlmApplyInterfaceMetadataProc	 applyInterfaceMetadata;
    XmlmLoadMetadataFileProc		 loadMetadataFile;
    XmlmUnloadMetadataFileProc		 unloadMetadataFile;
    XmlmLoadInterfaceFromDefaultFileProc loadInterfaceFromDefaultFile;
    XmlmEnsureInterfaceMetadataProc	 ensureInterfaceMetadata;

    XmlmInterfaceAddedProc interfaceAdded;

    SignalProc interfaceAddedDelegate;
} XmlmBranchVTable;

struct _XmlmInterface {
    xmlXPathObject *props;
    xmlXPathObject *nodes;
};

typedef struct _XmlmNode {
    xmlXPathObject *interfaces;
    xmlXPathObject *connections;
} XmlmNode;

typedef struct _XmlmDoc {
    xmlDocPtr doc;
    XmlmNode  node;
} XmlmDoc;

typedef struct _XmlmBranch {
    CompInterfaceData base;

    XmlmDoc *doc;
    int	    nDoc;
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

static const char *
xmlmNodeProp (xmlNodePtr node,
	      const char *name)
{
    xmlAttrPtr attr;

    attr = xmlHasProp (node, BAD_CAST name);
    if (!attr)
	return NULL;

    if (attr->_private)
	return (const char *) attr->_private;

    attr->_private = xmlGetProp (node, BAD_CAST name);

    return (const char *) attr->_private;
}

static void
xmlmFreeInterfaceNode (CompBranch *b,
		       xmlNodePtr node)
{
    XmlmInterface *interface = (XmlmInterface *) node->_private;

    if (!interface)
	return;

    if (interface->props)
	xmlXPathFreeObject (interface->props);

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
		XmlmNode *node = (XmlmNode *) nodeTab[i]->_private;

		if (!node)
		    continue;

		if (b)
		    (*b->u.base.vTable->forEachChildObject) (&b->u.base,
							     xmlmForEachObject,
							     (void *) node);

		if (node->connections)
		    xmlXPathFreeObject (node->connections);

		if (node->interfaces)
		{
		    if (node->interfaces->nodesetval)
		    {
			xmlXPathObjectPtr interfaces = node->interfaces;
			int		  j;

			for (j = 0; j < interfaces->nodesetval->nodeNr; j++)
			{
			    xmlNodePtr interfaceNode =
				interfaces->nodesetval->nodeTab[j];

			    xmlmFreeInterfaceNode (b, interfaceNode);
			}
		    }

		    xmlXPathFreeObject (node->interfaces);
		}

		free (node);
	    }
	}

	xmlXPathFreeObject (interface->nodes);
    }

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
    char	  baseExpr[2048], xpathExpr[2048];

    node->_private = NULL;

    if (!xmlmNodeProp (node, "name"))
	return;

    interface = malloc (sizeof (XmlmInterface));
    if (!interface)
	return;

    interface->props = NULL;
    interface->nodes = NULL;

    node->_private = interface;

    if (snprintf (baseExpr, sizeof (baseExpr),
		  "%s[%d]", path, index + 1) >= sizeof (baseExpr))
	return;

    if (snprintf (xpathExpr, sizeof (xpathExpr),
		  "%s"
		  "/property[@name and @type and "
		  "(@type='b' or @type='i' or @type='d' or @type='s')]"
		  "/default[1]/text()", baseExpr) >= sizeof (xpathExpr))
	return;

    interface->props = xmlXPathEvalExpression (BAD_CAST xpathExpr, ctx);

    if (snprintf (xpathExpr, sizeof (xpathExpr),
		  "%s"
		  "/node", baseExpr) >= sizeof (xpathExpr))
	return;

    interface->nodes = xmlXPathEvalExpression (BAD_CAST xpathExpr, ctx);

    if (interface->nodes && interface->nodes->nodesetval)
    {
	XmlmNode *node;
	char	 nodeExpr[2048];
	int	 i;

	for (i = 0; i < interface->nodes->nodesetval->nodeNr; i++)
	{
	    node = malloc (sizeof (XmlmNode));
	    if (!node)
		continue;

	    node->interfaces  = NULL;
	    node->connections = NULL;

	    interface->nodes->nodesetval->nodeTab[i]->_private = node;

	    if (snprintf (nodeExpr, sizeof (nodeExpr),
			  "%s"
			  "[%d]/interface[@name]",
			  xpathExpr, i + 1) >= sizeof (nodeExpr))
		continue;

	    node->interfaces = xmlXPathEvalExpression (BAD_CAST nodeExpr, ctx);
	    if (node->interfaces && node->interfaces->nodesetval)
	    {
		xmlXPathObjectPtr interfaces = node->interfaces;
		int		  j;

		for (j = 0; j < interfaces->nodesetval->nodeNr; j++)
		    xmlmBuildInterfaceNode (interfaces->nodesetval->nodeTab[j],
					    ctx,
					    nodeExpr,
					    j);
	    }

	    if (snprintf (nodeExpr, sizeof (nodeExpr),
			  "%s"
			  "[%d]/connect[@signal and @method]",
			  xpathExpr, i + 1) >= sizeof (nodeExpr))
		continue;

	    node->connections = xmlXPathEvalExpression (BAD_CAST nodeExpr,
							ctx);
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
	    XmlmNode *node = (XmlmNode *) &xb->doc[i].node;

	    if (node->connections)
		xmlXPathFreeObject (node->connections);

	    if (node->interfaces)
	    {
		xmlXPathObjectPtr interfaces = node->interfaces;
		int		  j;

		for (j = 0; j < interfaces->nodesetval->nodeNr; j++)
		    xmlmFreeInterfaceNode (0,
					   interfaces->nodesetval->nodeTab[j]);

		xmlXPathFreeObject (interfaces);
	    }

	    xmlFreeDoc (xb->doc[i].doc);
	}

	free (xb->doc);
    }
}

static XmlmInterface *
xmlmGetMetadataInterfaceFromNode (XmlmNode   *node,
				  const char *name)
{
    xmlNodePtr *nodeTab;
    int	       nodeNr, i;

    if (!node->interfaces->nodesetval)
	return NULL;

    nodeTab = node->interfaces->nodesetval->nodeTab;
    nodeNr  = node->interfaces->nodesetval->nodeNr;

    for (i = 0; i < nodeNr; i++)
	if (!strcmp (xmlmNodeProp (nodeTab[i], "name"), name))
	    return (XmlmInterface *) nodeTab[i]->_private;

    return NULL;
}

static void
xmlmApplyInterfaceMetadata (CompBranch		*b,
			    CompObject		*object,
			    const XmlmInterface *interface,
			    const char		*name)
{
    xmlNodePtr *nodeTab;
    int	       nodeNr, i;

    if (interface->nodes && interface->nodes->nodesetval)
    {
	CompObject *node;

	nodeTab = interface->nodes->nodesetval->nodeTab;
	nodeNr  = interface->nodes->nodesetval->nodeNr;

	for (i = 0; i < nodeNr; i++)
	{
	    const char *nodeName;

	    nodeName = xmlmNodeProp (nodeTab[i], "name");
	    if (nodeName)
	    {
		node = compLookupDescendantVa (object, nodeName, NULL);
		if (!node)
		    compLog (&b->u.base,
			     getXmlmBranchObjectInterface (),
			     offsetof (XmlmBranchVTable,
				       applyInterfaceMetadata),
			     "'%s' node could not be located", nodeName);
	    }
	    else
	    {
		char childName[256];

		sprintf (childName, "%p", nodeTab[i]);

		node = compLookupDescendantVa (object, childName, NULL);
		if (!node)
		{
		    const CompObjectType *type;
		    const char		 *nodeType;

		    nodeType = xmlmNodeProp (nodeTab[i], "type");
		    if (nodeType)
		    {
			type = compLookupObjectType (&b->factory, nodeType);
			if (!type)
			    compLog (&b->u.base,
				     getXmlmBranchObjectInterface (),
				     offsetof (XmlmBranchVTable,
					       applyInterfaceMetadata),
				     "'%s' is not a registered object type",
				     nodeType);
		    }
		    else
		    {
			type = getObjectType ();
		    }

		    if (type)
		    {
			char *error;

			node = (*b->u.vTable->newObject) (b,
							  object,
							  type,
							  childName,
							  &error);
			if (!node && error)
			{
			    compLog (&b->u.base,
				     getXmlmBranchObjectInterface (),
				     offsetof (XmlmBranchVTable,
					       applyInterfaceMetadata),
				     error);
			    free (error);
			}
		    }
		}
	    }

	    if (node && nodeTab[i]->_private)
	    {
		XmlmNode   *n = (XmlmNode *) nodeTab[i]->_private;
		xmlNodePtr *tab;
		int	   nr, j;

		node->privates[xmlmObjectPrivateIndex].ptr = n;

		if (n->interfaces && n->interfaces->nodesetval)
		{
		    tab = n->interfaces->nodesetval->nodeTab;
		    nr  = n->interfaces->nodesetval->nodeNr;

		    for (j = 0; j < nr; j++)
			xmlmApplyInterfaceMetadata (b,
						    node,
						    (const XmlmInterface *)
						    tab[j]->_private,
						    xmlmNodeProp (tab[j],
								  name));
		}

		if (n->connections && n->connections->nodesetval)
		{
		    tab = n->connections->nodesetval->nodeTab;
		    nr  = n->connections->nodesetval->nodeNr;

		    for (j = 0; j < nr; j++)
		    {
			const char *signal, *method, *iface;
			char       *error;

			signal = xmlmNodeProp (tab[j], "signal");
			method = xmlmNodeProp (tab[j], "method");
			iface  = xmlmNodeProp (tab[j], "interface");

			if (!(b->u.vTable->connectDescendants) (b,
								node,
								iface,
								signal,
								object,
								name,
								method,
								NULL,
								&error))
			{
			    if (error)
			    {
				compLog (&b->u.base,
					 getXmlmBranchObjectInterface (),
					 offsetof (XmlmBranchVTable,
						   applyInterfaceMetadata),
					 error);
				free (error);
			    }
			}
		    }
		}
	    }
	}
    }

    if (interface->props && interface->props->nodesetval)
    {
	nodeTab = interface->props->nodesetval->nodeTab;
	nodeNr  = interface->props->nodesetval->nodeNr;

	for (i = 0; i < nodeNr; i++)
	{
	    const char *value = (const char *) nodeTab[i]->content;
	    const char *prop, *type;
	    char       *error;
	    CompBool   status = TRUE;

	    prop = xmlmNodeProp (nodeTab[i]->parent->parent, "name");
	    type = xmlmNodeProp (nodeTab[i]->parent->parent, "type");

	    switch (*type) {
	    case 'b':
		status = (*object->vTable->setBool) (object, name, prop,
						     *value == 't' ?
						     TRUE : FALSE,
						     &error);
		break;
	    case 'i':
		status = (*object->vTable->setInt) (object, name, prop,
						    strtol (value, NULL, 0),
						    &error);
		break;
	    case 'd':
		status = (*object->vTable->setDouble) (object, name, prop,
						       strtod (value, NULL),
						       &error);
		break;
	    case 's':
		status = (*object->vTable->setString) (object, name, prop,
						       value,
						       &error);
	    default:
		break;
	    }

	    if (!status && error)
	    {
		compLog (&b->u.base,
			 getXmlmBranchObjectInterface (),
			 offsetof (XmlmBranchVTable, applyInterfaceMetadata),
			 error);
		free (error);
	    }
	}
    }
}

static CompBool
xmlmLoadMetadataFile (CompBranch *b,
		      const char *filename,
		      char	 **error)
{
    XmlmDoc	       *doc;
    xmlXPathContextPtr ctx;

    XMLM_BRANCH (b);

    doc = realloc (xb->doc, sizeof (XmlmDoc) * (xb->nDoc + 1));
    if (!doc)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    doc[xb->nDoc].doc = xmlReadFile (filename, NULL, 0);
    if (!doc[xb->nDoc].doc)
    {
	esprintf (error, "Failed to read XML data from '%s'", filename);
	return FALSE;
    }

    ctx = xmlXPathNewContext (doc[xb->nDoc].doc);
    if (ctx)
    {
	xmlXPathObjectPtr interfaces;

	interfaces =
	    xmlXPathEvalExpression (BAD_CAST XML_ROOT "/interface[@name]",
				    ctx);
	if (interfaces && interfaces->nodesetval)
	{
	    int i;

	    for (i = 0; i < interfaces->nodesetval->nodeNr; i++)
		xmlmBuildInterfaceNode (interfaces->nodesetval->nodeTab[i],
					ctx,
					XML_ROOT "/interface[@name]",
					i);
	}

	doc[xb->nDoc].node.interfaces  = interfaces;
	doc[xb->nDoc].node.connections = NULL;

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
    XmlmDoc *doc;
    int	    i;

    XMLM_BRANCH (b);

    for (i = 0; i < xb->nDoc; i++)
	if (strcmp (xb->doc[i].doc->name, filename) == 0)
	    break;

    if (i == xb->nDoc)
    {
	esprintf (error, "File '%s' is not currently loaded", filename);
	return FALSE;
    }

    if (xb->doc[i].node.connections)
	xmlXPathFreeObject (xb->doc[i].node.connections);

    if (xb->doc[i].node.interfaces)
    {
	xmlXPathObjectPtr interfaces = xb->doc[i].node.interfaces;
	int		  j;

	for (j = 0; j < interfaces->nodesetval->nodeNr; j++)
	    xmlmFreeInterfaceNode (b, interfaces->nodesetval->nodeTab[i]);

	xmlXPathFreeObject (interfaces);
    }

    xmlFreeDoc (xb->doc[i].doc);

    xb->nDoc--;

    if (i < xb->nDoc)
	memmove (xb->doc, &xb->doc[i + 1], sizeof (XmlmDoc) * (xb->nDoc - i));

    doc = realloc (xb->doc, sizeof (XmlmDoc) * xb->nDoc);
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

static void
xmlmEnsureInterfaceMetadata (CompBranch *b,
			     const char *path,
			     const char *name)
{
    CompObject *object;
    int	       i;

    XMLM_BRANCH (b);

    object = compLookupDescendant (&b->u.base, path);
    if (!object)
	return;

    /* already set? */
    if (object->privates[xmlmObjectPrivateIndex].ptr)
	return;

    for (i = 0; i < xb->nDoc; i++)
    {
	const XmlmInterface *interface;

	interface = xmlmGetMetadataInterfaceFromNode (&xb->doc[i].node, name);
	if (interface)
	    xmlmApplyInterfaceMetadata (b,
					object,
					interface,
					name);
    }
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

    compConnect (&b->u.base,
		 getXmlmBranchObjectInterface (),
		 offsetof (XmlmBranchVTable, interfaceAdded),
		 &b->u.base,
		 getXmlmBranchObjectInterface (),
		 offsetof (XmlmBranchVTable, ensureInterfaceMetadata),
		 NULL);

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

    .applyInterfaceMetadata       = xmlmApplyInterfaceMetadata,
    .loadMetadataFile		  = xmlmLoadMetadataFile,
    .unloadMetadataFile		  = xmlmUnloadMetadataFile,
    .loadInterfaceFromDefaultFile = xmlmLoadInterfaceFromDefaultFile,
    .ensureInterfaceMetadata      = xmlmEnsureInterfaceMetadata,

    .interfaceAdded = xmlmInterfaceAdded,

    .interfaceAddedDelegate = xmlmInterfaceAddedDelegate
};

static const CMethod xmlMetadataBranchMethod[] = {
    C_METHOD (loadMetadataFile, "s", "", XmlmBranchVTable, marshal__S__E),
    C_METHOD (unloadMetadataFile, "s", "", XmlmBranchVTable, marshal__S__E),
    C_METHOD (loadInterfaceFromDefaultFile, "s", "", XmlmBranchVTable,
	      marshal__S__E),
    C_METHOD (ensureInterfaceMetadata, "os", "", XmlmBranchVTable,
	      marshal__SS__)
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
