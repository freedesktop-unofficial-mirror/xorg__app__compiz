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
#include <string.h>

#include <compiz/signal-match.h>
#include <compiz/signal-arg-map.h>
#include <compiz/widget.h>
#include <compiz/keyboard.h>
#include <compiz/pointer.h>
#include <compiz/c-object.h>
#include <compiz/marshal.h>

static void
signalMatchGetProp (CompObject   *object,
		    unsigned int what,
		    void	 *value)
{
    cGetObjectProp (&GET_SIGNAL_MATCH (object)->data.base,
		    getSignalMatchObjectType (),
		    what, value);
}

static CompBool
match (CompSignalMatch *sm,
       const char      *path,
       const char      *interface,
       const char      *name,
       const char      *signature,
       CompAnyValue    *value,
       int	       nValue,
       const char      *args,
       CompAnyValue    *argValue)
{
    int i;

    for (i = 0; args[i] != COMP_TYPE_INVALID; i++)
    {
	if (i < sm->data.args.nChild)
	{
	    CompSignalArgMap *sa = NULL;

	    switch (args[i]) {
	    case COMP_TYPE_BOOLEAN:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getBoolSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    case COMP_TYPE_INT32:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getIntSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    case COMP_TYPE_DOUBLE:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getDoubleSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    case COMP_TYPE_STRING:
	    case COMP_TYPE_OBJECT:
		sa = COMP_TYPE_CAST (sm->data.args.child[i].ref,
				     getStringSignalArgMapObjectType (),
				     CompSignalArgMap);
		break;
	    }

	    if (sa)
	    {
		if (!(*sa->u.vTable->map) (sa,
					   path,
					   interface,
					   name,
					   signature,
					   value,
					   nValue,
					   &argValue[i]))
		    memset (&argValue[i], 0, sizeof (CompAnyValue));
	    }
	    else
	    {
		compLog (&sm->u.base,
			 getSignalMatchObjectType (),
			 offsetof (CompSignalMatchVTable, match),
			 "Signal argument mapping object for '%c' argument "
			 "has bad type", args[i]);
		memset (&argValue[i], 0, sizeof (CompAnyValue));
	    }
	}
	else
	{
	    memset (&argValue[i], 0, sizeof (CompAnyValue));
	}
    }

    return TRUE;
}

static CompBool
noopMatch (CompSignalMatch *sm,
	   const char      *path,
	   const char      *interface,
	   const char      *name,
	   const char      *signature,
	   CompAnyValue    *value,
	   int	           nValue,
	   const char      *args,
	   CompAnyValue    *argValue)
{
    CompBool status;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable signalMatchObjectVTable = {
    .base.getProp = signalMatchGetProp,

    .match = match
};

static const CompSignalMatchVTable noopSignalMatchObjectVTable = {
    .match = noopMatch
};

static const CChildObject signalMatchTypeChildObject[] = {
    C_CHILD (args, CompSignalMatchData, COMPIZ_OBJECT_TYPE_NAME)
};

const CompObjectType *
getSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_SIGNAL_MATCH_TYPE_NAME,
	    .i.version	     = COMPIZ_SIGNAL_MATCH_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &signalMatchObjectVTable.base,
	    .i.vTable.noop   = &noopSignalMatchObjectVTable.base,
	    .i.vTable.size   = sizeof (signalMatchObjectVTable),
	    .i.instance.size = sizeof (CompSignalMatch),

	    .child  = signalMatchTypeChildObject,
	    .nChild = N_ELEMENTS (signalMatchTypeChildObject)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}

static CompObjectType *
signalMatchObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface signalMatchTemplate = *template;

    if (!signalMatchTemplate.i.base.name)
    {
	signalMatchTemplate.i.base.name    = COMPIZ_SIGNAL_MATCH_TYPE_NAME;
	signalMatchTemplate.i.base.version = COMPIZ_SIGNAL_MATCH_VERSION;
    }

    if (!signalMatchTemplate.i.vTable.size)
	signalMatchTemplate.i.vTable.size = sizeof (signalMatchObjectVTable);

    if (!signalMatchTemplate.i.version)
	signalMatchTemplate.i.version = COMPIZ_SIGNAL_MATCH_VERSION;

    return cObjectTypeFromTemplate (&signalMatchTemplate);
}

static void
simpleSignalMatchGetProp (CompObject   *object,
			  unsigned int what,
			  void	       *value)
{
    cGetObjectProp (&GET_SIMPLE_SIGNAL_MATCH (object)->data.base,
		    getSimpleSignalMatchObjectType (),
		    what, value);
}

static CompBool
simpleMatch (CompSignalMatch *sm,
	     const char      *path,
	     const char      *interface,
	     const char      *name,
	     const char      *signature,
	     CompAnyValue    *value,
	     int	     nValue,
	     const char      *args,
	     CompAnyValue    *argValue)
{
    CompBool status;

    SIMPLE_SIGNAL_MATCH (sm);

    if (strcmp (ssm->data.path, "//*") != 0)
    {
	int i;

	for (i = 0; path[i]; i++)
	    if (ssm->data.path[i] != path[i])
		break;

	if (path[i])
	{
	    if (ssm->data.path[i] != '*')
		return FALSE;
	}
	else if (ssm->data.path[i] != '\0')
	{
	    return FALSE;
	}
    }

    if (strcmp (interface, ssm->data.interface) ||
	strcmp (name,	   ssm->data.name)      ||
	strcmp (signature, ssm->data.signature))
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable simpleSignalMatchObjectVTable = {
    .base.getProp = simpleSignalMatchGetProp,

    .match = simpleMatch
};

static const CStringProp simpleSignalMatchTypeStringProp[] = {
    C_PROP (path,      CompSimpleSignalMatchData),
    C_PROP (interface, CompSimpleSignalMatchData),
    C_PROP (name,      CompSimpleSignalMatchData),
    C_PROP (signature, CompSimpleSignalMatchData)
};

const CompObjectType *
getSimpleSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_SIMPLE_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &simpleSignalMatchObjectVTable.base,
	    .i.instance.size = sizeof (CompSimpleSignalMatch),

	    .stringProp  = simpleSignalMatchTypeStringProp,
	    .nStringProp = N_ELEMENTS (simpleSignalMatchTypeStringProp)
	};

	type = signalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static void
snSignalMatchGetProp (CompObject   *object,
		      unsigned int what,
		      void	   *value)
{
    cGetObjectProp (&GET_STRUCTURE_NOTIFY_SIGNAL_MATCH (object)->data.base,
		    getStructureNotifySignalMatchObjectType (),
		    what, value);
}

static CompBool
snMatch (CompSignalMatch *sm,
	 const char      *path,
	 const char      *interface,
	 const char      *name,
	 const char      *signature,
	 CompAnyValue    *value,
	 int		 nValue,
	 const char      *args,
	 CompAnyValue    *argValue)
{
    CompBool status;
    int      i;

    STRUCTURE_NOTIFY_SIGNAL_MATCH (sm);

    for (i = 0; snsm->data.object[i]; i++)
	if (snsm->data.object[i] != path[i])
	    return FALSE;

    if (strcmp (interface, getObjectType ()->name))
	return FALSE;

    if (strcmp (name, "interfaceAdded")   &&
	strcmp (name, "interfaceRemoved") &&
	strcmp (name, "boolChanged")      &&
	strcmp (name, "intChanged")       &&
	strcmp (name, "doubleChanged")    &&
	strcmp (name, "stringChanged"))
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable snSignalMatchObjectVTable = {
    .base.getProp = snSignalMatchGetProp,

    .match = snMatch
};

static const CStringProp snSignalMatchTypeStringProp[] = {
    C_PROP (object, CompStructureNotifySignalMatchData)
};

const CompObjectType *
getStructureNotifySignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_STRUCTURE_NOTIFY_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &snSignalMatchObjectVTable.base,
	    .i.instance.size = sizeof (CompStructureNotifySignalMatch),

	    .stringProp  = snSignalMatchTypeStringProp,
	    .nStringProp = N_ELEMENTS (snSignalMatchTypeStringProp)
	};

	type = signalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static CompBool
keyEventSignalMatchInit (CompObject *object)
{
    KEY_EVENT_SIGNAL_MATCH (object);

    kesm->oldKey       = NULL;
    kesm->oldModifiers = 0;

    return TRUE;
}

static void
keyEventSignalMatchFini (CompObject *object)
{
    KEY_EVENT_SIGNAL_MATCH (object);

    if (kesm->oldKey)
	free (kesm->oldKey);
}

static void
keyEventSignalMatchInsert (CompObject *object,
			   CompObject *parent)
{
    KEY_EVENT_SIGNAL_MATCH (object);

    if (kesm->oldKey)
	(*kesm->u.vTable->newState) (kesm,
				     kesm->oldKey,
				     kesm->oldModifiers);
}

static void
keyEventSignalMatchRemove (CompObject *object)
{
    KEY_EVENT_SIGNAL_MATCH (object);

    if (kesm->oldKey)
	(*kesm->u.vTable->oldState) (kesm,
				     kesm->oldKey,
				     kesm->oldModifiers);
}

static void
keyEventSignalMatchGetProp (CompObject   *object,
			    unsigned int what,
			    void	 *value)
{
    cGetObjectProp (&GET_KEY_EVENT_SIGNAL_MATCH (object)->data.base,
		    getKeyEventSignalMatchObjectType (),
		    what, value);
}

static CompBool
keyEventMatch (CompSignalMatch *sm,
	       const char      *path,
	       const char      *interface,
	       const char      *name,
	       const char      *signature,
	       CompAnyValue    *value,
	       int	       nValue,
	       const char      *args,
	       CompAnyValue    *argValue)
{
    CompBool status;

    KEY_EVENT_SIGNAL_MATCH (sm);

    if (strcmp (interface, getWidgetObjectType ()->name))
	return FALSE;

    if (strcmp (signature, "si"))
	return FALSE;

    if (strcmp (value[0].s, kesm->data.key))
	return FALSE;

    if ((value[1].i & kesm->data.modifiers) != kesm->data.modifiers)
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static void
updateState (CompKeyEventSignalMatch *kesm)
{
    if (kesm->data.key)
    {
	if (kesm->oldKey)
	{
	    if (strcmp (kesm->data.key, kesm->oldKey) ||
		kesm->data.modifiers != kesm->oldModifiers)
	    {
		(*kesm->u.vTable->newState) (kesm,
					     kesm->data.key,
					     kesm->data.modifiers);
		(*kesm->u.vTable->oldState) (kesm,
					     kesm->oldKey,
					     kesm->oldModifiers);
	    }

	    free (kesm->oldKey);
	}
	else
	{
	    (*kesm->u.vTable->newState) (kesm,
					 kesm->data.key,
					 kesm->data.modifiers);
	}

	kesm->oldKey = strdup (kesm->data.key);
    }
    else
    {
	if (kesm->oldKey)
	{
	    (*kesm->u.vTable->oldState) (kesm,
					 kesm->oldKey,
					 kesm->oldModifiers);
	    free (kesm->oldKey);
	    kesm->oldKey = NULL;
	}
    }

    kesm->oldModifiers = kesm->data.modifiers;
}

static void
noopUpdateState (CompKeyEventSignalMatch *kesm)
{
    FOR_BASE (&kesm->u.base.u.base, (*kesm->u.vTable->updateState) (kesm));
}

static void
newState (CompKeyEventSignalMatch *kesm,
	  const char		  *key,
	  int32_t		  modifiers)
{
    C_EMIT_SIGNAL (&kesm->u.base.u.base, KeyEventStateChangeProc,
		   offsetof (CompKeyEventSignalMatchVTable, newState),
		   key, modifiers);
}

static void
noopNewState (CompKeyEventSignalMatch *kesm,
	      const char	      *key,
	      int32_t		      modifiers)
{
    FOR_BASE (&kesm->u.base.u.base, (*kesm->u.vTable->newState) (kesm,
								 key,
								 modifiers));
}

static void
oldState (CompKeyEventSignalMatch *kesm,
	  const char		  *key,
	  int32_t		  modifiers)
{
    C_EMIT_SIGNAL (&kesm->u.base.u.base, KeyEventStateChangeProc,
		   offsetof (CompKeyEventSignalMatchVTable, oldState),
		   key, modifiers);
}

static void
noopOldState (CompKeyEventSignalMatch *kesm,
	      const char	      *key,
	      int32_t		      modifiers)
{
    FOR_BASE (&kesm->u.base.u.base, (*kesm->u.vTable->oldState) (kesm,
								 key,
								 modifiers));
}

static const CompKeyEventSignalMatchVTable keSignalMatchObjectVTable = {
    .base.base.getProp = keyEventSignalMatchGetProp,
    .base.match        = keyEventMatch,

    .updateState = updateState,
    .newState    = newState,
    .oldState    = oldState
};

static const CompKeyEventSignalMatchVTable noopKeSignalMatchObjectVTable = {
    .updateState = noopUpdateState,
    .newState    = noopNewState,
    .oldState    = noopOldState
};

static const CMethod keyEventSignalMatchTypeMethod[] = {
    C_METHOD (updateState, "", "", CompKeyEventSignalMatchVTable, marshal____)
};

static const CSignal keyEventSignalMatchTypeSignal[] = {
    C_SIGNAL (newState, "si", CompKeyEventSignalMatchVTable),
    C_SIGNAL (oldState, "si", CompKeyEventSignalMatchVTable),
};

static const CIntProp keyEventSignalMatchTypeIntProp[] = {
    C_INT_PROP (modifiers, CompKeyEventSignalMatchData,
		0, (1 << KEYBOARD_MODIFIER_NUM) - 1)
};

static const CStringProp keyEventSignalMatchTypeStringProp[] = {
    C_PROP (key, CompKeyEventSignalMatchData)
};

const CompObjectType *
getKeyEventSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_KEY_EVENT_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &keSignalMatchObjectVTable.base.base,
	    .i.vTable.noop   = &noopKeSignalMatchObjectVTable.base.base,
	    .i.vTable.size   = sizeof (keSignalMatchObjectVTable),
	    .i.instance.size = sizeof (CompKeyEventSignalMatch),

	    .method  = keyEventSignalMatchTypeMethod,
	    .nMethod = N_ELEMENTS (keyEventSignalMatchTypeMethod),

	    .signal  = keyEventSignalMatchTypeSignal,
	    .nSignal = N_ELEMENTS (keyEventSignalMatchTypeSignal),

	    .intProp  = keyEventSignalMatchTypeIntProp,
	    .nIntProp = N_ELEMENTS (keyEventSignalMatchTypeIntProp),

	    .stringProp  = keyEventSignalMatchTypeStringProp,
	    .nStringProp = N_ELEMENTS (keyEventSignalMatchTypeStringProp),

	    .init = keyEventSignalMatchInit,
	    .fini = keyEventSignalMatchFini,

	    .insert = keyEventSignalMatchInsert,
	    .remove = keyEventSignalMatchRemove
	};

	type = signalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static CompObjectType *
keyEventSignalMatchObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface keyEventSignalMatchTemplate = *template;

    if (!keyEventSignalMatchTemplate.i.base.name)
	keyEventSignalMatchTemplate.i.base.name =
	    COMPIZ_KEY_EVENT_SIGNAL_MATCH_TYPE_NAME;

    if (!keyEventSignalMatchTemplate.i.vTable.size)
	keyEventSignalMatchTemplate.i.vTable.size =
	    sizeof (keSignalMatchObjectVTable);

    return signalMatchObjectTypeFromTemplate (&keyEventSignalMatchTemplate);
}

static void
keyPressSignalMatchGetProp (CompObject   *object,
			    unsigned int what,
			    void	 *value)
{
    cGetObjectProp (&GET_KEY_PRESS_SIGNAL_MATCH (object)->data,
		    getKeyPressSignalMatchObjectType (),
		    what, value);
}

static CompBool
keyPressMatch (CompSignalMatch *sm,
	       const char      *path,
	       const char      *interface,
	       const char      *name,
	       const char      *signature,
	       CompAnyValue    *value,
	       int	       nValue,
	       const char      *args,
	       CompAnyValue    *argValue)
{
    CompBool status;

    if (strcmp (name, "keyPress"))
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompKeyEventSignalMatchVTable kpSignalMatchObjectVTable = {
    .base.base.getProp = keyPressSignalMatchGetProp,

    .base.match = keyPressMatch
};

const CompObjectType *
getKeyPressSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_KEY_PRESS_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &kpSignalMatchObjectVTable.base.base,
	    .i.instance.size = sizeof (CompKeyPressSignalMatch)
	};

	type = keyEventSignalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static void
keyReleaseSignalMatchGetProp (CompObject   *object,
			      unsigned int what,
			      void	   *value)
{
    cGetObjectProp (&GET_KEY_RELEASE_SIGNAL_MATCH (object)->data,
		    getKeyReleaseSignalMatchObjectType (),
		    what, value);
}

static CompBool
keyReleaseMatch (CompSignalMatch *sm,
		 const char      *path,
		 const char      *interface,
		 const char      *name,
		 const char      *signature,
		 CompAnyValue    *value,
		 int	         nValue,
		 const char      *args,
		 CompAnyValue    *argValue)
{
    CompBool status;

    if (strcmp (name, "keyRelease"))
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompKeyEventSignalMatchVTable krSignalMatchObjectVTable = {
    .base.base.getProp = keyReleaseSignalMatchGetProp,

    .base.match = keyReleaseMatch
};

const CompObjectType *
getKeyReleaseSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_KEY_RELEASE_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &krSignalMatchObjectVTable.base.base,
	    .i.instance.size = sizeof (CompKeyReleaseSignalMatch)
	};

	type = keyEventSignalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static void
buttonEventSignalMatchGetProp (CompObject   *object,
			       unsigned int what,
			       void	 *value)
{
    cGetObjectProp (&GET_BUTTON_EVENT_SIGNAL_MATCH (object)->data.base,
		    getButtonEventSignalMatchObjectType (),
		    what, value);
}

static CompBool
buttonEventMatch (CompSignalMatch *sm,
		  const char      *path,
		  const char      *interface,
		  const char      *name,
		  const char      *signature,
		  CompAnyValue    *value,
		  int	          nValue,
		  const char      *args,
		  CompAnyValue    *argValue)
{
    CompBool status;

    BUTTON_EVENT_SIGNAL_MATCH (sm);

    if (strcmp (interface, getWidgetObjectType ()->name))
	return FALSE;

    if (strcmp (signature, "iiii"))
	return FALSE;

    if (value[0].i != besm->data.button)
	return FALSE;

    if ((value[1].i & besm->data.modifiers) != besm->data.modifiers)
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable buttonEventSignalMatchObjectVTable = {
    .base.getProp = buttonEventSignalMatchGetProp,

    .match = buttonEventMatch
};

static const CIntProp buttonEventSignalMatchTypeIntProp[] = {
    C_INT_PROP (button,    CompButtonEventSignalMatchData, 0,
		POINTER_BUTTON_MAX),
    C_INT_PROP (modifiers, CompButtonEventSignalMatchData,
		0, (1 << KEYBOARD_MODIFIER_NUM) - 1)
};

const CompObjectType *
getButtonEventSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_BUTTON_EVENT_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &buttonEventSignalMatchObjectVTable.base,
	    .i.instance.size = sizeof (CompButtonEventSignalMatch),

	    .intProp  = buttonEventSignalMatchTypeIntProp,
	    .nIntProp = N_ELEMENTS (buttonEventSignalMatchTypeIntProp)
	};

	type = signalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static CompObjectType *
buttonEventSignalMatchObjectTypeFromTemplate (const CObjectInterface *template)
{
    CObjectInterface buttonEventSignalMatchTemplate = *template;

    if (!buttonEventSignalMatchTemplate.i.base.name)
	buttonEventSignalMatchTemplate.i.base.name =
	    COMPIZ_BUTTON_EVENT_SIGNAL_MATCH_TYPE_NAME;

    return signalMatchObjectTypeFromTemplate (&buttonEventSignalMatchTemplate);
}

static void
buttonPressSignalMatchGetProp (CompObject   *object,
			       unsigned int what,
			       void	    *value)
{
    cGetObjectProp (&GET_BUTTON_PRESS_SIGNAL_MATCH (object)->data,
		    getButtonPressSignalMatchObjectType (),
		    what, value);
}

static CompBool
buttonPressMatch (CompSignalMatch *sm,
		  const char      *path,
		  const char      *interface,
		  const char      *name,
		  const char      *signature,
		  CompAnyValue    *value,
		  int	          nValue,
		  const char      *args,
		  CompAnyValue    *argValue)
{
    CompBool status;

    if (strcmp (name, "buttonPress"))
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable buttonPressSignalMatchObjectVTable = {
    .base.getProp = buttonPressSignalMatchGetProp,

    .match = buttonPressMatch
};

const CompObjectType *
getButtonPressSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_BUTTON_PRESS_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &buttonPressSignalMatchObjectVTable.base,
	    .i.instance.size = sizeof (CompButtonPressSignalMatch)
	};

	type = buttonEventSignalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}

static void
buttonReleaseSignalMatchGetProp (CompObject   *object,
				 unsigned int what,
				 void	      *value)
{
    cGetObjectProp (&GET_BUTTON_RELEASE_SIGNAL_MATCH (object)->data,
		    getButtonReleaseSignalMatchObjectType (),
		    what, value);
}

static CompBool
buttonReleaseMatch (CompSignalMatch *sm,
		    const char      *path,
		    const char      *interface,
		    const char      *name,
		    const char      *signature,
		    CompAnyValue    *value,
		    int	            nValue,
		    const char      *args,
		    CompAnyValue    *argValue)
{
    CompBool status;

    if (strcmp (name, "buttonRelease"))
	return FALSE;

    FOR_BASE (&sm->u.base, status = (*sm->u.vTable->match) (sm,
							    path,
							    interface,
							    name,
							    signature,
							    value,
							    nValue,
							    args,
							    argValue));

    return status;
}

static const CompSignalMatchVTable buttonReleaseSignalMatchObjectVTable = {
    .base.getProp = buttonReleaseSignalMatchGetProp,

    .match = buttonReleaseMatch
};

const CompObjectType *
getButtonReleaseSignalMatchObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_BUTTON_RELEASE_SIGNAL_MATCH_TYPE_NAME,
	    .i.vTable.impl   = &buttonReleaseSignalMatchObjectVTable.base,
	    .i.instance.size = sizeof (CompButtonReleaseSignalMatch)
	};

	type = buttonEventSignalMatchObjectTypeFromTemplate (&template);
    }

    return type;
}
