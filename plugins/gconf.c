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

static CompMetadata gconfMetadata;

#define APP_NAME        "compiz"
#define MAX_PATH_LENGTH 1024

static int corePrivateIndex;

typedef struct _GConfCore {
    GConfClient *client;
    guint	cnxn;

    CompTimeoutHandle reloadHandle;
    int		      signalHandle;
} GConfCore;

#define GET_GCONF_CORE(c)				\
    ((GConfCore *) (c)->privates[corePrivateIndex].ptr)

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

    if (strcmp (object->name, "core") == 0)
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

    gconfReloadObjectTree (&c->u.base, closure);

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
    gchar      **token, *prop;
    guint      nToken;

    token  = g_strsplit (entry->key, "/", MAX_PATH_LENGTH);
    nToken = g_strv_length (token);

    prop = token[nToken - 1];
    token[nToken - 1] = NULL;

    object = compLookupObject (&c->u.base, &token[3]);
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

    token[nToken - 1] = prop;

    g_strfreev (token);
}

static void
gconfPropChanged (CompCore   *c,
		  CompObject *object,
		  const char *signal,
		  const char *signature,
		  va_list    args)
{
    GConfValue *gvalue = NULL;
    const char *interface;
    const char *name;
    va_list    ap;

    va_copy (ap, args);

    interface = va_arg (ap, const char *);
    name      = va_arg (ap, const char *);

    if (signature[2] == 'b' && strcmp (signal, "boolChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_BOOL);
	if (gvalue)
	    gconf_value_set_bool (gvalue, va_arg (ap, CompBool));
    }
    else if (signature[2] == 'i' && strcmp (signal, "intChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_INT);
	if (gvalue)
	    gconf_value_set_int (gvalue, va_arg (ap, int32_t));
    }
    else if (signature[2] == 'd' && strcmp (signal, "doubleChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_FLOAT);
	if (gvalue)
	    gconf_value_set_float (gvalue, va_arg (ap, double));
    }
    else if (signature[2] == 's' && strcmp (signal, "stringChanged") == 0)
    {
	gvalue = gconf_value_new (GCONF_VALUE_STRING);
	if (gvalue)
	    gconf_value_set_string (gvalue, va_arg (ap, const char *));
    }

    va_end (ap);

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
gconfHandleSignal (CompObject *object,
		   void	      *data,
		   ...)
{
    CompObject *source;
    const char *interface;
    const char *name;
    const char *signature;
    va_list    ap, args;

    CORE (object);

    va_start (ap, data);

    source    = va_arg (ap, CompObject *);
    interface = va_arg (ap, const char *);
    name      = va_arg (ap, const char *);
    signature = va_arg (ap, const char *);
    args      = va_arg (ap, va_list);

    va_end (ap);

    if (strcmp (interface, "object") == 0)
    {
	if (strcmp (name,      "childObjectAdded") == 0 &&
	    strcmp (signature, "o")		   == 0)
	{
	    va_copy (ap, args);
	    gconfReloadObjectTree (va_arg (ap, CompObject *), (void *) c);
	    va_end (ap);
	}
    }
    else if (strcmp (interface, "properties") == 0)
    {
	if (strncmp (signature, "ss", 2) == 0)
	    gconfPropChanged (c, source, name, signature, args);
    }
}

static CompBool
gconfInitCore (CompCore *c)
{
    CompBasicArgs args;

    GCONF_CORE (c);

    if (!compObjectCheckVersion (&c->u.base, "object", CORE_ABIVERSION))
	return FALSE;

    gc->signalHandle =
	(*c->u.base.vTable->signal.connect) (&c->u.base, "signal",
					     offsetof (CompSignalVTable, signal),
					     gconfHandleSignal,
					     NULL);
    if (gc->signalHandle < 0)
	return FALSE;

    g_type_init ();

    gc->client = gconf_client_get_default ();

    gconf_client_add_dir (gc->client, "/apps/" APP_NAME,
			  GCONF_CLIENT_PRELOAD_NONE, NULL);

    gc->reloadHandle = compAddTimeout (0, gconfReload, (void *) c);

    gc->cnxn = gconf_client_notify_add (gc->client, "/apps/" APP_NAME,
					gconfKeyChanged, c, NULL, NULL);

    compInitBasicArgs (&args, NULL, NULL);
    compInvokeMethod (&c->u.base, "glib", "wakeUp", "", "", &args.base);
    compFiniBasicArgs (&args);

    return TRUE;
}

static void
gconfFiniCore (CompCore *c)
{
    GCONF_CORE (c);

    (*c->u.base.vTable->signal.disconnect) (&c->u.base, "signal",
					    offsetof (CompSignalVTable, signal),
					    gc->signalHandle);

    if (gc->reloadHandle)
	compRemoveTimeout (gc->reloadHandle);

    if (gc->cnxn)
	gconf_client_notify_remove (gc->client, gc->cnxn);

    gconf_client_remove_dir (gc->client, "/apps/" APP_NAME, NULL);
    gconf_client_clear_cache (gc->client);
}

static CObjectPrivate gconfObj[] = {
    {
	"core",
	&corePrivateIndex, sizeof (GConfCore), NULL, NULL,
	(InitObjectProc) gconfInitCore,
	(FiniObjectProc) gconfFiniCore
    }
};

static Bool
gconfInit (CompPlugin *p)
{
    if (!compInitObjectMetadataFromInfo (&gconfMetadata, p->vTable->name,
					 0, 0))
	return FALSE;

    compAddMetadataFromFile (&gconfMetadata, p->vTable->name);

    if (!cObjectInitPrivates (gconfObj, N_ELEMENTS (gconfObj)))
    {
	compFiniMetadata (&gconfMetadata);
	return FALSE;
    }

    return TRUE;
}

static void
gconfFini (CompPlugin *p)
{
    cObjectFiniPrivates (gconfObj, N_ELEMENTS (gconfObj));
    compFiniMetadata (&gconfMetadata);
}

CompPluginVTable gconfVTable = {
    "gconf",
    0, /* GetMetadata */
    gconfInit,
    gconfFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &gconfVTable;
}
