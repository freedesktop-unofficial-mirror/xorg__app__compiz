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

#ifndef _COMPIZ_BRANCH_H
#define _COMPIZ_BRANCH_H

#include <compiz/container.h>

#define COMPIZ_BRANCH_VERSION 20071116

COMPIZ_BEGIN_DECLS

typedef struct _CompBranch CompBranch;

typedef CompBool (*RegisterTypeProc) (CompBranch	   *branch,
				      const char	   *interface,
				      const CompObjectType *type);

typedef CompObject *(*CreateObjectProc) (CompBranch *branch,
					 const char *type,
					 char	    **error);

typedef void (*DestroyObjectProc) (CompBranch *branch,
				   CompObject *object);

typedef struct _CompBranchVTable {
    CompObjectVTable base;

    RegisterTypeProc  registerType;
    CreateObjectProc  createObject;
    DestroyObjectProc destroyObject;
} CompBranchVTable;

typedef struct _CompObjectPrivatesNode {
    struct _CompObjectPrivatesNode *next;
    const char			   *name;
    CompObjectPrivates		   privates;
} CompObjectPrivatesNode;

typedef struct _CompFactory CompFactory;

typedef int (*AllocatePrivateIndexProc) (CompFactory *factory,
					 const char  *name,
					 int	     size);

typedef void (*FreePrivateIndexProc) (CompFactory *factory,
				      const char  *name,
				      int	  index);

struct _CompFactory {
    CompObjectFactory base;

    AllocatePrivateIndexProc allocatePrivateIndex;
    FreePrivateIndexProc     freePrivateIndex;

    CompObjectPrivatesNode *privates;
};

struct _CompBranch {
    union {
	CompContainer	       base;
	const CompBranchVTable *vTable;
    } u;

    CompObjectData    data;
    CompObjectFactory factory;
};

#define GET_BRANCH(object) ((CompBranch *) (object))
#define BRANCH(object) CompBranch *b = GET_BRANCH (object)

#define BRANCH_TYPE_NAME "branch"

const CompObjectType *
getBranchObjectType (void);

int
compAllocatePrivateIndex (CompFactory *factory,
			  int	      size);

void
compFreePrivateIndex (CompFactory *factory,
		      int	  index);



COMPIZ_END_DECLS

#endif
