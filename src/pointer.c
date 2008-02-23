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

#include <compiz/pointer.h>
#include <compiz/c-object.h>

static CompBool
pointerInitObject (CompObject *object)
{
    POINTER (object);

    p->x = 0;
    p->y = 0;

    return TRUE;
}

static void
pointerGetProp (CompObject   *object,
		unsigned int what,
		void	     *value)
{
    cGetObjectProp (&GET_POINTER (object)->data,
		    getPointerObjectType (),
		    what, value);
}

static const CompObjectVTable pointerObjectVTable = {
    .getProp = pointerGetProp
};

const CompObjectType *
getPointerObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	    = COMPIZ_POINTER_TYPE_NAME,
	    .i.version	    = COMPIZ_POINTER_VERSION,
	    .i.base.name    = COMPIZ_INPUT_TYPE_NAME,
	    .i.base.version = COMPIZ_INPUT_VERSION,
	    .i.vTable.impl  = &pointerObjectVTable,
	    .i.vTable.size  = sizeof (pointerObjectVTable),

	    .init = pointerInitObject
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
