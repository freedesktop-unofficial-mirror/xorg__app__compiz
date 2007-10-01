/*
 * Copyright © 2005 Novell, Inc.
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

#include <compiz-core.h>

int
allocatePrivateIndex (int		  *len,
		      int		  **indices,
		      ReallocPrivatesProc reallocProc,
		      void		  *closure)
{
    int *newIndices;

    newIndices = realloc (*indices, (*len + 1) * sizeof (int));
    if (!newIndices)
	return -1;

    newIndices[*len] = 1;
    *indices = newIndices;

    if (!(*reallocProc) (*len + 1, closure))
	return -1;

    return (*len)++;
}

void
freePrivateIndex (int		      *len,
		  int		      **indices,
		  ReallocPrivatesProc reallocProc,
		  void		      *closure,
		  int		      index)
{
    int *newIndices;

    (*len)--;

    if (index < *len)
	memmove ((*indices) + index, (*indices) + (index + 1),
		 *len - index);

    (*indices)[(*len)] = 0;

    newIndices = realloc (*indices, (*len) * sizeof (int));
    if (!*len || newIndices)
	*indices = newIndices;

    (*reallocProc) (*len, closure);
}
