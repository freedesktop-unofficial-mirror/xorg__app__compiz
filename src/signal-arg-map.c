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

#include <compiz/signal-arg-map.h>
#include <compiz/error.h>
#include <compiz/c-object.h>

static void
signalArgMapGetProp (CompObject   *object,
		     unsigned int what,
		     void	  *value)
{
    cGetObjectProp (&GET_SIGNAL_ARG_MAP (object)->data.base,
		    getSignalArgMapObjectType (),
		    what, value);
}

static CompBool
map (CompSignalArgMap *sa,
     const char       *path,
     const char       *interface,
     const char       *name,
     const char       *signature,
     CompAnyValue     *value,
     int	      nValue,
     CompAnyValue     *v)
{
    return FALSE;
}

static CompBool
noopMap (CompSignalArgMap *sa,
	 const char       *path,
	 const char       *interface,
	 const char       *name,
	 const char       *signature,
	 CompAnyValue     *value,
	 int	          nValue,
	 CompAnyValue     *v)
{
    CompBool status;

    FOR_BASE (&sa->u.base, status = (*sa->u.vTable->map) (sa,
							  path,
							  interface,
							  name,
							  signature,
							  value,
							  nValue,
							  v));

    return status;
}

static const CompSignalArgMapVTable signalArgMapObjectVTable = {
    .base.getProp = signalArgMapGetProp,

    .map = map
};

static const CompSignalArgMapVTable noopSignalArgMapObjectVTable = {
    .map = noopMap
};

static const CIntProp signalArgMapTypeIntProp[] = {
    C_PROP (source, CompSignalArgMapData)
};

const CompObjectType *
getSignalArgMapObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_SIGNAL_ARG_MAP_TYPE_NAME,
	    .i.version	     = COMPIZ_SIGNAL_ARG_MAP_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &signalArgMapObjectVTable.base,
	    .i.vTable.noop   = &noopSignalArgMapObjectVTable.base,
	    .i.vTable.size   = sizeof (signalArgMapObjectVTable),
	    .i.instance.size = sizeof (CompSignalArgMap),

	    .intProp  = signalArgMapTypeIntProp,
	    .nIntProp = N_ELEMENTS (signalArgMapTypeIntProp)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

static CompObjectType *
signalArgMapObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface signalArgMapTemplate = *template;

    if (!signalArgMapTemplate.i.base.name)
    {
	signalArgMapTemplate.i.base.name    = COMPIZ_SIGNAL_ARG_MAP_TYPE_NAME;
	signalArgMapTemplate.i.base.version = COMPIZ_SIGNAL_ARG_MAP_VERSION;
    }

    if (!signalArgMapTemplate.i.vTable.size)
	signalArgMapTemplate.i.vTable.size = sizeof (signalArgMapObjectVTable);

    if (!signalArgMapTemplate.i.version)
	signalArgMapTemplate.i.version = COMPIZ_SIGNAL_ARG_MAP_VERSION;

    return cObjectTypeFromTemplate (&signalArgMapTemplate);
}

static void
boolSignalArgMapGetProp (CompObject   *object,
			 unsigned int what,
			 void	      *value)
{
    cGetObjectProp (&GET_BOOL_SIGNAL_ARG_MAP (object)->data.base,
		    getBoolSignalArgMapObjectType (),
		    what, value);
}

static CompBool
boolMap (CompSignalArgMap *sa,
	 const char       *path,
	 const char       *interface,
	 const char       *name,
	 const char       *signature,
	 CompAnyValue     *value,
	 int	          nValue,
	 CompAnyValue     *v)
{
    if (sa->data.source)
    {
	int source = sa->data.source - SIGNAL_MAP_SOURCE_VALUE_BASE;

	if (source < 0 || source >= nValue)
	    return FALSE;

	if (signature[source] != 'b')
	    return FALSE;

	v->b = value[source].b;
    }
    else
    {
	v->b = GET_BOOL_SIGNAL_ARG_MAP (sa)->data.constant;
    }

    return TRUE;
}

static const CompSignalArgMapVTable boolSignalArgMapObjectVTable = {
    .base.getProp = boolSignalArgMapGetProp,

    .map = boolMap
};

static const CBoolProp signalArgMapTypeBoolProp[] = {
    C_PROP (constant, CompBoolSignalArgMapData)
};

const CompObjectType *
getBoolSignalArgMapObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_BOOL_SIGNAL_ARG_MAP_TYPE_NAME,
	    .i.vTable.impl   = &boolSignalArgMapObjectVTable.base,
	    .i.instance.size = sizeof (CompBoolSignalArgMap),

	    .boolProp  = signalArgMapTypeBoolProp,
	    .nBoolProp = N_ELEMENTS (signalArgMapTypeBoolProp)
	};

	type = signalArgMapObjectTypeFromTemplate (&template);
    }

    return type;
}


static void
intSignalArgMapGetProp (CompObject   *object,
			unsigned int what,
			void	     *value)
{
    cGetObjectProp (&GET_INT_SIGNAL_ARG_MAP (object)->data.base,
		    getIntSignalArgMapObjectType (),
		    what, value);
}

static CompInt
intMap (CompSignalArgMap *sa,
	const char       *path,
	const char       *interface,
	const char       *name,
	const char       *signature,
	CompAnyValue     *value,
	int	         nValue,
	CompAnyValue     *v)
{
    if (sa->data.source)
    {
	int source = sa->data.source - SIGNAL_MAP_SOURCE_VALUE_BASE;

	if (source < 0 || source >= nValue)
	    return FALSE;

	if (signature[source] != 'i')
	    return FALSE;

	v->i = value[source].i;
    }
    else
    {
	v->i = GET_INT_SIGNAL_ARG_MAP (sa)->data.constant;
    }

    return TRUE;
}

static const CompSignalArgMapVTable intSignalArgMapObjectVTable = {
    .base.getProp = intSignalArgMapGetProp,

    .map = intMap
};

static const CIntProp intSignalArgMapTypeIntProp[] = {
    C_INT_PROP (constant, CompIntSignalArgMapData, 0, ~0)
};

const CompObjectType *
getIntSignalArgMapObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_INT_SIGNAL_ARG_MAP_TYPE_NAME,
	    .i.vTable.impl   = &intSignalArgMapObjectVTable.base,
	    .i.instance.size = sizeof (CompIntSignalArgMap),

	    .intProp  = signalArgMapTypeIntProp,
	    .nIntProp = N_ELEMENTS (signalArgMapTypeIntProp)
	};

	type = signalArgMapObjectTypeFromTemplate (&template);
    }

    return type;
}


static void
doubleSignalArgMapGetProp (CompObject   *object,
			   unsigned int what,
			   void	        *value)
{
    cGetObjectProp (&GET_DOUBLE_SIGNAL_ARG_MAP (object)->data.base,
		    getDoubleSignalArgMapObjectType (),
		    what, value);
}

static CompBool
doubleMap (CompSignalArgMap *sa,
	   const char       *path,
	   const char       *interface,
	   const char       *name,
	   const char       *signature,
	   CompAnyValue     *value,
	   int	            nValue,
	   CompAnyValue     *v)
{
    if (sa->data.source)
    {
	int source = sa->data.source - SIGNAL_MAP_SOURCE_VALUE_BASE;

	if (source < 0 || source >= nValue)
	    return FALSE;

	if (signature[source] != 'd')
	    return FALSE;

	v->d = value[source].d;
    }
    else
    {
	v->d = GET_DOUBLE_SIGNAL_ARG_MAP (sa)->data.constant;
    }

    return TRUE;
}

static const CompSignalArgMapVTable doubleSignalArgMapObjectVTable = {
    .base.getProp = doubleSignalArgMapGetProp,

    .map = doubleMap
};

static const CDoubleProp signalArgMapTypeDoubleProp[] = {
    C_PROP (constant, CompDoubleSignalArgMapData)
};

const CompObjectType *
getDoubleSignalArgMapObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_DOUBLE_SIGNAL_ARG_MAP_TYPE_NAME,
	    .i.vTable.impl   = &doubleSignalArgMapObjectVTable.base,
	    .i.instance.size = sizeof (CompDoubleSignalArgMap),

	    .doubleProp  = signalArgMapTypeDoubleProp,
	    .nDoubleProp = N_ELEMENTS (signalArgMapTypeDoubleProp)
	};

	type = signalArgMapObjectTypeFromTemplate (&template);
    }

    return type;
}


static void
stringSignalArgMapGetProp (CompObject   *object,
			   unsigned int what,
			   void	        *value)
{
    cGetObjectProp (&GET_STRING_SIGNAL_ARG_MAP (object)->data.base,
		    getStringSignalArgMapObjectType (),
		    what, value);
}

static CompBool
stringMap (CompSignalArgMap *sa,
	   const char       *path,
	   const char       *interface,
	   const char       *name,
	   const char       *signature,
	   CompAnyValue     *value,
	   int	            nValue,
	   CompAnyValue     *v)
{
    char *s;

    if (sa->data.source)
    {
	if (sa->data.source < SIGNAL_MAP_SOURCE_VALUE_BASE)
	{
	    switch (sa->data.source) {
	    case SIGNAL_MAP_SOURCE_PATH:
		s = strdup (path);
		break;
	    case SIGNAL_MAP_SOURCE_INTERFACE:
		s = strdup (interface);
		break;
	    case SIGNAL_MAP_SOURCE_NAME:
		s = strdup (name);
		break;
	    default:
		s = strdup (signature);
		break;
	    }
	}
	else
	{
	    int source = sa->data.source - SIGNAL_MAP_SOURCE_VALUE_BASE;

	    if (signature[source] != 's' && signature[source] != 'o')
		return FALSE;

	    s = strdup (value[source].s);
	}
    }
    else
    {
	s = strdup (GET_STRING_SIGNAL_ARG_MAP (sa)->data.constant);
    }

    if (!s)
    {
	compLog (&sa->u.base,
		 getStringSignalArgMapObjectType (),
		 offsetof (CompSignalArgMapVTable, map),
		 NO_MEMORY_ERROR_STRING);

	return FALSE;
    }

    v->s = s;

    return TRUE;
}

static const CompSignalArgMapVTable stringSignalArgMapObjectVTable = {
    .base.getProp = stringSignalArgMapGetProp,

    .map = stringMap
};

static const CStringProp signalArgMapTypeStringProp[] = {
    C_PROP (constant, CompStringSignalArgMapData)
};

const CompObjectType *
getStringSignalArgMapObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_STRING_SIGNAL_ARG_MAP_TYPE_NAME,
	    .i.vTable.impl   = &stringSignalArgMapObjectVTable.base,
	    .i.instance.size = sizeof (CompStringSignalArgMap),

	    .stringProp  = signalArgMapTypeStringProp,
	    .nStringProp = N_ELEMENTS (signalArgMapTypeStringProp)
	};

	type = signalArgMapObjectTypeFromTemplate (&template);
    }

    return type;
}
