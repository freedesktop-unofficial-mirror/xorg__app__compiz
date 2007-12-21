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

static void
propGetObjectProp (CompObjectData  *data,
		   const CMetadata *template,
		   unsigned int    what,
		   void		   *value)
{
    CMetadata propTemplate = *template;

    if (!propTemplate.version)
	propTemplate.version = COMPIZ_PROP_VERSION;

    cGetObjectProp (data, &propTemplate, what, value);
}


/* prop */

static CInterface propInterface[] = {
    C_INTERFACE (prop, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static void
propGetProp (CompObject   *object,
	     unsigned int what,
	     void	  *value)
{
    static const CMetadata template = {
	.interface  = propInterface,
	.nInterface = N_ELEMENTS (propInterface)
    };

    propGetObjectProp (&GET_PROP (object)->data, &template, what, value);
}

static CompObjectVTable propObjectVTable = {
    .getProp = propGetProp
};


/* bool prop */

static CBoolProp boolPropTypeBoolProp[] = {
    C_PROP (value, CompBoolPropData)
};

static CInterface boolPropInterface[] = {
    C_INTERFACE (boolProp, Type, CompObjectVTable, _, _, _, X, _, _, _, _)
};

static void
boolPropGetProp (CompObject   *object,
		 unsigned int what,
		 void	      *value)
{
    static const CMetadata template = {
	.interface  = boolPropInterface,
	.nInterface = N_ELEMENTS (boolPropInterface),
    };

    propGetObjectProp (&GET_BOOL_PROP (object)->data.base, &template, what,
		       value);
}

static CompObjectVTable boolPropObjectVTable = {
    .getProp = boolPropGetProp
};


/* int prop */

static CIntProp intPropTypeIntProp[] = {
    C_PROP (value, CompIntPropData)
};

static CInterface intPropInterface[] = {
    C_INTERFACE (intProp, Type, CompObjectVTable, _, _, _, _, X, _, _, _)
};

static void
intPropGetProp (CompObject   *object,
		 unsigned int what,
		 void	      *value)
{
    static const CMetadata template = {
	.interface  = intPropInterface,
	.nInterface = N_ELEMENTS (intPropInterface),
    };

    propGetObjectProp (&GET_INT_PROP (object)->data.base, &template, what,
		       value);
}

static CompObjectVTable intPropObjectVTable = {
    .getProp = intPropGetProp
};


/* double prop */

static CDoubleProp doublePropTypeDoubleProp[] = {
    C_PROP (value, CompDoublePropData)
};

static CInterface doublePropInterface[] = {
    C_INTERFACE (doubleProp, Type, CompObjectVTable, _, _, _, _, _, X, _, _)
};

static void
doublePropGetProp (CompObject   *object,
		   unsigned int what,
		   void	        *value)
{
    static const CMetadata template = {
	.interface  = doublePropInterface,
	.nInterface = N_ELEMENTS (doublePropInterface),
    };

    propGetObjectProp (&GET_DOUBLE_PROP (object)->data.base, &template, what,
		       value);
}

static CompObjectVTable doublePropObjectVTable = {
    .getProp = doublePropGetProp
};


/* string prop */

static CStringProp stringPropTypeStringProp[] = {
    C_PROP (value, CompStringPropData)
};

static CInterface stringPropInterface[] = {
    C_INTERFACE (stringProp, Type, CompObjectVTable, _, _, _, _, _, _, X, _)
};

static void
stringPropGetProp (CompObject   *object,
		   unsigned int what,
		   void	        *value)
{
    static const CMetadata template = {
	.interface  = stringPropInterface,
	.nInterface = N_ELEMENTS (stringPropInterface),
    };

    propGetObjectProp (&GET_STRING_PROP (object)->data.base, &template, what,
		       value);
}

static CompObjectVTable stringPropObjectVTable = {
    .getProp = stringPropGetProp
};


static CompObjectType *
propObjectTypeFromTemplate (const CompObjectType *template)
{
    CompObjectType propTemplate = *template;

    if (!propTemplate.name.base)
	propTemplate.name.base = PROP_TYPE_NAME;

    if (!propTemplate.vTable.size)
	propTemplate.vTable.size = sizeof (propObjectVTable);

    return cObjectTypeFromTemplate (&propTemplate);
}

const CompObjectType **
getPropObjectTypes (int *n)
{
    static const CompObjectType **type = NULL;
    static int			nType = 0;

    if (!type)
    {
	static const CompObjectType template[] = {
	    {
		.name.name   = PROP_TYPE_NAME,
		.name.base   = OBJECT_TYPE_NAME,
		.vTable.impl = &propObjectVTable
	    }, {
		.name.name   = BOOL_PROP_TYPE_NAME,
		.vTable.impl = &boolPropObjectVTable
	    }, {
		.name.name   = INT_PROP_TYPE_NAME,
		.vTable.impl = &intPropObjectVTable
	    }, {
		.name.name   = DOUBLE_PROP_TYPE_NAME,
		.vTable.impl = &doublePropObjectVTable
	    }, {
		.name.name   = STRING_PROP_TYPE_NAME,
		.vTable.impl = &stringPropObjectVTable
	    }
	};

	type = malloc (sizeof (CompObjectType *) * N_ELEMENTS (template));
	if (type)
	{
	    int i;

	    for (i = 0; i < N_ELEMENTS (template); i++)
		type[i] = propObjectTypeFromTemplate (&template[i]);

	    nType = N_ELEMENTS (template);
	}
    }

    *n = nType;
    return type;
}
