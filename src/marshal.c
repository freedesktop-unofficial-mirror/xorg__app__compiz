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

#include <compiz-core.h>

void
marshal____ (CompObject *object,
	     void       (*method) (CompObject *),
	     CompArgs   *args)
{
    (*method) (object);
}

void
marshal_I_S (CompObject *object,
	     CompBool   (*method) (CompObject *,
				   char       *),
	     CompArgs   *args)
{
    char *in0;
    int  result;

    (*args->load) (args, "s", &in0);

    result = (*method) (object, in0);

    (*args->store) (args, "i", &result);
}

void
marshal__I__E (CompObject *object,
	       CompBool   (*method) (CompObject *,
				     int32_t     ,
				     char       **),
	       CompArgs   *args)
{
    int32_t in0;
    char    *error;

    (*args->load) (args, "i", &in0);

    if (!(*method) (object, in0, &error))
    {
	(*args->error) (args, error);
    }
}

void
marshal__SSB__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       CompBool    ,
				       char       **),
		 CompArgs   *args)
{
    char     *in0;
    char     *in1;
    CompBool in2;
    char     *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);
    (*args->load) (args, "b", &in2);

    if (!(*method) (object, in0, in1, in2, &error))
    {
	(*args->error) (args, error);
    }
}

void
marshal__SS_B_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       CompBool   *,
				       char       **),
		 CompArgs   *args)
{
    char     *in0;
    char     *in1;
    CompBool out0;
    char     *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);

    if (!(*method) (object, in0, in1, &out0, &error))
    {
	(*args->error) (args, error);
    }
    else
    {
	(*args->store) (args, "b", &out0);
    }
}

void
marshal__SSI__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       int32_t     ,
				       char       **),
		 CompArgs   *args)
{
    char    *in0;
    char    *in1;
    int32_t in2;
    char    *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);
    (*args->load) (args, "i", &in2);

    if (!(*method) (object, in0, in1, in2, &error))
    {
	(*args->error) (args, error);
    }
}

void
marshal__SS_I_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       int32_t    *,
				       char       **),
		 CompArgs   *args)
{
    char    *in0;
    char    *in1;
    int32_t out0;
    char    *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);

    if (!(*method) (object, in0, in1, &out0, &error))
    {
	(*args->error) (args, error);
    }
    else
    {
	(*args->store) (args, "i", &out0);
    }
}

void
marshal__SSD__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       double      ,
				       char       **),
		 CompArgs   *args)
{
    char   *in0;
    char   *in1;
    double in2;
    char   *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);
    (*args->load) (args, "d", &in2);

    if (!(*method) (object, in0, in1, in2, &error))
    {
	(*args->error) (args, error);
    }
}

void
marshal__SS_D_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       double     *,
				       char       **),
		 CompArgs   *args)
{
    char   *in0;
    char   *in1;
    double out0;
    char   *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);

    if (!(*method) (object, in0, in1, &out0, &error))
    {
	(*args->error) (args, error);
    }
    else
    {
	(*args->store) (args, "d", &out0);
    }
}

void
marshal__SSS__E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       char       *,
				       char       **),
		 CompArgs   *args)
{
    char *in0;
    char *in1;
    char *in2;
    char *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);
    (*args->load) (args, "s", &in2);

    if (!(*method) (object, in0, in1, in2, &error))
    {
	(*args->error) (args, error);
    }
}

void
marshal__SS_S_E (CompObject *object,
		 CompBool   (*method) (CompObject *,
				       char       *,
				       char       *,
				       char	  **,
				       char       **),
		 CompArgs   *args)
{
    char *in0;
    char *in1;
    char *out0;
    char *error;

    (*args->load) (args, "s", &in0);
    (*args->load) (args, "s", &in1);

    if (!(*method) (object, in0, in1, &out0, &error))
    {
	(*args->error) (args, error);
    }
    else
    {
	(*args->store) (args, "s", &out0);
    }
}

void
marshal__S_S_E (CompObject *object,
		CompBool   (*method) (CompObject *,
				      char	 *,
				      char	 **,
				      char       **),
		CompArgs   *args)
{
    char *in0;
    char *out0;
    char *error;

    (*args->load) (args, "s", &in0);

    if (!(*method) (object, in0, &out0, &error))
    {
	(*args->error) (args, error);
    }
    else
    {
	(*args->store) (args, "s", &out0);
    }
}

