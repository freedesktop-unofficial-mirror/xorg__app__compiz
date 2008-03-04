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
#include <compiz/signal-arg-map.h>
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
    int i;

    for (i = 0; args[i] != COMP_TYPE_INVALID; i++)
    {
	if (i < sm->data.args.nChild)
	{
	    CompSignalArgMap *sa = NULL;

	    switch (args[i]) {
	    case COMP_TYPE_BOOLEAN:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getBoolSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    case COMP_TYPE_INT32:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getIntSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    case COMP_TYPE_DOUBLE:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getDoubleSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    case COMP_TYPE_STRING:
	    case COMP_TYPE_OBJECT:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getStringSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    }

	    if (sa)
	    {
		if (!(*sa->u.vTable->map) (sa,
					   path,
					   interface,
					   name,
					   signature,
					   value,
					   nValue,
					   &argValue[i]))
		    memset (&argValue[i], 0, sizeof (CompAnyValue));
	    }
	    else
	    {
		compLog (&sm->u.base,
			 getSignalMatchObjectType (),
			 offsetof (CompSignalMatchVTable, match),
			 "Signal argument mapping object for '%c' argument "
			 "has bad type", args[i]);
		memset (&argValue[i], 0, sizeof (CompAnyValue));
	    }
	}
	else
	{
	    memset (&argValue[i], 0, sizeof (CompAnyValue));
	}
    }

    return TRUE;
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

	    .child  = signalMatchTypeChildObject,
	    .nChild = N_ELEMENTS (signalMatchTypeChildObject)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

static void
simpleSignalMatchGetProp (CompObject   *object,
			  unsigned int what,
			  void	       *value)
{
    cGetObjectProp (&GET_SIMPLE_SIGNAL_MATCH (object)->data.base,
		    getSimpleSignalMatchObjectType (),
		    what, value);
}

static CompBool
simpleMatch (CompSignalMatch *sm,
	     const char      *path,
	     const char      *interface,
	     const char      *name,
	     const char      *signature,
	     CompAnyValue    *value,
	     int	     nValue,
	     const char      *args,
	     CompAnyValue    *argValue)
{
    CompBool status;

    SIMPLE_SIGNAL_MATCH (sm);

    if (strcmp (ssm->data.path, "//*") != 0)
    {
	int i;

	for (i = 0; path[i]; i++)
	    if (ssm->data.path[i] != path[i])
		break;

	if (path[i])
	{
	    if (ssm->data.path[i] != '*')
		return FALSE;
	}
	else if (ssm->data.path[i] != '\0')
	{
	    return FALSE;
	}
    }

    if (strcmp (interface, ssm->data.interface) ||
	strcmp (name,	   ssm->data.name)      ||
	strcmp (signature, ssm->data.signature))
	return FALSE;

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

static const CompSignalMatchVTable simpleSignalMatchObjectVTable = {
    .base.getProp = simpleSignalMatchGetProp,

    .match = simpleMatch
};

static const CStringProp simpleSignalMatchTypeStringProp[] = {
    C_PROP (path,      CompSimpleSignalMatchData),
    C_PROP (interface, CompSimpleSignalMatchData),
    C_PROP (name,      CompSimpleSignalMatchData),
    C_PROP (signature, CompSimpleSignalMatchData)
};

const CompObjectType *
getSimpleSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_SIMPLE_SIGNAL_MATCH_TYPE_NAME,
	    .i.version	     = COMPIZ_SIMPLE_SIGNAL_MATCH_VERSION,
	    .i.base.name     = COMPIZ_SIGNAL_MATCH_TYPE_NAME,
	    .i.base.version  = COMPIZ_SIGNAL_MATCH_VERSION,
	    .i.vTable.impl   = &simpleSignalMatchObjectVTable.base,
	    .i.vTable.size   = sizeof (simpleSignalMatchObjectVTable),
	    .i.instance.size = sizeof (CompSimpleSignalMatch),

	    .stringProp  = simpleSignalMatchTypeStringProp,
	    .nStringProp = N_ELEMENTS (simpleSignalMatchTypeStringProp)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
