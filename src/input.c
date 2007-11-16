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

#include <compiz/core.h>

static CompBool
inputForBaseObject (CompObject	           *object,
		    BaseObjectCallBackProc proc,
		    void		   *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    INPUT (object);

    UNWRAP (&i->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&i->object, object, vTable, v.vTable);

    return status;
}

static CompBool
inputForEachType (CompObject       *object,
		  TypeCallBackProc proc,
		  void	           *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    INPUT (object);

    if (!(*proc) (object, getInputObjectType (), closure))
	return FALSE;

    UNWRAP (&i->object, object, vTable);
    status = (*object->vTable->forEachType) (object, proc, closure);
    WRAP (&i->object, object, vTable, v.vTable);

    return status;
}

static CompObjectVTable inputObjectVTable = {
    .forBaseObject = inputForBaseObject,
    .forEachType   = inputForEachType
};

static CompBool
inputInitObject (CompObject *object)
{
    INPUT (object);

    if (!compObjectInit (&i->base, getObjectType ()))
	return FALSE;

    WRAP (&i->object, &i->base, vTable, &inputObjectVTable);

    return TRUE;
}

static void
inputFiniObject (CompObject *object)
{
    INPUT (object);

    UNWRAP (&i->object, &i->base, vTable);

    compObjectFini (&i->base, getObjectType ());
}

static void
inputInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType inputObjectType = {
    INPUT_TYPE_NAME,
    {
	inputInitObject,
	inputFiniObject
    },
    NULL,
    inputInitVTable
};

CompObjectType *
getInputObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	inputInitVTable (&inputObjectVTable);
	init = TRUE;
    }

    return &inputObjectType;
}
