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
outputInitObject (const CompObjectInstantiator *instantiator,
		  CompObject		       *object,
		  const CompObjectFactory      *factory)
{
    return cObjectInit (instantiator, object, factory);
}

static void
outputFiniObject (const CompObjectInstantiator *instantiator,
		  CompObject		       *object,
		  const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType outputObjectType = {
    .name.name   = OUTPUT_TYPE_NAME,
    .name.base   = OBJECT_TYPE_NAME,
    .vTable.impl = &outputObjectVTable,
    .vTable.size = sizeof (outputObjectVTable),
    .funcs.init  = outputInitObject,
    .funcs.fini  = outputFiniObject
};

CompObjectType *
getOutputObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&outputObjectVTable);
	init = TRUE;
    }

    return &outputObjectType;
}
