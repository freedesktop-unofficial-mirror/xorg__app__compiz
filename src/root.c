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
#include <stdlib.h>

#include <compiz/root.h>

struct _CompSignal {
    struct _CompSignal *next;
    char	       *path;
    char	       *interface;
    char	       *name;
    char	       *signature;
    CompAnyValue       *value;
    int		       nValue;
};

static CompBool
rootForBaseObject (CompObject	       *object,
		   BaseObjectCallBackProc proc,
		   void		       *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    ROOT (object);

    UNWRAP (&r->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&r->object, object, vTable, v.vTable);

    return status;
}

typedef struct _HandleSignalContext {
    const char *path;
    CompSignal *signal;
} HandleSignalContext;

static CompBool
handleSignal (CompObject *object,
	      void	 *closure)
{
    HandleSignalContext *pCtx = (HandleSignalContext *) closure;
    int			i;

    for (i = 0; pCtx->path[i] && object->name[i]; i++)
	if (pCtx->path[i] != object->name[i])
	    break;

    if (object->name[i] == '\0')
    {
	if (pCtx->path[i] == '/')
	{
	    HandleSignalContext ctx;

	    ctx.path   = &pCtx->path[++i];
	    ctx.signal = pCtx->signal;

	    (*object->vTable->forEachChildObject) (object,
						   handleSignal,
						   (void *) &ctx);
	}
	else if (pCtx->path[i] != '\0')
	{
	    return TRUE;
	}

	(*object->vTable->signal.signal) (object, &pCtx->path[i],
					  pCtx->signal->interface,
					  pCtx->signal->name,
					  pCtx->signal->signature,
					  pCtx->signal->value,
					  pCtx->signal->nValue);

	return FALSE;
    }

    return TRUE;
}

static void
processSignals (CompRoot *r)
{
    while (r->signal.head)
    {
	HandleSignalContext ctx;
	CompSignal	    *s = r->signal.head;

	if (s->next)
	    r->signal.head = s->next;
	else
	    r->signal.head = r->signal.tail = NULL;

	ctx.path   = s->path;
	ctx.signal = s;

	(*r->u.base.base.vTable->forEachChildObject) (&r->u.base.base,
						      handleSignal,
						      (void *) &ctx);

	(*r->u.base.base.vTable->signal.signal) (&r->u.base.base,
						 s->path,
						 s->interface,
						 s->name,
						 s->signature,
						 s->value,
						 s->nValue);

	free (s);
    }
}

static CompRootVTable rootObjectVTable = {
    .base.forBaseObject = rootForBaseObject,
    .processSignals     = processSignals
};

static CompBool
forCoreObject (CompObject	       *object,
	       ChildObjectCallBackProc proc,
	       void		       *closure)
{
    ROOT (object);

    if (!(*proc) (r->core, closure))
	return FALSE;

    return TRUE;
}

static CompBool
rootInitObject (CompObject *object)
{
    ROOT (object);

    r->signal.head = NULL;
    r->signal.tail = NULL;

    if (!compObjectInit (&r->u.base.base, getContainerObjectType ()))
	return FALSE;

    WRAP (&r->object, object, vTable, &rootObjectVTable.base);

    r->u.base.forEachChildObject = forCoreObject;

    r->core = NULL;

    return TRUE;
}

static void
rootFiniObject (CompObject *object)
{
    ROOT (object);

    UNWRAP (&r->object, object, vTable);

    compObjectFini (object, getContainerObjectType ());
}

static void
rootInitVTable (CompRootVTable *vTable)
{
    (*getContainerObjectType ()->initVTable) (vTable);
}

static CompObjectType rootObjectType = {
    ROOT_TYPE_NAME,
    {
	rootInitObject,
	rootFiniObject
    },
    0,
    NULL,
    (InitVTableProc) rootInitVTable
};

CompObjectType *
getRootObjectType (void)
{
    static int init = 0;

    if (!init)
    {
	rootInitVTable (&rootObjectVTable);
	init = 1;
    }
    return &rootObjectType;
}

void
emitSignalSignal (CompObject *object,
		  const char *interface,
		  const char *name,
		  const char *signature,
		  ...)
{
    CompRoot   *r = NULL;
    CompObject *node;
    CompSignal *signal;
    char       *data, *str;
    int        pathSize = 0;
    int	       interfaceSize = strlen (interface) + 1;
    int	       nameSize = strlen (name) + 1;
    int	       signatureSize = strlen (signature) + 1;
    int	       valueSize = 0;
    int	       nValue = 0;
    int	       i, offset;
    va_list    args;

    for (node = object; node->parent; node = node->parent)
	pathSize += strlen (node->name) + 1;

    r = (CompRoot *) node;

    va_start (args, signature);

    for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
    {
	switch (signature[i]) {
	case COMP_TYPE_BOOLEAN:
	    va_arg (args, CompBool);
	    break;
	case COMP_TYPE_INT32:
	    va_arg (args, int32_t);
	    break;
	case COMP_TYPE_DOUBLE:
	    va_arg (args, double);
	    break;
	case COMP_TYPE_STRING:
	case COMP_TYPE_OBJECT:
	    str = va_arg (args, char *);
	    if (str)
		valueSize += strlen (str) + 1;
	    break;
	}

	nValue++;
    }

    va_end (args);

    signal = malloc (sizeof (CompSignal) +
		     pathSize +
		     interfaceSize +
		     nameSize +
		     signatureSize +
		     sizeof (CompAnyValue) * nValue +
		     valueSize);
    if (!signal)
	return;

    signal->next = NULL;
    signal->path = (char *) (signal + 1);

    offset = pathSize;
    signal->path[offset - 1] = '\0';

    for (node = object; node->parent; node = node->parent)
    {
	int len;

	if (offset-- < pathSize)
	    signal->path[offset] = '/';

	len = strlen (node->name);
	offset -= len;
	memcpy (&signal->path[offset], node->name, len);
    }

    signal->interface = (char *) (signal->path + pathSize);
    strcpy (signal->interface, interface);

    signal->name = (char *) (signal->interface + interfaceSize);
    strcpy (signal->name, name);

    signal->signature = (char *) (signal->name + nameSize);
    strcpy (signal->signature, signature);

    signal->value  = (CompAnyValue *) (signal->signature + signatureSize);
    signal->nValue = nValue;

    data = (char *) (signal->value + nValue);

    va_start (args, signature);

    for (i = 0; signature[i] != COMP_TYPE_INVALID; i++)
    {
	switch (signature[i]) {
	case COMP_TYPE_BOOLEAN:
	    signal->value[i].b = va_arg (args, CompBool);
	    break;
	case COMP_TYPE_INT32:
	    signal->value[i].i = va_arg (args, int32_t);
	    break;
	case COMP_TYPE_DOUBLE:
	    signal->value[i].d = va_arg (args, double);
	    break;
	case COMP_TYPE_STRING:
	case COMP_TYPE_OBJECT:
	    str = va_arg (args, char *);
	    if (str)
	    {
		signal->value[i].s = data;
		strcpy (signal->value[i].s, str);
		data += strlen (str) + 1;
	    }
	    else
	    {
		signal->value[i].s = NULL;
	    }
	    break;
	}
    }

    va_end (args);

    if (r->signal.tail)
	r->signal.tail->next = signal;
    else
	r->signal.head = signal;

    r->signal.tail = signal;
}
