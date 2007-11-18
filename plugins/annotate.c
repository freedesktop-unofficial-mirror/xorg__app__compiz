/*
 * Copyright © 2006 Novell, Inc.
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
#include <math.h>
#include <string.h>
#include <cairo-xlib-xrender.h>

#include <compiz/core.h>
#include <compiz/c-object.h>

#define COMPIZ_ANNOTATE_VERSION 20071116

static CompMetadata annoMetadata;

static int annoDisplayPrivateIndex;

static int annoLastPointerX = 0;
static int annoLastPointerY = 0;

#define ANNO_DISPLAY_OPTION_INITIATE_BUTTON 0
#define ANNO_DISPLAY_OPTION_DRAW_BUTTON	    1
#define ANNO_DISPLAY_OPTION_ERASE_BUTTON    2
#define ANNO_DISPLAY_OPTION_CLEAR_KEY       3
#define ANNO_DISPLAY_OPTION_CLEAR_BUTTON    4
#define ANNO_DISPLAY_OPTION_FILL_COLOR      5
#define ANNO_DISPLAY_OPTION_STROKE_COLOR    6
#define ANNO_DISPLAY_OPTION_NUM	            7


typedef struct _AnnoDisplay {
    CompObjectVTableVec object;

    double lineWidth;
    double strokeWidth;

    HandleEventProc handleEvent;

    CompOption opt[ANNO_DISPLAY_OPTION_NUM];
} AnnoDisplay;

static int annoScreenPrivateIndex;

typedef struct _AnnoScreen {
    CompObjectVTableVec object;

    PaintOutputProc paintOutput;
    int		    grabIndex;

    Pixmap	    pixmap;
    CompTexture	    texture;
    cairo_surface_t *surface;
    cairo_t	    *cairo;
    Bool            content;

    Bool eraseMode;
} AnnoScreen;

#define GET_ANNO_DISPLAY(d)					 \
    ((AnnoDisplay *) (d)->privates[annoDisplayPrivateIndex].ptr)

#define ANNO_DISPLAY(d)			   \
    AnnoDisplay *ad = GET_ANNO_DISPLAY (d)

#define GET_ANNO_SCREEN(s)				       \
    ((AnnoScreen *) (s)->privates[annoScreenPrivateIndex].ptr)

#define ANNO_SCREEN(s)			 \
    AnnoScreen *as = GET_ANNO_SCREEN (s)


static CDoubleProp annotateDisplayDoubleProp[] = {
    C_PROP (lineWidth, AnnoDisplay),
    C_PROP (strokeWidth, AnnoDisplay)
};

static const CInterface annoDisplayInterface[] = {
    C_INTERFACE (annotate, Display, CompObjectVTable, _, _, _, _, _, X, _, _)
};

static const CInterface annoScreenInterface[] = {
    C_INTERFACE (annotate, Screen, CompObjectVTable, _, _, _, _, _, _, _, _)
};

static void
annoCairoClear (CompScreen *s,
		cairo_t    *cr)
{
    ANNO_SCREEN (s);

    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_restore (cr);

    as->content = FALSE;
}

static cairo_t *
annoCairoContext (CompScreen *s)
{
    ANNO_SCREEN (s);

    if (!as->cairo)
    {
	XRenderPictFormat *format;
	Screen		  *screen;
	int		  w, h;

	screen = ScreenOfDisplay (s->display->display, s->screenNum);

	w = s->width;
	h = s->height;

	format = XRenderFindStandardFormat (s->display->display,
					    PictStandardARGB32);

	as->pixmap = XCreatePixmap (s->display->display, s->root, w, h, 32);

	if (!bindPixmapToTexture (s, &as->texture, as->pixmap, w, h, 32))
	{
	    compLogMessage (s->display, "annotate",
			    CompLogLevelError,
			    "Couldn't bind pixmap 0x%x to texture",
			    (int) as->pixmap);

	    XFreePixmap (s->display->display, as->pixmap);

	    return NULL;
	}

	as->surface =
	    cairo_xlib_surface_create_with_xrender_format (s->display->display,
							   as->pixmap, screen,
							   format, w, h);

	as->cairo = cairo_create (as->surface);

	annoCairoClear (s, as->cairo);
    }

    return as->cairo;
}

static void
annoSetSourceColor (cairo_t	   *cr,
		    unsigned short *color)
{
    cairo_set_source_rgba (cr,
			   (double) color[0] / 0xffff,
			   (double) color[1] / 0xffff,
			   (double) color[2] / 0xffff,
			   (double) color[3] / 0xffff);
}

static void
annoDrawCircle (CompScreen     *s,
		double	       xc,
		double	       yc,
		double	       radius,
		unsigned short *fillColor,
		unsigned short *strokeColor,
		double	       strokeWidth)
{
    REGION  reg;
    cairo_t *cr;

    ANNO_SCREEN (s);

    cr = annoCairoContext (s);
    if (cr)
    {
	double  ex1, ey1, ex2, ey2;

	annoSetSourceColor (cr, fillColor);
	cairo_arc (cr, xc, yc, radius, 0, 2 * M_PI);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, strokeWidth);
	cairo_stroke_extents (cr, &ex1, &ey1, &ex2, &ey2);
	annoSetSourceColor (cr, strokeColor);
	cairo_stroke (cr);

	reg.rects    = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = ex1;
	reg.extents.y1 = ey1;
	reg.extents.x2 = ex2;
	reg.extents.y2 = ey2;

	as->content = TRUE;
	damageScreenRegion (s, &reg);
    }
}

static void
annoDrawRectangle (CompScreen	  *s,
		   double	  x,
		   double	  y,
		   double	  w,
		   double	  h,
		   unsigned short *fillColor,
		   unsigned short *strokeColor,
		   double	  strokeWidth)
{
    REGION reg;
    cairo_t *cr;

    ANNO_SCREEN (s);

    cr = annoCairoContext (s);
    if (cr)
    {
	double  ex1, ey1, ex2, ey2;

	annoSetSourceColor (cr, fillColor);
	cairo_rectangle (cr, x, y, w, h);
	cairo_fill_preserve (cr);
	cairo_set_line_width (cr, strokeWidth);
	cairo_stroke_extents (cr, &ex1, &ey1, &ex2, &ey2);
	annoSetSourceColor (cr, strokeColor);
	cairo_stroke (cr);

	reg.rects    = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = ex1;
	reg.extents.y1 = ey1;
	reg.extents.x2 = ex2 + 2.0;
	reg.extents.y2 = ey2 + 2.0;

	as->content = TRUE;
	damageScreenRegion (s, &reg);
    }
}

static void
annoDrawLine (CompScreen     *s,
	      double	     x1,
	      double	     y1,
	      double	     x2,
	      double	     y2,
	      double	     width,
	      unsigned short *color)
{
    REGION reg;
    cairo_t *cr;

    ANNO_SCREEN (s);

    cr = annoCairoContext (s);
    if (cr)
    {
	double ex1, ey1, ex2, ey2;

	cairo_set_line_width (cr, width);
	cairo_move_to (cr, x1, y1);
	cairo_line_to (cr, x2, y2);
	cairo_stroke_extents (cr, &ex1, &ey1, &ex2, &ey2);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	annoSetSourceColor (cr, color);
	cairo_stroke (cr);

	reg.rects    = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = ex1;
	reg.extents.y1 = ey1;
	reg.extents.x2 = ex2;
	reg.extents.y2 = ey2;

	as->content = TRUE;
	damageScreenRegion (s, &reg);
    }
}

static void
annoDrawText (CompScreen     *s,
	      double	     x,
	      double	     y,
	      char	     *text,
	      char	     *fontFamily,
	      double	     fontSize,
	      int	     fontSlant,
	      int	     fontWeight,
	      unsigned short *fillColor,
	      unsigned short *strokeColor,
	      double	     strokeWidth)
{
    REGION  reg;
    cairo_t *cr;

    ANNO_SCREEN (s);

    cr = annoCairoContext (s);
    if (cr)
    {
	cairo_text_extents_t extents;

	cairo_set_line_width (cr, strokeWidth);
	annoSetSourceColor (cr, fillColor);
	cairo_select_font_face (cr, fontFamily, fontSlant, fontWeight);
	cairo_set_font_size (cr, fontSize);
	cairo_text_extents (cr, text, &extents);
	cairo_save (cr);
	cairo_move_to (cr, x, y);
	cairo_text_path (cr, text);
	cairo_fill_preserve (cr);
	annoSetSourceColor (cr, strokeColor);
	cairo_stroke (cr);
	cairo_restore (cr);

	reg.rects    = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = x;
	reg.extents.y1 = y + extents.y_bearing - 2.0;
	reg.extents.x2 = x + extents.width + 20.0;
	reg.extents.y2 = y + extents.height;

	as->content = TRUE;
	damageScreenRegion (s, &reg);
    }
}

static Bool
annoDraw (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int		  nOption)
{
    CompScreen *s;
    Window     xid;

    xid  = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	cairo_t *cr;

	cr = annoCairoContext (s);
	if (cr)
	{
	    char	   *tool;
	    unsigned short *fillColor, *strokeColor;
	    double	   lineWidth, strokeWidth;

	    ANNO_DISPLAY (d);

	    tool = getStringOptionNamed (option, nOption, "tool", "line");

	    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

	    fillColor = ad->opt[ANNO_DISPLAY_OPTION_FILL_COLOR].value.c;
	    fillColor = getColorOptionNamed (option, nOption, "fill_color",
					     fillColor);

	    strokeColor = ad->opt[ANNO_DISPLAY_OPTION_STROKE_COLOR].value.c;
	    strokeColor = getColorOptionNamed (option, nOption,
					       "stroke_color", strokeColor);

	    strokeWidth = ad->strokeWidth;
	    strokeWidth = getFloatOptionNamed (option, nOption, "stroke_width",
					       strokeWidth);

	    lineWidth = ad->lineWidth;
	    lineWidth = getFloatOptionNamed (option, nOption, "line_width",
					     lineWidth);

	    if (strcasecmp (tool, "rectangle") == 0)
	    {
		double x, y, w, h;

		x = getFloatOptionNamed (option, nOption, "x", 0);
		y = getFloatOptionNamed (option, nOption, "y", 0);
		w = getFloatOptionNamed (option, nOption, "w", 100);
		h = getFloatOptionNamed (option, nOption, "h", 100);

		annoDrawRectangle (s, x, y, w, h, fillColor, strokeColor,
				   strokeWidth);
	    }
	    else if (strcasecmp (tool, "circle") == 0)
	    {
		double xc, yc, r;

		xc = getFloatOptionNamed (option, nOption, "xc", 0);
		yc = getFloatOptionNamed (option, nOption, "yc", 0);
		r  = getFloatOptionNamed (option, nOption, "radius", 100);

		annoDrawCircle (s, xc, yc, r, fillColor, strokeColor,
				strokeWidth);
	    }
	    else if (strcasecmp (tool, "line") == 0)
	    {
		double x1, y1, x2, y2;

		x1 = getFloatOptionNamed (option, nOption, "x1", 0);
		y1 = getFloatOptionNamed (option, nOption, "y1", 0);
		x2 = getFloatOptionNamed (option, nOption, "x2", 100);
		y2 = getFloatOptionNamed (option, nOption, "y2", 100);

		annoDrawLine (s, x1, y1, x2, y2, lineWidth, fillColor);
	    }
	    else if (strcasecmp (tool, "text") == 0)
	    {
		double	     x, y, size;
		char	     *text, *family;
		unsigned int slant, weight;
		char	     *str;

		str = getStringOptionNamed (option, nOption, "slant", "");
		if (strcasecmp (str, "oblique") == 0)
		    slant = CAIRO_FONT_SLANT_OBLIQUE;
		else if (strcasecmp (str, "italic") == 0)
		    slant = CAIRO_FONT_SLANT_ITALIC;
		else
		    slant = CAIRO_FONT_SLANT_NORMAL;

		str = getStringOptionNamed (option, nOption, "weight", "");
		if (strcasecmp (str, "bold") == 0)
		    weight = CAIRO_FONT_WEIGHT_BOLD;
		else
		    weight = CAIRO_FONT_WEIGHT_NORMAL;

		x      = getFloatOptionNamed (option, nOption, "x", 0);
		y      = getFloatOptionNamed (option, nOption, "y", 0);
		text   = getStringOptionNamed (option, nOption, "text", "");
		family = getStringOptionNamed (option, nOption, "family",
					       "Sans");
		size   = getFloatOptionNamed (option, nOption, "size", 36.0);

		annoDrawText (s, x, y, text, family, size, slant, weight,
			      fillColor, strokeColor, strokeWidth);
	    }
	}
    }

    return FALSE;
}

static Bool
annoInitiate (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	ANNO_SCREEN (s);

	if (otherScreenGrabExist (s, 0))
	    return FALSE;

	if (!as->grabIndex)
	    as->grabIndex = pushScreenGrab (s, None, "annotate");

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (state & CompActionStateInitKey)
	    action->state |= CompActionStateTermKey;

	annoLastPointerX = pointerX;
	annoLastPointerY = pointerY;

	as->eraseMode = FALSE;
    }

    return TRUE;
}

static Bool
annoTerminate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	ANNO_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (as->grabIndex)
	{
	    removeScreenGrab (s, as->grabIndex, NULL);
	    as->grabIndex = 0;
	}
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static Bool
annoEraseInitiate (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int	           nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	ANNO_SCREEN (s);

	if (otherScreenGrabExist (s, 0))
	    return FALSE;

	if (!as->grabIndex)
	    as->grabIndex = pushScreenGrab (s, None, "annotate");

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (state & CompActionStateInitKey)
	    action->state |= CompActionStateTermKey;

	annoLastPointerX = pointerX;
	annoLastPointerY = pointerY;

	as->eraseMode = TRUE;
    }

    return FALSE;
}

static Bool
annoClear (CompDisplay     *d,
	   CompAction      *action,
	   CompActionState state,
	   CompOption      *option,
	   int		   nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	ANNO_SCREEN (s);

	if (as->content)
	{
	    cairo_t *cr;

	    cr = annoCairoContext (s);
	    if (cr)
		annoCairoClear (s, as->cairo);

	    damageScreen (s);
	}

	return TRUE;
    }

    return FALSE;
}

static Bool
annoPaintOutput (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 CompOutput		 *output,
		 unsigned int		 mask)
{
    Bool status;

    ANNO_SCREEN (s);

    UNWRAP (as, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (as, s, paintOutput, annoPaintOutput);

    if (status && as->content && region->numRects)
    {
	BoxPtr pBox;
	int    nBox;

	glPushMatrix ();

	prepareXCoords (s, output, -DEFAULT_Z_CAMERA);

	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glEnable (GL_BLEND);

	enableTexture (s, &as->texture, COMP_TEXTURE_FILTER_FAST);

	pBox = region->rects;
	nBox = region->numRects;

	glBegin (GL_QUADS);

	while (nBox--)
	{
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x1),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y2));
	    glVertex2i (pBox->x1, pBox->y2);
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x2),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y2));
	    glVertex2i (pBox->x2, pBox->y2);
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x2),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y1));
	    glVertex2i (pBox->x2, pBox->y1);
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x1),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y1));
	    glVertex2i (pBox->x1, pBox->y1);

	    pBox++;
	}

	glEnd ();

	disableTexture (s, &as->texture);

	glDisable (GL_BLEND);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);

	glPopMatrix ();
    }

    return status;
}

static void
annoHandleMotionEvent (CompScreen *s,
		       int	  xRoot,
		       int	  yRoot)
{
    ANNO_SCREEN (s);

    if (as->grabIndex)
    {
	if (as->eraseMode)
	{
	    static unsigned short color[] = { 0, 0, 0, 0 };

	    annoDrawLine (s,
			  annoLastPointerX, annoLastPointerY,
			  xRoot, yRoot,
			  20.0, color);
	}
	else
	{
	    ANNO_DISPLAY(s->display);

	    annoDrawLine (s,
			  annoLastPointerX, annoLastPointerY,
			  xRoot, yRoot,
			  ad->lineWidth,
			  ad->opt[ANNO_DISPLAY_OPTION_FILL_COLOR].value.c);
	}

	annoLastPointerX = xRoot;
	annoLastPointerY = yRoot;
    }
}

static void
annoHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    ANNO_DISPLAY (d);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    annoHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    annoHandleMotionEvent (s, pointerX, pointerY);
    default:
	break;
    }

    UNWRAP (ad, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (ad, d, handleEvent, annoHandleEvent);
}

static const CompMetadataOptionInfo annoDisplayOptionInfo[] = {
    { "initiate_button", "button", 0, annoInitiate, annoTerminate },
    { "draw", "action", 0, annoDraw, 0 },
    { "erase_button", "button", 0, annoEraseInitiate, annoTerminate },
    { "clear_key", "key", 0, annoClear, 0 },
    { "clear_button", "button", 0, annoClear, 0 },
    { "fill_color", "color", 0, 0, 0 },
    { "stroke_color", "color", 0, 0, 0 }
};

static CompDisplayVTable annoDisplayObjectVTable = { { 0 } };

static CompBool
annoInitDisplay (CompDisplay *d)
{
    ANNO_DISPLAY (d);

    if (!compObjectCheckVersion (&d->u.base, "display", CORE_ABIVERSION))
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &annoMetadata,
					     annoDisplayOptionInfo,
					     ad->opt,
					     ANNO_DISPLAY_OPTION_NUM))
	return FALSE;

    ad->lineWidth   = 3.0;
    ad->strokeWidth = 1.0;

    if (!cObjectInterfaceInit (&d->u.base, &annoDisplayObjectVTable.base))
    {
	compFiniDisplayOptions (d, ad->opt, ANNO_DISPLAY_OPTION_NUM);
	return FALSE;
    }

    WRAP (ad, d, handleEvent, annoHandleEvent);

    return TRUE;
}

static void
annoFiniDisplay (CompDisplay *d)
{
    ANNO_DISPLAY (d);

    UNWRAP (ad, d, handleEvent);

    cObjectInterfaceFini (&d->u.base);

    compFiniDisplayOptions (d, ad->opt, ANNO_DISPLAY_OPTION_NUM);
}

static void
annoDisplayGetCContext (CompObject *object,
			CContext   *ctx)
{
    ANNO_DISPLAY (object);

    ctx->interface  = annoDisplayInterface;
    ctx->nInterface = N_ELEMENTS (annoDisplayInterface);
    ctx->type	    = NULL;
    ctx->data	    = (char *) ad;
    ctx->vtStore    = &ad->object;
    ctx->version    = COMPIZ_ANNOTATE_VERSION;
}

static CompObjectVTable annoScreenObjectVTable = { 0 };

static Bool
annoInitScreen (CompScreen *s)
{
    ANNO_SCREEN (s);

    as->grabIndex = 0;
    as->surface   = NULL;
    as->pixmap    = None;
    as->cairo     = NULL;
    as->content   = FALSE;

    initTexture (s, &as->texture);

    WRAP (as, s, paintOutput, annoPaintOutput);

    return TRUE;
}

static void
annoFiniScreen (CompScreen *s)
{
    ANNO_SCREEN (s);

    if (as->cairo)
	cairo_destroy (as->cairo);

    if (as->surface)
	cairo_surface_destroy (as->surface);

    finiTexture (s, &as->texture);

    if (as->pixmap)
	XFreePixmap (s->display->display, as->pixmap);

    UNWRAP (as, s, paintOutput);
}

static void
annoScreenGetCContext (CompObject *object,
		       CContext   *ctx)
{
    ANNO_SCREEN (GET_SCREEN (object));

    ctx->interface  = annoScreenInterface;
    ctx->nInterface = N_ELEMENTS (annoScreenInterface);
    ctx->type	    = NULL;
    ctx->data	    = (char *) as;
    ctx->vtStore    = &as->object;
    ctx->version    = COMPIZ_ANNOTATE_VERSION;
}

static CObjectPrivate annoObj[] = {
    C_OBJECT_PRIVATE ("display", anno, Display, AnnoDisplay, X, X),
    C_OBJECT_PRIVATE ("screen",  anno, Screen,  AnnoScreen,  X, X)
};

static Bool
annoInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&annoMetadata,
					 p->vTable->name,
					 annoDisplayOptionInfo,
					 ANNO_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    compAddMetadataFromFile (&annoMetadata, p->vTable->name);

    if (!cObjectInitPrivates (annoObj, N_ELEMENTS (annoObj)))
    {
	compFiniMetadata (&annoMetadata);
	return FALSE;
    }

    return TRUE;
}

static void
annoFini (CompPlugin *p)
{
    cObjectFiniPrivates (annoObj, N_ELEMENTS (annoObj));
    compFiniMetadata (&annoMetadata);
}

static CompPluginVTable annoVTable = {
    "annotate",
    0, /* GetMetadata */
    annoInit,
    annoFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &annoVTable;
}
