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
propForBaseObject (CompObject	          *object,
		   BaseObjectCallBackProc proc,
		   void		          *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    PROP (object);

    UNWRAP (&p->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&p->object, object, vTable, v.vTable);

    return status;
}

static CompBool
propForEachType (CompObject       *object,
		 TypeCallBackProc proc,
		 void	          *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    PROP (object);

    if (!(*proc) (object, getPropObjectType (), closure))
	return FALSE;

    UNWRAP (&p->object, object, vTable);
    status = (*object->vTable->forEachType) (object, proc, closure);
    WRAP (&p->object, object, vTable, v.vTable);

    return status;
}

static CompObjectVTable propObjectVTable = {
    .forBaseObject = propForBaseObject,
    .forEachType   = propForEachType,
};

static CompBool
propInitObject (CompObject *object)
{
    PROP (object);

    if (!compObjectInit (&p->base, getObjectType ()))
	return FALSE;

    WRAP (&p->object, &p->base, vTable, &propObjectVTable);

    return TRUE;
}

static void
propFiniObject (CompObject *object)
{
    PROP (object);

    UNWRAP (&p->object, &p->base, vTable);

    compObjectFini (&p->base, getObjectType ());
}

static void
propInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType propObjectType = {
    PROP_TYPE_NAME,
    {
	propInitObject,
	propFiniObject
    },
    NULL,
    propInitVTable
};

CompObjectType *
getPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	propInitVTable (&propObjectVTable);
	init = TRUE;
    }

    return &propObjectType;
}
