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

#include <compiz/input.h>
#include <compiz/c-object.h>

static const CInterface inputInterface[] = {
    C_INTERFACE (input, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompObjectVTable inputObjectVTable = { 0 };

static CompBool
inputInitObject (const CompObjectFactory *factory,
		 CompObject	         *object)
{
    return cObjectInit (factory, object, getObjectType (), &inputObjectVTable);
}

static void
inputFiniObject (const CompObjectFactory *factory,
		 CompObject	         *object)
{
    cObjectFini (factory, object, getObjectType ());
}

static void
inputInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType inputObjectType = {
    INPUT_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	inputInitObject,
	inputFiniObject
    },
    0,
    NULL,
    inputInitVTable
};

static void
inputGetCContext (CompObject *object,
		  CContext   *ctx)
{
    INPUT (object);

    ctx->interface  = inputInterface;
    ctx->nInterface = N_ELEMENTS (inputInterface);
    ctx->type	    = &inputObjectType;
    ctx->data	    = (char *) i;
    ctx->svOffset   = 0;
    ctx->vtStore    = &i->object;
    ctx->version    = COMPIZ_INPUT_VERSION;
}

CompObjectType *
getInputObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&inputObjectVTable, inputGetCContext,
			   inputObjectType.initVTable);
	init = TRUE;
    }

    return &inputObjectType;
}
