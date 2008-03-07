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

#ifndef _COMPIZ_DELEGATE_H
#define _COMPIZ_DELEGATE_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompDelegate CompDelegate;

typedef void (*DelegateSignalProc) (CompDelegate *d,
				    const char   *path,
				    const char   *interface,
				    const char   *name,
				    const char   *signature,
				    CompAnyValue *value,
				    int	         nValue);

typedef struct _CompDelegateVTable {
    CompObjectVTable base;

    DelegateSignalProc processSignal;
} CompDelegateVTable;

typedef struct _CompDelegateData {
    CompObjectData base;

    CompBool compress;

    CompObject matches;
} CompDelegateData;

struct _CompDelegate {
    union {
	CompObject	         base;
	const CompDelegateVTable *vTable;
    } u;

    CompDelegateData data;

    int pending;
};

#define GET_DELEGATE(object) ((CompDelegate *) (object))
#define DELEGATE(object) CompDelegate *d = GET_DELEGATE (object)

#define COMPIZ_DELEGATE_VERSION   20080302
#define COMPIZ_DELEGATE_TYPE_NAME "org.compiz.delegate"

const CompObjectType *
getDelegateObjectType (void);


typedef struct _CompVoidDelegate CompVoidDelegate;

typedef void (*VoidNotifyProc) (CompVoidDelegate *vd);

typedef struct _CompVoidDelegateVTable {
    CompDelegateVTable base;

    VoidNotifyProc notify;
} CompVoidDelegateVTable;

struct _CompVoidDelegate {
    union {
	CompDelegate		     base;
	const CompVoidDelegateVTable *vTable;
    } u;

    CompObjectData data;
};

#define COMPIZ_VOID_DELEGATE_TYPE_NAME "org.compiz.delegate.void"

#define GET_VOID_DELEGATE(object) ((CompVoidDelegate *) (object))
#define VOID_DELEGATE(object) CompVoidDelegate *vd = GET_VOID_DELEGATE (object)

const CompObjectType *
getVoidDelegateObjectType (void);


typedef struct _CompIDelegate CompIDelegate;

typedef void (*INotifyProc) (CompIDelegate *id,
			     int32_t	   value0);

typedef struct _CompIDelegateVTable {
    CompDelegateVTable base;

    INotifyProc notify;
} CompIDelegateVTable;

struct _CompIDelegate {
    union {
	CompDelegate		  base;
	const CompIDelegateVTable *vTable;
    } u;

    CompObjectData data;
};

#define COMPIZ_I_DELEGATE_TYPE_NAME "org.compiz.delegate.i"

#define GET_I_DELEGATE(object) ((CompIDelegate *) (object))
#define I_DELEGATE(object) CompIDelegate *id = GET_I_DELEGATE (object)

const CompObjectType *
getIDelegateObjectType (void);

COMPIZ_END_DECLS

#endif
