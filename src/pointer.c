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

static CompBool
pointerForBaseObject (CompObject	     *object,
		      BaseObjectCallBackProc proc,
		      void		     *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    POINTER (object);

    UNWRAP (&p->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&p->object, object, vTable, v.vTable);

    return status;
}

static CompBool
pointerForEachType (CompObject       *object,
		    TypeCallBackProc proc,
		    void	     *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    POINTER (object);

    if (!(*proc) (object, getPointerObjectType (), closure))
	return FALSE;

    UNWRAP (&p->object, object, vTable);
    status = (*object->vTable->forEachType) (object, proc, closure);
    WRAP (&p->object, object, vTable, v.vTable);

    return status;
}

static CompObjectVTable pointerObjectVTable = {
    .forBaseObject = pointerForBaseObject,
    .forEachType   = pointerForEachType
};

static CompBool
pointerInitObject (CompObject *object)
{
    POINTER (object);

    if (!compObjectInit (&p->base.base, getInputObjectType ()))
	return FALSE;

    WRAP (&p->object, &p->base.base, vTable, &pointerObjectVTable);

    p->x = 0;
    p->y = 0;

    return TRUE;
}

static void
pointerFiniObject (CompObject *object)
{
    POINTER (object);

    UNWRAP (&p->object, &p->base.base, vTable);

    compObjectFini (&p->base.base, getInputObjectType ());
}

static void
pointerInitVTable (void *vTable)
{
    (*getInputObjectType ()->initVTable) (vTable);
}

static CompObjectType pointerObjectType = {
    POINTER_TYPE_NAME,
    {
	pointerInitObject,
	pointerFiniObject
    },
    NULL,
    pointerInitVTable
};

CompObjectType *
getPointerObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	pointerInitVTable (&pointerObjectVTable);
	init = TRUE;
    }

    return &pointerObjectType;
}
