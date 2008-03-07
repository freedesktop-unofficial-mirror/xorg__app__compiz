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

#include <compiz/widget.h>
#include <compiz/c-object.h>

static void
widgetGetProp (CompObject   *object,
	       unsigned int what,
	       void	    *value)
{
    cGetObjectProp (&GET_WIDGET (object)->data,
		    getWidgetObjectType (),
		    what, value);
}

static void
buttonPress (CompWidget *w,
	     int32_t    button,
	     int32_t    modifiers,
	     int32_t    x,
	     int32_t    y)
{
    C_EMIT_SIGNAL (&w->u.base, ButtonEventProc,
		   offsetof (CompWidgetVTable, buttonPress),
		   button, modifiers, x, y);
}

static void
noopButtonPress (CompWidget *w,
		 int32_t    button,
		 int32_t    modifiers,
		 int32_t    x,
		 int32_t    y)
{
    FOR_BASE (&w->u.base, (*w->u.vTable->buttonPress) (w,
						       button,
						       modifiers,
						       x,
						       y));
}

static void
buttonRelease (CompWidget *w,
	       int32_t    button,
	       int32_t    modifiers,
	       int32_t    x,
	       int32_t    y)
{
    C_EMIT_SIGNAL (&w->u.base, ButtonEventProc,
		   offsetof (CompWidgetVTable, buttonRelease),
		   button, modifiers, x, y);
}

static void
noopButtonRelease (CompWidget *w,
		   int32_t    button,
		   int32_t    modifiers,
		   int32_t    x,
		   int32_t    y)
{
    FOR_BASE (&w->u.base, (*w->u.vTable->buttonRelease) (w,
							 button,
							 modifiers,
							 x,
							 y));
}

static void
keyPress (CompWidget *w,
	  const char *key,
	  int32_t    modifiers)
{
    C_EMIT_SIGNAL (&w->u.base, KeyEventProc,
		   offsetof (CompWidgetVTable, keyPress),
		   key, modifiers);
}

static void
noopKeyPress (CompWidget *w,
	      const char *key,
	      int32_t    modifiers)
{
    FOR_BASE (&w->u.base, (*w->u.vTable->keyPress) (w, key, modifiers));
}

static void
keyRelease (CompWidget *w,
	    const char *key,
	    int32_t    modifiers)
{
    C_EMIT_SIGNAL (&w->u.base, KeyEventProc,
		   offsetof (CompWidgetVTable, keyRelease),
		   key, modifiers);
}

static void
noopKeyRelease (CompWidget *w,
		const char *key,
		int32_t    modifiers)
{
    FOR_BASE (&w->u.base, (*w->u.vTable->keyRelease) (w, key, modifiers));
}

static const CompWidgetVTable widgetObjectVTable = {
    .base.getProp = widgetGetProp,

    /* public signals */
    .buttonPress   = buttonPress,
    .buttonRelease = buttonRelease,
    .keyPress      = keyPress,
    .keyRelease    = keyRelease
};

static const CompWidgetVTable noopWidgetObjectVTable = {
    .buttonPress   = noopButtonPress,
    .buttonRelease = noopButtonRelease,
    .keyPress      = noopKeyPress,
    .keyRelease    = noopKeyRelease
};

static const CSignal widgetTypeSignal[] = {
    C_SIGNAL (buttonPress,   "iiii", CompWidgetVTable),
    C_SIGNAL (buttonRelease, "iiii", CompWidgetVTable),
    C_SIGNAL (keyPress,      "si",   CompWidgetVTable),
    C_SIGNAL (keyRelease,    "si",   CompWidgetVTable)
};

const CompObjectType *
getWidgetObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_WIDGET_TYPE_NAME,
	    .i.version	     = COMPIZ_WIDGET_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &widgetObjectVTable.base,
	    .i.vTable.noop   = &noopWidgetObjectVTable.base,
	    .i.vTable.size   = sizeof (widgetObjectVTable),
	    .i.instance.size = sizeof (CompWidget),

	    .signal  = widgetTypeSignal,
	    .nSignal = N_ELEMENTS (widgetTypeSignal)
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
