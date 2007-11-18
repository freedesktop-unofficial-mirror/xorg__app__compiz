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

#include <compiz/output.h>

static CompBool
outputForBaseObject (CompObject	            *object,
		     BaseObjectCallBackProc proc,
		     void		    *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    OUTPUT (object);

    UNWRAP (&o->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&o->object, object, vTable, v.vTable);

    return status;
}

static CompBool
outputForEachType (CompObject       *object,
		   TypeCallBackProc proc,
		   void	            *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    OUTPUT (object);

    if (!(*proc) (object, getOutputObjectType (), closure))
	return FALSE;

    UNWRAP (&o->object, object, vTable);
    status = (*object->vTable->forEachType) (object, proc, closure);
    WRAP (&o->object, object, vTable, v.vTable);

    return status;
}

static CompObjectVTable outputObjectVTable = {
    .forBaseObject = outputForBaseObject,
    .forEachType   = outputForEachType
};

static CompBool
outputInitObject (CompObject *object)
{
    OUTPUT (object);

    if (!compObjectInit (&o->base, getObjectType ()))
	return FALSE;

    WRAP (&o->object, &o->base, vTable, &outputObjectVTable);

    return TRUE;
}

static void
outputFiniObject (CompObject *object)
{
    OUTPUT (object);

    UNWRAP (&o->object, &o->base, vTable);

    compObjectFini (&o->base, getObjectType ());
}

static void
outputInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType outputObjectType = {
    OUTPUT_TYPE_NAME,
    {
	outputInitObject,
	outputFiniObject
    },
    NULL,
    outputInitVTable
};

CompObjectType *
getOutputObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	outputInitVTable (&outputObjectVTable);
	init = TRUE;
    }

    return &outputObjectType;
}
