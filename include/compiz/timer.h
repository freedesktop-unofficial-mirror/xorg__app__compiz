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

#ifndef _COMPIZ_TIMER_H
#define _COMPIZ_TIMER_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompTimer CompTimer;

typedef void (*StartProc) (CompTimer *t);

typedef void (*StopProc) (CompTimer *t);

typedef void (*AlarmProc) (CompTimer *t);

typedef struct _CompTimerVTable {
    CompObjectVTable base;

    StartProc start;
    StopProc  stop;
    AlarmProc alarm;
} CompTimerVTable;

typedef struct _CompTimerData {
    CompObjectData base;
    int32_t        delay;
} CompTimerData;

struct _CompTimer {
    union {
	CompObject	      base;
	const CompTimerVTable *vTable;
    } u;

    CompTimerData data;
    CompBool	  running;
    int		  handle;
};

#define GET_TIMER(object) ((CompTimer *) (object))
#define TIMER(object) CompTimer *t = GET_TIMER (object)

#define COMPIZ_TIMER_VERSION   20080304
#define COMPIZ_TIMER_TYPE_NAME "org.compiz.timer"

const CompObjectType *
getTimerObjectType (void);

COMPIZ_END_DECLS

#endif
