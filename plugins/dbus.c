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
#include <poll.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <libxml/xmlwriter.h>

#include <compiz/core.h>
#include <compiz/c-object.h>

#define COMPIZ_DBUS_VERSION 20071123

#define COMPIZ_DBUS_SERVICE_NAME   "org.freedesktop.compiz"
#define COMPIZ_DBUS_INTERFACE_BASE "org.freedesktop.compiz."
#define COMPIZ_DBUS_PATH_ROOT	   "/org/freedesktop/compiz"

static int dbusCorePrivateIndex;

typedef SignalProc DBusSignalProc;

typedef void (*DBusObjectSignalProc) (CompObject *object,
				      const char *path,
				      const char *name);

typedef struct _DBusCoreVTable {
    CompCoreVTable base;

    DBusSignalProc	 signal;
    SignalProc		 signalDelegate;
    DBusObjectSignalProc objectSignal;
    DBusSignalProc       objectSignalDelegate;
    DBusObjectSignalProc registerObjectPath;
    DBusObjectSignalProc unregisterObjectPath;
    DBusSignalProc       emitSignal;
} DBusCoreVTable;

typedef struct _DBusCore {
    CompInterfaceData base;
    DBusConnection    *connection;
    CompWatchFdHandle watchFdHandle;
} DBusCore;

#define GET_DBUS_CORE(c)					     \
    ((DBusCore *) (c)->data.base.privates[dbusCorePrivateIndex].ptr)

#define DBUS_CORE(c)		     \
    DBusCore *dc = GET_DBUS_CORE (c)


static CSignal dbusSignal = C_SIGNAL (signal, 0, DBusCoreVTable);
static CSignal dbusObjectSignal = C_SIGNAL (objectSignal, 0, DBusCoreVTable);

static CSignal *dbusCoreSignal[] = {
    &dbusSignal,
    &dbusObjectSignal
};

static CInterface dbusCoreInterface[] = {
    C_INTERFACE (dbus, Core, DBusCoreVTable, _, _, X, _, _, _, _, _)
};

static char *
dbusGetPath (const char *path)
{
    char *dbusPath;

    dbusPath = malloc (strlen (COMPIZ_DBUS_PATH_ROOT) + strlen (path) + 2);
    if (!dbusPath)
	return NULL;

    sprintf (dbusPath, "%s/%s", COMPIZ_DBUS_PATH_ROOT, path);

    return dbusPath;
}

static char *
dbusGetObjectPath (CompObject *object)
{
    char *parentPath, *path;
    int  parentPathLen;

    if (object->parent)
	parentPath = dbusGetObjectPath (object->parent);
    else
	parentPath = strdup (COMPIZ_DBUS_PATH_ROOT);

    if (!parentPath)
	return NULL;

    if (!object->name || strcmp (object->name, CORE_TYPE_NAME) == 0)
	return parentPath;

    parentPathLen = strlen (parentPath);

    path = realloc (parentPath, parentPathLen + strlen (object->name) + 2);
    if (!path)
    {
	free (parentPath);
	return NULL;
    }

    path[parentPathLen] = '/';
    strcpy (path + parentPathLen + 1, object->name);

    return path;
}

static char *
dbusGetInterface (const char *name)
{
    char *interface;

    interface = malloc (strlen (COMPIZ_DBUS_INTERFACE_BASE) +
			strlen (name) + 1);
    if (interface)
	sprintf (interface, "%s%s", COMPIZ_DBUS_INTERFACE_BASE, name);

    return interface;
}

static void
dbusArgToXml (xmlTextWriterPtr  writer,
	      const char        *type,
	      const char        *direction)
{
    xmlTextWriterStartElement (writer, BAD_CAST "arg");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "type", BAD_CAST type);
    xmlTextWriterWriteAttribute (writer, BAD_CAST "direction",
				 BAD_CAST direction);
    xmlTextWriterEndElement (writer);
}

static void
dbusSignatureToXml (xmlTextWriterPtr  writer,
		    const char        *signature,
		    const char        *direction)
{
    DBusSignatureIter iter;

    dbus_signature_iter_init (&iter, signature);

    do {
	char *argSignature;

	argSignature = dbus_signature_iter_get_signature (&iter);
	if (argSignature)
	{
	    dbusArgToXml (writer, argSignature, direction);
	    dbus_free (argSignature);
	}
    } while (dbus_signature_iter_next (&iter));
}

static void
dbusMethodToXml (xmlTextWriterPtr writer,
		 const char	  *name,
		 const char	  *in,
		 const char	  *out)
{
    if (!dbus_signature_validate (in, NULL))
	return;

    if (!dbus_signature_validate (out, NULL))
	return;

    xmlTextWriterStartElement (writer, BAD_CAST "method");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST name);

    if (strlen (in))
	dbusSignatureToXml (writer, in, "in");

    if (strlen (out))
	dbusSignatureToXml (writer, out, "out");

    xmlTextWriterEndElement (writer);
}

static CompBool
dbusObjectMethodToXml (CompObject	 *object,
		       const char	 *name,
		       const char	 *in,
		       const char	 *out,
		       size_t		 offset,
		       MethodMarshalProc marshal,
		       void		 *closure)
{
    xmlTextWriterPtr writer = (xmlTextWriterPtr) closure;

    dbusMethodToXml (writer, name, in, out);

    return TRUE;
}

static CompBool
dbusCheckForObjectMethod (CompObject	    *object,
			  const char	    *name,
			  const char	    *in,
			  const char	    *out,
			  size_t	    offset,
			  MethodMarshalProc marshal,
			  void		    *closure)
{
    return FALSE;
}

static void
dbusSignalToXml (xmlTextWriterPtr writer,
		 const char	  *name,
		 const char	  *out)
{
    if (!dbus_signature_validate (out, NULL))
	return;

    xmlTextWriterStartElement (writer, BAD_CAST "signal");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST name);

    if (strlen (out))
	dbusSignatureToXml (writer, out, "out");

    xmlTextWriterEndElement (writer);
}

static CompBool
dbusObjectSignalToXml (CompObject *object,
		       const char *name,
		       const char *out,
		       size_t	  offset,
		       void	  *closure)
{
    xmlTextWriterPtr writer = (xmlTextWriterPtr) closure;

    dbusSignalToXml (writer, name, out);

    return TRUE;
}

static CompBool
dbusCheckForObjectSignal (CompObject *object,
			  const char *name,
			  const char *out,
			  size_t     offset,
			  void	     *closure)
{
    return FALSE;
}

static void
dbusPropToXml (xmlTextWriterPtr writer,
	       const char	*name,
	       const char	*type)
{
    if (!dbus_signature_validate (type, NULL))
	return;

    xmlTextWriterStartElement (writer, BAD_CAST "property");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST name);
    xmlTextWriterWriteAttribute (writer, BAD_CAST "type", BAD_CAST type);
    xmlTextWriterEndElement (writer);
}

static CompBool
dbusObjectPropToXml (CompObject *object,
		     const char *name,
		     int	type,
		     void	*closure)
{
    xmlTextWriterPtr writer = (xmlTextWriterPtr) closure;
    char	     sig[2];

    sig[0] = type;
    sig[1] = '\0';

    dbusPropToXml (writer, name, sig);

    return TRUE;
}

static CompBool
dbusCheckForObjectProp (CompObject *object,
			const char *name,
			int	   type,
			void	   *closure)
{
    return FALSE;
}

static CompBool
dbusObjectInterfaceToXml (CompObject	       *object,
			  const char	       *name,
			  size_t	       offset,
			  const CompObjectType *type,
			  void		       *closure)
{
    xmlTextWriterPtr writer = (xmlTextWriterPtr) closure;
    char	     *dbusInterface;

    /* avoid interfaces without public members */
    if ((*object->vTable->forEachMethod) (object,
					  name,
					  dbusCheckForObjectMethod,
					  NULL) &&
	(*object->vTable->forEachSignal) (object,
					  name,
					  dbusCheckForObjectSignal,
					  NULL) &&
	(*object->vTable->forEachProp) (object,
					name,
					dbusCheckForObjectProp,
					NULL))
	return TRUE;

    dbusInterface = dbusGetInterface (name);
    if (dbusInterface)
    {
	xmlTextWriterStartElement (writer, BAD_CAST "interface");
	xmlTextWriterWriteAttribute (writer, BAD_CAST "name",
				     BAD_CAST dbusInterface);

	(*object->vTable->forEachMethod) (object,
					  name,
					  dbusObjectMethodToXml,
					  closure);

	(*object->vTable->forEachSignal) (object,
					  name,
					  dbusObjectSignalToXml,
					  closure);

	(*object->vTable->forEachProp) (object,
					name,
					dbusObjectPropToXml,
					closure);

	xmlTextWriterEndElement (writer);

	free (dbusInterface);
    }

    return TRUE;
}

static DBusMessage *
dbusObjectToXml (DBusConnection *connection,
		 DBusMessage    *message,
		 CompObject     *object)
{
    DBusMessage     *reply;
    xmlBufferPtr     buffer;
    xmlTextWriterPtr writer;
    char	     *path;

    buffer = xmlBufferCreate ();
    if (!buffer)
	return dbus_message_new_error (message,
				       DBUS_ERROR_FAILED,
				       "Failed to create XML buffer");

    writer = xmlNewTextWriterMemory (buffer, 0);
    if (!writer)
    {
	xmlBufferFree (buffer);
	return dbus_message_new_error (message,
				       DBUS_ERROR_FAILED,
				       "Failed to create XML text writer");
    }

    xmlTextWriterSetIndent (writer, TRUE);

    xmlTextWriterStartDTD (writer,
			   BAD_CAST "node",
			   BAD_CAST DBUS_INTROSPECT_1_0_XML_PUBLIC_IDENTIFIER,
			   BAD_CAST DBUS_INTROSPECT_1_0_XML_SYSTEM_IDENTIFIER);
    xmlTextWriterEndDTD (writer);

    xmlTextWriterStartElement (writer, BAD_CAST "node");

    (*object->vTable->forEachInterface) (object,
					 dbusObjectInterfaceToXml,
					 (void *) writer);

    xmlTextWriterStartElement (writer, BAD_CAST "interface");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST
				 DBUS_INTERFACE_INTROSPECTABLE);

    dbusMethodToXml (writer, "Introspect", "", "s");

    xmlTextWriterEndElement (writer);

    path = dbusGetObjectPath (object);
    if (path)
    {
	char **childEntries;

	if (dbus_connection_list_registered (connection, path, &childEntries))
	{
	    int i;

	    for (i = 0; childEntries[i]; i++)
	    {
		xmlTextWriterStartElement (writer, BAD_CAST "node");
		xmlTextWriterWriteAttribute (writer, BAD_CAST "name",
					     BAD_CAST childEntries[i]);
		xmlTextWriterEndElement (writer);
	    }

	    dbus_free_string_array (childEntries);
	}

	free (path);
    }

    xmlTextWriterEndElement (writer);

    xmlFreeTextWriter (writer);

    reply = dbus_message_new_method_return (message);
    if (reply)
	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING,
				  &buffer->content,
				  DBUS_TYPE_INVALID);

    xmlBufferFree (buffer);

    return reply;
}

typedef struct _DBusArgs {
    CompArgs base;

    char *path;
    int  pathLen;

    DBusMessage *message;
    DBusMessage *reply;

    DBusMessageIter messageIter;
    DBusMessageIter replyIter;
} DBusArgs;

static void
dbusArgsIntError (DBusArgs   *dArgs,
		  const char *name,
		  const char *message)
{
    if (dArgs->reply)
	dbus_message_unref (dArgs->reply);

    dArgs->reply = dbus_message_new_error (dArgs->message, name, message);
}

static void
dbusArgsLoad (CompArgs *args,
	      int      type,
	      void     *value)
{
    DBusArgs *dArgs = (DBusArgs *) args;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
    case COMP_TYPE_INT32:
    case COMP_TYPE_DOUBLE:
    case COMP_TYPE_STRING:
	dbus_message_iter_get_basic (&dArgs->messageIter, value);
	break;
    case COMP_TYPE_OBJECT: {
	char *object;

	dbus_message_iter_get_basic (&dArgs->messageIter, &object);

	if (strlen (object) > dArgs->pathLen)
	    *((char **) value) = object + dArgs->pathLen + 1;
	else
	    *((char **) value) = "";
    } break;
    }

    dbus_message_iter_next (&dArgs->messageIter);
}

static void
dbusArgsStore (CompArgs *args,
	       int      type,
	       void     *value)
{
    DBusArgs *dArgs = (DBusArgs *) args;

    if (!dArgs->reply)
	return;

    if (dbus_message_get_type (dArgs->reply) == DBUS_MESSAGE_TYPE_ERROR)
	return;

    switch (type) {
    case COMP_TYPE_BOOLEAN:
    case COMP_TYPE_INT32:
    case COMP_TYPE_DOUBLE:
    case COMP_TYPE_STRING:
	if (!dbus_message_iter_append_basic (&dArgs->replyIter, type, value))
	    dbusArgsIntError (dArgs,
			      DBUS_ERROR_NO_MEMORY,
			      "Failed to append output argument");
	break;
    case COMP_TYPE_OBJECT: {
	char *dbusPath, *path = *((char **) value);

	dbusPath = malloc (dArgs->pathLen + strlen (path) + 2);
	if (dbusPath)
	{
	    sprintf (dbusPath, "%s/%s", dArgs->path, path);

	    if (!dbus_message_iter_append_basic (&dArgs->replyIter,
						 DBUS_TYPE_OBJECT_PATH,
						 dbusPath))
		dbusArgsIntError (dArgs,
				  DBUS_ERROR_NO_MEMORY,
				  "Failed to append object path output "
				  "argument");

	    free (dbusPath);
	}
	else
	{
	    dbusArgsIntError (dArgs,
			      DBUS_ERROR_NO_MEMORY,
			      "Failed to convert object path argument");
	}
    } break;
    }
}

static void
dbusArgsError (CompArgs *args,
	       char     *error)
{
    DBusArgs *dArgs = (DBusArgs *) args;

    dbusArgsIntError (dArgs, DBUS_ERROR_FAILED, error);

    free (error);
}

static CompBool
dbusInitArgs (DBusArgs	  *dArgs,
	      CompObject  *object,
	      DBusMessage *message)
{
    dArgs->base.load  = dbusArgsLoad;
    dArgs->base.store = dbusArgsStore;
    dArgs->base.error = dbusArgsError;

    dArgs->path = dbusGetObjectPath (object);
    if (!dArgs->path)
	return FALSE;

    dArgs->pathLen = strlen (dArgs->path);

    dArgs->reply = dbus_message_new_method_return (message);
    if (!dArgs->reply)
    {
	free (dArgs->path);
	return FALSE;
    }

    dArgs->message = message;

    dbus_message_iter_init (message, &dArgs->messageIter);
    dbus_message_iter_init_append (dArgs->reply, &dArgs->replyIter);

    return TRUE;
}

static void
dbusFiniArgs (DBusArgs *dArgs)
{
    if (dArgs->reply)
	dbus_message_unref (dArgs->reply);

    free (dArgs->path);
}

static DBusMessage *
dbusInvokeMethod (DBusMessage *message,
		  CompObject  *object,
		  const char  *interface,
		  const char  *name,
		  const char  *in)
{
    DBusMessage *reply = NULL;
    DBusArgs	args;

    if (!dbusInitArgs (&args, object, message))
	return dbus_message_new_error (message,
				       DBUS_ERROR_NO_MEMORY,
				       "Failed to create return message");

    if (compInvokeMethodWithArgs (object,
				  interface,
				  name,
				  in,
				  NULL,
				  &args.base))
	reply = dbus_message_ref (args.reply);

    dbusFiniArgs (&args);

    return reply;
}

static DBusMessage *
dbusHandleMethodCall (CompBranch     *branch,
		      DBusConnection *connection,
		      DBusMessage    *message,
		      CompObject     *object)
{
    static int baseLen = 0;
    const char *interface, *member, *signature;

    if (!baseLen)
	baseLen = strlen (COMPIZ_DBUS_INTERFACE_BASE);

    interface = dbus_message_get_interface (message);
    member    = dbus_message_get_member (message);
    signature = dbus_message_get_signature (message);

    if (!interface || strncmp (interface,
			       COMPIZ_DBUS_INTERFACE_BASE,
			       baseLen) == 0)
    {
	DBusMessage *reply;
	const char  *methodInterface = NULL;

	if (interface)
	    methodInterface = interface + baseLen;

	reply = dbusInvokeMethod (message,
				  object,
				  methodInterface,
				  member,
				  signature);
	if (reply)
	    return reply;
    }

    if (!interface || strcmp (interface, DBUS_INTERFACE_INTROSPECTABLE) == 0)
    {
	if (strcmp (member, "Introspect") == 0 && strcmp (signature, "") == 0)
	    return dbusObjectToXml (connection, message, object);
    }

    return NULL;
}

static DBusHandlerResult
dbusHandleMessage (DBusConnection *connection,
		   DBusMessage    *message,
		   void           *userData)
{
    const char  *path;
    CompObject  *object;
    DBusMessage *reply;

    CORE (userData);

    if (dbus_message_get_type (message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    path = dbus_message_get_path (message) + strlen (COMPIZ_DBUS_PATH_ROOT "/");

    object = compLookupObject (&c->u.base.u.base, path);
    if (!object)
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    reply = dbusHandleMethodCall (&c->u.base, connection, message, object);
    if (reply)
    {
	dbus_connection_send (connection, reply, NULL);
	dbus_connection_flush (connection);
	dbus_message_unref (reply);

	return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable dbusObjectPathVTable = {
    0, /* unregister */
    dbusHandleMessage
};

static void
dbusRegisterObject (CompCore   *c,
		    CompObject *object)
{
    char *path;

    DBUS_CORE (c);

    path = dbusGetObjectPath (object);
    if (path)
    {
	dbus_connection_register_object_path (dc->connection, path,
					      &dbusObjectPathVTable,
					      (void *) c);

	free (path);
    }
}

static void
dbusUnregisterObject (CompCore   *c,
		      CompObject *object)
{
    char *path;

    DBUS_CORE (c);

    path = dbusGetObjectPath (object);
    if (path)
    {
	dbus_connection_unregister_object_path (dc->connection, path);
	free (path);
    }
}

static CompBool
dbusRegisterObjectTree (CompObject *object,
			void	   *closure)
{
    CORE (closure);

    dbusRegisterObject (c, object);

    (*object->vTable->forEachChildObject) (object,
					   dbusRegisterObjectTree,
					   closure);

    return TRUE;
}

static CompBool
dbusUnregisterObjectTree (CompObject *object,
			  void	     *closure)
{
    CORE (closure);

    (*object->vTable->forEachChildObject) (object,
					   dbusUnregisterObjectTree,
					   closure);

    dbusUnregisterObject (c, object);

    return TRUE;
}

static void
dbusSignalImpl (CompObject   *object,
		const char   *path,
		const char   *interface,
		const char   *name,
		const char   *signature,
		CompAnyValue *value,
		int	     nValue)
{
    C_EMIT_SIGNAL (object, DBusSignalProc, &dbusSignal,
		   path, interface, name, signature, value, nValue);
}

static void
dbusSignalDelegate (CompObject   *object,
		    const char   *path,
		    const char   *interface,
		    const char   *name,
		    const char   *signature,
		    CompAnyValue *value,
		    int		 nValue)
{
    char *dbusPath, *dbusInterface;

    dbusPath = dbusGetPath (compTranslateObjectPath (object->parent, object,
						     path));
    if (!dbusPath)
	return;

    dbusInterface = dbusGetInterface (interface);
    if (!dbusInterface)
    {
	free (dbusPath);
	return;
    }

    dbusSignalImpl (object, dbusPath, dbusInterface, name, signature,
		    value, nValue);

    free (dbusInterface);
    free (dbusPath);
}

static void
dbusObjectSignalImpl (CompObject *object,
		      const char *path,
		      const char *name)
{
    C_EMIT_SIGNAL (object, DBusObjectSignalProc, &dbusObjectSignal,
		   path, name);
}

static void
dbusObjectSignalDelegate (CompObject   *object,
			  const char   *path,
			  const char   *interface,
			  const char   *name,
			  const char   *signature,
			  CompAnyValue *value,
			  int	       nValue)
{
    dbusObjectSignalImpl (object, path, name);
}

static void
dbusRegisterObjectPath (CompObject *object,
			const char *path,
			const char *event)
{
    DBUS_CORE (GET_CORE (object));

    dbus_connection_register_object_path (dc->connection, path,
					  &dbusObjectPathVTable,
					  (void *) object);
}

static void
dbusUnregisterObjectPath (CompObject *object,
			  const char *path,
			  const char *event)
{
    DBUS_CORE (GET_CORE (object));

    dbus_connection_unregister_object_path (dc->connection, path);
}

static void
dbusEmitSignal (CompObject   *object,
		const char   *path,
		const char   *interface,
		const char   *name,
		const char   *signature,
		CompAnyValue *value,
		int	     nValue)
{
    DBusMessage *signal;

    DBUS_CORE (GET_CORE (object));

    signal = dbus_message_new_signal (path, interface, name);
    if (signal)
    {
	DBusMessageIter iter;
	int		i;

	dbus_message_iter_init_append (signal, &iter);

	for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
	{
	    switch (signature[i]) {
	    case COMP_TYPE_BOOLEAN:
	    case COMP_TYPE_INT32:
	    case COMP_TYPE_DOUBLE:
	    case COMP_TYPE_STRING:
		dbus_message_iter_append_basic (&iter,
						signature[i],
						&value[i]);
		break;
	    case COMP_TYPE_OBJECT: {
		char *p;

		p = malloc (strlen (path) + strlen (value[i].s) + 2);
		if (p)
		{
		    sprintf (p, "%s/%s", path, value[i].s);
		    dbus_message_iter_append_basic (&iter,
						    DBUS_TYPE_OBJECT_PATH,
						    &p);

		    free (p);
		}
	    } break;
	    }
	}

	if (strcmp (dbus_message_get_signature (signal), signature) == 0)
	    dbus_connection_send (dc->connection, signal, NULL);

	dbus_message_unref (signal);
    }
}

static CompBool
dbusRequestName (CompCore   *c,
		 const char *name)
{
    DBusError error;

    DBUS_CORE (c);

    dbus_error_init (&error);

    dbus_bus_request_name (dc->connection, name,
			   DBUS_NAME_FLAG_REPLACE_EXISTING |
			   DBUS_NAME_FLAG_ALLOW_REPLACEMENT,
			   &error);

    if (dbus_error_is_set (&error))
    {
	dbus_error_free (&error);
	return FALSE;
    }

    dbus_error_free (&error);

    return TRUE;
}

static void
dbusReleaseName (CompCore   *c,
		 const char *name)
{
    DBUS_CORE (c);

    dbus_bus_release_name (dc->connection, name, NULL);
}

static CompBool
dbusProcessMessages (void *data)
{
    DBusDispatchStatus status;

    DBUS_CORE (GET_CORE (data));

    do
    {
	dbus_connection_read_write_dispatch (dc->connection, 0);
	status = dbus_connection_get_dispatch_status (dc->connection);
    }
    while (status == DBUS_DISPATCH_DATA_REMAINS);

    return TRUE;
}

static CompBool
dbusInitCore (CompObject *object)
{
    DBusError   error;
    dbus_bool_t status;
    int		fd;

    CORE (object);
    DBUS_CORE (c);

    dbus_error_init (&error);

    dc->connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
	dbus_error_free (&error);
	return FALSE;
    }

    dbus_error_free (&error);

    status = dbus_connection_get_unix_fd (dc->connection, &fd);
    if (!status)
	return FALSE;

    dc->watchFdHandle = compAddWatchFd (fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					dbusProcessMessages,
					c);

    dbusRequestName (c, COMPIZ_DBUS_SERVICE_NAME);

    dbusRegisterObjectTree (&c->u.base.u.base, (void *) c);

    return TRUE;
}

static void
dbusFiniCore (CompObject *object)
{
    CORE (object);
    DBUS_CORE (c);

    dbusUnregisterObjectTree (object, (void *) c);

    dbusReleaseName (c, COMPIZ_DBUS_SERVICE_NAME);

    compRemoveWatchFd (dc->watchFdHandle);
}

static void
dbusGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    static const CMetadata template = {
	.interface  = dbusCoreInterface,
	.nInterface = N_ELEMENTS (dbusCoreInterface),
	.init       = dbusInitCore,
	.fini       = dbusFiniCore,
	.version    = COMPIZ_DBUS_VERSION
    };

    CORE (object);

    cGetInterfaceProp (&GET_DBUS_CORE (c)->base, &template, what, value);
}

static DBusCoreVTable dbusCoreObjectVTable = {
    .base.base.base.getProp = dbusGetProp,

    .signal		  = dbusSignalImpl,
    .signalDelegate	  = dbusSignalDelegate,
    .objectSignal	  = dbusObjectSignalImpl,
    .objectSignalDelegate = dbusObjectSignalDelegate,
    .registerObjectPath   = dbusRegisterObjectPath,
    .unregisterObjectPath = dbusUnregisterObjectPath,
    .emitSignal           = dbusEmitSignal
};

static CObjectPrivate dbusObj[] = {
    C_OBJECT_PRIVATE (CORE_TYPE_NAME, dbus, Core, DBusCore, X, X)
};

static CompBool
dbusInsert (CompObject *parent,
	    CompBranch *branch)
{
    if (!cObjectInitPrivates (branch, dbusObj, N_ELEMENTS (dbusObj)))
	return FALSE;

    compConnect (parent,
		 "signal", offsetof (CompSignalVTable, signal),
		 &branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, signalDelegate),
		 "o", "//*");

    compConnect (&branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, signal),
		 &branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, objectSignalDelegate),
		 "os", "//*", "object");

    compConnect (&branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, objectSignal),
		 &branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, registerObjectPath),
		 "os", "//*", "inserted");

    compConnect (&branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, objectSignal),
		 &branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, unregisterObjectPath),
		 "os", "//*", "removed");

    compConnect (&branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, signal),
		 &branch->u.base,
		 "dbus", offsetof (DBusCoreVTable, emitSignal),
		 NULL);

    return TRUE;
}

static void
dbusRemove (CompObject *parent,
	    CompBranch *branch)
{
    cObjectFiniPrivates (branch, dbusObj, N_ELEMENTS (dbusObj));
}

static CompBool
dbusInit (CompFactory *factory)
{
    if (!cObjectAllocPrivateIndices (factory, dbusObj, N_ELEMENTS (dbusObj)))
	return FALSE;

    return TRUE;
}

static void
dbusFini (CompFactory *factory)
{
    cObjectFreePrivateIndices (factory, dbusObj, N_ELEMENTS (dbusObj));
}

CompPluginVTable dbusVTable = {
    "dbus",
    0, /* GetMetadata */
    dbusInit,
    dbusFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0, /* SetObjectOption */
    dbusInsert,
    dbusRemove
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &dbusVTable;
}
