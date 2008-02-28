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
#include <compiz/marshal.h>

#define COMPIZ_XML_METADATA_VERSION        20080220
#define COMPIZ_XML_METADATA_INTERFACE_NAME "org.compiz.metadata.xml"

const CompObjectInterface *
getXmlmBranchObjectInterface (void);

#define XML_DATADIR      METADATADIR
#define HOME_XML_DATADIR ".compiz-0/data"
#define XML_EXTENSION    ".xml"

static int xmlmBranchPrivateIndex;

typedef CompBool (*XmlmLoadInterfaceProc) (CompBranch *branch,
					   const char *name,
					   const char *filename,
					   char	      **error);

typedef CompBool (*XmlmUnloadInterfaceProc) (CompBranch *branch,
					     const char *name,
					     char	**error);

typedef CompBool (*XmlmLoadInterfaceFromDefaultFileProc) (CompBranch *branch,
							  const char *name,
							  char	     **error);

typedef CompBool (*XmlmSetPropertiesToDefaultValuesProc) (CompBranch *branch,
							  const char *path,
							  const char *name,
							  char	     **error);

typedef CompBool (*XmlmGetInterfaceDataProc) (CompBranch *branch,
					      const char *name,
					      char	 **data,
					      char	 **error);

typedef void (*XmlmInterfaceAddedProc) (CompBranch *branch,
					const char *path,
					const char *name);

typedef struct _XmlmBranchVTable {
    CompBranchVTable base;

    XmlmLoadInterfaceProc		 loadInterface;
    XmlmUnloadInterfaceProc		 unloadInterface;
    XmlmLoadInterfaceFromDefaultFileProc loadInterfaceFromDefaultFile;
    XmlmSetPropertiesToDefaultValuesProc setPropertiesToDefaultValues;
    XmlmGetInterfaceDataProc		 getInterfaceData;

    XmlmInterfaceAddedProc interfaceAdded;

    SignalProc interfaceAddedDelegate;
} XmlmBranchVTable;

typedef struct _XmlmInterface {
    xmlDocPtr	   doc;
    xmlXPathObject *obj;
    char	   *name;
} XmlmInterface;

typedef struct _XmlmBranch {
    CompInterfaceData base;
    XmlmInterface     *interface;
    int		      nInterface;
} XmlmBranch;

#define GET_XMLM_BRANCH(b)						 \
    ((XmlmBranch *) (b)->data.base.privates[xmlmBranchPrivateIndex].ptr)

#define XMLM_BRANCH(b)			 \
    XmlmBranch *xb = GET_XMLM_BRANCH (b)


static CompBool
xmlmInitBranch (CompObject *object)
{
    BRANCH (object);
    XMLM_BRANCH (b);

    xb->interface  = NULL;
    xb->nInterface = 0;

    return TRUE;
}

static void
xmlmFiniBranch (CompObject *object)
{
    BRANCH (object);
    XMLM_BRANCH (b);

    if (xb->interface)
    {
	while (xb->nInterface--)
	{
	    xmlXPathFreeObject (xb->interface[xb->nInterface].obj);
	    xmlFreeDoc (xb->interface[xb->nInterface].doc);
	    free (xb->interface[xb->nInterface].name);
	}

	free (xb->interface);
    }
}

static CompBool
xmlmLoadInterface (CompBranch *b,
		   const char *name,
		   const char *filename,
		   char	      **error)
{
    xmlDocPtr	       doc;
    xmlXPathObjectPtr  obj;
    xmlXPathContextPtr ctx;
    char	       path[256];
    int		       i;

    XMLM_BRANCH (b);

    doc = xmlReadFile (filename, NULL, 0);
    if (!doc)
    {
	esprintf (error, "Failed to read XML data from '%s'", filename);
	return FALSE;
    }

    ctx = xmlXPathNewContext (doc);
    if (!ctx)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    sprintf (path,
	     "/compiz/interface[@name=\"%s\"]/property[@name and @type and "
	     "(@type='b' or @type='i' or @type='d' or @type='s')]"
	     "/default[1]/text()",
	     name);

    obj = xmlXPathEvalExpression (BAD_CAST path, ctx);

    xmlXPathFreeContext (ctx);

    if (!obj)
    {
	esprintf (error, "Failed to evaluate '%s' XPath expression", path);
	xmlFreeDoc (doc);
	return FALSE;
    }

    for (i = 0; i < xb->nInterface; i++)
	if (strcmp (xb->interface[i].name, name) == 0)
	    break;

    if (i == xb->nInterface)
    {
	XmlmInterface *interface;

	interface = realloc (xb->interface, sizeof (XmlmInterface) *
			     (xb->nInterface + 1));
	if (!interface)
	{
	    xmlXPathFreeObject (obj);
	    xmlFreeDoc (doc);

	    esprintf (error, NO_MEMORY_ERROR_STRING);
	    return FALSE;
	}

	interface[xb->nInterface].name = strdup (name);
	if (!interface[xb->nInterface].name)
	{
	    xmlXPathFreeObject (obj);
	    xmlFreeDoc (doc);

	    esprintf (error, NO_MEMORY_ERROR_STRING);
	    return FALSE;
	}

	interface[xb->nInterface].doc = doc;
	interface[xb->nInterface].obj = obj;

	xb->interface = interface;
	xb->nInterface++;
    }
    else
    {
	xmlXPathFreeObject (xb->interface[i].obj);
	xmlFreeDoc (xb->interface[i].doc);

	xb->interface[i].doc = doc;
	xb->interface[i].obj = obj;
    }

    return TRUE;
}

static CompBool
xmlmUnloadInterface (CompBranch *b,
		     const char *name,
		     char       **error)
{
    XmlmInterface *interface;
    int		  i;

    XMLM_BRANCH (b);

    for (i = 0; i < xb->nInterface; i++)
	if (strcmp (xb->interface[i].name, name) == 0)
	    break;

    if (i == xb->nInterface)
    {
	esprintf (error, "Interface '%s' is not currently loaded", name);
	return FALSE;
    }

    xmlXPathFreeObject (xb->interface[i].obj);
    xmlFreeDoc (xb->interface[i].doc);
    free (xb->interface[i].name);

    xb->nInterface--;

    if (i < xb->nInterface)
	memmove (xb->interface, &xb->interface[i + 1],
		 sizeof (XmlmInterface) * (xb->nInterface - i));

    interface = realloc (xb->interface, sizeof (XmlmInterface) *
			 xb->nInterface);
    if (interface || !xb->nInterface)
	xb->interface = interface;

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
    const char	*file = name;
    struct stat buf;

    if (strchr (file, '.'))
	file = strrchr (file, '.') + 1;

    home = getenv ("HOME");
    if (home)
    {
	path = malloc (strlen (home) + strlen (HOME_XML_DATADIR) +
		       strlen (file) + strlen (XML_EXTENSION) + 3);
	if (!path)
	{
	    esprintf (error, NO_MEMORY_ERROR_STRING);
	    return FALSE;
	}

	sprintf (path, "%s/%s/%s%s", home, HOME_XML_DATADIR, file,
		 XML_EXTENSION);

	if (stat (path, &buf) == 0)
	{
	    status = xmlmLoadInterface (b, name, path, error);
	    free (path);
	    return status;
	}

	free (path);
    }

    path = malloc (strlen (XML_DATADIR) + strlen (file) +
		   strlen (XML_EXTENSION) + 2);
    if (!path)
    {
	esprintf (error, NO_MEMORY_ERROR_STRING);
	return FALSE;
    }

    sprintf (path, "%s/%s%s", XML_DATADIR, file, XML_EXTENSION);

    if (stat (path, &buf) == 0)
    {
	status = xmlmLoadInterface (b, name, path, error);
	free (path);
	return status;
    }

    free (path);
    return FALSE;
}

static CompBool
xmlmSetPropertiesToDefaultValues (CompBranch *b,
				  const char *path,
				  const char *name,
				  char       **error)
{
    CompObject *object;
    xmlNodePtr *nodeTab;
    int	       nodeNr, i;

    XMLM_BRANCH (b);

    object = compLookupDescendant (&b->u.base, path);
    if (!object)
	return TRUE;

    for (i = 0; i < xb->nInterface; i++)
	if (strcmp (xb->interface[i].name, name) == 0)
	    break;

    if (i == xb->nInterface)
	return TRUE;

    if (!xb->interface[i].obj->nodesetval)
	return TRUE;

    nodeTab = xb->interface[i].obj->nodesetval->nodeTab;
    nodeNr  = xb->interface[i].obj->nodesetval->nodeNr;

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

    return TRUE;
}

static CompBool
xmlmGetInterfaceData (CompBranch *b,
		      const char *name,
		      char	 **data,
		      char	 **error)
{
    int i, size;

    XMLM_BRANCH (b);

    for (i = 0; i < xb->nInterface; i++)
    {
	if (strcmp (xb->interface[i].name, name) == 0)
	{
	    xmlDocDumpMemory (xb->interface[i].doc, (xmlChar **) data, &size);
	    return TRUE;
	}
    }

    esprintf (error, "Interface '%s' is not currently loaded", name);
    return FALSE;
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
					       setPropertiesToDefaultValues));

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

    .loadInterface		  = xmlmLoadInterface,
    .unloadInterface		  = xmlmUnloadInterface,
    .loadInterfaceFromDefaultFile = xmlmLoadInterfaceFromDefaultFile,
    .setPropertiesToDefaultValues = xmlmSetPropertiesToDefaultValues,
    .getInterfaceData		  = xmlmGetInterfaceData,

    .interfaceAdded = xmlmInterfaceAdded,

    .interfaceAddedDelegate = xmlmInterfaceAddedDelegate
};

static const CMethod xmlMetadataBranchMethod[] = {
    C_METHOD (loadInterface, "ss", "", XmlmBranchVTable, marshal__SS__E),
    C_METHOD (unloadInterface, "s", "", XmlmBranchVTable, marshal__S__E),
    C_METHOD (loadInterfaceFromDefaultFile, "s", "", XmlmBranchVTable,
	      marshal__S__E),
    C_METHOD (setPropertiesToDefaultValues, "os", "", XmlmBranchVTable,
	      marshal__SS__E),
    C_METHOD (getInterfaceData, "s", "s", XmlmBranchVTable, marshal__S_S_E)
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
	getXmlmBranchObjectInterface
    };

    *n = N_ELEMENTS (interfaces);
    return interfaces;
}
