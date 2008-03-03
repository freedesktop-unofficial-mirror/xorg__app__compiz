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

#ifndef _COMPIZ_SIGNALMATCH_H
#define _COMPIZ_SIGNALMATCH_H

#include <compiz/object.h>

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
    char           *path;
    char           *interface;
    char           *name;
    char           *signature;
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

COMPIZ_END_DECLS

#endif
