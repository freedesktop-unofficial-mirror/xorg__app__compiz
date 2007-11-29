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

#define COMPIZ_GCONF_VERSION 20071123

#define APP_NAME        "compiz"
#define MAX_PATH_LENGTH 1024

static int gconfCorePrivateIndex;

typedef struct _GConfCoreVTable {
    CompCoreVTable base;
    SignalProc     handleSignal;
} GConfCoreVTable;

typedef struct _GConfCore {
    CompObjectVTableVec object;

    GConfClient *client;
    guint	cnxn;

    CompTimeoutHandle reloadHandle;
} GConfCore;

#define GET_GCONF_CORE(c)				     \
    ((GConfCore *) (c)->privates[gconfCorePrivateIndex].ptr)

#define GCONF_CORE(c)		       \
    GConfCore *gc = GET_GCONF_CORE (c)


static CInterface gconfCoreInterface[] = {
    C_INTERFACE (gconf, Core, GConfCoreVTable, _, _, _, _, _, _, _, _)
};

static gchar *
gconfGetObjectPath (CompObject *object)
{
    gchar *parentPath, *path;

    if (object->parent)
	parentPath = gconfGetObjectPath (object->parent);
    else
	parentPath = g_strdup ("/apps/" APP_NAME);

    if (!object->name || strcmp (object->name, "core") == 0)
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

    /* use interface as prefix when it's not equal to a type name */
    if (compObjectFindType (interface))
	key = g_strdup_printf ("%s/%s", path, name);
    else
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
    const CompPropertiesVTable *vTable = &object->vTable->properties;

    switch (gvalue->type) {
    case GCONF_VALUE_BOOL:
	return (*vTable->setBool) (object,
				   interface,
				   name,
				   gconf_value_get_bool (gvalue),
				   NULL);
    case GCONF_VALUE_INT:
	return (*vTable->setInt) (object,
				  interface,
				  name,
				  gconf_value_get_int (gvalue),
				  NULL);
    case GCONF_VALUE_FLOAT:
	return (*vTable->setDouble) (object,
				     interface,
				     name,
				     gconf_value_get_float (gvalue),
				     NULL);
    case GCONF_VALUE_STRING:
	return (*vTable->setString) (object,
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
gconfReloadInterface (CompObject	   *object,
		      const char	   *name,
		      size_t		   offset,
		      const CompObjectType *type,
		      void		   *closure)
{
    ReloadPropContext ctx;

    ctx.core      = GET_CORE (closure);
    ctx.interface = name;

    (*object->vTable->forEachProp) (object, name,
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
gconfLoadProp (CompObject	    *object,
	       const char	    *name,
	       size_t		    offset,
	       const CompObjectType *type,
	       void		    *closure)
{
    LoadPropContext *pCtx = (LoadPropContext *) closure;

    gconfSetPropToEntryValue (object, name, pCtx->name, pCtx->entry);

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

    object = compLookupObject (&c->u.base.u.base, path);
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
gconfPropChanged (CompCore     *c,
		  CompObject   *object,
		  const char   *signal,
		  const char   *signature,
		  CompAnyValue *value,
		  int	       nValue)
{
    GConfValue *gvalue = NULL;
    const char *interface = value[0].s;
    const char *name = value[1].s;

    if (signature[2] == 'b' && strcmp (signal, "boolChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_BOOL);
	if (gvalue)
	    gconf_value_set_bool (gvalue, value[2].b);
    }
    else if (signature[2] == 'i' && strcmp (signal, "intChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_INT);
	if (gvalue)
	    gconf_value_set_int (gvalue, value[2].i);
    }
    else if (signature[2] == 'd' && strcmp (signal, "doubleChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_FLOAT);
	if (gvalue)
	    gconf_value_set_float (gvalue, value[2].d);
    }
    else if (signature[2] == 's' && strcmp (signal, "stringChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_STRING);
	if (gvalue)
	    gconf_value_set_string (gvalue, value[2].s);
    }

    if (gvalue)
    {
	gchar *key;

	GCONF_CORE (c);

	key = gconfGetKey (object, interface, name);
	if (key)
	{
	    gconf_client_set (gc->client, key, gvalue, NULL);
	    g_free (key);
	}

	gconf_value_free (gvalue);
    }
}

static void
gconfHandleSignal (CompObject   *object,
		   const char   *path,
		   const char   *interface,
		   const char   *name,
		   const char   *signature,
		   CompAnyValue *value,
		   int	        nValue)
{
    CompObject *source;

    CORE (object);

    if (strcmp (interface, "object") == 0)
    {
	if (strcmp (name,      "interfaceAdded") == 0 &&
	    strcmp (signature, "s")		 == 0)
	{
	    source = compLookupObject (object, path);
	    if (source)
		gconfReloadInterface (source, value[0].s, 0, NULL, (void *) c);
	}
    }
    else if (strcmp (interface, "properties") == 0)
    {
	if (strncmp (signature, "ss", 2) == 0)
	{
	    source = compLookupObject (object, path);
	    if (source)
		gconfPropChanged (c, source, name, signature, value, nValue);
	}
    }
}

static GConfCoreVTable gconfCoreObjectVTable = {
    .handleSignal = gconfHandleSignal
};

static CompBool
gconfInitCore (CompCore *c)
{
    GCONF_CORE (c);

    if (!compObjectCheckVersion (&c->u.base.u.base, "object", CORE_ABIVERSION))
	return FALSE;

   if (!cObjectInterfaceInit (&c->u.base.u.base,
			      &gconfCoreObjectVTable.base.base.base))
	return FALSE;

    g_type_init ();

    gc->client = gconf_client_get_default ();

    gconf_client_add_dir (gc->client, "/apps/" APP_NAME,
			  GCONF_CLIENT_PRELOAD_NONE, NULL);

    gc->reloadHandle = compAddTimeout (0, gconfReload, (void *) c);

    gc->cnxn = gconf_client_notify_add (gc->client, "/apps/" APP_NAME,
					gconfKeyChanged, c, NULL, NULL);

    compInvokeMethod (&c->u.base.u.base, "glib", "wakeUp", "", "", NULL);

    return TRUE;
}

static void
gconfFiniCore (CompCore *c)
{
    GCONF_CORE (c);

    if (gc->reloadHandle)
	compRemoveTimeout (gc->reloadHandle);

    if (gc->cnxn)
	gconf_client_notify_remove (gc->client, gc->cnxn);

    gconf_client_remove_dir (gc->client, "/apps/" APP_NAME, NULL);
    gconf_client_clear_cache (gc->client);

    cObjectInterfaceFini (&c->u.base.u.base);
}

static void
gconfCoreGetCContext (CompObject *object,
		      CContext   *ctx)
{
    GCONF_CORE (GET_CORE (object));

    ctx->interface  = gconfCoreInterface;
    ctx->nInterface = N_ELEMENTS (gconfCoreInterface);
    ctx->type	    = NULL;
    ctx->data	    = (char *) gc;
    ctx->svOffset   = 0;
    ctx->vtStore    = &gc->object;
    ctx->version    = COMPIZ_GCONF_VERSION;
}

static CObjectPrivate gconfObj[] = {
    C_OBJECT_PRIVATE ("core", gconf, Core, GConfCore, X, X)
};

static CompBool
gconfInsert (CompObject *parent,
	     CompBranch *branch)
{
    if (!cObjectInitPrivates (gconfObj, N_ELEMENTS (gconfObj)))
	return FALSE;

    compConnect (parent,
		 "signal",
		 offsetof (CompSignalVTable, signal),
		 &branch->u.base,
		 "gconf",
		 offsetof (GConfCoreVTable, handleSignal),
		 NULL);

    return TRUE;
}

static void
gconfRemove (CompObject *parent,
	     CompBranch *branch)
{
    cObjectFiniPrivates (gconfObj, N_ELEMENTS (gconfObj));
}

static Bool
gconfInit (CompPlugin *p)
{
    return TRUE;
}

static void
gconfFini (CompPlugin *p)
{
}

CompPluginVTable gconfVTable = {
    "gconf",
    0, /* GetMetadata */
    gconfInit,
    gconfFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0, /* SetObjectOption */
    gconfInsert,
    gconfRemove
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &gconfVTable;
}
