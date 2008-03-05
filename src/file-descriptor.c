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

#include <poll.h>

#include <compiz/file-descriptor.h>
#include <compiz/c-object.h>
#include <compiz/marshal.h>

/* XXX: until file-descriptor code has been moved to root object */
#include <compiz/core.h>

static CompBool
fdProcess (void *data)
{
    FILE_DESCRIPTOR (data);

    (*fd->u.vTable->ready) (fd);

    return TRUE;
}

static CompBool
fdInit (CompObject *object)
{
    FILE_DESCRIPTOR (object);

    fd->fd     = -1;
    fd->handle = 0;

    return TRUE;
}

static void
fdInsert (CompObject *object,
	  CompObject *parent)
{
    FILE_DESCRIPTOR (object);

    if (fd->fd >= 0)
    {
	fd->handle = compAddWatchFd (fd->fd,
				     POLLIN | POLLPRI | POLLHUP | POLLERR,
				     fdProcess,
				     (void *) fd);
	if (!fd->handle)
	    fd->fd = -1;
    }
}

static void
fdRemove (CompObject *object)
{
    FILE_DESCRIPTOR (object);

    if (fd->fd >= 0)
    {
	compRemoveWatchFd (fd->handle);
	fd->handle = 0;
    }
}

static void
fdGetProp (CompObject   *object,
	   unsigned int what,
	   void	        *value)
{
    cGetObjectProp (&GET_FILE_DESCRIPTOR (object)->data,
		    getFileDescriptorObjectType (),
		    what, value);
}

static void
set (CompFileDescriptor *fd,
     int		unixFd)
{
    if (fd->fd == unixFd)
	return;

    if (fd->fd)
	compRemoveWatchFd (fd->handle);

    fd->fd     = unixFd;
    fd->handle = compAddWatchFd (fd->fd,
				 POLLIN | POLLPRI | POLLHUP | POLLERR,
				 fdProcess,
				 (void *) fd);
    if (!fd->handle)
	fd->fd = -1;
}

static void
noopSet (CompFileDescriptor *fd,
	 int		    unixFd)
{
    FOR_BASE (&fd->u.base, (*fd->u.vTable->set) (fd, unixFd));
}

static void
ready (CompFileDescriptor *fd)
{
    C_EMIT_SIGNAL (&fd->u.base, ReadyProc,
		   offsetof (CompFileDescriptorVTable, ready));
}

static void
noopReady (CompFileDescriptor *fd)
{
    FOR_BASE (&fd->u.base, (*fd->u.vTable->ready) (fd));
}

static const CompFileDescriptorVTable fileDescriptorObjectVTable = {
    .base.getProp = fdGetProp,

    .set   = set,
    .ready = ready
};

static const CompFileDescriptorVTable noopFileDescriptorObjectVTable = {
    .set   = noopSet,
    .ready = noopReady
};

static const CSignal fileDescriptorTypeSignal[] = {
    C_SIGNAL (ready, "", CompFileDescriptorVTable)
};

const CompObjectType *
getFileDescriptorObjectType (void)
{
    static CompObjectType *type = NULL;

    if (!type)
    {
	static const CObjectInterface template = {
	    .i.name	     = COMPIZ_FILE_DESCRIPTOR_TYPE_NAME,
	    .i.version	     = COMPIZ_FILE_DESCRIPTOR_VERSION,
	    .i.base.name     = COMPIZ_OBJECT_TYPE_NAME,
	    .i.base.version  = COMPIZ_OBJECT_VERSION,
	    .i.vTable.impl   = &fileDescriptorObjectVTable.base,
	    .i.vTable.noop   = &noopFileDescriptorObjectVTable.base,
	    .i.vTable.size   = sizeof (fileDescriptorObjectVTable),
	    .i.instance.size = sizeof (CompFileDescriptor),

	    .signal  = fileDescriptorTypeSignal,
	    .nSignal = N_ELEMENTS (fileDescriptorTypeSignal),

	    .init = fdInit,

	    .insert = fdInsert,
	    .remove = fdRemove
	};

	type = cObjectTypeFromTemplate (&template);
    }

    return type;
}
