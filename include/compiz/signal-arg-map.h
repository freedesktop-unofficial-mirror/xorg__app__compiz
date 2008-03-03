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

#ifndef _COMPIZ_SIGNAL_ARG_MAP_H
#define _COMPIZ_SIGNAL_ARG_MAP_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

#define SIGNAL_MAP_SOURCE_CONSTANT   0
#define SIGNAL_MAP_SOURCE_PATH       1
#define SIGNAL_MAP_SOURCE_INTERFACE  2
#define SIGNAL_MAP_SOURCE_NAME       3
#define SIGNAL_MAP_SOURCE_SIGNATURE  4
#define SIGNAL_MAP_SOURCE_VALUE_BASE 5

typedef struct _CompSignalArgMap CompSignalArgMap;

typedef CompBool (*MapProc) (CompSignalArgMap *sa,
			     const char       *path,
			     const char       *interface,
			     const char	      *name,
			     const char	      *signature,
			     CompAnyValue     *value,
			     int	      nValue,
			     CompAnyValue     *arg);

typedef struct _CompSignalArgMapVTable {
    CompObjectVTable base;

    MapProc map;
} CompSignalArgMapVTable;

typedef struct _CompSignalArgMapData {
    CompObjectData base;
    int32_t	   source;
} CompSignalArgMapData;

struct _CompSignalArgMap {
    union {
	CompObject		     base;
	const CompSignalArgMapVTable *vTable;
    } u;

    CompSignalArgMapData data;
};

#define GET_SIGNAL_ARG_MAP(object) ((CompSignalArgMap *) (object))
#define SIGNAL_ARG_MAP(object)			       \
    CompSignalArgMap *sa = GET_SIGNAL_ARG_MAP (object)

#define COMPIZ_SIGNAL_ARG_MAP_VERSION   20080303
#define COMPIZ_SIGNAL_ARG_MAP_TYPE_NAME "org.compiz.signalArgMap"

const CompObjectType *
getSignalArgMapObjectType (void);


typedef struct _CompBoolSignalArgMapData {
    CompObjectData base;
    CompBool	   constant;
} CompBoolSignalArgMapData;

typedef struct _CompBoolSignalArgMap {
    CompSignalArgMap base;

    CompBoolSignalArgMapData data;
} CompBoolSignalArgMap;

#define GET_BOOL_SIGNAL_ARG_MAP(object) ((CompBoolSignalArgMap *) (object))
#define BOOL_SIGNAL_ARG_MAP(object)			        \
    CompBoolSignalArgMap *ba = GET_BOOL_SIGNAL_ARG_MAP (object)

#define COMPIZ_BOOL_SIGNAL_ARG_MAP_TYPE_NAME "org.compiz.signalArgMap.bool"

const CompObjectType *
getBoolSignalArgMapObjectType (void);


typedef struct _CompIntSignalArgMapData {
    CompObjectData base;
    int32_t	   constant;
} CompIntSignalArgMapData;

typedef struct _CompIntSignalArgMap {
    CompSignalArgMap base;

    CompIntSignalArgMapData data;
} CompIntSignalArgMap;

#define GET_INT_SIGNAL_ARG_MAP(object) ((CompIntSignalArgMap *) (object))
#define INT_SIGNAL_ARG_MAP(object)			      \
    CompIntSignalArgMap *ia = GET_INT_SIGNAL_ARG_MAP (object)

#define COMPIZ_INT_SIGNAL_ARG_MAP_TYPE_NAME "org.compiz.signalArgMap.int"

const CompObjectType *
getIntSignalArgMapObjectType (void);


typedef struct _CompDoubleSignalArgMapData {
    CompObjectData base;
    double	   constant;
} CompDoubleSignalArgMapData;

typedef struct _CompDoubleSignalArgMap {
    CompSignalArgMap base;

    CompDoubleSignalArgMapData data;
} CompDoubleSignalArgMap;

#define GET_DOUBLE_SIGNAL_ARG_MAP(object) \
    ((CompDoubleSignalArgMap *) (object))
#define DOUBLE_SIGNAL_ARG_MAP(object)				    \
    CompDoubleSignalArgMap *da = GET_DOUBLE_SIGNAL_ARG_MAP (object)

#define COMPIZ_DOUBLE_SIGNAL_ARG_MAP_TYPE_NAME "org.compiz.signalArgMap.double"

const CompObjectType *
getDoubleSignalArgMapObjectType (void);


typedef struct _CompStringSignalArgMapData {
    CompObjectData base;
    char	   *constant;
} CompStringSignalArgMapData;

typedef struct _CompStringSignalArgMap {
    CompSignalArgMap base;

    CompStringSignalArgMapData data;
} CompStringSignalArgMap;

#define GET_STRING_SIGNAL_ARG_MAP(object) \
    ((CompStringSignalArgMap *) (object))
#define STRING_SIGNAL_ARG_MAP(object)				    \
    CompStringSignalArgMap *da = GET_STRING_SIGNAL_ARG_MAP (object)

#define COMPIZ_STRING_SIGNAL_ARG_MAP_TYPE_NAME "org.compiz.signalArgMap.string"

const CompObjectType *
getStringSignalArgMapObjectType (void);

COMPIZ_END_DECLS

#endif
