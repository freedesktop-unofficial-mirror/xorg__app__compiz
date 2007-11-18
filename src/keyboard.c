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

static CompBool
keyboardForBaseObject (CompObject	      *object,
		       BaseObjectCallBackProc proc,
		       void		      *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    KEYBOARD (object);

    UNWRAP (&k->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&k->object, object, vTable, v.vTable);

    return status;
}

static CompBool
keyboardForEachType (CompObject       *object,
		     TypeCallBackProc proc,
		     void	      *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    KEYBOARD (object);

    if (!(*proc) (object, getKeyboardObjectType (), closure))
	return FALSE;

    UNWRAP (&k->object, object, vTable);
    status = (*object->vTable->forEachType) (object, proc, closure);
    WRAP (&k->object, object, vTable, v.vTable);

    return status;
}

static CompObjectVTable keyboardObjectVTable = {
    .forBaseObject = keyboardForBaseObject,
    .forEachType   = keyboardForEachType
};

static CompBool
keyboardInitObject (CompObject *object)
{
    KEYBOARD (object);

    if (!compObjectInit (&k->base.base, getInputObjectType ()))
	return FALSE;

    WRAP (&k->object, &k->base.base, vTable, &keyboardObjectVTable);

    k->state = 0;

    return TRUE;
}

static void
keyboardFiniObject (CompObject *object)
{
    KEYBOARD (object);

    UNWRAP (&k->object, &k->base.base, vTable);

    compObjectFini (&k->base.base, getInputObjectType ());
}

static void
keyboardInitVTable (void *vTable)
{
    (*getInputObjectType ()->initVTable) (vTable);
}

static CompObjectType keyboardObjectType = {
    KEYBOARD_TYPE_NAME,
    {
	keyboardInitObject,
	keyboardFiniObject
    },
    NULL,
    keyboardInitVTable
};

CompObjectType *
getKeyboardObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	keyboardInitVTable (&keyboardObjectVTable);
	init = TRUE;
    }

    return &keyboardObjectType;
}
