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

static char *
nameObject (CompObject *object)
{
    return NULL;
}

static CompBool
forEachChildObject (CompObject		    *object,
		    ChildObjectCallBackProc proc,
		    void		    *closure)
{
    return TRUE;
}

static CompObject *
findChildObject (CompObject *parent,
		 const char *type,
		 const char *name)
{
    return NULL;
}

static CompObjectVTable objectVTable = {
    nameObject,
    forEachChildObject,
    findChildObject
};

CompBool
compObjectInit (CompObject       *object,
		CompObjectType   *type,
		CompObjectTypeID id)
{
    if (type->privateLen)
    {
	object->privates = malloc (type->privateLen * sizeof (CompPrivate));
	if (!object->privates)
	    return FALSE;
    }
    else
    {
	object->privates = NULL;
    }

    object->id     = id;
    object->type   = type;
    object->vTable = &objectVTable;

    return TRUE;
}

void
compObjectFini (CompObject *object)
{
    if (object->privates)
	free (object->privates);
}

typedef struct _ReallocObjectPrivatesContext {
    CompObjectType *type;
    int		   size;
} ReallocObjectPrivatesContext;

static CompBool
reallocObjectPrivates (CompObject *object,
		       int	  size)
{
    void *privates;

    privates = realloc (object->privates, size * sizeof (CompPrivate));
    if (!privates)
	return FALSE;

    object->privates = (CompPrivate *) privates;

    return TRUE;
}

static CompBool
reallocObjectPrivatesTree (CompChildObject *object,
			   void		   *closure)
{
    CompObjectVTable		 *vTable = object->base.vTable;
    ReallocObjectPrivatesContext *ctx =
	(ReallocObjectPrivatesContext *) closure;

    if (object->base.type == ctx->type)
	if (!reallocObjectPrivates (&object->base, ctx->size))
	    return FALSE;

    return (*vTable->forEachChildObject) (&object->base,
					  reallocObjectPrivatesTree,
					  closure);
}

static int
reallocObjectPrivate (int  size,
		      void *closure)
{
    ReallocObjectPrivatesContext ctx;
    void			 *privates;

    ctx.type = (CompObjectType *) closure;
    ctx.size = size;

    privates = realloc (ctx.type->privates, size * sizeof (CompPrivate));
    if (!privates)
	return FALSE;

    ctx.type->privates = privates;

    if (core.base.type == ctx.type)
	if (!reallocObjectPrivates (&core.base, size))
	    return FALSE;

    return (*core.base.vTable->forEachChildObject) (&core.base,
						    reallocObjectPrivatesTree,
						    (void *) &ctx);
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

CompBool
compChildObjectInit (CompChildObject  *object,
		     CompObjectType   *type,
		     CompObjectTypeID id)
{
    if (!compObjectInit (&object->base, type, id))
	return FALSE;

    object->parent = NULL;

    return TRUE;
}

void
compChildObjectFini (CompChildObject *object)
{
    compObjectFini (&object->base);
}

typedef struct _FindTypeContext {
    const char	   *name;
    CompObjectType *type;
} FindTypeContext;

static CompBool
checkType (CompObjectType *type,
	   void		  *closure)
{
    FindTypeContext *ctx = (FindTypeContext *) closure;

    if (strcmp (ctx->name, type->name) == 0)
    {
	ctx->type = type;
	return FALSE;
    }

    return TRUE;
}

CompObjectType *
compObjectFindType (const char *name)
{
    FindTypeContext ctx;

    ctx.name = name;
    ctx.type = NULL;

    (*core.forEachObjectType) (checkType, (void *) &ctx);

    return ctx.type;
}
