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

#ifndef _COMPIZ_WIDGET_H
#define _COMPIZ_WIDGET_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompWidget CompWidget;

typedef void (*ButtonEventProc) (CompWidget *w,
				 int32_t    button,
				 int32_t    modifiers,
				 int32_t    x,
				 int32_t    y);

typedef void (*KeyEventProc) (CompWidget *w,
			      const char *key,
			      int32_t    modifiers);

typedef struct _CompWidgetVTable {
    CompObjectVTable base;

    /* public signals */
    ButtonEventProc buttonPress;
    ButtonEventProc buttonRelease;
    KeyEventProc    keyPress;
    KeyEventProc    keyRelease;
} CompWidgetVTable;

struct _CompWidget {
    union {
	CompObject	       base;
	const CompWidgetVTable *vTable;
    } u;

    CompObjectData data;
};

#define GET_WIDGET(object) ((CompWidget *) (object))
#define WIDGET(object) CompWidget *w = GET_WIDGET (object)

#define COMPIZ_WIDGET_VERSION   20080307
#define COMPIZ_WIDGET_TYPE_NAME "org.compiz.widget"

const CompObjectType *
getWidgetObjectType (void);

COMPIZ_END_DECLS

#endif
