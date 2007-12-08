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
#include <compiz/c-object.h>

static const CInterface outputInterface[] = {
    C_INTERFACE (output, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompObjectVTable outputObjectVTable = { 0 };

static CompBool
outputInitObject (CompObject *object)
{
    return cObjectInit (object, getObjectType (), &outputObjectVTable);
}

static void
outputFiniObject (CompObject *object)
{
    cObjectFini (object, getObjectType ());
}

static void
outputInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType outputObjectType = {
    OUTPUT_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	outputInitObject,
	outputFiniObject
    },
    0,
    NULL,
    outputInitVTable
};

static void
outputGetCContext (CompObject *object,
		   CContext   *ctx)
{
    OUTPUT (object);

    ctx->interface  = outputInterface;
    ctx->nInterface = N_ELEMENTS (outputInterface);
    ctx->type	    = &outputObjectType;
    ctx->data	    = (char *) o;
    ctx->svOffset   = 0;
    ctx->vtStore    = &o->object;
    ctx->version    = COMPIZ_OUTPUT_VERSION;
}

CompObjectType *
getOutputObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&outputObjectVTable, outputGetCContext,
			   outputObjectType.initVTable);
	init = TRUE;
    }

    return &outputObjectType;
}
