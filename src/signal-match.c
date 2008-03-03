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

#include <compiz/signal-match.h>
#include <compiz/c-object.h>

static void
signalMatchGetProp (CompObject   *object,
		    unsigned int what,
		    void	 *value)
{
    cGetObjectProp (&GET_SIGNAL_MATCH (object)->data.base,
		    getSignalMatchObjectType (),
		    what, value);
}

static CompBool
match (CompSignalMatch *sm,
       const char      *path,
       const char      *interface,
       const char      *name,
       const char      *signature,
       CompAnyValue    *value,
       int	       nValue,
       const char      *args,
       CompAnyValue    *argValue)
{
    if (strcmp (path,	   sm->data.path)      == 0 &&
	strcmp (interface, sm->data.interface) == 0 &&
	strcmp (name,	   sm->data.name)      == 0 &&
	strcmp (signature, sm->data.signature) == 0)
    {
	int i;

	for (i = 0; args[i] != COMP_TYPE_INVALID; i++)
	{
	    switch (args[i]) {
	    case COMP_TYPE_BOOLEAN:
		argValue[i].b = FALSE;
		break;
	    case COMP_TYPE_INT32:
		argValue[i].i = 0;
		break;
	    case COMP_TYPE_DOUBLE:
		argValue[i].d = 0.0;
		break;
	    case COMP_TYPE_STRING:
	    case COMP_TYPE_OBJECT:
		argValue[i].s = NULL;
		break;
	    }
	}

	return TRUE;
    }

    return FALSE;
}

static CompBool
noopMatch (CompSignalMatch *sm,
	   const char      *path,
	   const char      *interface,
	   const char      *name,
	   const char      *signature,
	   CompAnyValue    *value,
	   int	           nValue,
	   const char      *args,
	   CompAnyValue    *argValue)
{
    CompBool status;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable signalMatchObjectVTable = {
    .base.getProp = signalMatchGetProp,

    .match = match
};

static const CompSignalMatchVTable noopSignalMatchObjectVTable = {
    .match = noopMatch
};

static const CStringProp signalMatchTypeStringProp[] = {
    C_PROP (path,      CompSignalMatchData),
    C_PROP (interface, CompSignalMatchData),
    C_PROP (name,      CompSignalMatchData),
    C_PROP (signature, CompSignalMatchData)
};

static const CChildObject signalMatchTypeChildObject[] = {
    C_CHILD (args, CompSignalMatchData, COMPIZ_OBJECT_TYPE_NAME)
};

const CompObjectType *
getSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_SIGNAL_MATCH_TYPE_NAME,
	    .i.version	     = COMPIZ_SIGNAL_MATCH_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &signalMatchObjectVTable.base,
	    .i.vTable.noop   = &noopSignalMatchObjectVTable.base,
	    .i.vTable.size   = sizeof (signalMatchObjectVTable),
	    .i.instance.size = sizeof (CompSignalMatch),

	    .stringProp  = signalMatchTypeStringProp,
	    .nStringProp = N_ELEMENTS (signalMatchTypeStringProp),

	    .child  = signalMatchTypeChildObject,
	    .nChild = N_ELEMENTS (signalMatchTypeChildObject)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
