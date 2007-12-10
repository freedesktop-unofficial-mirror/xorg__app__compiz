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

#ifndef _COMPIZ_PLUGIN_H
#define _COMPIZ_PLUGIN_H

#include <compiz/branch.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompPlugin   CompPlugin;
typedef struct _CompOption   CompOption;
typedef struct _CompMetadata CompMetadata;

typedef union _CompOptionValue CompOptionValue;

typedef CompBool (*InitPluginProc) (CompFactory *factory);
typedef void     (*FiniPluginProc) (CompFactory *factory);

typedef CompMetadata *(*GetPluginMetadataProc) (CompPlugin *plugin);

typedef CompBool (*InitPluginObjectProc) (CompPlugin *plugin,
					  CompObject *object);
typedef void (*FiniPluginObjectProc) (CompPlugin  *plugin,
				      CompObject *object);

typedef CompOption *(*GetPluginObjectOptionsProc) (CompPlugin *plugin,
						   CompObject *object,
						   int	      *count);
typedef CompBool (*SetPluginObjectOptionProc) (CompPlugin      *plugin,
					       CompObject      *object,
					       const char      *name,
					       CompOptionValue *value);

typedef CompBool (*InsertPluginProc) (CompObject *parent,
				      CompBranch *branch);
typedef void     (*RemovePluginProc) (CompObject *parent,
				      CompBranch *branch);

typedef struct _CompPluginVTable {
    const char *name;

    GetPluginMetadataProc getMetadata;

    InitPluginProc init;
    FiniPluginProc fini;

    InitPluginObjectProc initObject;
    FiniPluginObjectProc finiObject;

    GetPluginObjectOptionsProc getObjectOptions;
    SetPluginObjectOptionProc  setObjectOption;

    InsertPluginProc insert;
    RemovePluginProc remove;
} CompPluginVTable;

CompPluginVTable *
getCompPluginInfo20070830 (void);

COMPIZ_END_DECLS

#endif
