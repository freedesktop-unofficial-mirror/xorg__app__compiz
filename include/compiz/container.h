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

#ifndef _COMPIZ_CONTAINER_H
#define _COMPIZ_CONTAINER_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompContainerItem {
    CompObject *object;
    char       *name;
} CompContainerItem;

typedef struct _CompContainer {
    CompObject	      base;
    CompObjectData    data;
    CompContainerItem *item;
    int		      nItem;
} CompContainer;

#define GET_CONTAINER(object) ((CompContainer *) (object))
#define CONTAINER(object) CompContainer *c = GET_CONTAINER (object)

#define COMPIZ_CONTAINER_VERSION   20080221
#define COMPIZ_CONTAINER_TYPE_NAME COMPIZ_NAME_PREFIX "container"

const CompObjectType *
getContainerObjectType (void);

COMPIZ_END_DECLS

#endif
