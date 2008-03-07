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

#include <string.h>

#include <compiz/delegate.h>
#include <compiz/signal-match.h>
#include <compiz/c-object.h>

static CompBool
delegateInit (CompObject *object)
{
    DELEGATE (object);

    d->pending = 0;

    return TRUE;
}

static void
delegateInsert (CompObject *object,
		CompObject *parent)
{
    if (!compConnect (parent,
		      getObjectType (),
		      offsetof (CompObjectVTable, signal),
		      object,
		      getDelegateObjectType (),
		      offsetof (CompDelegateVTable, processSignal),
		      NULL))
    {
	compLog (object,
		 getDelegateObjectType (),
		 offsetof (CompObjectVTable, insertObject),
		 "Failed to connect '%s' delegate to parent",
		 object->name);
    }
}

static void
delegateGetProp (CompObject   *object,
		 unsigned int what,
		 void	      *value)
{
    cGetObjectProp (&GET_DELEGATE (object)->data.base,
		    getDelegateObjectType (),
		    what, value);
}

static void
delegateEmit (CompObject *object,
	      const char *interface,
	      const char *name,
	      const char *signature,
	      va_list	 args)
{
    DELEGATE (object);

    d->pending++;

    FOR_BASE (object, (*object->vTable->emit) (object,
					       interface,
					       name,
					       signature,
					       args));
}

static void
processSignal (CompDelegate *d,
	       const char   *path,
	       const char   *interface,
	       const char   *name,
	       const char   *signature,
	       CompAnyValue *value,
	       int	    nValue)
{
    if (strcmp (path, d->u.base.name) == 0)
	if (strcmp (interface, getObjectType ()->name))
	    d->pending--;
}

static void
noopProcessSignal (CompDelegate *d,
		   const char   *path,
		   const char   *interface,
		   const char   *name,
		   const char   *signature,
		   CompAnyValue *value,
		   int	        nValue)
{
    FOR_BASE (&d->u.base, (*d->u.vTable->processSignal) (d,
							 path,
							 interface,
							 name,
							 signature,
							 value,
							 nValue));
}

static const CompDelegateVTable delegateObjectVTable = {
    .base.getProp = delegateGetProp,
    .base.emit    = delegateEmit,

    .processSignal = processSignal
};

static const CompDelegateVTable noopDelegateObjectVTable = {
    .processSignal = noopProcessSignal
};

static const CBoolProp delegateTypeBoolProp[] = {
    C_PROP (compress, CompDelegateData)
};

static const CChildObject delegateTypeChildObject[] = {
    C_CHILD (matches, CompDelegateData, COMPIZ_OBJECT_TYPE_NAME)
};

const CompObjectType *
getDelegateObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_DELEGATE_TYPE_NAME,
	    .i.version	     = COMPIZ_DELEGATE_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &delegateObjectVTable.base,
	    .i.vTable.noop   = &noopDelegateObjectVTable.base,
	    .i.vTable.size   = sizeof (delegateObjectVTable),
	    .i.instance.size = sizeof (CompDelegate),

	    .boolProp  = delegateTypeBoolProp,
	    .nBoolProp = N_ELEMENTS (delegateTypeBoolProp),

	    .child  = delegateTypeChildObject,
	    .nChild = N_ELEMENTS (delegateTypeChildObject),

	    .init   = delegateInit,
	    .insert = delegateInsert
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

static CompObjectType *
delegateObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface delegateTemplate = *template;

    if (!delegateTemplate.i.base.name)
    {
	delegateTemplate.i.base.name    = COMPIZ_DELEGATE_TYPE_NAME;
	delegateTemplate.i.base.version = COMPIZ_DELEGATE_VERSION;
    }

    if (!delegateTemplate.i.version)
	delegateTemplate.i.version = COMPIZ_DELEGATE_VERSION;

    return cObjectTypeFromTemplate (&delegateTemplate);
}


static void
voidDelegateGetProp (CompObject   *object,
		     unsigned int what,
		     void	  *value)
{
    cGetObjectProp (&GET_VOID_DELEGATE (object)->data,
		    getVoidDelegateObjectType (),
		    what, value);
}

static void
voidDelegateProcessSignal (CompDelegate *d,
			   const char   *path,
			   const char   *interface,
			   const char   *name,
			   const char   *signature,
			   CompAnyValue *value,
			   int		nValue)
{
    int i;

    VOID_DELEGATE (d);

    for (i = 0; i < d->data.matches.nChild; i++)
    {
	CompSignalMatch *sm;

	if (d->data.compress && d->pending)
	    break;

	sm = COMP_TYPE_CAST (d->data.matches.child[i].ref,
			     getSignalMatchObjectType (),
			     CompSignalMatch);
	if (sm)
	{
	    if ((*sm->u.vTable->match) (sm,
					path,
					interface,
					name,
					signature,
					value,
					nValue,
					"",
					NULL))
		(vd->u.vTable->notify) (vd);
	}
    }

    FOR_BASE (&d->u.base, (*d->u.vTable->processSignal) (d,
							 path,
							 interface,
							 name,
							 signature,
							 value,
							 nValue));
}

static void
notify (CompVoidDelegate *vd)
{
    C_EMIT_SIGNAL (&vd->u.base.u.base, VoidNotifyProc,
		   offsetof (CompVoidDelegateVTable, notify));
}

static void
noopNotify (CompVoidDelegate *vd)
{
    FOR_BASE (&vd->u.base.u.base, (*vd->u.vTable->notify) (vd));
}

static const CompVoidDelegateVTable voidDelegateObjectVTable = {
    .base.base.getProp  = voidDelegateGetProp,
    .base.processSignal = voidDelegateProcessSignal,

    .notify = notify
};

static const CompVoidDelegateVTable noopVoidDelegateObjectVTable = {
    .notify = noopNotify
};

static const CSignal voidDelegateTypeSignal[] = {
    C_SIGNAL (notify, "", CompVoidDelegateVTable)
};

const CompObjectType *
getVoidDelegateObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_VOID_DELEGATE_TYPE_NAME,
	    .i.vTable.impl   = &voidDelegateObjectVTable.base.base,
	    .i.vTable.noop   = &noopVoidDelegateObjectVTable.base.base,
	    .i.vTable.size   = sizeof (voidDelegateObjectVTable),
	    .i.instance.size = sizeof (CompVoidDelegate),

	    .signal  = voidDelegateTypeSignal,
	    .nSignal = N_ELEMENTS (voidDelegateTypeSignal)
	};

	type = delegateObjectTypeFromTemplate (&template);
    }

    return type;
}
