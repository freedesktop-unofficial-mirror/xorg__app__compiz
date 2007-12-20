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

static CInterface containerInterface[] = {
    C_INTERFACE (container, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompBool
containerInitObject (CompObject	*object)
{
    CONTAINER (object);

    c->forEachChildObject = NULL;

    return TRUE;
}

static void
containerGetProp (CompObject   *object,
		  unsigned int what,
		  void	       *value)
{
    cGetObjectProp (&GET_CONTAINER (object)->data,
		    containerInterface, N_ELEMENTS (containerInterface),
		    containerInitObject, NULL, COMPIZ_CONTAINER_VERSION,
		    what, value);
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

static CompObjectType containerObjectType = {
    .name.name   = CONTAINER_TYPE_NAME,
    .name.base   = OBJECT_TYPE_NAME,
    .vTable.impl = &containerObjectVTable,
    .vTable.size = sizeof (containerObjectVTable),
    .funcs.init  = cObjectInit,
    .funcs.fini  = cObjectFini
};

CompObjectType *
getContainerObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&containerObjectVTable);
	cInterfaceInit (containerInterface, N_ELEMENTS (containerInterface),
			&containerObjectType);
	init = TRUE;
    }

    return &containerObjectType;
}
