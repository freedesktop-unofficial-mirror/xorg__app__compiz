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

#include <string.h>
#include <gconf/gconf-client.h>

#include <compiz/core.h>
#include <compiz/c-object.h>

#define COMPIZ_GCONF_INTERFACE_NAME "org.compiz.gconf"
#define COMPIZ_GCONF_VERSION        20071123

const CompObjectInterface *
getGConfCoreObjectInterface (void);

#define APP_NAME        "compiz"
#define MAX_PATH_LENGTH 1024

static int gconfCorePrivateIndex;

typedef void (*GConfLoadInterfaceProc) (CompCore   *c,
					const char *path,
					const char *name);

typedef void (*GConfPropSignalProc) (CompCore	  *c,
				     const char	  *interface,
				     const char	  *name,
				     CompAnyValue *value,
				     CompObject   *descendant);

typedef void (*GConfInterfaceAddedProc) (CompCore   *c,
					 const char *path,
					 const char *name);

typedef struct _GConfCoreVTable {
    CompCoreVTable base;

    GConfLoadInterfaceProc loadInterface;
    GConfPropSignalProc    storeBoolProp;
    GConfPropSignalProc    storeIntProp;
    GConfPropSignalProc    storeDoubleProp;
    GConfPropSignalProc    storeStringProp;

    GConfPropSignalProc     propSignal;
    GConfInterfaceAddedProc interfaceAdded;

    SignalProc propSignalDelegate;
    SignalProc interfaceAddedDelegate;
} GConfCoreVTable;

typedef struct _GConfCore {
    CompInterfaceData base;
    GConfClient       *client;
    guint	      cnxn;
    CompTimeoutHandle reloadHandle;
} GConfCore;

#define GET_GCONF_CORE(c)					       \
    ((GConfCore *) (c)->data.base.privates[gconfCorePrivateIndex].ptr)

#define GCONF_CORE(c)		       \
    GConfCore *gc = GET_GCONF_CORE (c)


static gchar *
gconfGetObjectPath (CompObject *object)
{
    gchar *parentPath, *path;

    if (object->parent)
	parentPath = gconfGetObjectPath (object->parent);
    else
	parentPath = g_strdup ("/apps/" APP_NAME);

    if (!object->name || strcmp (object->name, COMPIZ_CORE_TYPE_NAME) == 0)
	return parentPath;

    path = g_strdup_printf ("%s/%s", parentPath, object->name);

    g_free (parentPath);

    return path;
}

static gchar *
gconfGetKey (CompObject  *object,
	     const gchar *interface,
	     const gchar *name)
{
    gchar *key, *path;

    path = gconfGetObjectPath (object);
    key = g_strdup_printf ("%s/%s_%s", path, interface, name);
    g_free (path);

    return key;
}

static CompBool
gconfSetPropToValue (CompObject       *object,
		     const char       *interface,
		     const char       *name,
		     const GConfValue *gvalue)
{
    switch (gvalue->type) {
    case GCONF_VALUE_BOOL:
	return (*object->vTable->setBool) (object,
					   interface,
					   name,
					   gconf_value_get_bool (gvalue),
					   NULL);
    case GCONF_VALUE_INT:
	return (*object->vTable->setInt) (object,
					  interface,
					  name,
					  gconf_value_get_int (gvalue),
					  NULL);
    case GCONF_VALUE_FLOAT:
	return (*object->vTable->setDouble) (object,
					     interface,
					     name,
					     gconf_value_get_float (gvalue),
					     NULL);
    case GCONF_VALUE_STRING:
	return (*object->vTable->setString) (object,
					     interface,
					     name,
					     gconf_value_get_string (gvalue),
					     NULL);
    case GCONF_VALUE_SCHEMA:
    case GCONF_VALUE_LIST:
    case GCONF_VALUE_PAIR:
    case GCONF_VALUE_INVALID:
	break;
    }

    return FALSE;
}

static CompBool
gconfSetPropToEntryValue (CompObject *object,
			  const char *interface,
			  const char *name,
			  GConfEntry *entry)
{
    GConfValue *gvalue;

    gvalue = gconf_entry_get_value (entry);
    if (!gvalue)
	return FALSE;

    return gconfSetPropToValue (object, interface, name, gvalue);
}

typedef struct _ReloadPropContext {
    CompCore   *core;
    const char *interface;
} ReloadPropContext;

static CompBool
gconfReloadProp (CompObject *object,
		 const char *name,
		 int	    type,
		 void	    *closure)
{
    ReloadPropContext *pCtx = (ReloadPropContext *) closure;
    CompCore          *c = pCtx->core;
    const char        *interface = pCtx->interface;
    gchar	      *key = gconfGetKey (object, interface, name);
    GConfEntry	      *entry;

    GCONF_CORE (c);

    entry = gconf_client_get_entry (gc->client, key, NULL, TRUE, NULL);
    if (entry)
    {
	gconfSetPropToEntryValue (object, interface, name, entry);
	gconf_entry_free (entry);
    }

    g_free (key);

    return TRUE;
}

typedef struct _ReloadInterfaceContext {
    CompCore   *core;
    CompObject *object;
} ReloadInterfaceContext;

static CompBool
gconfReloadInterface (CompObject	        *object,
		      const CompObjectInterface *interface,
		      void		        *closure)
{
    ReloadPropContext ctx;

    ctx.core      = GET_CORE (closure);
    ctx.interface = interface->name;

    (*object->vTable->forEachProp) (object, interface,
				    gconfReloadProp,
				    (void *) &ctx);

    return TRUE;
}

static CompBool
gconfReloadObjectTree (CompObject *object,
		       void	  *closure)
{
    (*object->vTable->forEachInterface) (object,
					 gconfReloadInterface,
					 closure);

    (*object->vTable->forEachChildObject) (object,
					   gconfReloadObjectTree,
					   closure);

    return TRUE;
}

static Bool
gconfReload (void *closure)
{
    CompCore *c = (CompCore *) closure;

    GCONF_CORE (c);

    gconfReloadObjectTree (&c->u.base.u.base, closure);

    gc->reloadHandle = 0;

    return FALSE;
}

typedef struct _LoadPropContext {
    GConfEntry  *entry;
    const gchar *name;
} LoadPropContext;

static CompBool
gconfLoadProp (CompObject	         *object,
	       const CompObjectInterface *interface,
	       void		         *closure)
{
    LoadPropContext *pCtx = (LoadPropContext *) closure;

    gconfSetPropToEntryValue (object, interface->name, pCtx->name,
			      pCtx->entry);

    return TRUE;
}

static void
gconfKeyChanged (GConfClient *client,
		 guint	     cnxnId,
		 GConfEntry  *entry,
		 gpointer    userData)
{
    CompCore   *c = (CompCore *) userData;
    CompObject *object;
    gchar      **token, *prop, *path;
    guint      nToken;

    token  = g_strsplit (entry->key, "/", MAX_PATH_LENGTH);
    nToken = g_strv_length (token);

    prop = token[nToken - 1];
    token[nToken - 1] = NULL;

    path = g_strjoinv ("/", &token[3]);

    object = compLookupDescendant (&c->u.base.u.base, path);
    if (object)
    {
	LoadPropContext ctx;

	ctx.entry = entry;
	ctx.name  = prop;

	/* try to load property for each interface */
	(*object->vTable->forEachInterface) (object,
					     gconfLoadProp,
					     (void *) &ctx);
    }

    g_free (path);

    token[nToken - 1] = prop;

    g_strfreev (token);
}

static void
gconfGetProp (CompObject   *object,
	      unsigned int what,
	      void	   *value)
{
    cGetInterfaceProp (&GET_GCONF_CORE (GET_CORE (object))->base,
		       getGConfCoreObjectInterface (),
		       what, value);
}

static void
gconfLoadInterface (CompCore   *c,
		    const char *path,
		    const char *name)
{
    CompObject *object;

    object = compLookupDescendant (&c->u.base.u.base, path);
    if (object)
	compForInterface (object,
			  name,
			  gconfReloadInterface,
			  (void *) c);
}

static void
gconfStoreProp (CompCore   *c,
		CompObject *descendant,
		const char *interface,
		const char *name,
		GConfValue *gvalue)
{
    gchar *key;

    GCONF_CORE (c);

    key = gconfGetKey (descendant, interface, name);
    if (key)
    {
	gconf_client_set (gc->client, key, gvalue, NULL);
	g_free (key);
    }
}

static void
gconfStoreBoolProp (CompCore     *c,
		    const char   *interface,
		    const char   *name,
		    CompAnyValue *value,
		    CompObject   *descendant)
{
    GConfValue *gvalue;

    gvalue = gconf_value_new (GCONF_VALUE_BOOL);
    if (gvalue)
    {
	gconf_value_set_bool (gvalue, value->b);
	gconfStoreProp (c, descendant, interface, name, gvalue);
	gconf_value_free (gvalue);
    }
}

static void
gconfStoreIntProp (CompCore     *c,
		   const char   *interface,
		   const char   *name,
		   CompAnyValue *value,
		   CompObject   *descendant)
{
    GConfValue *gvalue;

    gvalue = gconf_value_new (GCONF_VALUE_INT);
    if (gvalue)
    {
	gconf_value_set_int (gvalue, value->i);
	gconfStoreProp (c, descendant, interface, name, gvalue);
	gconf_value_free (gvalue);
    }
}

static void
gconfStoreDoubleProp (CompCore     *c,
		      const char   *interface,
		      const char   *name,
		      CompAnyValue *value,
		      CompObject   *descendant)
{
    GConfValue *gvalue;

    gvalue = gconf_value_new (GCONF_VALUE_FLOAT);
    if (gvalue)
    {
	gconf_value_set_float (gvalue, value->d);
	gconfStoreProp (c, descendant, interface, name, gvalue);
	gconf_value_free (gvalue);
    }
}

static void
gconfStoreStringProp (CompCore     *c,
		      const char   *interface,
		      const char   *name,
		      CompAnyValue *value,
		      CompObject   *descendant)
{
    GConfValue *gvalue;

    gvalue = gconf_value_new (GCONF_VALUE_STRING);
    if (gvalue)
    {
	gconf_value_set_string (gvalue, value->s);
	gconfStoreProp (c, descendant, interface, name, gvalue);
	gconf_value_free (gvalue);
    }
}

static void
gconfPropSignal (CompCore     *c,
		 const char   *interface,
		 const char   *name,
		 CompAnyValue *value,
		 CompObject   *descendant)
{
    C_EMIT_SIGNAL (&c->u.base.u.base, GConfPropSignalProc,
		   offsetof (GConfCoreVTable, propSignal),
		   interface, name, value, descendant);
}

static void
gconfInterfaceAdded (CompCore   *c,
		     const char *path,
		     const char *name)
{
    C_EMIT_SIGNAL (&c->u.base.u.base, GConfInterfaceAddedProc,
		   offsetof (GConfCoreVTable, interfaceAdded),
		   path, name);
}

static void
gconfPropSignalDelegate (CompObject   *object,
			 const char   *path,
			 const char   *interface,
			 const char   *name,
			 const char   *signature,
			 CompAnyValue *value,
			 int	      nValue)
{
    const char *decendantPath;

    decendantPath = compTranslateObjectPath (object->parent, object, path);
    if (decendantPath)
    {
	CompObject *descendant;

	CORE (object);

	descendant = compLookupDescendant (object, decendantPath);
	if (descendant)
	    gconfPropSignal (c,
			     value[0].s,
			     value[1].s,
			     &value[2],
			     descendant);
    }
}

static void
gconfInterfaceAddedDelegate (CompObject   *object,
			     const char   *path,
			     const char   *interface,
			     const char   *name,
			     const char   *signature,
			     CompAnyValue *value,
			     int	  nValue)
{
    const char *decendantPath;

    CORE (object);

    decendantPath = compTranslateObjectPath (object->parent, object, path);
    if (decendantPath)
	gconfInterfaceAdded (c, decendantPath, value[0].s);
}

static CompBool
gconfInitCore (CompObject *object)
{
    CORE (object);
    GCONF_CORE (c);

    gc->client = gconf_client_get_default ();

    gconf_client_add_dir (gc->client, "/apps/" APP_NAME,
			  GCONF_CLIENT_PRELOAD_NONE, NULL);

    gc->reloadHandle = compAddTimeout (0, gconfReload, (void *) c);

    gc->cnxn = gconf_client_notify_add (gc->client, "/apps/" APP_NAME,
					gconfKeyChanged, c, NULL, NULL);

    compInvokeMethod (object, "glib", "wakeUp", "", "", NULL);

    return TRUE;
}

static void
gconfFiniCore (CompObject *object)
{
    GCONF_CORE (GET_CORE (object));

    if (gc->reloadHandle)
	compRemoveTimeout (gc->reloadHandle);

    if (gc->cnxn)
	gconf_client_notify_remove (gc->client, gc->cnxn);

    gconf_client_remove_dir (gc->client, "/apps/" APP_NAME, NULL);
    gconf_client_clear_cache (gc->client);
}

static void
gconfInsertCore (CompObject *object,
		 CompObject *parent)
{
    static const struct {
	const char *signal;
	size_t	   offset;
    } properties[] = {
	{ "boolPropChanged",   offsetof (GConfCoreVTable, storeBoolProp)   },
	{ "intPropChanged",    offsetof (GConfCoreVTable, storeIntProp)    },
	{ "doublePropChanged", offsetof (GConfCoreVTable, storeDoubleProp) },
	{ "stringPropChanged", offsetof (GConfCoreVTable, storeStringProp) }
    };
    int i;

    CORE (object);

    compConnect (parent,
		 getObjectType (),
		 offsetof (CompObjectVTable, signal),
		 &c->u.base.u.base,
		 getGConfCoreObjectInterface (),
		 offsetof (GConfCoreVTable, interfaceAddedDelegate),
		 "osss", "//*", getObjectType ()->name, "interfaceAdded", "s");

    compConnect (&c->u.base.u.base,
		 getGConfCoreObjectInterface (),
		 offsetof (GConfCoreVTable, interfaceAdded),
		 &c->u.base.u.base,
		 getGConfCoreObjectInterface (),
		 offsetof (GConfCoreVTable, loadInterface),
		 NULL);

    for (i = 0; i < N_ELEMENTS (properties); i++)
    {
	compConnect (parent,
		     getObjectType (),
		     offsetof (CompObjectVTable, signal),
		     &c->u.base.u.base,
		     getGConfCoreObjectInterface (),
		     offsetof (GConfCoreVTable, propSignalDelegate),
		     "oss", "//*", getObjectType ()->name,
		     properties[i].signal);

	compConnect (&c->u.base.u.base,
		     getGConfCoreObjectInterface (),
		     offsetof (GConfCoreVTable, propSignal),
		     &c->u.base.u.base,
		     getGConfCoreObjectInterface (),
		     properties[i].offset,
		     "s", properties[i].signal);
    }
}

static const GConfCoreVTable gconfCoreObjectVTable = {
    .base.base.base.getProp = gconfGetProp,

    .loadInterface   = gconfLoadInterface,
    .storeBoolProp   = gconfStoreBoolProp,
    .storeIntProp    = gconfStoreIntProp,
    .storeDoubleProp = gconfStoreDoubleProp,
    .storeStringProp = gconfStoreStringProp,

    .propSignal     = gconfPropSignal,
    .interfaceAdded = gconfInterfaceAdded,

    .propSignalDelegate     = gconfPropSignalDelegate,
    .interfaceAddedDelegate = gconfInterfaceAddedDelegate
};

static const CSignal gconfCoreSignal[] = {
    C_SIGNAL (propSignal,     0, GConfCoreVTable),
    C_SIGNAL (interfaceAdded, 0, GConfCoreVTable)
};

const CompObjectInterface *
getGConfCoreObjectInterface (void)
{
    static CompObjectInterface *interface = NULL;

    if (!interface)
    {
	static const CObjectInterface template = {
	    .i.name	    = COMPIZ_GCONF_INTERFACE_NAME,
	    .i.version	    = COMPIZ_GCONF_VERSION,
	    .i.base.name    = COMPIZ_CORE_TYPE_NAME,
	    .i.base.version = COMPIZ_CORE_VERSION,
	    .i.vTable.impl  = &gconfCoreObjectVTable.base.base.base,
	    .i.vTable.size  = sizeof (gconfCoreObjectVTable),

	    .signal  = gconfCoreSignal,
	    .nSignal = N_ELEMENTS (gconfCoreSignal),

	    .init = gconfInitCore,
	    .fini = gconfFiniCore,

	    .insert = gconfInsertCore
	};

	g_type_init ();

	interface = cObjectInterfaceFromTemplate (&template,
						  &gconfCorePrivateIndex,
						  sizeof (GConfCore));
    }

    return interface;
}

const GetInterfaceProc *
getCompizObjectInterfaces20080220 (int *n)
{
    static const GetInterfaceProc interfaces[] = {
	getGConfCoreObjectInterface
    };

    *n = N_ELEMENTS (interfaces);
    return interfaces;
}
