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

#ifndef _COMPIZ_SIGNAL_MATCH_H
#define _COMPIZ_SIGNAL_MATCH_H

#include <compiz/object.h>
#include <compiz/keyboard.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompSignalMatch CompSignalMatch;

typedef CompBool (*MatchProc) (CompSignalMatch *sm,
			       const char	*path,
			       const char	*interface,
			       const char	*name,
			       const char	*signature,
			       CompAnyValue	*value,
			       int	        nValue,
			       const char      *args,
			       CompAnyValue    *argValue);

typedef struct _CompSignalMatchVTable {
    CompObjectVTable base;

    MatchProc match;
} CompSignalMatchVTable;

typedef struct _CompSignalMatchData {
    CompObjectData base;
    CompObject     args;
} CompSignalMatchData;

struct _CompSignalMatch {
    union {
	CompObject	         base;
	const CompSignalMatchVTable *vTable;
    } u;

    CompSignalMatchData data;
};

#define GET_SIGNAL_MATCH(object) ((CompSignalMatch *) (object))
#define SIGNAL_MATCH(object) CompSignalMatch *sm = GET_SIGNAL_MATCH (object)

#define COMPIZ_SIGNAL_MATCH_VERSION   20080303
#define COMPIZ_SIGNAL_MATCH_TYPE_NAME "org.compiz.signalMatch"

const CompObjectType *
getSignalMatchObjectType (void);


typedef struct _CompSimpleSignalMatchData {
    CompObjectData base;
    char           *path;
    char           *interface;
    char           *name;
    char           *signature;
} CompSimpleSignalMatchData;

typedef struct _CompSimpleSignalMatch {
    union {
	CompSignalMatch		    base;
	const CompSignalMatchVTable *vTable;
    } u;

    CompSimpleSignalMatchData data;
} CompSimpleSignalMatch;

#define GET_SIMPLE_SIGNAL_MATCH(object) ((CompSimpleSignalMatch *) (object))
#define SIMPLE_SIGNAL_MATCH(object)				  \
    CompSimpleSignalMatch *ssm = GET_SIMPLE_SIGNAL_MATCH (object)

#define COMPIZ_SIMPLE_SIGNAL_MATCH_TYPE_NAME "org.compiz.signalMatch.simple"

const CompObjectType *
getSimpleSignalMatchObjectType (void);


typedef struct _CompStructureNotifySignalMatchData {
    CompObjectData base;
    char           *object;
} CompStructureNotifySignalMatchData;

typedef struct _CompStructureNotifySignalMatch {
    union {
	CompSignalMatch		    base;
	const CompSignalMatchVTable *vTable;
    } u;

    CompStructureNotifySignalMatchData data;
} CompStructureNotifySignalMatch;

#define GET_STRUCTURE_NOTIFY_SIGNAL_MATCH(object) \
    ((CompStructureNotifySignalMatch *) (object))
#define STRUCTURE_NOTIFY_SIGNAL_MATCH(object)      \
    CompStructureNotifySignalMatch *snsm =	   \
	GET_STRUCTURE_NOTIFY_SIGNAL_MATCH (object)

#define COMPIZ_STRUCTURE_NOTIFY_SIGNAL_MATCH_TYPE_NAME \
    "org.compiz.signalMatch.structureNotify"

const CompObjectType *
getStructureNotifySignalMatchObjectType (void);


typedef struct _CompKeyEventSignalMatchData {
    CompObjectData base;

    CompKeyEventDescription key;
} CompKeyEventSignalMatchData;

typedef struct _CompKeyEventSignalMatch {
    union {
	CompSignalMatch		    base;
	const CompSignalMatchVTable *vTable;
    } u;

    CompKeyEventSignalMatchData data;
} CompKeyEventSignalMatch;

#define GET_KEY_EVENT_SIGNAL_MATCH(object) \
    ((CompKeyEventSignalMatch *) (object))
#define KEY_EVENT_SIGNAL_MATCH(object) \
    CompKeyEventSignalMatch *kesm = GET_KEY_EVENT_SIGNAL_MATCH (object)

#define COMPIZ_KEY_EVENT_SIGNAL_MATCH_TYPE_NAME \
    "org.compiz.signalMatch.keyEvent"

const CompObjectType *
getKeyEventSignalMatchObjectType (void);


typedef struct _CompKeyPressSignalMatch {
    union {
	CompKeyEventSignalMatch	    base;
	const CompSignalMatchVTable *vTable;
    } u;

    CompObjectData data;
} CompKeyPressSignalMatch;

#define GET_KEY_PRESS_SIGNAL_MATCH(object) \
    ((CompKeyPressSignalMatch *) (object))
#define KEY_PRESS_SIGNAL_MATCH(object) \
    CompKeyPressSignalMatch *kpsm = GET_KEY_PRESS_SIGNAL_MATCH (object)

#define COMPIZ_KEY_PRESS_SIGNAL_MATCH_TYPE_NAME \
    "org.compiz.signalMatch.keyPress"

const CompObjectType *
getKeyPressSignalMatchObjectType (void);


typedef struct _CompKeyReleaseSignalMatch {
    union {
	CompKeyEventSignalMatch	    base;
	const CompSignalMatchVTable *vTable;
    } u;

    CompObjectData data;
} CompKeyReleaseSignalMatch;

#define GET_KEY_RELEASE_SIGNAL_MATCH(object) \
    ((CompKeyReleaseSignalMatch *) (object))
#define KEY_RELEASE_SIGNAL_MATCH(object) \
    CompKeyReleaseSignalMatch *krsm = GET_KEY_RELEASE_SIGNAL_MATCH (object)

#define COMPIZ_KEY_RELEASE_SIGNAL_MATCH_TYPE_NAME \
    "org.compiz.signalMatch.keyRelease"

const CompObjectType *
getKeyReleaseSignalMatchObjectType (void);

COMPIZ_END_DECLS

#endif
