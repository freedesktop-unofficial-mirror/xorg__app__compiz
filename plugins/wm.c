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

#include <compiz-core.h>

static CompMetadata wmMetadata;

static CompOption *
wmGetDisplayOptions (CompPlugin  *plugin,
		     CompDisplay *display,
		     int	 *count)
{
    *count = 1;
    return &display->opt[COMP_DISPLAY_OPTION_ABI];
}

static Bool
wmInitDisplay (CompPlugin  *p,
	       CompDisplay *d)
{
    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    if (d->screens)
    {
	compLogMessage (p->vTable->name, CompLogLevelError,
			"%s plugin must be loaded before screens are "
			"initialize", p->vTable->name);
	return FALSE;
    }

    windowManagement = TRUE;

    return TRUE;
}

static CompBool
wmInitObject (CompPlugin *p,
	      CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) wmInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static Bool
wmInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&wmMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    compAddMetadataFromFile (&wmMetadata, p->vTable->name);

    return TRUE;
}

static void
wmFini (CompPlugin *p)
{
    compFiniMetadata (&wmMetadata);
}

static CompMetadata *
wmGetMetadata (CompPlugin *plugin)
{
    return &wmMetadata;
}

CompPluginVTable wmVTable = {
    "wm",
    wmGetMetadata,
    wmInit,
    wmFini,
    wmInitObject,
    0, /* FiniObject */
    wmGetDisplayOptions,
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &wmVTable;
}
