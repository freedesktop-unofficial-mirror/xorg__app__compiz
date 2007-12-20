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
    .name.name   = PROP_TYPE_NAME,
    .name.base   = OBJECT_TYPE_NAME,
    .vTable.impl = &propObjectVTable,
    .vTable.size = sizeof (propObjectVTable),
    .funcs.init  = propInitObject,
    .funcs.fini  = propFiniObject
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
    .name.name   = BOOL_PROP_TYPE_NAME,
    .name.base   = PROP_TYPE_NAME,
    .vTable.impl = &boolPropObjectVTable,
    .vTable.size = sizeof (boolPropObjectVTable),
    .funcs.init  = boolPropInitObject,
    .funcs.fini  = boolPropFiniObject
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
    .name.name   = INT_PROP_TYPE_NAME,
    .name.base   = PROP_TYPE_NAME,
    .vTable.impl = &intPropObjectVTable,
    .vTable.size = sizeof (intPropObjectVTable),
    .funcs.init  = intPropInitObject,
    .funcs.fini  = intPropFiniObject
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
    .name.name   = DOUBLE_PROP_TYPE_NAME,
    .name.base   = PROP_TYPE_NAME,
    .vTable.impl = &doublePropObjectVTable,
    .vTable.size = sizeof (doublePropObjectVTable),
    .funcs.init  = doublePropInitObject,
    .funcs.fini  = doublePropFiniObject
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
    .name.name   = STRING_PROP_TYPE_NAME,
    .name.base   = PROP_TYPE_NAME,
    .vTable.impl = &stringPropObjectVTable,
    .vTable.size = sizeof (stringPropObjectVTable),
    .funcs.init  = stringPropInitObject,
    .funcs.fini  = stringPropFiniObject
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
