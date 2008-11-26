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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#include <compiz-core.h>
#include <decoration.h>

static CompMetadata moveMetadata;

struct _MoveKeys {
    char *name;
    int  dx;
    int  dy;
} mKeys[] = {
    { "Left",  -1,  0 },
    { "Right",  1,  0 },
    { "Up",     0, -1 },
    { "Down",   0,  1 }
};

#define NUM_KEYS (sizeof (mKeys) / sizeof (mKeys[0]))

#define KEY_MOVE_INC 24

#define SNAP_BACK 20
#define SNAP_OFF  100

typedef struct _MoveBox {
    decor_point_t p1;
    decor_point_t p2;
} MoveBox;

static int displayPrivateIndex;

#define MOVE_DISPLAY_OPTION_INITIATE_BUTTON   0
#define MOVE_DISPLAY_OPTION_INITIATE_KEY      1
#define MOVE_DISPLAY_OPTION_OPACITY	      2
#define MOVE_DISPLAY_OPTION_CONSTRAIN_Y	      3
#define MOVE_DISPLAY_OPTION_SNAPOFF_MAXIMIZED 4
#define MOVE_DISPLAY_OPTION_LAZY_POSITIONING  5
#define MOVE_DISPLAY_OPTION_NUM		      6

typedef struct _MoveDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[MOVE_DISPLAY_OPTION_NUM];

    CompWindow *w;
    int	       savedX;
    int	       savedY;
    int	       x;
    int	       y;
    Region     region;
    int        status;
    KeyCode    key[NUM_KEYS];

    Atom moveAtom;

    int releaseButton;

    GLushort moveOpacity;
} MoveDisplay;

typedef struct _MoveScreen {
    int windowPrivateIndex;

    PaintWindowProc paintWindow;

    int grabIndex;

    Cursor moveCursor;

    unsigned int origState;

    int	snapOffY;
    int	snapBackY;
} MoveScreen;

typedef struct _MoveWindow {
    int     button;
    MoveBox *box;
    int     nBox;
} MoveWindow;

#define GET_MOVE_DISPLAY(d)					  \
    ((MoveDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define MOVE_DISPLAY(d)		           \
    MoveDisplay *md = GET_MOVE_DISPLAY (d)

#define GET_MOVE_SCREEN(s, md)					      \
    ((MoveScreen *) (s)->base.privates[(md)->screenPrivateIndex].ptr)

#define MOVE_SCREEN(s)						        \
    MoveScreen *ms = GET_MOVE_SCREEN (s, GET_MOVE_DISPLAY (s->display))

#define GET_MOVE_WINDOW(w, ms)					      \
    ((MoveWindow *) (w)->base.privates[(ms)->windowPrivateIndex].ptr)

#define MOVE_WINDOW(w)					     \
    MoveWindow *mw = GET_MOVE_WINDOW  (w,		     \
		     GET_MOVE_SCREEN  (w->screen,	     \
		     GET_MOVE_DISPLAY (w->screen->display)))


#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))


static Bool
movePointInBoxes (int     x,
		  int     y,
		  MoveBox *box,
		  int	  nBox,
		  int	  width,
		  int	  height)
{
    int x0, y0, x1, y1;

    while (nBox--)
    {
	decor_apply_gravity (box->p1.gravity, box->p1.x, box->p1.y,
			     width, height,
			     &x0, &y0);

	decor_apply_gravity (box->p2.gravity, box->p2.x, box->p2.y,
			     width, height,
			     &x1, &y1);

	if (x >= x0 && x < x1 && y >= y0 && y < y1)
	    return TRUE;

	box++;
    }

    return FALSE;
}

static void
moveWindowUpdate (CompWindow *w)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *propData;
    MoveBox	  *box = NULL;
    int		  nBox = 0;

    MOVE_DISPLAY (w->screen->display);
    MOVE_WINDOW (w);

    result = XGetWindowProperty (w->screen->display->display, w->id,
				 md->moveAtom, 0L, 8192L, FALSE,
				 XA_INTEGER, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData)
    {
	if (n >= 2)
	{
	    long *data = (long *) propData;

	    mw->button = data[1];

	    nBox = (n - 2) / 6;
	    if (nBox)
	    {
		box = malloc (sizeof (MoveBox) * nBox);
		if (box)
		{
		    int i;

		    data += 2;

		    for (i = 0; i < nBox; i++)
		    {
			box[i].p1.gravity = *data++;
			box[i].p1.x       = *data++;
			box[i].p1.y       = *data++;
			box[i].p2.gravity = *data++;
			box[i].p2.x       = *data++;
			box[i].p2.y       = *data++;
		    }
		}
	    }
	}

	XFree (propData);
    }

    if (mw->box)
	free (mw->box);

    mw->box  = box;
    mw->nBox = nBox;
}

static Bool
moveInitiate (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    CompWindow *w;
    Window     xid;

    MOVE_DISPLAY (d);

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionMoveMask))
    {
	CompWindow   *p;
	XRectangle   workArea;
	unsigned int mods;
	int          x, y, button;

	MOVE_SCREEN (w->screen);

	x = w->width / 2;
	y = w->height / 2;
	for (p = w; p; p = p->parent)
	{
	    x += p->attrib.x;
	    y += p->attrib.y;
	}

	mods = getIntOptionNamed (option, nOption, "modifiers", 0);

	x = getIntOptionNamed (option, nOption, "x", x);
	y = getIntOptionNamed (option, nOption, "y", y);

	button = getIntOptionNamed (option, nOption, "button", -1);

	if (otherScreenGrabExist (w->screen, "move", 0))
	    return FALSE;

	if (md->w)
	    return FALSE;

	if (w->type & (CompWindowTypeDesktopMask |
		       CompWindowTypeDockMask    |
		       CompWindowTypeFullscreenMask))
	    return FALSE;

	if (w->attrib.override_redirect)
	    return FALSE;

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (md->region)
	{
	    XDestroyRegion (md->region);
	    md->region = NULL;
	}

	md->status = RectangleOut;

	md->savedX = w->serverX;
	md->savedY = w->serverY;

	md->x = 0;
	md->y = 0;

	lastPointerX = x;
	lastPointerY = y;

	ms->origState = w->state;

	getWorkareaForOutput (w->screen,
			      outputDeviceForWindow (w),
			      &workArea);

	ms->snapBackY = w->serverY - workArea.y;
	ms->snapOffY  = y - workArea.y;

	if (!ms->grabIndex)
	    ms->grabIndex = pushScreenGrab (w->screen, ms->moveCursor, "move");

	if (ms->grabIndex)
	{
	    md->w = w;

	    md->releaseButton = button;

	    for (p = w->parent; p; p = p->parent)
	    {
		x -= p->attrib.x;
		y -= p->attrib.y;
	    }

	    (w->screen->windowGrabNotify) (w, x, y, mods,
					   CompWindowGrabMoveMask |
					   CompWindowGrabButtonMask);

	    if (state & CompActionStateInitKey)
	    {
		int xRoot, yRoot;

		xRoot = w->attrib.x + (w->width  / 2);
		yRoot = w->attrib.y + (w->height / 2);

		warpPointer (w->screen, xRoot - pointerX, yRoot - pointerY);
	    }

	    if (md->moveOpacity != OPAQUE)
		addWindowDamage (w);
	}
    }

    return FALSE;
}

static Bool
moveTerminate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    MOVE_DISPLAY (d);

    if (md->w)
    {
	MOVE_SCREEN (md->w->screen);

	if (state & CompActionStateCancel)
	    moveWindow (md->w,
			md->savedX - md->w->attrib.x,
			md->savedY - md->w->attrib.y,
			TRUE, FALSE);

	syncWindowPosition (md->w);

	/* update window attributes as window constraints may have
	   changed - needed e.g. if a maximized window was moved
	   to another output device */
	updateWindowAttributes (md->w, CompStackingUpdateModeNone);

	(md->w->screen->windowUngrabNotify) (md->w);

	if (ms->grabIndex)
	{
	    removeScreenGrab (md->w->screen, ms->grabIndex, NULL);
	    ms->grabIndex = 0;
	}

	if (md->moveOpacity != OPAQUE)
	    addWindowDamage (md->w);

	md->w             = 0;
	md->releaseButton = 0;
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

/* creates a region containing top and bottom struts. only struts that are
   outside the screen workarea are considered. */
static Region
moveGetYConstrainRegion (CompScreen *s)
{
    CompWindow *w;
    Region     region;
    REGION     r;
    XRectangle workArea;
    BoxRec     extents;
    int	       i;

    region = XCreateRegion ();
    if (!region)
	return NULL;

    r.rects    = &r.extents;
    r.numRects = r.size = 1;

    r.extents.x1 = MINSHORT;
    r.extents.y1 = 0;
    r.extents.x2 = 0;
    r.extents.y2 = s->height;

    XUnionRegion (&r, region, region);

    r.extents.x1 = s->width;
    r.extents.x2 = MAXSHORT;

    XUnionRegion (&r, region, region);

    for (i = 0; i < s->nOutputDev; i++)
    {
	XUnionRegion (&s->outputDev[i].region, region, region);

	getWorkareaForOutput (s, i, &workArea);
	extents = s->outputDev[i].region.extents;

	for (w = s->root.windows; w; w = w->next)
	{
	    if (!w->mapNum)
		continue;

	    if (w->struts)
	    {
		r.extents.x1 = w->struts->top.x;
		r.extents.y1 = w->struts->top.y;
		r.extents.x2 = r.extents.x1 + w->struts->top.width;
		r.extents.y2 = r.extents.y1 + w->struts->top.height;

		if (r.extents.x1 < extents.x1)
		    r.extents.x1 = extents.x1;
		if (r.extents.x2 > extents.x2)
		    r.extents.x2 = extents.x2;
		if (r.extents.y1 < extents.y1)
		    r.extents.y1 = extents.y1;
		if (r.extents.y2 > extents.y2)
		    r.extents.y2 = extents.y2;

		if (r.extents.x1 < r.extents.x2 && r.extents.y1 < r.extents.y2)
		{
		    if (r.extents.y2 <= workArea.y)
			XSubtractRegion (region, &r, region);
		}

		r.extents.x1 = w->struts->bottom.x;
		r.extents.y1 = w->struts->bottom.y;
		r.extents.x2 = r.extents.x1 + w->struts->bottom.width;
		r.extents.y2 = r.extents.y1 + w->struts->bottom.height;

		if (r.extents.x1 < extents.x1)
		    r.extents.x1 = extents.x1;
		if (r.extents.x2 > extents.x2)
		    r.extents.x2 = extents.x2;
		if (r.extents.y1 < extents.y1)
		    r.extents.y1 = extents.y1;
		if (r.extents.y2 > extents.y2)
		    r.extents.y2 = extents.y2;

		if (r.extents.x1 < r.extents.x2 && r.extents.y1 < r.extents.y2)
		{
		    if (r.extents.y1 >= (workArea.y + workArea.height))
			XSubtractRegion (region, &r, region);
		}
	    }
	}
    }

    return region;
}

static void
moveHandleMotionEvent (CompScreen *s,
		       int	  xRoot,
		       int	  yRoot)
{
    MOVE_SCREEN (s);

    if (ms->grabIndex)
    {
	CompWindow *w;
	int	   dx, dy;
	int	   wX, wY;
	int	   wWidth, wHeight;

	MOVE_DISPLAY (s->display);

	w = md->w;

	wX      = w->serverX;
	wY      = w->serverY;
	wWidth  = w->serverWidth  + w->serverBorderWidth * 2;
	wHeight = w->serverHeight + w->serverBorderWidth * 2;

	md->x += xRoot - lastPointerX;
	md->y += yRoot - lastPointerY;

	if (w->type & CompWindowTypeFullscreenMask)
	{
	    dx = dy = 0;
	}
	else
	{
	    XRectangle workArea;
	    int	       min, max;

	    dx = md->x;
	    dy = md->y;

	    getWorkareaForOutput (s,
				  outputDeviceForWindow (w),
				  &workArea);

	    if (md->opt[MOVE_DISPLAY_OPTION_CONSTRAIN_Y].value.b)
	    {
		if (!md->region)
		    md->region = moveGetYConstrainRegion (s);

		/* make sure that the top frame extents or the top row of
		   pixels are within what is currently our valid screen
		   region */
		if (md->region)
		{
		    int x, y, width, height;
		    int status;

		    x	   = wX + dx - w->input.left;
		    y	   = wY + dy - w->input.top;
		    width  = wWidth + w->input.left + w->input.right;
		    height = w->input.top ? w->input.top : 1;

		    status = XRectInRegion (md->region, x, y, width, height);

		    /* only constrain movement if previous position was valid */
		    if (md->status == RectangleIn)
		    {
			int xStatus = status;

			while (dx && xStatus != RectangleIn)
			{
			    xStatus = XRectInRegion (md->region,
						     x, y - dy,
						     width, height);

			    if (xStatus != RectangleIn)
				dx += (dx < 0) ? 1 : -1;

			    x = wX + dx - w->input.left;
			}

			while (dy && status != RectangleIn)
			{
			    status = XRectInRegion (md->region,
						    x, y,
						    width, height);

			    if (status != RectangleIn)
				dy += (dy < 0) ? 1 : -1;

			    y = wY + dy - w->input.top;
			}
		    }
		    else
		    {
			md->status = status;
		    }
		}
	    }

	    if (md->opt[MOVE_DISPLAY_OPTION_SNAPOFF_MAXIMIZED].value.b)
	    {
		if (w->state & CompWindowStateMaximizedVertMask)
		{
		    if (abs ((yRoot - workArea.y) - ms->snapOffY) >= SNAP_OFF)
		    {
			if (!otherScreenGrabExist (s, "move", 0))
			{
			    int width = w->serverWidth;

			    w->saveMask |= CWX | CWY;

			    if (w->saveMask & CWWidth)
				width = w->saveWc.width;

			    w->saveWc.x = xRoot - (width >> 1);
			    w->saveWc.y = yRoot + (w->input.top >> 1);

			    md->x = md->y = 0;

			    maximizeWindow (w, 0);

			    ms->snapOffY = ms->snapBackY;

			    return;
			}
		    }
		}
		else if (ms->origState & CompWindowStateMaximizedVertMask)
		{
		    if (abs ((yRoot - workArea.y) - ms->snapBackY) < SNAP_BACK)
		    {
			if (!otherScreenGrabExist (s, "move", 0))
			{
			    int wy;

			    /* update server position before maximizing
			       window again so that it is maximized on
			       correct output */
			    syncWindowPosition (w);

			    maximizeWindow (w, ms->origState);

			    wy  = workArea.y + (w->input.top >> 1);
			    wy += w->sizeHints.height_inc >> 1;

			    warpPointer (s, 0, wy - pointerY);

			    return;
			}
		    }
		}
	    }

	    if (w->state & CompWindowStateMaximizedVertMask)
	    {
		min = workArea.y + w->input.top;
		max = workArea.y + workArea.height - w->input.bottom - wHeight;

		if (wY + dy < min)
		    dy = min - wY;
		else if (wY + dy > max)
		    dy = max - wY;
	    }

	    if (w->state & CompWindowStateMaximizedHorzMask)
	    {
		if (wX > s->width || wX + w->width < 0)
		    return;

		if (wX + wWidth < 0)
		    return;

		min = workArea.x + w->input.left;
		max = workArea.x + workArea.width - w->input.right - wWidth;

		if (wX + dx < min)
		    dx = min - wX;
		else if (wX + dx > max)
		    dx = max - wX;
	    }
	}

	if (dx || dy)
	{
	    Bool lazy = md->opt[MOVE_DISPLAY_OPTION_LAZY_POSITIONING].value.b;

	    moveWindow (w,
			wX + dx - w->attrib.x,
			wY + dy - w->attrib.y,
			TRUE, FALSE);

	    /* lazy positioning cannot be used without manual compositing */
	    if (!manualCompositeManagement)
		lazy = FALSE;

	    if (lazy)
	    {
		/* FIXME: This form of lazy positioning is broken and should
		   be replaced asap. Current code exists just to avoid a
		   major performance regression in the 0.5.2 release. */
		w->serverX = w->attrib.x;
		w->serverY = w->attrib.y;
	    }
	    else
	    {
		syncWindowPosition (w);
	    }

	    md->x -= dx;
	    md->y -= dy;
	}
    }
}

static void
moveHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;
    CompWindow *w;

    MOVE_DISPLAY (d);

    switch (event->type) {
    case ButtonPress:
	w = findClientWindowAtDisplay (d, event->xbutton.window);
	if (w)
	{
	    CompWindow *p;
	    int        option = MOVE_DISPLAY_OPTION_INITIATE_BUTTON;
	    int        x = event->xbutton.x_root;
	    int        y = event->xbutton.y_root;

	    MOVE_WINDOW (w);

	    for (p = w; p; p = p->parent)
	    {
		x -= p->attrib.x;
		y -= p->attrib.y;
	    }

	    if (event->xbutton.button == mw->button &&
		movePointInBoxes (x, y,
				  mw->box, mw->nBox,
				  w->width, w->height))
	    {
		CompOption o[5];

		o[0].type    = CompOptionTypeInt;
		o[0].name    = "window";
		o[0].value.i = w->id;

		o[1].type    = CompOptionTypeInt;
		o[1].name    = "modifiers";
		o[1].value.i = event->xbutton.state;

		o[2].type    = CompOptionTypeInt;
		o[2].name    = "x";
		o[2].value.i = event->xbutton.x_root;

		o[3].type    = CompOptionTypeInt;
		o[3].name    = "y";
		o[3].value.i = event->xbutton.y_root;

		o[4].type    = CompOptionTypeInt;
		o[4].name    = "button";
		o[4].value.i = event->xbutton.button;

		moveInitiate (d,
			      &md->opt[option].value.action,
			      CompActionStateInitButton,
			      o, 5);
	    }
	}
	break;
    case ButtonRelease:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    MOVE_SCREEN (s);

	    if (ms->grabIndex)
	    {
		if (md->releaseButton == -1 ||
		    md->releaseButton == event->xbutton.button)
		{
		    CompAction *action;
		    int        opt = MOVE_DISPLAY_OPTION_INITIATE_BUTTON;

		    action = &md->opt[opt].value.action;
		    moveTerminate (d, action, CompActionStateTermButton,
				   NULL, 0);
		}
	    }
	}
	break;
    case KeyPress:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    MOVE_SCREEN (s);

	    if (ms->grabIndex)
	    {
		int i;

		for (i = 0; i < NUM_KEYS; i++)
		{
		    if (event->xkey.keycode == md->key[i])
		    {
			XWarpPointer (d->display, None, None, 0, 0, 0, 0,
				      mKeys[i].dx * KEY_MOVE_INC,
				      mKeys[i].dy * KEY_MOVE_INC);
			break;
		    }
		}
	    }
	}
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    moveHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    moveHandleMotionEvent (s, pointerX, pointerY);
	break;
    case ClientMessage:
	if (event->xclient.message_type == d->wmMoveResizeAtom)
	{
	    if (event->xclient.data.l[2] == WmMoveResizeMove ||
		event->xclient.data.l[2] == WmMoveResizeMoveKeyboard)
	    {
		w = findWindowAtDisplay (d, event->xclient.window);
		if (w)
		{
		    CompOption o[5];
		    int	       xRoot, yRoot;
		    int	       option;

		    o[0].type    = CompOptionTypeInt;
		    o[0].name    = "window";
		    o[0].value.i = event->xclient.window;

		    if (event->xclient.data.l[2] == WmMoveResizeMoveKeyboard)
		    {
			option = MOVE_DISPLAY_OPTION_INITIATE_KEY;

			moveInitiate (d, &md->opt[option].value.action,
				      CompActionStateInitKey,
				      o, 1);
		    }
		    else
		    {
			unsigned int mods;
			Window	     root, child;
			int	     i;

			option = MOVE_DISPLAY_OPTION_INITIATE_BUTTON;

			XQueryPointer (d->display, w->screen->root.id,
				       &root, &child, &xRoot, &yRoot,
				       &i, &i, &mods);

			/* TODO: not only button 1 */
			if (mods & Button1Mask)
			{
			    o[1].type	 = CompOptionTypeInt;
			    o[1].name	 = "modifiers";
			    o[1].value.i = mods;

			    o[2].type	 = CompOptionTypeInt;
			    o[2].name	 = "x";
			    o[2].value.i = event->xclient.data.l[0];

			    o[3].type	 = CompOptionTypeInt;
			    o[3].name	 = "y";
			    o[3].value.i = event->xclient.data.l[1];

			    o[4].type    = CompOptionTypeInt;
			    o[4].name    = "button";
			    o[4].value.i = event->xclient.data.l[3] ?
				           event->xclient.data.l[3] : -1;

			    moveInitiate (d,
					  &md->opt[option].value.action,
					  CompActionStateInitButton,
					  o, 5);

			    moveHandleMotionEvent (w->screen, xRoot, yRoot);
			}
		    }
		}
	    }
	    else if (md->w && event->xclient.data.l[2] == WmMoveResizeCancel)
	    {
		if (md->w->id == event->xclient.window)
		{
		    int option;

		    option = MOVE_DISPLAY_OPTION_INITIATE_BUTTON;
		    moveTerminate (d,
				   &md->opt[option].value.action,
				   CompActionStateCancel, NULL, 0);
		    option = MOVE_DISPLAY_OPTION_INITIATE_KEY;
		    moveTerminate (d,
				   &md->opt[option].value.action,
				   CompActionStateCancel, NULL, 0);
		}
	    }
	}
	break;
    case DestroyNotify:
	if (md->w && md->w->id == event->xdestroywindow.window)
	{
	    int option;

	    option = MOVE_DISPLAY_OPTION_INITIATE_BUTTON;
	    moveTerminate (d,
			   &md->opt[option].value.action,
			   0, NULL, 0);
	    option = MOVE_DISPLAY_OPTION_INITIATE_KEY;
	    moveTerminate (d,
			   &md->opt[option].value.action,
			   0, NULL, 0);
	}
	break;
    case UnmapNotify:
	if (md->w && md->w->id == event->xunmap.window)
	{
	    int option;

	    option = MOVE_DISPLAY_OPTION_INITIATE_BUTTON;
	    moveTerminate (d,
			   &md->opt[option].value.action,
			   0, NULL, 0);
	    option = MOVE_DISPLAY_OPTION_INITIATE_KEY;
	    moveTerminate (d,
			   &md->opt[option].value.action,
			   0, NULL, 0);
	}
    default:
	break;
    }

    UNWRAP (md, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (md, d, handleEvent, moveHandleEvent);

    if (event->type == PropertyNotify)
    {
	if (event->xproperty.atom == md->moveAtom)
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		moveWindowUpdate (w);
	}
    }
}

static Bool
movePaintWindow (CompWindow		 *w,
		 const WindowPaintAttrib *attrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 unsigned int		 mask)
{
    WindowPaintAttrib sAttrib;
    CompScreen	      *s = w->screen;
    Bool	      status;

    MOVE_SCREEN (s);

    if (ms->grabIndex)
    {
	MOVE_DISPLAY (s->display);

	if (md->w == w && md->moveOpacity != OPAQUE)
	{
	    /* modify opacity of windows that are not active */
	    sAttrib = *attrib;
	    attrib  = &sAttrib;

	    sAttrib.opacity = (sAttrib.opacity * md->moveOpacity) >> 16;
	}
    }

    UNWRAP (ms, s, paintWindow);
    status = (*s->paintWindow) (w, attrib, transform, region, mask);
    WRAP (ms, s, paintWindow, movePaintWindow);

    return status;
}

static CompOption *
moveGetDisplayOptions (CompPlugin  *plugin,
		       CompDisplay *display,
		       int	   *count)
{
    MOVE_DISPLAY (display);

    *count = NUM_OPTIONS (md);
    return md->opt;
}

static Bool
moveSetDisplayOption (CompPlugin      *plugin,
		      CompDisplay     *display,
		      const char      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    MOVE_DISPLAY (display);

    o = compFindOption (md->opt, NUM_OPTIONS (md), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case MOVE_DISPLAY_OPTION_OPACITY:
	if (compSetIntOption (o, value))
	{
	    md->moveOpacity = (o->value.i * OPAQUE) / 100;
	    return TRUE;
	}
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo moveDisplayOptionInfo[] = {
    { "initiate_button", "button", 0, moveInitiate, moveTerminate },
    { "initiate_key", "key", 0, moveInitiate, moveTerminate },
    { "opacity", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "constrain_y", "bool", 0, 0, 0 },
    { "snapoff_maximized", "bool", 0, 0, 0 },
    { "lazy_positioning", "bool", 0, 0, 0 }
};

static Bool
moveInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    MoveDisplay *md;
    int	        i;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    md = malloc (sizeof (MoveDisplay));
    if (!md)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &moveMetadata,
					     moveDisplayOptionInfo,
					     md->opt,
					     MOVE_DISPLAY_OPTION_NUM))
    {
	free (md);
	return FALSE;
    }

    md->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (md->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, md->opt, MOVE_DISPLAY_OPTION_NUM);
	free (md);
	return FALSE;
    }

    md->moveOpacity =
	(md->opt[MOVE_DISPLAY_OPTION_OPACITY].value.i * OPAQUE) / 100;

    md->w             = 0;
    md->region        = NULL;
    md->status        = RectangleOut;
    md->releaseButton = 0;

    for (i = 0; i < NUM_KEYS; i++)
	md->key[i] = XKeysymToKeycode (d->display,
				       XStringToKeysym (mKeys[i].name));

    md->moveAtom = XInternAtom (d->display, "_COMPIZ_WM_WINDOW_MOVE_DECOR", 0);

    WRAP (md, d, handleEvent, moveHandleEvent);

    d->base.privates[displayPrivateIndex].ptr = md;

    return TRUE;
}

static void
moveFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    MOVE_DISPLAY (d);

    freeScreenPrivateIndex (d, md->screenPrivateIndex);

    UNWRAP (md, d, handleEvent);

    compFiniDisplayOptions (d, md->opt, MOVE_DISPLAY_OPTION_NUM);

    free (md);
}

static Bool
moveInitScreen (CompPlugin *p,
		CompScreen *s)
{
    MoveScreen *ms;

    MOVE_DISPLAY (s->display);

    ms = malloc (sizeof (MoveScreen));
    if (!ms)
	return FALSE;

    ms->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ms->windowPrivateIndex < 0)
    {
	free (ms);
	return FALSE;
    }

    ms->grabIndex = 0;

    ms->moveCursor = XCreateFontCursor (s->display->display, XC_fleur);

    WRAP (ms, s, paintWindow, movePaintWindow);

    s->base.privates[md->screenPrivateIndex].ptr = ms;

    return TRUE;
}

static void
moveFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    MOVE_SCREEN (s);

    UNWRAP (ms, s, paintWindow);

    if (ms->moveCursor)
	XFreeCursor (s->display->display, ms->moveCursor);

    freeWindowPrivateIndex (s, ms->windowPrivateIndex);

    free (ms);
}

static Bool
moveInitWindow (CompPlugin *p,
		CompWindow *w)
{
    MoveWindow *mw;

    MOVE_SCREEN (w->screen);

    mw = malloc (sizeof (MoveWindow));
    if (!mw)
	return FALSE;

    mw->button = 0;
    mw->box    = NULL;
    mw->nBox   = 0;

    w->base.privates[ms->windowPrivateIndex].ptr = mw;

    moveWindowUpdate (w);

    return TRUE;
}

static void
moveFiniWindow (CompPlugin *p,
		CompWindow *w)
{
    MOVE_WINDOW (w);

    if (mw->box)
	free (mw->box);

    free (mw);
}

static CompBool
moveInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) moveInitDisplay,
	(InitPluginObjectProc) moveInitScreen,
	(InitPluginObjectProc) moveInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
moveFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) moveFiniDisplay,
	(FiniPluginObjectProc) moveFiniScreen,
	(FiniPluginObjectProc) moveFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
moveGetObjectOptions (CompPlugin *plugin,
		      CompObject *object,
		      int	 *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) moveGetDisplayOptions
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) (*count = 0), (plugin, object, count));
}

static CompBool
moveSetObjectOption (CompPlugin      *plugin,
		     CompObject      *object,
		     const char      *name,
		     CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) moveSetDisplayOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
moveInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&moveMetadata,
					 p->vTable->name,
					 moveDisplayOptionInfo,
					 MOVE_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&moveMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&moveMetadata, p->vTable->name);

    return TRUE;
}

static void
moveFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&moveMetadata);
}

static CompMetadata *
moveGetMetadata (CompPlugin *plugin)
{
    return &moveMetadata;
}

CompPluginVTable moveVTable = {
    "move",
    moveGetMetadata,
    moveInit,
    moveFini,
    moveInitObject,
    moveFiniObject,
    moveGetObjectOptions,
    moveSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &moveVTable;
}
