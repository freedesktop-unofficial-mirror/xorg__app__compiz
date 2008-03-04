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

#include <compiz/delegate.h>
#include <compiz/signal-match.h>
#include <compiz/c-object.h>

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

/* delegate */

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
delegateProcessSignal (CompDelegate *d,
		       const char   *path,
		       const char   *interface,
		       const char   *name,
		       const char   *signature,
		       CompAnyValue *value,
		       int	    nValue)
{
}

static void
noopDelegateProcessSignal (CompDelegate *d,
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

    .processSignal = delegateProcessSignal
};

static const CompDelegateVTable noopDelegateObjectVTable = {
    .processSignal = noopDelegateProcessSignal
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

	    .child  = delegateTypeChildObject,
	    .nChild = N_ELEMENTS (delegateTypeChildObject),

	    .insert = delegateInsert
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

/* void */

static void
delegateVoidGetProp (CompObject   *object,
		     unsigned int what,
		     void	  *value)
{
    cGetObjectProp (&GET_DELEGATE_VOID (object)->data,
		    getDelegateVoidObjectType (),
		    what, value);
}

static void
delegateVoidProcessSignal (CompDelegate *d,
			   const char   *path,
			   const char   *interface,
			   const char   *name,
			   const char   *signature,
			   CompAnyValue *value,
			   int		nValue)
{
    int i;

    DELEGATE_VOID (d);

    for (i = 0; i < d->data.matches.nChild; i++)
    {
	CompSignalMatch *sm;

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
		(dv->u.vTable->signalVoid) (dv);
	}
    }
}

static void
signalVoid (CompDelegateVoid *dv)
{
    C_EMIT_SIGNAL (&dv->u.base.u.base, SignalVoidProc,
		   offsetof (CompDelegateVoidVTable, signalVoid));
}

static void
noopSignalVoid (CompDelegateVoid *dv)
{
    FOR_BASE (&dv->u.base.u.base, (*dv->u.vTable->signalVoid) (dv));
}

static const CompDelegateVoidVTable delegateVoidObjectVTable = {
    .base.base.getProp  = delegateVoidGetProp,
    .base.processSignal = delegateVoidProcessSignal,

    .signalVoid = signalVoid
};

static const CompDelegateVoidVTable noopDelegateVoidObjectVTable = {
    .signalVoid = noopSignalVoid
};

static const CSignal delegateVoidTypeSignal[] = {
    C_SIGNAL (signalVoid, "", CompDelegateVoidVTable)
};

const CompObjectType *
getDelegateVoidObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_DELEGATE_VOID_TYPE_NAME,
	    .i.vTable.impl   = &delegateVoidObjectVTable.base.base,
	    .i.vTable.noop   = &noopDelegateVoidObjectVTable.base.base,
	    .i.vTable.size   = sizeof (delegateVoidObjectVTable),
	    .i.instance.size = sizeof (CompDelegateVoid),

	    .signal  = delegateVoidTypeSignal,
	    .nSignal = N_ELEMENTS (delegateVoidTypeSignal)
	};

	type = delegateObjectTypeFromTemplate (&template);
    }

    return type;
}
