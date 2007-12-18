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

#include <compiz/prop.h>
#include <compiz/c-object.h>

static const CInterface propInterface[] = {
    C_INTERFACE (prop, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompObjectVTable propObjectVTable = { 0 };

static CompBool
propInitObject (const CompObjectInstantiator *instantiator,
		CompObject		     *object,
		const CompObjectFactory      *factory)
{
    return cObjectInit (instantiator, object, factory);
}

static void
propFiniObject (const CompObjectInstantiator *instantiator,
		CompObject		     *object,
		const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType propObjectType = {
    PROP_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	propInitObject,
	propFiniObject
    },
    0,
    sizeof (CompObjectVTable),
    &propObjectVTable,
    NULL
};

CompObjectType *
getPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&propObjectVTable);
	init = TRUE;
    }

    return &propObjectType;
}

static CBoolProp boolPropTypeBoolProp[] = {
    C_PROP (value, CompBoolProp)
};

static const CInterface boolPropInterface[] = {
    C_INTERFACE (boolProp, Type, CompObjectVTable, _, _, _, X, _, _, _, _)
};

static CompObjectVTable boolPropObjectVTable = { 0 };

static CompBool
boolPropInitObject (const CompObjectInstantiator *instantiator,
		    CompObject		         *object,
		    const CompObjectFactory      *factory)
{
    return cObjectInit (instantiator, object, factory);
}

static void
boolPropFiniObject (const CompObjectInstantiator *instantiator,
		    CompObject		         *object,
		    const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType boolPropObjectType = {
    BOOL_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	boolPropInitObject,
	boolPropFiniObject
    },
    0,
    sizeof (CompObjectVTable),
    &boolPropObjectVTable,
    NULL
};

CompObjectType *
getBoolPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&boolPropObjectVTable);
	init = TRUE;
    }

    return &boolPropObjectType;
}

static CIntProp intPropTypeIntProp[] = {
    C_PROP (value, CompIntProp)
};

static const CInterface intPropInterface[] = {
    C_INTERFACE (intProp, Type, CompObjectVTable, _, _, _, _, X, _, _, _)
};

static CompObjectVTable intPropObjectVTable = { 0 };

static CompBool
intPropInitObject (const CompObjectInstantiator *instantiator,
		   CompObject		        *object,
		   const CompObjectFactory      *factory)
{
    return cObjectInit (instantiator, object, factory);
}

static void
intPropFiniObject (const CompObjectInstantiator *instantiator,
		   CompObject		        *object,
		   const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType intPropObjectType = {
    INT_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	intPropInitObject,
	intPropFiniObject
    },
    0,
    sizeof (CompObjectVTable),
    &intPropObjectVTable,
    NULL
};

CompObjectType *
getIntPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&intPropObjectVTable);
	init = TRUE;
    }

    return &intPropObjectType;
}

static CDoubleProp doublePropTypeDoubleProp[] = {
    C_PROP (value, CompDoubleProp)
};

static const CInterface doublePropInterface[] = {
    C_INTERFACE (doubleProp, Type, CompObjectVTable, _, _, _, _, _, X, _, _)
};

static CompObjectVTable doublePropObjectVTable = { 0 };

static CompBool
doublePropInitObject (const CompObjectInstantiator *instantiator,
		      CompObject		   *object,
		      const CompObjectFactory      *factory)
{
    return cObjectInit (instantiator, object, factory);
}

static void
doublePropFiniObject (const CompObjectInstantiator *instantiator,
		      CompObject		   *object,
		      const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType doublePropObjectType = {
    DOUBLE_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	doublePropInitObject,
	doublePropFiniObject
    },
    0,
    sizeof (CompObjectVTable),
    &doublePropObjectVTable,
    NULL
};

CompObjectType *
getDoublePropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&doublePropObjectVTable);
	init = TRUE;
    }

    return &doublePropObjectType;
}

static CStringProp stringPropTypeStringProp[] = {
    C_PROP (value, CompStringProp)
};

static const CInterface stringPropInterface[] = {
    C_INTERFACE (stringProp, Type, CompObjectVTable, _, _, _, _, _, _, X, _)
};

static CompObjectVTable stringPropObjectVTable = { 0 };

static CompBool
stringPropInitObject (const CompObjectInstantiator *instantiator,
		      CompObject		   *object,
		      const CompObjectFactory      *factory)
{
    return cObjectInit (instantiator, object, factory);
}

static void
stringPropFiniObject (const CompObjectInstantiator *instantiator,
		      CompObject		   *object,
		      const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType stringPropObjectType = {
    STRING_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	stringPropInitObject,
	stringPropFiniObject
    },
    0,
    sizeof (CompObjectVTable),
    &stringPropObjectVTable,
    NULL
};

CompObjectType *
getStringPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&stringPropObjectVTable);
	init = TRUE;
    }

    return &stringPropObjectType;
}
