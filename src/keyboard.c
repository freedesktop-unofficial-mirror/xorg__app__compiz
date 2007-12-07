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

static const CInterface keyboardInterface[] = {
    C_INTERFACE (keyboard, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompObjectVTable keyboardObjectVTable = { 0 };

static CompBool
keyboardInitObject (CompObject *object)
{
    KEYBOARD (object);

    if (!cObjectInit (object, getInputObjectType (), &keyboardObjectVTable))
	return FALSE;

    k->state = 0;

    return TRUE;
}

static void
keyboardFiniObject (CompObject *object)
{
    cObjectFini (object, getInputObjectType ());
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
    0,
    NULL,
    keyboardInitVTable
};

static void
keyboardGetCContext (CompObject *object,
		   CContext   *ctx)
{
    KEYBOARD (object);

    ctx->interface  = keyboardInterface;
    ctx->nInterface = N_ELEMENTS (keyboardInterface);
    ctx->type	    = &keyboardObjectType;
    ctx->data	    = (char *) k;
    ctx->svOffset   = 0;
    ctx->vtStore    = &k->object;
    ctx->version    = COMPIZ_KEYBOARD_VERSION;
}

CompObjectType *
getKeyboardObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&keyboardObjectVTable, keyboardGetCContext,
			   keyboardObjectType.initVTable);
	init = TRUE;
    }

    return &keyboardObjectType;
}
