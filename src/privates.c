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

#include <stdlib.h>
#include <string.h>

#include <compiz/macros.h>
#include <compiz/privates.h>

static void
updatePrivates (CompPrivate *privates,
		int	    len,
		int	    *sizes)
{
    char *data = (char *) (privates + len);
    int	 i;

    for (i = 0; i < len; i++)
    {
	if (sizes[i] > 0)
	{
	    if (sizes[i] > sizeof (CompPrivate))
	    {
		privates[i].ptr = (void *) data;
		data += sizes[i] - sizeof (CompPrivate);
	    }
	}
	else
	{
	    data += sizeof (CompPrivate) - sizes[i];
	}
    }
}

typedef struct _PrivatesHandle {
    CompPrivate *oldPrivates;
    CompPrivate *newPrivates;
} PrivatesHandle;

typedef struct _PrivatesList {
    PrivatesHandle *handle;
    int		   n;
} PrivatesList;

static void
initPrivatesList (PrivatesList *list)
{
    list->handle = NULL;
    list->n      = 0;
}

static void
finiPrivatesList (PrivatesList *list)
{
    if (list->handle)
	free (list->handle);
}

static CompBool
getPrivates (GetPrivatesProc get,
	     SetPrivatesProc set,
	     void	     *data,
	     void	     *closure)
{
    PrivatesList   *list = (PrivatesList *) data;
    PrivatesHandle *handle;

    handle = realloc (list->handle, (list->n + 1) * sizeof (PrivatesHandle));
    if (!handle)
	return FALSE;

    handle[list->n].oldPrivates = (*get) (closure);
    handle[list->n].newPrivates = NULL;

    list->handle = handle;
    list->n++;

    return TRUE;
}

static CompBool
setPrivates (GetPrivatesProc get,
	     SetPrivatesProc set,
	     void	     *data,
	     void	     *closure)
{
    PrivatesList *list = (PrivatesList *) data;

    list->n--;

    (*set) (closure, list->handle[list->n].newPrivates);

    if (list->handle[list->n].oldPrivates)
	free (list->handle[list->n].oldPrivates);

    return TRUE;
}

static CompBool
initNewPrivates (PrivatesHandle *handle,
		 int		size)
{
    CompPrivate *privates = NULL;

    if (size)
    {
	privates = malloc (size);
	if (!privates)
	    return FALSE;
    }

    handle->newPrivates = privates;

    return TRUE;
}

static CompBool
initAllNewPrivates (PrivatesList *list,
		    int		 size)
{
    int i;

    for (i = 0; i < list->n; i++)
	if (!initNewPrivates (&list->handle[i], size))
	    break;

    if (i < list->n)
    {
	while (i--)
	    free (list->handle[i].newPrivates);

	return FALSE;
    }

    return TRUE;
}

static void
copyPrivates (PrivatesHandle *handle,
	      int	     oldLen,
	      int	     newLen,
	      int	     dataSize,
	      int	     *sizes)
{
    char *oldData, *newData;
    int  minLen = MIN (newLen, oldLen);

    if (minLen)
	memcpy (handle->newPrivates, handle->oldPrivates,
		minLen * sizeof (CompPrivate));

    newData = (char *) (handle->newPrivates + newLen);
    oldData = (char *) (handle->oldPrivates + oldLen);

    if (dataSize)
	memcpy (newData, oldData, dataSize);

    updatePrivates (handle->newPrivates, newLen, sizes);
}

static void
copyAllPrivates (PrivatesList *list,
		 int	      oldLen,
		 int	      newLen,
		 int	      dataSize,
		 int	      *sizes)
{
    int i;

    for (i = 0; i < list->n; i++)
	copyPrivates (&list->handle[i], oldLen, newLen, dataSize, sizes);
}

int
allocatePrivateIndex (int		  *len,
		      int		  **sizes,
		      int		  *totalSize,
		      int		  size,
		      ForEachPrivatesProc forEachPrivates,
		      void		  *closure)
{
    PrivatesList list;
    int		 *newSizes, newSize;

    /* round up sizes for proper alignment */
    size = ((size + (sizeof (long) - 1)) / sizeof (long)) * sizeof (long);

    /* always allocate a new index instead of trying to reuse an old one
       as the indices are most likely removed in the same order they got
       added */
    newSizes = realloc (*sizes, (*len + 1) * sizeof (int));
    if (!newSizes)
	return -1;

    newSizes[*len] = size + sizeof (CompPrivate);
    *sizes = newSizes;

    newSize = *totalSize + newSizes[*len];

    initPrivatesList (&list);

    if (!(*forEachPrivates) (getPrivates, (void *) &list, closure))
    {
	finiPrivatesList (&list);
	return -1;
    }

    if (!initAllNewPrivates (&list, newSize))
    {
	finiPrivatesList (&list);
	return -1;
    }

    copyAllPrivates (&list, *len, *len + 1,
		     *totalSize - *len * sizeof (CompPrivate),
		     newSizes);

    (*forEachPrivates) (setPrivates, (void *) &list, closure);

    finiPrivatesList (&list);

    *totalSize = newSize;
    return (*len)++;
}

void
freePrivateIndex (int		      *len,
		  int		      **sizes,
		  int		      *totalSize,
		  ForEachPrivatesProc forEachPrivates,
		  void		      *closure,
		  int		      index)
{
    int	newLen = *len;

    /* mark private data as unused */
    (*sizes)[index] = -(*sizes)[index];

    /* check for private data that can be freed */
    while (newLen && (*sizes)[newLen - 1] < 0)
    {
	*totalSize += (*sizes)[newLen - 1];
	newLen--;
    }

    /* free unused private data */
    if (newLen != *len)
    {
	PrivatesList list;
	int	     *newSizes;

	initPrivatesList (&list);

	if ((*forEachPrivates) (getPrivates, (void *) &list, closure))
	{
	    if (initAllNewPrivates (&list, *totalSize))
	    {
		copyAllPrivates (&list, *len, newLen,
				 *totalSize - newLen * sizeof (CompPrivate),
				 *sizes);

		(*forEachPrivates) (setPrivates, (void *) &list, closure);

		newSizes = realloc (*sizes, newLen * sizeof (int));
		if (!newLen || newSizes)
		    *sizes = newSizes;

		*len = newLen;
	    }
	}

	finiPrivatesList (&list);
    }
}

CompPrivate *
allocatePrivates (int len,
		  int *sizes,
		  int totalSize)
{
    CompPrivate *privates;

    privates = malloc (totalSize);
    if (!privates)
	return NULL;

    updatePrivates (privates, len, sizes);

    return privates;
}
