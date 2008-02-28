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

#ifndef _COMPIZ_ROOT_H
#define _COMPIZ_ROOT_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompSignal CompSignal;
typedef struct _CompRoot   CompRoot;

typedef void (*ProcessSignalsProc) (CompRoot *r);

typedef void (*UpdatePluginsProc) (CompRoot   *r,
				   const char *branch);

typedef struct _CompRootVTable {
    CompObjectVTable   base;
    ProcessSignalsProc processSignals;
    UpdatePluginsProc  updatePlugins;
} CompRootVTable;

struct _CompRoot {
    union {
	CompObject	     base;
	const CompRootVTable *vTable;
    } u;

    CompObjectVTableVec object;

    struct {
	CompSignal *head;
	CompSignal *tail;
    } signal;

    char **stack;
    int  nStack;

    char **request;
    int  nRequest;
};

#define GET_ROOT(object) ((CompRoot *) (object))
#define ROOT(object) CompRoot *r = GET_ROOT (object)

#define COMPIZ_ROOT_VERSION   COMPIZ_PROP_VERSION
#define COMPIZ_ROOT_TYPE_NAME "org.compiz.root"

const CompObjectType *
getRootObjectType (void);

void
compEmitSignedSignal (CompObject *object,
		      const char *interface,
		      const char *name,
		      const char *signature,
		      ...);

COMPIZ_END_DECLS

#endif
