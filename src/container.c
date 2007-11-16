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

#define INTERFACE_VERSION_containerType COMPIZ_CONTAINER_VERSION

static const CommonInterface containerInterface[] = {
    C_INTERFACE (container, Type, CompObjectVTable, _, _, _, _, _, _, _, _, _)
};

static CompBool
containerForBaseObject (CompObject	       *object,
			BaseObjectCallBackProc proc,
			void		       *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    CONTAINER (object);

    UNWRAP (&c->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&c->object, object, vTable, v.vTable);

    return status;
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

    return handleForEachChildObject (object,
				     containerInterface,
				     N_ELEMENTS (containerInterface),
				     proc,
				     closure);
}

static CompBool
containerForEachInterface (CompObject		 *object,
			   InterfaceCallBackProc proc,
			   void			 *closure)
{
    return handleForEachInterface (object,
				   containerInterface,
				   N_ELEMENTS (containerInterface),
				   getContainerObjectType (),
				   proc, closure);
}

static CompBool
containerForEachType (CompObject       *object,
		      TypeCallBackProc proc,
		      void	       *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    CONTAINER (object);

    if (!(*proc) (object, getContainerObjectType (), closure))
	return FALSE;

    UNWRAP (&c->object, object, vTable);
    status = (*object->vTable->forEachType) (object, proc, closure);
    WRAP (&c->object, object, vTable, v.vTable);

    return status;
}

static CompObjectVTable containerObjectVTable = {
    .forBaseObject	= containerForBaseObject,
    .forEachType	= containerForEachType,
    .forEachChildObject = containerForEachChildObject,
    .forEachInterface   = containerForEachInterface,
    .version.get        = commonGetVersion
};

static CompBool
containerInitObject (CompObject *object)
{
    CONTAINER (object);

    if (!compObjectInit (&c->base, getObjectType ()))
	return FALSE;

    WRAP (&c->object, &c->base, vTable, &containerObjectVTable);

    c->forEachChildObject = NULL;

    return TRUE;
}

static void
containerFiniObject (CompObject *object)
{
    CONTAINER (object);

    UNWRAP (&c->object, &c->base, vTable);

    compObjectFini (&c->base, getObjectType ());
}

static void
containerInitVTable (void *vTable)
{
    (*getObjectType ()->initVTable) (vTable);
}

static CompObjectType containerObjectType = {
    CONTAINER_TYPE_NAME,
    {
	containerInitObject,
	containerFiniObject
    },
    NULL,
    containerInitVTable
};

CompObjectType *
getContainerObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	containerInitVTable (&containerObjectVTable);
	init = TRUE;
    }

    return &containerObjectType;
}
