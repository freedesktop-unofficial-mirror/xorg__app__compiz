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
propInitObject (const CompObjectFactory *factory,
		CompObject		*object)
{
    return cObjectInit (factory, object, getObjectType (), &propObjectVTable);
}

static void
propFiniObject (const CompObjectFactory *factory,
		CompObject		*object)
{
    cObjectFini (factory, object, getObjectType ());
}

static void
propInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType propObjectType = {
    PROP_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	propInitObject,
	propFiniObject
    },
    0,
    NULL,
    propInitVTable
};

static void
propGetCContext (CompObject *object,
		 CContext   *ctx)
{
    PROP (object);

    ctx->interface  = propInterface;
    ctx->nInterface = N_ELEMENTS (propInterface);
    ctx->type	    = &propObjectType;
    ctx->data	    = (char *) p;
    ctx->svOffset   = 0;
    ctx->vtStore    = &p->object;
    ctx->version    = COMPIZ_PROP_VERSION;
}

CompObjectType *
getPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&propObjectVTable, propGetCContext,
			   propObjectType.initVTable);
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
boolPropInitObject (const CompObjectFactory *factory,
		    CompObject		    *object)
{
    return cObjectInit (factory, object, getPropObjectType (),
			&boolPropObjectVTable);
}

static void
boolPropFiniObject (const CompObjectFactory *factory,
		    CompObject		    *object)
{
    cObjectFini (factory, object, getPropObjectType ());
}

static void
boolPropInitVTable (void *vTable)
{
    (*getPropObjectType ()->initVTable) (vTable);
}

static CompObjectType boolPropObjectType = {
    BOOL_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	boolPropInitObject,
	boolPropFiniObject
    },
    0,
    NULL,
    boolPropInitVTable
};

static void
boolPropGetCContext (CompObject *object,
		     CContext   *ctx)
{
    BOOL_PROP (object);

    ctx->interface  = boolPropInterface;
    ctx->nInterface = N_ELEMENTS (boolPropInterface);
    ctx->type	    = &boolPropObjectType;
    ctx->data	    = (char *) b;
    ctx->svOffset   = 0;
    ctx->vtStore    = &b->object;
    ctx->version    = COMPIZ_PROP_VERSION;
}

CompObjectType *
getBoolPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&boolPropObjectVTable, boolPropGetCContext,
			   boolPropObjectType.initVTable);
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
intPropInitObject (const CompObjectFactory *factory,
		   CompObject		   *object)
{
    return cObjectInit (factory, object, getPropObjectType (),
			&intPropObjectVTable);
}

static void
intPropFiniObject (const CompObjectFactory *factory,
		   CompObject		   *object)
{
    cObjectFini (factory, object, getPropObjectType ());
}

static void
intPropInitVTable (void *vTable)
{
    (*getPropObjectType ()->initVTable) (vTable);
}

static CompObjectType intPropObjectType = {
    INT_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	intPropInitObject,
	intPropFiniObject
    },
    0,
    NULL,
    intPropInitVTable
};

static void
intPropGetCContext (CompObject *object,
		    CContext   *ctx)
{
    INT_PROP (object);

    ctx->interface  = intPropInterface;
    ctx->nInterface = N_ELEMENTS (intPropInterface);
    ctx->type	    = &intPropObjectType;
    ctx->data	    = (char *) i;
    ctx->svOffset   = 0;
    ctx->vtStore    = &i->object;
    ctx->version    = COMPIZ_PROP_VERSION;
}

CompObjectType *
getIntPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&intPropObjectVTable, intPropGetCContext,
			   intPropObjectType.initVTable);
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
doublePropInitObject (const CompObjectFactory *factory,
		      CompObject	      *object)
{
    return cObjectInit (factory, object, getPropObjectType (),
			&doublePropObjectVTable);
}

static void
doublePropFiniObject (const CompObjectFactory *factory,
		      CompObject	      *object)
{
    cObjectFini (factory, object, getPropObjectType ());
}

static void
doublePropInitVTable (void *vTable)
{
    (*getPropObjectType ()->initVTable) (vTable);
}

static CompObjectType doublePropObjectType = {
    DOUBLE_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	doublePropInitObject,
	doublePropFiniObject
    },
    0,
    NULL,
    doublePropInitVTable
};

static void
doublePropGetCContext (CompObject *object,
		       CContext   *ctx)
{
    DOUBLE_PROP (object);

    ctx->interface  = doublePropInterface;
    ctx->nInterface = N_ELEMENTS (doublePropInterface);
    ctx->type	    = &doublePropObjectType;
    ctx->data	    = (char *) d;
    ctx->svOffset   = 0;
    ctx->vtStore    = &d->object;
    ctx->version    = COMPIZ_PROP_VERSION;
}

CompObjectType *
getDoublePropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&doublePropObjectVTable, doublePropGetCContext,
			   doublePropObjectType.initVTable);
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
stringPropInitObject (const CompObjectFactory *factory,
		      CompObject	      *object)
{
    return cObjectInit (factory, object, getPropObjectType (),
			&stringPropObjectVTable);
}

static void
stringPropFiniObject (const CompObjectFactory *factory,
		      CompObject	      *object)
{
    cObjectFini (factory, object, getPropObjectType ());
}

static void
stringPropInitVTable (void *vTable)
{
    (*getPropObjectType ()->initVTable) (vTable);
}

static CompObjectType stringPropObjectType = {
    STRING_PROP_TYPE_NAME, PROP_TYPE_NAME,
    {
	stringPropInitObject,
	stringPropFiniObject
    },
    0,
    NULL,
    stringPropInitVTable
};

static void
stringPropGetCContext (CompObject *object,
		       CContext   *ctx)
{
    STRING_PROP (object);

    ctx->interface  = stringPropInterface;
    ctx->nInterface = N_ELEMENTS (stringPropInterface);
    ctx->type	    = &stringPropObjectType;
    ctx->data	    = (char *) s;
    ctx->svOffset   = 0;
    ctx->vtStore    = &s->object;
    ctx->version    = COMPIZ_PROP_VERSION;
}

CompObjectType *
getStringPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&stringPropObjectVTable, stringPropGetCContext,
			   stringPropObjectType.initVTable);
	init = TRUE;
    }

    return &stringPropObjectType;
}
