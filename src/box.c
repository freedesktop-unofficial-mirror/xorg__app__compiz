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

#include <compiz/box.h>
#include <compiz/c-object.h>

static void
boxGetProp (CompObject   *object,
	    unsigned int what,
	    void	 *value)
{
    cGetObjectProp (&GET_BOX (object)->data.base,
		    getBoxObjectType (),
		    what, value);
}

static const CompObjectVTable boxObjectVTable = {
    .getProp = boxGetProp
};

static const CIntProp boxTypeIntProp[] = {
    C_PROP (x1, CompBoxData),
    C_PROP (y1, CompBoxData),
    C_PROP (x2, CompBoxData),
    C_PROP (y2, CompBoxData)
};

const CompObjectType *
getBoxObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name.name     = COMPIZ_BOX_TYPE_NAME,
	    .i.vTable.impl   = &boxObjectVTable,
	    .i.instance.size = sizeof (CompBox),

	    .intProp  = boxTypeIntProp,
	    .nIntProp = N_ELEMENTS (boxTypeIntProp),

	    .version = COMPIZ_BOX_VERSION
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
