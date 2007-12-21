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

static CInterface pointerInterface[] = {
    C_INTERFACE (pointer, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompObjectVTable pointerObjectVTable = { 0 };

static CompBool
pointerInitObject (const CompObjectInstantiator *instantiator,
		   CompObject		        *object,
		   const CompObjectFactory      *factory)
{
    POINTER (object);

    if (!cObjectInit (instantiator, object, factory))
	return FALSE;

    p->x = 0;
    p->y = 0;

    return TRUE;
}

static void
pointerFiniObject (const CompObjectInstantiator *instantiator,
		   CompObject		        *object,
		   const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType pointerObjectType = {
    .name.name   = POINTER_TYPE_NAME,
    .name.base   = INPUT_TYPE_NAME,
    .vTable.impl = &pointerObjectVTable,
    .vTable.size = sizeof (pointerObjectVTable),
    .funcs.init  = pointerInitObject,
    .funcs.fini  = pointerFiniObject
};

const CompObjectType *
getPointerObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&pointerObjectVTable);
	init = TRUE;
    }

    return &pointerObjectType;
}
