/*
 * Copyright © 2008 Novell, Inc.
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

#include <compiz/p-object.h>
#include <compiz/error.h>

typedef struct _PObjectInterface {
    CompObjectInterface i;

    PInitObjectPrivateProc init;

    int	*index;
    int	size;
} PObjectInterface;

static CompBool
pInstallObjectInterface (const CompObjectInterface *interface,
			 CompFactory		   *factory,
			 char			   **error)
{
    const PObjectInterface *pInterface = (const PObjectInterface *) interface;
    int			   index;

    index = (*factory->allocatePrivateIndex) (factory,
					      pInterface->i.base.name,
					      pInterface->size);
    if (index < 0)
    {
	esprintf (error, "Failed to allocated private index "
		  "and data block of size '%d' in base object type '%s'",
		  pInterface->size, pInterface->i.base.name);
	return FALSE;
    }

    *(pInterface->index) = index;

    return TRUE;
}

static void
pUninstallObjectInterface (const CompObjectInterface *interface,
			   CompFactory	             *factory)
{
    const PObjectInterface *pInterface = (const PObjectInterface *) interface;
    int			   index = *(pInterface->index);

    (*factory->freePrivateIndex) (factory,
				  pInterface->i.base.name,
				  index);
}

static CompBool
pObjectInit (const CompObjectInstantiator *instantiator,
	     CompObject			  *object,
	     const CompObjectFactory      *factory)
{
    const PObjectInterface	  *pInterface =
	(const PObjectInterface *) instantiator->interface;
    const CompObjectInstantiator *base = instantiator->base;

    if (!(*base->init) (base, object, factory))
	return FALSE;

    if (pInterface->init)
	(*pInterface->init) (object);

    return TRUE;
}

static CompBool
pInitInterface (const CompObjectInstantiator *instantiator,
		CompObject		     *object,
		const CompObjectFactory      *factory)
{
    const PObjectInterface *pInterface =
	(const PObjectInterface *) instantiator->interface;

    if (pInterface->init)
	(*pInterface->init) (object);

    return TRUE;
}

CompObjectInterface *
pObjectInterfaceFromTemplate (const CompObjectInterface *template,
			      PInitObjectPrivateProc    init,
			      int		        *index,
			      int		        size)
{
    PObjectInterface *interface;

    interface = calloc (1, sizeof (PObjectInterface));
    if (!interface)
	return NULL;

    interface->i.name	      = template->name;
    interface->i.version      = template->version;
    interface->i.base.name    = template->base.name;
    interface->i.base.version = template->base.version;

    interface->index = index;
    interface->size  = size;
    interface->init  = init;

    interface->i.instance.init = pObjectInit,

    interface->i.factory.install   = pInstallObjectInterface;
    interface->i.factory.uninstall = pUninstallObjectInterface;

    interface->i.interface.init = pInitInterface;

    return &interface->i;
}
