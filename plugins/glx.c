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

static CompMetadata glxMetadata;

static Bool
glxInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    if (d->screens)
    {
	compLogMessage (d, p->vTable->name, CompLogLevelError,
			"%s plugin must be loaded before screens are "
			"initialize", p->vTable->name);
	return FALSE;
    }

    manualCompositeManagement = TRUE;

    return TRUE;
}

static CompBool
glxInitObject (CompPlugin *p,
	       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) glxInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static Bool
glxInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&glxMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    compAddMetadataFromFile (&glxMetadata, p->vTable->name);

    return TRUE;
}

static void
glxFini (CompPlugin *p)
{
    compFiniMetadata (&glxMetadata);
}

static CompMetadata *
glxGetMetadata (CompPlugin *plugin)
{
    return &glxMetadata;
}

CompPluginVTable glxVTable = {
    "glx",
    glxGetMetadata,
    glxInit,
    glxFini,
    glxInitObject,
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &glxVTable;
}
