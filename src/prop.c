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

#include <compiz/prop.h>
#include <compiz/c-object.h>

static const CInterface propInterface[] = {
    C_INTERFACE (prop, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompObjectVTable propObjectVTable = { 0 };

static CompBool
propInitObject (CompObject *object)
{
    return cObjectInit (object, getObjectType (), &propObjectVTable, 0);
}

static void
propFiniObject (CompObject *object)
{
    cObjectFini (object, getObjectType (), 0);
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

static void
propGetCContect (CompObject *object,
		 CContext   *ctx)
{
    PROP (object);

    ctx->interface  = propInterface;
    ctx->nInterface = N_ELEMENTS (propInterface);
    ctx->type	    = &propObjectType;
    ctx->data	    = (char *) p;
    ctx->vtStore    = &p->object;
    ctx->version    = COMPIZ_PROP_VERSION;
}

CompObjectType *
getPropObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&propObjectVTable, propGetCContect,
			   propObjectType.initVTable);
	init = TRUE;
    }

    return &propObjectType;
}
