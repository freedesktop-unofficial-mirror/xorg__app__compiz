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

#ifndef _COMPIZ_PROP_H
#define _COMPIZ_PROP_H

#include <compiz/object.h>

#define COMPIZ_PROP_VERSION 20071116

COMPIZ_BEGIN_DECLS

typedef struct _CompProp {
    CompObject     base;
    CompObjectData data;
} CompProp;

#define GET_PROP(object) ((CompProp *) (object))
#define PROP(object) CompProp *p = GET_PROP (object)

#define PROP_TYPE_NAME "prop"

const CompObjectType **
getPropObjectTypes (int *n);


typedef struct _CompBoolPropData {
    CompObjectData base;
    CompBool       value;
} CompBoolPropData;

typedef struct _CompBoolProp {
    CompProp         base;
    CompBoolPropData data;
} CompBoolProp;

#define GET_BOOL_PROP(object) ((CompBoolProp *) (object))
#define BOOL_PROP(object) CompBoolProp *b = GET_BOOL_PROP (object)

#define BOOL_PROP_TYPE_NAME "boolProp"


typedef struct _CompIntPropData {
    CompObjectData base;
    int32_t        value;
} CompIntPropData;

typedef struct _CompIntProp {
    CompProp        base;
    CompIntPropData data;
} CompIntProp;

#define GET_INT_PROP(object) ((CompIntProp *) (object))
#define INT_PROP(object) CompIntProp *i = GET_INT_PROP (object)

#define INT_PROP_TYPE_NAME "intProp"


typedef struct _CompDoublePropData {
    CompObjectData base;
    double        value;
} CompDoublePropData;

typedef struct _CompDoubleProp {
    CompProp           base;
    CompDoublePropData data;
} CompDoubleProp;

#define GET_DOUBLE_PROP(object) ((CompDoubleProp *) (object))
#define DOUBLE_PROP(object) CompDoubleProp *d = GET_DOUBLE_PROP (object)

#define DOUBLE_PROP_TYPE_NAME "doubleProp"


typedef struct _CompStringPropData {
    CompObjectData base;
    char           *value;
} CompStringPropData;

typedef struct _CompStringProp {
    CompProp           base;
    CompStringPropData data;
} CompStringProp;

#define GET_STRING_PROP(object) ((CompStringProp *) (object))
#define STRING_PROP(object) CompStringProp *s = GET_STRING_PROP (object)

#define STRING_PROP_TYPE_NAME "stringProp"

COMPIZ_END_DECLS

#endif
