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

#define COMPIZ_DBUS_INTERFACE_NAME "org.compiz.dbus"
#define COMPIZ_DBUS_VERSION        20071123

const CompObjectInterface *
getDBusCoreObjectInterface (void);

#define COMPIZ_DBUS_SERVICE_NAME "org.compiz"
#define COMPIZ_DBUS_PATH_ROOT	 "/org/compiz"

static int dbusCorePrivateIndex;

typedef void (*DBusRegisterObjectPathProc) (CompCore   *c,
					    const char *path,
					    const char *event);

typedef void (*DBusUnregisterObjectPathProc) (CompCore   *c,
					      const char *path,
					      const char *event);

typedef void (*DBusEmitSignalProc) (CompCore     *c,
				    const char   *path,
				    const char   *interface,
				    const char   *name,
				    const char   *signature,
				    CompAnyValue *value,
				    int	         nValue);

typedef void (*DBusSignalProc) (CompCore     *c,
				const char   *path,
				const char   *interface,
				const char   *name,
				const char   *signature,
				CompAnyValue *value,
				int	     nValue);

typedef void (*DBusObjectSignalProc) (CompCore   *c,
				      const char *path,
				      const char *event);

typedef struct _DBusCoreVTable {
    CompCoreVTable base;

    DBusRegisterObjectPathProc   registerObjectPath;
    DBusUnregisterObjectPathProc unregisterObjectPath;
    DBusEmitSignalProc           emitSignal;

    DBusSignalProc	 signal;
    DBusObjectSignalProc objectSignal;

    SignalProc signalDelegate;
    SignalProc objectSignalDelegate;
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


static char *
dbusGetPath (const char *path)
{
    char *dbusPath;

    if (!*path)
	return strdup (COMPIZ_DBUS_PATH_ROOT);

    dbusPath = malloc (strlen (COMPIZ_DBUS_PATH_ROOT) + strlen (path) + 2);
    if (!dbusPath)
	return NULL;

    sprintf (dbusPath, "%s/%s", COMPIZ_DBUS_PATH_ROOT, path);

    return dbusPath;
}

static char *
dbusGetObjectPath (CompBranch *branch,
		   CompObject *object)
{
    char *parentPath, *path;
    int  parentPathLen;

    if (object == &branch->u.base)
	return strdup (COMPIZ_DBUS_PATH_ROOT);

    parentPath = dbusGetObjectPath (branch, object->parent);
    if (!parentPath)
	return NULL;

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
dbusObjectInterfaceToXml (CompObject	            *object,
			  const CompObjectInterface *interface,
			  void		            *closure)
{
    xmlTextWriterPtr writer = (xmlTextWriterPtr) closure;

    /* avoid interfaces without public members */
    if ((*object->vTable->forEachMethod) (object,
					  interface,
					  dbusCheckForObjectMethod,
					  NULL) &&
	(*object->vTable->forEachSignal) (object,
					  interface,
					  dbusCheckForObjectSignal,
					  NULL) &&
	(*object->vTable->forEachProp) (object,
					interface,
					dbusCheckForObjectProp,
					NULL))
	return TRUE;

    xmlTextWriterStartElement (writer, BAD_CAST "interface");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name",
				 BAD_CAST interface->name);

    (*object->vTable->forEachMethod) (object,
				      interface,
				      dbusObjectMethodToXml,
				      closure);

    (*object->vTable->forEachSignal) (object,
				      interface,
				      dbusObjectSignalToXml,
				      closure);

    (*object->vTable->forEachProp) (object,
				    interface,
				    dbusObjectPropToXml,
				    closure);

    xmlTextWriterEndElement (writer);

    return TRUE;
}

static DBusMessage *
dbusObjectToXml (CompBranch     *branch,
		 DBusConnection *connection,
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

    path = dbusGetObjectPath (branch, object);
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
	      CompBranch  *branch,
	      CompObject  *object,
	      DBusMessage *message)
{
    dArgs->base.load  = dbusArgsLoad;
    dArgs->base.store = dbusArgsStore;
    dArgs->base.error = dbusArgsError;

    dArgs->path = dbusGetObjectPath (branch, object);
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
dbusInvokeMethod (CompBranch  *branch,
		  DBusMessage *message,
		  CompObject  *object,
		  const char  *interface,
		  const char  *name,
		  const char  *in)
{
    DBusMessage *reply = NULL;
    DBusArgs	args;

    if (!dbusInitArgs (&args, branch, object, message))
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
    DBusMessage *reply;
    const char  *interface, *member, *signature;

    interface = dbus_message_get_interface (message);
    member    = dbus_message_get_member (message);
    signature = dbus_message_get_signature (message);

    reply = dbusInvokeMethod (branch,
			      message,
			      object,
			      interface,
			      member,
			      signature);
    if (reply)
	return reply;

    if (!interface || strcmp (interface, DBUS_INTERFACE_INTROSPECTABLE) == 0)
	if (strcmp (member, "Introspect") == 0 && strcmp (signature, "") == 0)
	    return dbusObjectToXml (branch, connection, message, object);

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

    path = dbus_message_get_path (message) + strlen (COMPIZ_DBUS_PATH_ROOT);
    if (*path != '\0')
    {
	object = compLookupDescendant (&c->u.base.u.base, path + 1);
	if (!object)
	    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    else
    {
	object = &c->u.base.u.base;
    }

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

    path = dbusGetObjectPath (&c->u.base, object);
    if (path)
    {
	dbus_connection_register_object_path (dc->connection, path,
					      &dbusObjectPathVTable,
					      (void *) c);

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

static void
dbusRegisterObjectPath (CompCore   *c,
			const char *path,
			const char *event)
{
    void *data;

    DBUS_CORE (c);

    if (!dbus_connection_get_object_path_data (dc->connection, path, &data))
	data = NULL;

    if (!data)
	dbus_connection_register_object_path (dc->connection, path,
					      &dbusObjectPathVTable,
					      (void *) c);
}

static void
dbusUnregisterObjectPath (CompCore   *c,
			  const char *path,
			  const char *event)
{
    void *data;

    DBUS_CORE (c);

    if (!dbus_connection_get_object_path_data (dc->connection, path, &data))
	data = NULL;

    if (data)
	dbus_connection_unregister_object_path (dc->connection, path);
}

static void
dbusEmitSignal (CompCore     *c,
		const char   *path,
		const char   *interface,
		const char   *name,
		const char   *signature,
		CompAnyValue *value,
		int	     nValue)
{
    DBusMessage *signal;

    DBUS_CORE (c);

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
		int len;

		len = strlen (value[i].s);
		if (len)
		{
		    char *p;

		    p = malloc (strlen (path) + len + 2);
		    if (p)
		    {
			sprintf (p, "%s/%s", path, value[i].s);
			dbus_message_iter_append_basic (&iter,
							DBUS_TYPE_OBJECT_PATH,
							&p);

			free (p);
		    }
		}
		else
		{
		    dbus_message_iter_append_basic (&iter,
						    DBUS_TYPE_OBJECT_PATH,
						    &path);
		}
		} break;
	    }
	}

	if (strcmp (dbus_message_get_signature (signal), signature) == 0)
	    dbus_connection_send (dc->connection, signal, NULL);

	dbus_message_unref (signal);
    }
}

static void
dbusSignal (CompCore     *c,
	    const char   *path,
	    const char   *interface,
	    const char   *name,
	    const char   *signature,
	    CompAnyValue *value,
	    int	         nValue)
{
    C_EMIT_SIGNAL (&c->u.base.u.base, DBusSignalProc,
		   offsetof (DBusCoreVTable, signal),
		   path, interface, name, signature, value, nValue);
}

static void
dbusObjectSignal (CompCore   *c,
		  const char *path,
		  const char *event)
{
    C_EMIT_SIGNAL (&c->u.base.u.base, DBusObjectSignalProc,
		   offsetof (DBusCoreVTable, objectSignal),
		   path, event);
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
    char *dbusPath;

    dbusPath = dbusGetPath (compTranslateObjectPath (object->parent, object,
						     path));
    if (dbusPath)
    {
	CORE (object);

	dbusSignal (c, dbusPath, interface, name, signature, value, nValue);

	free (dbusPath);
    }
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
    dbusObjectSignal (GET_CORE (object), path, name);
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

    dbusRequestName (c, COMPIZ_DBUS_SERVICE_NAME);

    dbusRegisterObjectTree (&c->u.base.u.base, (void *) c);

    dc->watchFdHandle = compAddWatchFd (fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					dbusProcessMessages,
					c);

    return TRUE;
}

static void
dbusFiniCore (CompObject *object)
{
    CORE (object);
    DBUS_CORE (c);

    compRemoveWatchFd (dc->watchFdHandle);

    dbus_connection_unregister_object_path (dc->connection,
					    COMPIZ_DBUS_PATH_ROOT);

    dbusReleaseName (c, COMPIZ_DBUS_SERVICE_NAME);
}

static void
dbusInsertCore (CompObject *object,
		CompObject *parent)
{
    CORE (object);

    compConnect (parent,
		 getObjectType (),
		 offsetof (CompObjectVTable, signal),
		 &c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, signalDelegate),
		 "o", "//*");

    compConnect (&c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, signal),
		 &c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, objectSignalDelegate),
		 "os", "//*", getObjectType ()->name);

    compConnect (&c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, objectSignal),
		 &c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, registerObjectPath),
		 "os", "//*", "inserted");

    compConnect (&c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, objectSignal),
		 &c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, unregisterObjectPath),
		 "os", "//*", "removed");

    compConnect (&c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, signal),
		 &c->u.base.u.base,
		 getDBusCoreObjectInterface (),
		 offsetof (DBusCoreVTable, emitSignal),
		 NULL);
}

static void
dbusGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    cGetInterfaceProp (&GET_DBUS_CORE (GET_CORE (object))->base,
		       getDBusCoreObjectInterface (),
		       what, value);
}

static DBusCoreVTable dbusCoreObjectVTable = {
    .base.base.base.getProp = dbusGetProp,

    .registerObjectPath   = dbusRegisterObjectPath,
    .unregisterObjectPath = dbusUnregisterObjectPath,
    .emitSignal           = dbusEmitSignal,

    .signal	  = dbusSignal,
    .objectSignal = dbusObjectSignal,

    .signalDelegate	  = dbusSignalDelegate,
    .objectSignalDelegate = dbusObjectSignalDelegate

};

static const CSignal dbusCoreSignal[] = {
    C_SIGNAL (signal,       0, DBusCoreVTable),
    C_SIGNAL (objectSignal, 0, DBusCoreVTable)
};

const CompObjectInterface *
getDBusCoreObjectInterface (void)
{
    static CompObjectInterface *interface = NULL;

    if (!interface)
    {
	static const CObjectInterface template = {
	    .i.name	    = COMPIZ_DBUS_INTERFACE_NAME,
	    .i.version	    = COMPIZ_DBUS_VERSION,
	    .i.base.name    = COMPIZ_CORE_TYPE_NAME,
	    .i.base.version = COMPIZ_CORE_VERSION,
	    .i.vTable.impl  = &dbusCoreObjectVTable.base.base.base,
	    .i.vTable.size  = sizeof (dbusCoreObjectVTable),

	    .signal  = dbusCoreSignal,
	    .nSignal = N_ELEMENTS (dbusCoreSignal),

	    .init = dbusInitCore,
	    .fini = dbusFiniCore,

	    .insert = dbusInsertCore
	};

	interface = cObjectInterfaceFromTemplate (&template,
						  &dbusCorePrivateIndex,
						  sizeof (DBusCore));
    }

    return interface;
}

const GetInterfaceProc *
getCompizObjectInterfaces20080220 (int *n)
{
    static const GetInterfaceProc interfaces[] = {
	getDBusCoreObjectInterface
    };

    *n = N_ELEMENTS (interfaces);
    return interfaces;
}
