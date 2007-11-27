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

#include <compiz/branch.h>
#include <compiz/c-object.h>

static const CInterface branchInterface[] = {
    C_INTERFACE (branch, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

typedef struct _ForEachTypeContext {
    TypeCallBackProc proc;
    void	     *closure;
} ForEachTypeContext;

static CompBool
baseObjectForEachType (CompObject *object,
		       void       *closure)
{
    ForEachTypeContext *pCtx = (ForEachTypeContext *) closure;

    BRANCH (object);

    return (*b->u.vTable->forEachType) (object, pCtx->proc, pCtx->closure);
}

static CompBool
noopForEachType (CompObject	  *object,
		 TypeCallBackProc proc,
		 void	          *closure)
{
    ForEachTypeContext ctx;

    BRANCH (object);

    ctx.proc    = proc;
    ctx.closure = closure;

    return (*b->u.base.vTable->forBaseObject) (object,
					       baseObjectForEachType,
					       (void *) &ctx);
}

static CompBool
forEachType (CompObject	      *object,
	     TypeCallBackProc proc,
	     void	      *closure)
{
    int i;

    BRANCH (object);

    for (i = 0; i < b->nInstance; i++)
	if (!(*proc) (object, b->instance[i].type, closure))
	    return FALSE;

    return TRUE;
}

static CompBranchVTable branchObjectVTable = {
    .forEachType = forEachType
};

static CompBool
branchInitObject (CompObject *object)
{
    BRANCH (object);

    if (!cObjectInit (&b->u.base, getObjectType (), &branchObjectVTable.base))
	return FALSE;

    b->instance  = NULL;
    b->nInstance = 0;

    return TRUE;
}

static void
branchFiniObject (CompObject *object)
{
    cObjectFini (object, getObjectType ());
}

static void
branchInitVTable (CompBranchVTable *vTable)
{
    (*getObjectType ()->initVTable) (&vTable->base);

    ENSURE (vTable, forEachType, noopForEachType);
}

static CompObjectType branchObjectType = {
    BRANCH_TYPE_NAME,
    {
	branchInitObject,
	branchFiniObject
    },
    0,
    NULL,
    (InitVTableProc) branchInitVTable
};

static void
branchGetCContect (CompObject *object,
		   CContext   *ctx)
{
    BRANCH (object);

    ctx->interface  = branchInterface;
    ctx->nInterface = N_ELEMENTS (branchInterface);
    ctx->type	    = &branchObjectType;
    ctx->data	    = (char *) b;
    ctx->vtStore    = &b->object;
    ctx->version    = COMPIZ_BRANCH_VERSION;
}

CompObjectType *
getBranchObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&branchObjectVTable.base, branchGetCContect,
			   branchObjectType.initVTable);
	init = TRUE;
    }

    return &branchObjectType;
}
