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

#include <compiz-core.h>

void
compObjectInit (CompObject     *object,
		CompPrivate    *privates,
		CompObjectType *type,
		CompObjectTypeID id)
{
    object->id       = id;
    object->privates = privates;
    object->parent   = NULL;
    object->type     = type;
}

typedef struct _ReallocObjectPrivatesContext {
    CompObjectType *type;
    int		   size;
} ReallocObjectPrivatesContext;

static CompBool
reallocObjectPrivatesTree (CompObject *object,
			   void       *closure)
{
    ReallocObjectPrivatesContext *ctx =
	(ReallocObjectPrivatesContext *) closure;

    if (object->type == ctx->type)
    {
	void *privates;

	privates = realloc (object->privates,
			    ctx->size * sizeof (CompPrivate));
	if (!privates)
	    return FALSE;

	object->privates = (CompPrivate *) privates;
    }

    return (*object->type->forEachObject) (object,
					   reallocObjectPrivatesTree,
					   closure);
}

static int
reallocObjectPrivate (int  size,
		      void *closure)
{
    ReallocObjectPrivatesContext ctx;

    ctx.type = (CompObjectType *) closure;
    ctx.size = size;

    return reallocObjectPrivatesTree (&core.base, (void *) &ctx);
}

int
compObjectAllocatePrivateIndex (CompObjectType *type)
{
    return allocatePrivateIndex (&type->privateLen,
				 &type->privateIndices,
				 reallocObjectPrivate,
				 (void *) type);
}

void
compObjectFreePrivateIndex (CompObjectType *type,
			    int	           index)
{
    freePrivateIndex (type->privateLen, type->privateIndices, index);
}
