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

#include <compiz/keyboard.h>
#include <compiz/c-object.h>

static CInterface keyboardInterface[] = {
    C_INTERFACE (keyboard, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompBool
keyboardInitObject (CompObject *object)
{
    KEYBOARD (object);

    k->state = 0;

    return TRUE;
}

static void
keyboardGetProp (CompObject   *object,
		  unsigned int what,
		  void	       *value)
{
    static const CMetadata template = {
	.interface  = keyboardInterface,
	.nInterface = N_ELEMENTS (keyboardInterface),
	.init       = keyboardInitObject,
	.version    = COMPIZ_KEYBOARD_VERSION
    };

    cGetObjectProp (&GET_KEYBOARD (object)->data, &template, what, value);
}

static const CompObjectVTable keyboardObjectVTable = {
    .getProp = keyboardGetProp
};

const CompObjectType *
getKeyboardObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CompObjectType template = {
	    .name.name   = KEYBOARD_TYPE_NAME,
	    .name.base   = INPUT_TYPE_NAME,
	    .vTable.impl = &keyboardObjectVTable
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
