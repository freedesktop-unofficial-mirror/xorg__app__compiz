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

#ifndef _COMPIZ_KEYBOARD_H
#define _COMPIZ_KEYBOARD_H

#include <compiz/input.h>

COMPIZ_BEGIN_DECLS

#define KEYBOARD_MODIFIER_SHIFT	      0
#define KEYBOARD_MODIFIER_CONTROL     1
#define KEYBOARD_MODIFIER_ALT	      2
#define KEYBOARD_MODIFIER_META	      3
#define KEYBOARD_MODIFIER_SUPER	      4
#define KEYBOARD_MODIFIER_HYPER	      5
#define KEYBOARD_MODIFIER_MODE_SWITCH 6
#define KEYBOARD_MODIFIER_NUM	      7

typedef struct _CompKeyboard {
    CompInput      base;
    CompObjectData data;
    unsigned int   state;
} CompKeyboard;

#define GET_KEYBOARD(object) ((CompKeyboard *) (object))
#define KEYBOARD(object) CompKeyboard *k = GET_KEYBOARD (object)

#define COMPIZ_KEYBOARD_VERSION   20080221
#define COMPIZ_KEYBOARD_TYPE_NAME "org.compiz.keyboard"

const CompObjectType *
getKeyboardObjectType (void);

COMPIZ_END_DECLS

#endif
