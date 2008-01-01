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

#include <compiz/container.h>
#include <compiz/c-object.h>

static CInterface containerInterface[] = {
    C_INTERFACE (container, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompBool
containerInitObject (CompObject	*object)
{
    CONTAINER (object);

    c->child  = NULL;
    c->nChild = 0;

    return TRUE;
}

static void
containerGetProp (CompObject   *object,
		  unsigned int what,
		  void	       *value)
{
    static const CMetadata template = {
	.interface  = containerInterface,
	.nInterface = N_ELEMENTS (containerInterface),
	.init       = containerInitObject,
	.version    = COMPIZ_CONTAINER_VERSION
    };

    cGetObjectProp (&GET_CONTAINER (object)->data, &template, what, value);
}

static CompBool
containerAddChild (CompObject *object,
		   CompObject *child)
{
    CompObject **children;

    CONTAINER (object);

    children = realloc (c->child, sizeof (CompObject *) * (c->nChild + 1));
    if (!children)
	return FALSE;

    c->child = children;
    c->child[c->nChild++] = child;

    return TRUE;
}

static void
containerRemoveChild (CompObject *object,
		      CompObject *child)
{
    CompObject **children;
    int	       i;

    CONTAINER (object);

    for (i = 0; i < c->nChild; i++)
	if (c->child[i] == child)
	    break;

    c->nChild--;

    for (; i < c->nChild; i++)
	c->child[i] = c->child[i + 1];

    children = realloc (c->child, sizeof (CompObject *) * c->nChild);
    if (children || !c->nChild)
	c->child = children;
}


static CompBool
containerForEachChildObject (CompObject		     *object,
			     ChildObjectCallBackProc proc,
			     void		     *closure)
{
    CompBool status;
    int	     i;

    CONTAINER (object);

    for (i = 0; i < c->nChild; i++)
	if (!(*proc) (c->child[i], closure))
	    return FALSE;

    FOR_BASE (object,
	      status = (*object->vTable->forEachChildObject) (object,
							      proc,
							      closure));

    return status;
}

static const CompObjectVTable containerObjectVTable = {
    .getProp	        = containerGetProp,
    .addChild		= containerAddChild,
    .removeChild	= containerRemoveChild,
    .forEachChildObject = containerForEachChildObject
};

const CompObjectType *
getContainerObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CompObjectType template = {
	    .name.name   = CONTAINER_TYPE_NAME,
	    .vTable.impl = &containerObjectVTable
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
