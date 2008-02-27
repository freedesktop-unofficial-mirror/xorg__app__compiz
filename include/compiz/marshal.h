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

#ifndef _COMPIZ_MARSHAL_H
#define _COMPIZ_MARSHAL_H

#include <compiz/object.h>

COMPIZ_BEGIN_DECLS

void
marshal____ (CompObject *object,
	     void       (*method) (CompObject *),
	     CompArgs   *args);

void
marshal_I_S (CompObject *object,
	     int32_t     (*method) (CompObject *,
				    char       *),
	     CompArgs   *args);

void
marshal__I__ (CompObject *object,
	      void      (*method) (CompObject *,
				   int32_t     ),
	      CompArgs   *args);

void
marshal__I__E (CompObject *object,
	       CompBool   (*method) (CompObject *,
				     int32_t     ,
				     char       **),
	       CompArgs   *args);

void
marshal__S__ (CompObject *object,
	      void       (*method) (CompObject *,
				    const char *),
	      CompArgs   *args);

void
marshal__S__E (CompObject *object,
	       CompBool   (*method) (CompObject *,
				     const char *,
				     char       **),
	       CompArgs   *args);

void
marshal__SI__E (CompObject *object,
		CompBool   (*method) (CompObject *,
				      char       *,
				      int32_t     ,
				      char       **),
		CompArgs   *args);

void
marshal__SS__E (CompObject *object,
		CompBool   (*method) (CompObject *,
				      char       *,
				      char       *,
				      char       **),
		CompArgs   *args);

void
marshal__SSB__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       CompBool    ,
				       char       **),
		 CompArgs   *args);

void
marshal__SS_B_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       CompBool   *,
				       char       **),
		 CompArgs   *args);

void
marshal__SSI__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       int32_t     ,
				       char       **),
		 CompArgs   *args);

void
marshal__SS_I_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       int32_t    *,
				       char       **),
		 CompArgs   *args);

void
marshal__SSD__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       double      ,
				       char       **),
		 CompArgs   *args);

void
marshal__SS_D_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       double     *,
				       char       **),
		 CompArgs   *args);

void
marshal__SSS__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       char       *,
				       char       **),
		 CompArgs   *args);

void
marshal__SS_S_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       char	  **,
				       char       **),
		 CompArgs   *args);

void
marshal__S_S_E (CompObject *object,
		CompBool   (*method) (CompObject *,
				      char	 *,
				      char	 **,
				      char       **),
		CompArgs   *args);

COMPIZ_END_DECLS

#endif
