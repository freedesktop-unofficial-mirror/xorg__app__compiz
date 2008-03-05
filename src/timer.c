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

#include <compiz/timer.h>
#include <compiz/c-object.h>
#include <compiz/marshal.h>

/* XXX: until timeout code has been moved to root object */
#include <compiz/core.h>

static CompBool
timerTimeout (void *data)
{
    TIMER (data);

    (*t->u.vTable->alarm) (t);

    return TRUE;
}

static CompBool
timerInit (CompObject *object)
{
    TIMER (object);

    t->handle  = 0;
    t->running = FALSE;

    return TRUE;
}

static void
timerInsert (CompObject *object,
	     CompObject *parent)
{
    TIMER (object);

    if (t->running)
    {
	t->handle = compAddTimeout (t->data.delay, timerTimeout, (void *) t);
	if (!t->handle)
	    t->running = FALSE;
    }
}

static void
timerRemove (CompObject *object)
{
    TIMER (object);

    if (t->running)
    {
	compRemoveTimeout (t->handle);
	t->handle = 0;
    }
}

static void
timerGetProp (CompObject   *object,
	      unsigned int what,
	      void	   *value)
{
    cGetObjectProp (&GET_TIMER (object)->data.base,
		    getTimerObjectType (),
		    what, value);
}

static void
start (CompTimer *t)
{
    if (!t->running)
    {
	t->handle = compAddTimeout (t->data.delay, timerTimeout, (void *) t);
	if (t->handle)
	    t->running = TRUE;
    }
}

static void
noopStart (CompTimer *t)
{
    FOR_BASE (&t->u.base, (*t->u.vTable->start) (t));
}

static void
stop (CompTimer *t)
{
    if (t->running)
    {
	compRemoveTimeout (t->handle);

	t->handle  = 0;
	t->running = FALSE;
    }
}

static void
noopStop (CompTimer *t)
{
    FOR_BASE (&t->u.base, (*t->u.vTable->stop) (t));
}

static void
alarm (CompTimer *t)
{
    C_EMIT_SIGNAL (&t->u.base, AlarmProc, offsetof (CompTimerVTable, alarm));
}

static void
noopAlarm (CompTimer *t)
{
    FOR_BASE (&t->u.base, (*t->u.vTable->alarm) (t));
}

static const CompTimerVTable timerObjectVTable = {
    .base.getProp = timerGetProp,

    .start = start,
    .stop  = stop,
    .alarm = alarm
};

static const CompTimerVTable noopTimerObjectVTable = {
    .start = noopStart,
    .stop  = noopStop,
    .alarm = noopAlarm
};

static const CMethod timerTypeMethod[] = {
    C_METHOD (start, "", "", CompTimerVTable, marshal____),
    C_METHOD (stop,  "", "", CompTimerVTable, marshal____)
};

static const CSignal timerTypeSignal[] = {
    C_SIGNAL (alarm, "", CompTimerVTable)
};

static const CIntProp timerTypeIntProp[] = {
    C_PROP (delay, CompTimerData)
};

const CompObjectType *
getTimerObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_TIMER_TYPE_NAME,
	    .i.version	     = COMPIZ_TIMER_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &timerObjectVTable.base,
	    .i.vTable.noop   = &noopTimerObjectVTable.base,
	    .i.vTable.size   = sizeof (timerObjectVTable),
	    .i.instance.size = sizeof (CompTimer),

	    .method  = timerTypeMethod,
	    .nMethod = N_ELEMENTS (timerTypeMethod),

	    .signal  = timerTypeSignal,
	    .nSignal = N_ELEMENTS (timerTypeSignal),

	    .intProp  = timerTypeIntProp,
	    .nIntProp = N_ELEMENTS (timerTypeIntProp),

	    .init = timerInit,

	    .insert = timerInsert,
	    .remove = timerRemove
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
