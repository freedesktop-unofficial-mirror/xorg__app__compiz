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
    cGetObjectProp (&GET_KEYBOARD (object)->data,
		    getKeyboardObjectType (),
		    what, value);
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
	static const CObjectInterface template = {
	    .i.name	    = COMPIZ_KEYBOARD_TYPE_NAME,
	    .i.version	    = COMPIZ_KEYBOARD_VERSION,
	    .i.base.name    = COMPIZ_INPUT_TYPE_NAME,
	    .i.base.version = COMPIZ_INPUT_VERSION,
	    .i.vTable.impl  = &keyboardObjectVTable,

	    .init = keyboardInitObject
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

static void
keyEventDescGetProp (CompObject   *object,
		     unsigned int what,
		     void	  *value)
{
    cGetObjectProp (&GET_KEY_EVENT_DESCRIPTION (object)->data.base,
		    getKeyEventDescriptionObjectType (),
		    what, value);
}

static const CompObjectVTable keyEventDescObjectVTable = {
    .getProp = keyEventDescGetProp
};

static const CIntProp keyEventDescTypeIntProp[] = {
    C_INT_PROP (modifiers, CompKeyEventDescriptionData,
		0, (1 << KEYBOARD_MODIFIER_NUM) - 1)
};

static const CStringProp keyEventDescTypeStringProp[] = {
    C_PROP (key, CompKeyEventDescriptionData)
};

const CompObjectType *
getKeyEventDescriptionObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_KEY_EVENT_DESCRIPTION_TYPE_NAME,
	    .i.version	     = COMPIZ_KEY_EVENT_DESCRIPTION_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &keyEventDescObjectVTable,
	    .i.instance.size = sizeof (CompKeyEventDescription),

	    .intProp  = keyEventDescTypeIntProp,
	    .nIntProp = N_ELEMENTS (keyEventDescTypeIntProp),

	    .stringProp  = keyEventDescTypeStringProp,
	    .nStringProp = N_ELEMENTS (keyEventDescTypeStringProp)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
