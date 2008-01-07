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

static CIntProp boxTypeIntProp[] = {
    C_PROP (x1, CompBoxData),
    C_PROP (y1, CompBoxData),
    C_PROP (x2, CompBoxData),
    C_PROP (y2, CompBoxData)
};

static CInterface boxInterface[] = {
    C_INTERFACE (box, Type, CompObjectVTable, _, _, _, _, X, _, _, _)
};

static void
boxGetProp (CompObject   *object,
	    unsigned int what,
	    void	 *value)
{
    static const CMetadata template = {
	.interface  = boxInterface,
	.nInterface = N_ELEMENTS (boxInterface),
	.version    = COMPIZ_BOX_VERSION
    };

    cGetObjectProp (&GET_BOX (object)->data.base, &template, what, value);
}

static const CompObjectVTable boxObjectVTable = {
    .getProp = boxGetProp
};

const CompObjectType *
getBoxObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CompObjectType template = {
	    .name.name     = BOX_TYPE_NAME,
	    .vTable.impl   = &boxObjectVTable,
	    .instance.size = sizeof (CompBox)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
