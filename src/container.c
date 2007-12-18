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

#include <compiz/container.h>
#include <compiz/c-object.h>

static const CInterface containerInterface[] = {
    C_INTERFACE (container, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static void
containerGetProp (CompObject   *object,
		  unsigned int what,
		  void	       *value)
{
    switch (what) {
    case COMP_ADDRESS_BASE_VTABLE_STORE:
	*((CompObjectVTableVec **) value) = &GET_CONTAINER (object)->object;
    }
}

static CompBool
containerForEachChildObject (CompObject		     *object,
			     ChildObjectCallBackProc proc,
			     void		     *closure)
{
    CONTAINER (object);

    if (c->forEachChildObject)
	if (!(*c->forEachChildObject) (object, proc, closure))
	    return FALSE;

    return cForEachChildObject (object, proc, closure);
}

static CompObjectVTable containerObjectVTable = {
    .getProp	        = containerGetProp,
    .forEachChildObject = containerForEachChildObject
};

static CompBool
containerInitObject (const CompObjectInstantiator *instantiator,
		     CompObject			  *object,
		     const CompObjectFactory      *factory)
{
    CONTAINER (object);

    if (!cObjectInit (instantiator, object, factory))
	return FALSE;

    c->forEachChildObject = NULL;

    return TRUE;
}

static void
containerFiniObject (const CompObjectInstantiator *instantiator,
		     CompObject			  *object,
		     const CompObjectFactory      *factory)
{
    cObjectFini (instantiator, object, factory);
}

static CompObjectType containerObjectType = {
    CONTAINER_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	containerInitObject,
	containerFiniObject
    },
    0,
    sizeof (CompObjectVTable),
    &containerObjectVTable,
    NULL
};

static void
containerGetCContext (CompObject *object,
		      CContext   *ctx)
{
    CONTAINER (object);

    ctx->interface  = containerInterface;
    ctx->nInterface = N_ELEMENTS (containerInterface);
    ctx->type	    = &containerObjectType;
    ctx->data	    = (char *) c;
    ctx->svOffset   = 0;
    ctx->vtStore    = &c->object;
    ctx->version    = COMPIZ_CONTAINER_VERSION;
}

CompObjectType *
getContainerObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&containerObjectVTable, containerGetCContext);
	init = TRUE;
    }

    return &containerObjectType;
}
