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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#ifndef HAVE_VASPRINTF
#include <stdarg.h>
static int
vasprintf (char	      **strp,
	   const char *fmt,
	   va_list    ap)
{
    va_list ap2;
    int     len = 0;
    char    *str = NULL;

    for (;;)
    {
	char *s;
	int  n;

	va_copy (ap2, ap);
	n = vsnprintf (str, len, fmt, ap2);
	va_end (ap2);

	if (n > -1 && n < len)
	    return n;

	if (n > len)
	    len = n + 1;
	else
	    len = 256 + len * 2;

	s = realloc (str, len);
	if (!s)
	{
	    if (str)
		free (str);

	    return -1;
	}

	str = s;
    }
}
#endif /* HAVE_VASPRINTF */

#include <compiz-core.h>

int
esprintf (char	     **strp,
	  const char *fmt,
	  ...)
{
    va_list ap;
    int	    n;

    if (!strp)
	return 0;

    va_start (ap, fmt);
    n = vasprintf (strp, fmt, ap);
    va_end (ap);

    return n;
}
