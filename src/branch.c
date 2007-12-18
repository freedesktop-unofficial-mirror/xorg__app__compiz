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

#include <stdlib.h>
#include <string.h>

#include <compiz/branch.h>
#include <compiz/c-object.h>

static const CInterface branchInterface[] = {
    C_INTERFACE (branch, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static void
branchGetProp (CompObject   *object,
	       unsigned int what,
	       void	    *value)
{
    switch (what) {
    case COMP_ADDRESS_BASE_VTABLE_STORE:
	*((CompObjectVTableVec **) value) = &GET_BRANCH (object)->object;
    }
}

typedef struct _ForEachTypeContext {
    const char       *interface;
    TypeCallBackProc proc;
    void	     *closure;
} ForEachTypeContext;

static CompBool
baseObjectForEachType (CompObject *object,
		       void       *closure)
{
    ForEachTypeContext *pCtx = (ForEachTypeContext *) closure;

    BRANCH (object);

    return (*b->u.vTable->forEachType) (b,
					pCtx->interface,
					pCtx->proc,
					pCtx->closure);
}

static CompBool
noopForEachType (CompBranch	  *b,
		 const char       *interface,
		 TypeCallBackProc proc,
		 void	          *closure)
{
    ForEachTypeContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return (*b->u.base.vTable->forBaseObject) (&b->u.base,
					       baseObjectForEachType,
					       (void *) &ctx);
}

static CompBool
forEachType (CompBranch	      *b,
	     const char       *interface,
	     TypeCallBackProc proc,
	     void	      *closure)
{
    return TRUE;
}

typedef struct _RegisterTypeContext {
    const char           *interface;
    const CompObjectType *type;
} RegisterTypeContext;

static CompBool
baseObjectRegisterType (CompObject *object,
			void       *closure)
{
    RegisterTypeContext *pCtx = (RegisterTypeContext *) closure;

    BRANCH (object);

    return (*b->u.vTable->registerType) (b, pCtx->interface, pCtx->type);
}

static CompBool
noopRegisterType (CompBranch	       *b,
		  const char           *interface,
		  const CompObjectType *type)
{
    RegisterTypeContext ctx;

    ctx.interface = interface;
    ctx.type      = type;

    return (*b->u.base.vTable->forBaseObject) (&b->u.base,
					       baseObjectRegisterType,
					       (void *) &ctx);
}

static CompBool
registerType (CompBranch	   *b,
	      const char	   *interface,
	      const CompObjectType *type)
{
    return compFactoryRegisterType (&b->factory, interface, type);
}

static CompBranchVTable branchObjectVTable = {
    .base.getProp = branchGetProp,
    .forEachType  = forEachType,
    .registerType = registerType
};

static CompBool
branchInitObject (const CompObjectInstantiator *instantiator,
		  CompObject		       *object,
		  const CompObjectFactory      *factory)
{
    BRANCH (object);

    if (!cObjectInit (instantiator, object, factory))
	return FALSE;

    b->factory.master        = factory;
    b->factory.instantiators = NULL;

    return TRUE;
}

static void
branchFiniObject (const CompObjectInstantiator *instantiator,
		  CompObject		       *object,
		  const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static const CompBranchVTable noopBranchObjectVTable = {
    .forEachType  = noopForEachType,
    .registerType = noopRegisterType
};

static CompObjectType branchObjectType = {
    BRANCH_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	branchInitObject,
	branchFiniObject
    },
    0,
    sizeof (CompBranchVTable),
    &branchObjectVTable.base,
    &noopBranchObjectVTable.base
};

static void
branchGetCContext (CompObject *object,
		   CContext   *ctx)
{
    BRANCH (object);

    ctx->interface  = branchInterface;
    ctx->nInterface = N_ELEMENTS (branchInterface);
    ctx->type	    = &branchObjectType;
    ctx->data	    = (char *) b;
    ctx->svOffset   = 0;
    ctx->vtStore    = &b->object;
    ctx->version    = COMPIZ_BRANCH_VERSION;
}

CompObjectType *
getBranchObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&branchObjectVTable.base, branchGetCContext);
	init = TRUE;
    }

    return &branchObjectType;
}

#define FOR_BASE(object, ...)						\
    do {								\
	CompObjectVTable *__saveVTable = (object)->vTable;		\
	CompObjectVTable **__vTable = (CompObjectVTable **)		\
	    (*(object)->vTable->getAddress) (object,			\
					     COMP_ADDRESS_BASE_VTABLE); \
									\
	(object)->vTable = *__vTable;					\
	__VA_ARGS__;							\
	(object)->vTable = __saveVTable;				\
    } while (0)
