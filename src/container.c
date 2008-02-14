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
#include <assert.h>

#include <compiz/container.h>
#include <compiz/c-object.h>

static CInterface containerInterface[] = {
    C_INTERFACE (container, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static CompBool
containerInitObject (CompObject	*object)
{
    CONTAINER (object);

    c->item  = NULL;
    c->nItem = 0;

    return TRUE;
}

static void
containerFiniObject (CompObject	*object)
{
    CONTAINER (object);

    if (c->item)
    {
	int i;

	for (i = 0; i < c->nItem; i++)
	{
	    (*c->item[i].object->vTable->finalize) (c->item[i].object);

	    free (c->item[i].object);
	    free (c->item[i].name);
	}

	free (c->item);
    }
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
	.fini	    = containerFiniObject,
	.version    = COMPIZ_CONTAINER_VERSION
    };

    cGetObjectProp (&GET_CONTAINER (object)->data, &template, what, value);
}

static void
containerInsertObject (CompObject *object,
		       CompObject *parent,
		       const char *name)
{
    int i;

    CONTAINER (object);

    FOR_BASE (object, (*object->vTable->insertObject) (object, parent, name));

    for (i = 0; i < c->nItem; i++)
	(*c->item[i].object->vTable->insertObject) (c->item[i].object, object,
						    c->item[i].name);
}

static void
containerRemoveObject (CompObject *object)
{
    int i;

    CONTAINER (object);

    i = c->nItem;
    while (i--)
	(*c->item[i].object->vTable->removeObject) (c->item[i].object);

    FOR_BASE (object, (*object->vTable->removeObject) (object));
}

static CompBool
containerAddChild (CompObject *object,
		   CompObject *child,
		   const char *name)
{
    CompContainerItem *item;
    char	      *itemName;

    CONTAINER (object);

    itemName = strdup (name);
    if (!itemName)
	return FALSE;

    item = realloc (c->item, sizeof (CompContainerItem) * (c->nItem + 1));
    if (!item)
    {
	free (itemName);
	return FALSE;
    }

    item[c->nItem].object = child;
    item[c->nItem].name   = itemName;

    c->item = item;
    c->nItem++;

    if (object->parent)
	(*child->vTable->insertObject) (child, object, itemName);

    return TRUE;
}

static void
containerRemoveChild (CompObject *object,
		      CompObject *child)
{
    CompContainerItem *item;
    int		      i;

    CONTAINER (object);

    for (i = 0; i < c->nItem; i++)
	if (c->item[i].object == child)
	    break;

    assert (i < c->nItem);

    if (object->parent)
	(*child->vTable->removeObject) (child);

    free (c->item[i].name);

    c->nItem--;

    for (; i < c->nItem; i++)
	c->item[i] = c->item[i + 1];

    item = realloc (c->item, sizeof (CompContainerItem) * c->nItem);
    if (item || !c->nItem)
	c->item = item;
}

static CompBool
containerForEachChildObject (CompObject		     *object,
			     ChildObjectCallBackProc proc,
			     void		     *closure)
{
    CompBool status;
    int	     i;

    CONTAINER (object);

    for (i = 0; i < c->nItem; i++)
	if (!(*proc) (c->item[i].object, closure))
	    return FALSE;

    FOR_BASE (object,
	      status = (*object->vTable->forEachChildObject) (object,
							      proc,
							      closure));

    return status;
}

static const CompObjectVTable containerObjectVTable = {
    .getProp	        = containerGetProp,
    .insertObject	= containerInsertObject,
    .removeObject	= containerRemoveObject,
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
	    .name.name     = CONTAINER_TYPE_NAME,
	    .vTable.impl   = &containerObjectVTable,
	    .instance.size = sizeof (CompContainer)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
