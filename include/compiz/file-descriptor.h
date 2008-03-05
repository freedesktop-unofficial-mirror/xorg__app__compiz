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

#ifndef _COMPIZ_FILE_DESCRIPTOR_H
#define _COMPIZ_FILE_DESCRIPTOR_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

typedef struct _CompFileDescriptor CompFileDescriptor;

typedef void (*SetProc) (CompFileDescriptor *fd,
			 int		    unixFd);

typedef void (*ReadyProc) (CompFileDescriptor *fd);

typedef struct _CompFileDescriptorVTable {
    CompObjectVTable base;

    SetProc   set;
    ReadyProc ready;
} CompFileDescriptorVTable;

struct _CompFileDescriptor {
    union {
	CompObject		       base;
	const CompFileDescriptorVTable *vTable;
    } u;

    CompObjectData data;
    int		   fd;
    int		   handle;
};

#define GET_FILE_DESCRIPTOR(object) ((CompFileDescriptor *) (object))
#define FILE_DESCRIPTOR(object)				  \
    CompFileDescriptor *fd = GET_FILE_DESCRIPTOR (object)

#define COMPIZ_FILE_DESCRIPTOR_VERSION   20080304
#define COMPIZ_FILE_DESCRIPTOR_TYPE_NAME "org.compiz.fileDescriptor"

const CompObjectType *
getFileDescriptorObjectType (void);

COMPIZ_END_DECLS

#endif
