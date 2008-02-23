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

#include <stdlib.h>

#include <compiz/prop.h>
#include <compiz/c-object.h>

static CompObjectType *
propObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface propTemplate = *template;

    if (!propTemplate.i.base.name)
    {
	propTemplate.i.base.name    = COMPIZ_PROP_TYPE_NAME;
	propTemplate.i.base.version = COMPIZ_PROP_VERSION;
    }

    if (!propTemplate.i.version)
	propTemplate.i.version = COMPIZ_PROP_VERSION;

    return cObjectTypeFromTemplate (&propTemplate);
}

/* prop */

static void
propGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    cGetObjectProp (&GET_PROP (object)->data,
		    getPropObjectType (),
		    what, value);
}

static const CompObjectVTable propObjectVTable = {
    .getProp = propGetProp
};

const CompObjectType *
getPropObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_PROP_TYPE_NAME,
	    .i.version	     = COMPIZ_PROP_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &propObjectVTable,
	    .i.instance.size = sizeof (CompProp)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}


/* bool prop */

static void
boolPropGetProp (CompObject   *object,
		 unsigned int what,
		 void	      *value)
{
    cGetObjectProp (&GET_BOOL_PROP (object)->data.base,
		    getBoolPropObjectType (),
		    what, value);
}

static const CompObjectVTable boolPropObjectVTable = {
    .getProp = boolPropGetProp
};

static const CBoolProp boolPropTypeBoolProp[] = {
    C_PROP (value, CompBoolPropData)
};

const CompObjectType *
getBoolPropObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_BOOL_PROP_TYPE_NAME,
	    .i.vTable.impl   = &boolPropObjectVTable,
	    .i.instance.size = sizeof (CompBoolProp),

	    .boolProp  = boolPropTypeBoolProp,
	    .nBoolProp = N_ELEMENTS (boolPropTypeBoolProp)
	};

	type = propObjectTypeFromTemplate (&template);
    }

    return type;
}


/* int prop */

static void
intPropGetProp (CompObject   *object,
		unsigned int what,
		void	     *value)
{
    cGetObjectProp (&GET_INT_PROP (object)->data.base,
		    getIntPropObjectType (),
		    what, value);
}

static const CompObjectVTable intPropObjectVTable = {
    .getProp = intPropGetProp
};

static const CIntProp intPropTypeIntProp[] = {
    C_PROP (value, CompIntPropData)
};

const CompObjectType *
getIntPropObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name          = COMPIZ_INT_PROP_TYPE_NAME,
	    .i.vTable.impl   = &intPropObjectVTable,
	    .i.instance.size = sizeof (CompIntProp),

	    .intProp  = intPropTypeIntProp,
	    .nIntProp = N_ELEMENTS (intPropTypeIntProp)
	};

	type = propObjectTypeFromTemplate (&template);
    }

    return type;
}


/* double prop */

static void
doublePropGetProp (CompObject   *object,
		   unsigned int what,
		   void	        *value)
{
    cGetObjectProp (&GET_DOUBLE_PROP (object)->data.base,
		    getDoublePropObjectType (),
		    what, value);
}

static const CompObjectVTable doublePropObjectVTable = {
    .getProp = doublePropGetProp
};

static const CDoubleProp doublePropTypeDoubleProp[] = {
    C_PROP (value, CompDoublePropData)
};

const CompObjectType *
getDoublePropObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name          = COMPIZ_DOUBLE_PROP_TYPE_NAME,
	    .i.vTable.impl   = &doublePropObjectVTable,
	    .i.instance.size = sizeof (CompDoubleProp),

	    .doubleProp  = doublePropTypeDoubleProp,
	    .nDoubleProp = N_ELEMENTS (doublePropTypeDoubleProp)
	};

	type = propObjectTypeFromTemplate (&template);
    }

    return type;
}


/* string prop */

static void
stringPropGetProp (CompObject   *object,
		   unsigned int what,
		   void	        *value)
{
    cGetObjectProp (&GET_STRING_PROP (object)->data.base,
		    getStringPropObjectType (),
		    what, value);
}

static const CompObjectVTable stringPropObjectVTable = {
    .getProp = stringPropGetProp
};

static const CStringProp stringPropTypeStringProp[] = {
    C_PROP (value, CompStringPropData)
};

const CompObjectType *
getStringPropObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name          = COMPIZ_STRING_PROP_TYPE_NAME,
	    .i.vTable.impl   = &stringPropObjectVTable,
	    .i.instance.size = sizeof (CompStringProp),

	    .stringProp  = stringPropTypeStringProp,
	    .nStringProp = N_ELEMENTS (stringPropTypeStringProp)
	};

	type = propObjectTypeFromTemplate (&template);
    }

    return type;
}
