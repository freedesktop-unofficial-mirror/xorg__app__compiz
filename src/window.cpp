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

#include <compiz.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/extensions/shape.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <boost/bind.hpp>

#include <compiz-core.h>
#include <compicon.h>
#include <core/atoms.h>
#include "privatewindow.h"
#include "privatescreen.h"


CompPrivateStorage::Indices windowPrivateIndices (0);

int
CompWindow::allocPrivateIndex ()
{
    int i = CompPrivateStorage::allocatePrivateIndex (&windowPrivateIndices);
    foreach (CompWindow *w, ::screen->windows ())
	if (windowPrivateIndices.size () != w->privates.size ())
	    w->privates.resize (windowPrivateIndices.size ());
    return i;
}

void
CompWindow::freePrivateIndex (int index)
{
    CompPrivateStorage::freePrivateIndex (&windowPrivateIndices, index);
    foreach (CompWindow *w, ::screen->windows ())
	if (windowPrivateIndices.size () != w->privates.size ())
	    w->privates.resize (windowPrivateIndices.size ());
}

bool
PrivateWindow::isAncestorTo (CompWindow *transient,
			     CompWindow *ancestor)
{
    if (transient->priv->transientFor)
    {
	if (transient->priv->transientFor == ancestor->priv->id)
	    return true;

	transient = screen->findWindow (transient->priv->transientFor);
	if (transient)
	    return isAncestorTo (transient, ancestor);
    }

    return false;
}

void
PrivateWindow::recalcNormalHints ()
{
    int maxSize;

#warning fixme to max Texture size
    maxSize = MAXSHORT;
    maxSize -= serverGeometry.border () * 2;

    sizeHints.x      = serverGeometry.x ();
    sizeHints.y      = serverGeometry.y ();
    sizeHints.width  = serverGeometry.width ();
    sizeHints.height = serverGeometry.height ();

    if (!(sizeHints.flags & PBaseSize))
    {
	if (sizeHints.flags & PMinSize)
	{
	    sizeHints.base_width  = sizeHints.min_width;
	    sizeHints.base_height = sizeHints.min_height;
	}
	else
	{
	    sizeHints.base_width  = 0;
	    sizeHints.base_height = 0;
	}

	sizeHints.flags |= PBaseSize;
    }

    if (!(sizeHints.flags & PMinSize))
    {
	sizeHints.min_width  = sizeHints.base_width;
	sizeHints.min_height = sizeHints.base_height;
	sizeHints.flags |= PMinSize;
    }

    if (!(sizeHints.flags & PMaxSize))
    {
	sizeHints.max_width  = 65535;
	sizeHints.max_height = 65535;
	sizeHints.flags |= PMaxSize;
    }

    if (sizeHints.max_width < sizeHints.min_width)
	sizeHints.max_width = sizeHints.min_width;

    if (sizeHints.max_height < sizeHints.min_height)
	sizeHints.max_height = sizeHints.min_height;

    if (sizeHints.min_width < 1)
	sizeHints.min_width = 1;

    if (sizeHints.max_width < 1)
	sizeHints.max_width = 1;

    if (sizeHints.min_height < 1)
	sizeHints.min_height = 1;

    if (sizeHints.max_height < 1)
	sizeHints.max_height = 1;

    if (sizeHints.max_width > maxSize)
	sizeHints.max_width = maxSize;

    if (sizeHints.max_height > maxSize)
	sizeHints.max_height = maxSize;

    if (sizeHints.min_width > maxSize)
	sizeHints.min_width = maxSize;

    if (sizeHints.min_height > maxSize)
	sizeHints.min_height = maxSize;

    if (sizeHints.base_width > maxSize)
	sizeHints.base_width = maxSize;

    if (sizeHints.base_height > maxSize)
	sizeHints.base_height = maxSize;

    if (sizeHints.flags & PResizeInc)
    {
	if (sizeHints.width_inc == 0)
	    sizeHints.width_inc = 1;

	if (sizeHints.height_inc == 0)
	    sizeHints.height_inc = 1;
    }
    else
    {
	sizeHints.width_inc  = 1;
	sizeHints.height_inc = 1;
	sizeHints.flags |= PResizeInc;
    }

    if (sizeHints.flags & PAspect)
    {
	/* don't divide by 0 */
	if (sizeHints.min_aspect.y < 1)
	    sizeHints.min_aspect.y = 1;

	if (sizeHints.max_aspect.y < 1)
	    sizeHints.max_aspect.y = 1;
    }
    else
    {
	sizeHints.min_aspect.x = 1;
	sizeHints.min_aspect.y = 65535;
	sizeHints.max_aspect.x = 65535;
	sizeHints.max_aspect.y = 1;
	sizeHints.flags |= PAspect;
    }

    if (!(sizeHints.flags & PWinGravity))
    {
	sizeHints.win_gravity = NorthWestGravity;
	sizeHints.flags |= PWinGravity;
    }
}

void
PrivateWindow::updateNormalHints ()
{
    Status status;
    long   supplied;

    status = XGetWMNormalHints (screen->dpy (), priv->id,
				&priv->sizeHints, &supplied);

    if (!status)
	priv->sizeHints.flags = 0;

    priv->recalcNormalHints ();
}

void
PrivateWindow::updateWmHints ()
{
    XWMHints *hints;

    hints = XGetWMHints (screen->dpy (), priv->id);
    if (hints)
    {
	if (hints->flags & InputHint)
	    priv->inputHint = hints->input;

	XFree (hints);
    }
}

void
PrivateWindow::updateClassHints ()
{
    XClassHint classHint;
    int	       status;

    if (priv->resName)
    {
	free (priv->resName);
	priv->resName = NULL;
    }

    if (priv->resClass)
    {
	free (priv->resClass);
	priv->resClass = NULL;
    }

    status = XGetClassHint (screen->dpy (),
			    priv->id, &classHint);
    if (status)
    {
	if (classHint.res_name)
	{
	    priv->resName = strdup (classHint.res_name);
	    XFree (classHint.res_name);
	}

	if (classHint.res_class)
	{
	    priv->resClass = strdup (classHint.res_class);
	    XFree (classHint.res_class);
	}
    }
}

void
PrivateWindow::updateTransientHint ()
{
    Window transientFor;
    Status status;

    priv->transientFor = None;

    status = XGetTransientForHint (screen->dpy (),
				   priv->id, &transientFor);

    if (status)
    {
	CompWindow *ancestor;

	ancestor = screen->findWindow (transientFor);
	if (!ancestor)
	    return;

	/* protect against circular transient dependencies */
	if (transientFor == priv->id ||
	    PrivateWindow::isAncestorTo (ancestor, window))
	    return;

	priv->transientFor = transientFor;
    }
}

void
PrivateWindow::updateIconGeometry ()
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    result = XGetWindowProperty (screen->dpy (), priv->id,
				 Atoms::wmIconGeometry,
				 0L, 1024L, False, XA_CARDINAL,
				 &actual, &format, &n, &left, &data);

    priv->iconGeometrySet = false;

    if (result == Success && data)
    {
	if (n == 4)
	{
	    unsigned long *geometry = (unsigned long *) data;

	    priv->iconGeometry.x      = geometry[0];
	    priv->iconGeometry.y      = geometry[1];
	    priv->iconGeometry.width  = geometry[2];
	    priv->iconGeometry.height = geometry[3];

	    priv->iconGeometrySet = TRUE;
	}

	XFree (data);
    }
}

Window
PrivateWindow::getClientLeaderOfAncestor ()
{
    if (transientFor)
    {
	CompWindow *w = screen->findWindow (transientFor);
	if (w)
	{
	    if (w->priv->clientLeader)
		return w->priv->clientLeader;

	    return w->priv->getClientLeaderOfAncestor ();
	}
    }

    return None;
}

Window
PrivateWindow::getClientLeader ()
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    result = XGetWindowProperty (screen->dpy (), priv->id,
				 Atoms::wmClientLeader,
				 0L, 1L, False, XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	Window win;

	memcpy (&win, data, sizeof (Window));
	XFree ((void *) data);

	if (win)
	    return win;
    }

    return priv->getClientLeaderOfAncestor ();
}

char *
PrivateWindow::getStartupId ()
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    result = XGetWindowProperty (screen->dpy (), priv->id,
				 Atoms::startupId,
				 0L, 1024L, False,
				 Atoms::utf8String,
				 &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	char *id;

	id = strdup ((char *) data);
	XFree ((void *) data);

	return id;
    }

    return NULL;
}

void
CompWindow::changeState (unsigned int newState)
{
    unsigned int oldState;

    if (priv->state == newState)
	return;

    oldState = priv->state;
    priv->state = newState;

    recalcType ();
    recalcActions ();

    screen->priv->setWindowState (priv->state, priv->id);

    stateChangeNotify (oldState);
    screen->matchPropertyChanged (this);
}

static void
setWindowActions (CompScreen   *s,
		  unsigned int actions,
		  Window       id)
{
    Atom data[32];
    int	 i = 0;

    if (actions & CompWindowActionMoveMask)
	data[i++] = Atoms::winActionMove;
    if (actions & CompWindowActionResizeMask)
	data[i++] = Atoms::winActionResize;
    if (actions & CompWindowActionStickMask)
	data[i++] = Atoms::winActionStick;
    if (actions & CompWindowActionMinimizeMask)
	data[i++] = Atoms::winActionMinimize;
    if (actions & CompWindowActionMaximizeHorzMask)
	data[i++] = Atoms::winActionMaximizeHorz;
    if (actions & CompWindowActionMaximizeVertMask)
	data[i++] = Atoms::winActionMaximizeVert;
    if (actions & CompWindowActionFullscreenMask)
	data[i++] = Atoms::winActionFullscreen;
    if (actions & CompWindowActionCloseMask)
	data[i++] = Atoms::winActionClose;
    if (actions & CompWindowActionShadeMask)
	data[i++] = Atoms::winActionShade;
    if (actions & CompWindowActionChangeDesktopMask)
	data[i++] = Atoms::winActionChangeDesktop;
    if (actions & CompWindowActionAboveMask)
	data[i++] = Atoms::winActionAbove;
    if (actions & CompWindowActionBelowMask)
	data[i++] = Atoms::winActionBelow;

    XChangeProperty (s->dpy (), id, Atoms::wmAllowedActions,
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *) data, i);
}

void
CompWindow::recalcActions ()
{
    unsigned int actions = 0;
    unsigned int setActions, clearActions;

    switch (priv->type) {
    case CompWindowTypeFullscreenMask:
    case CompWindowTypeNormalMask:
	actions =
	    CompWindowActionMaximizeHorzMask |
	    CompWindowActionMaximizeVertMask |
	    CompWindowActionFullscreenMask   |
	    CompWindowActionMoveMask         |
	    CompWindowActionResizeMask       |
	    CompWindowActionStickMask        |
	    CompWindowActionMinimizeMask     |
	    CompWindowActionCloseMask	     |
	    CompWindowActionChangeDesktopMask;
	break;
    case CompWindowTypeUtilMask:
    case CompWindowTypeMenuMask:
    case CompWindowTypeToolbarMask:
	actions =
	    CompWindowActionMoveMask   |
	    CompWindowActionResizeMask |
	    CompWindowActionStickMask  |
	    CompWindowActionCloseMask  |
	    CompWindowActionChangeDesktopMask;
	break;
    case CompWindowTypeDialogMask:
    case CompWindowTypeModalDialogMask:
	actions =
	    CompWindowActionMaximizeHorzMask |
	    CompWindowActionMaximizeVertMask |
	    CompWindowActionMoveMask         |
	    CompWindowActionResizeMask       |
	    CompWindowActionStickMask        |
	    CompWindowActionCloseMask        |
	    CompWindowActionChangeDesktopMask;

	/* allow minimization for dialog windows if they
	   a) are not a transient (transients can be minimized
	      with their parent)
	   b) don't have the skip taskbar hint set (as those
	      have no target to be minimized to)
	*/
	if (!priv->transientFor &&
	    !(priv->state & CompWindowStateSkipTaskbarMask))
	{
	    actions |= CompWindowActionMinimizeMask;
	}
    default:
	break;
    }

    if (priv->input.top)
	actions |= CompWindowActionShadeMask;

    actions |= (CompWindowActionAboveMask | CompWindowActionBelowMask);

    switch (priv->wmType) {
    case CompWindowTypeNormalMask:
	actions |= CompWindowActionFullscreenMask |
	           CompWindowActionMinimizeMask;
    default:
	break;
    }

    if (priv->sizeHints.min_width  == priv->sizeHints.max_width &&
	priv->sizeHints.min_height == priv->sizeHints.max_height)
	actions &= ~(CompWindowActionResizeMask	      |
		     CompWindowActionMaximizeHorzMask |
		     CompWindowActionMaximizeVertMask |
		     CompWindowActionFullscreenMask);

    if (!(priv->mwmFunc & MwmFuncAll))
    {
	if (!(priv->mwmFunc & MwmFuncResize))
	    actions &= ~(CompWindowActionResizeMask	  |
			 CompWindowActionMaximizeHorzMask |
			 CompWindowActionMaximizeVertMask |
			 CompWindowActionFullscreenMask);

	if (!(priv->mwmFunc & MwmFuncMove))
	    actions &= ~(CompWindowActionMoveMask	  |
			 CompWindowActionMaximizeHorzMask |
			 CompWindowActionMaximizeVertMask |
			 CompWindowActionFullscreenMask);

	if (!(priv->mwmFunc & MwmFuncIconify))
	    actions &= ~CompWindowActionMinimizeMask;

	if (!(priv->mwmFunc & MwmFuncClose))
	    actions &= ~CompWindowActionCloseMask;
    }

    getAllowedActions (&setActions, &clearActions);
    actions &= ~clearActions;
    actions |= setActions;

    if (actions != priv->actions)
    {
	priv->actions = actions;
	setWindowActions (screen, actions, priv->id);
    }
}

void
CompWindow::getAllowedActions (unsigned int *setActions,
			       unsigned int *clearActions)
{
    WRAPABLE_HND_FUNC(1, getAllowedActions, setActions, clearActions)
    *setActions   = 0;
    *clearActions = 0;
}

unsigned int
CompWindow::constrainWindowState (unsigned int state,
				  unsigned int actions)
{
    if (!(actions & CompWindowActionMaximizeHorzMask))
	state &= ~CompWindowStateMaximizedHorzMask;

    if (!(actions & CompWindowActionMaximizeVertMask))
	state &= ~CompWindowStateMaximizedVertMask;

    if (!(actions & CompWindowActionShadeMask))
	state &= ~CompWindowStateShadedMask;

    if (!(actions & CompWindowActionFullscreenMask))
	state &= ~CompWindowStateFullscreenMask;

    return state;
}

unsigned int
PrivateWindow::windowTypeFromString (const char *str)
{
    if (strcasecmp (str, "desktop") == 0)
	return CompWindowTypeDesktopMask;
    else if (strcasecmp (str, "dock") == 0)
	return CompWindowTypeDockMask;
    else if (strcasecmp (str, "toolbar") == 0)
	return CompWindowTypeToolbarMask;
    else if (strcasecmp (str, "menu") == 0)
	return CompWindowTypeMenuMask;
    else if (strcasecmp (str, "utility") == 0)
	return CompWindowTypeUtilMask;
    else if (strcasecmp (str, "splash") == 0)
	return CompWindowTypeSplashMask;
    else if (strcasecmp (str, "dialog") == 0)
	return CompWindowTypeDialogMask;
    else if (strcasecmp (str, "normal") == 0)
	return CompWindowTypeNormalMask;
    else if (strcasecmp (str, "dropdownmenu") == 0)
	return CompWindowTypeDropdownMenuMask;
    else if (strcasecmp (str, "popupmenu") == 0)
	return CompWindowTypePopupMenuMask;
    else if (strcasecmp (str, "tooltip") == 0)
	return CompWindowTypeTooltipMask;
    else if (strcasecmp (str, "notification") == 0)
	return CompWindowTypeNotificationMask;
    else if (strcasecmp (str, "combo") == 0)
	return CompWindowTypeComboMask;
    else if (strcasecmp (str, "dnd") == 0)
	return CompWindowTypeDndMask;
    else if (strcasecmp (str, "modaldialog") == 0)
	return CompWindowTypeModalDialogMask;
    else if (strcasecmp (str, "fullscreen") == 0)
	return CompWindowTypeFullscreenMask;
    else if (strcasecmp (str, "unknown") == 0)
	return CompWindowTypeUnknownMask;
    else if (strcasecmp (str, "any") == 0)
	return ~0;

    return 0;
}

void
CompWindow::recalcType ()
{
    unsigned int type;

    type = priv->wmType;

    if (!overrideRedirect () && priv->wmType == CompWindowTypeUnknownMask)
	type = CompWindowTypeNormalMask;

    if (priv->state & CompWindowStateFullscreenMask)
	type = CompWindowTypeFullscreenMask;

    if (type == CompWindowTypeNormalMask)
    {
	if (priv->transientFor)
	    type = CompWindowTypeDialogMask;
    }

    if (type == CompWindowTypeDockMask && (priv->state & CompWindowStateBelowMask))
	type = CompWindowTypeNormalMask;

    if ((type & (CompWindowTypeNormalMask | CompWindowTypeDialogMask)) &&
	(priv->state & CompWindowStateModalMask))
	type = CompWindowTypeModalDialogMask;

    priv->type = type;
}


void
PrivateWindow::updateFrameWindow ()
{
    if (input.left || input.right || input.top || input.bottom)
    {
	int	   x, y, width, height;
	int	   bw = serverGeometry.border () * 2;

	x      = serverGeometry.x () - input.left;
	y      = serverGeometry.y () - input.top;
	width  = serverGeometry.width () + input.left + input.right + bw;
	height = serverGeometry.height () + input.top  + input.bottom + bw;

	if (shaded)
	    height = input.top + input.bottom;

	if (!frame)
	    reparent ();

	XMoveResizeWindow (screen->dpy (), frame, x, y, width, height);
	XMoveResizeWindow (screen->dpy (), wrapper, input.left, input.top,
			   serverGeometry.width (), serverGeometry.height ());
        XMoveResizeWindow (screen->dpy (), id, 0, 0,
			   serverGeometry.width (), serverGeometry.height ());
        window->sendConfigureNotify ();

	window->updateFrameRegion ();
	window->windowNotify (CompWindowNotifyFrameUpdate);
    }
    else
    {
	if (frame)
	{
	    unreparent ();
	    frame = None;
	    XSubtractRegion (&emptyRegion, &emptyRegion, frameRegion);
	}
    }

    window->recalcActions ();
}



void
CompWindow::updateWindowOutputExtents ()
{
    CompWindowExtents output;

    getOutputExtents (&output);

    if (output.left   != priv->output.left  ||
	output.right  != priv->output.right ||
	output.top    != priv->output.top   ||
	output.bottom != priv->output.bottom)
    {
	priv->output = output;

	resizeNotify (0, 0, 0, 0);
    }
}

void
CompWindow::getOutputExtents (CompWindowExtents *output)
{
    WRAPABLE_HND_FUNC(0, getOutputExtents, output)
    output->left   = 0;
    output->right  = 0;
    output->top    = 0;
    output->bottom = 0;
}

void
PrivateWindow::updateRegion ()
{
    REGION     rect;
    XRectangle r, *rects, *shapeRects = 0;
    int	       i, n = 0;

    EMPTY_REGION (priv->region);

    if (screen->XShape ())
    {
	int order;

	shapeRects = XShapeGetRectangles (screen->dpy (), priv->id,
					  ShapeBounding, &n, &order);
    }

    if (n < 2)
    {
	r.x      = -priv->attrib.border_width;
	r.y      = -priv->attrib.border_width;
	r.width  = priv->width;
	r.height = priv->height;

	rects = &r;
	n = 1;
    }
    else
    {
	rects = shapeRects;
    }

    rect.rects = &rect.extents;
    rect.numRects = rect.size = 1;

    for (i = 0; i < n; i++)
    {
	rect.extents.x1 = rects[i].x + priv->attrib.border_width;
	rect.extents.y1 = rects[i].y + priv->attrib.border_width;
	rect.extents.x2 = rect.extents.x1 + rects[i].width;
	rect.extents.y2 = rect.extents.y1 + rects[i].height;

	if (rect.extents.x1 < 0)
	    rect.extents.x1 = 0;
	if (rect.extents.y1 < 0)
	    rect.extents.y1 = 0;
	if (rect.extents.x2 > priv->width)
	    rect.extents.x2 = priv->width;
	if (rect.extents.y2 > priv->height)
	    rect.extents.y2 = priv->height;

	if (rect.extents.y1 < rect.extents.y2 &&
	    rect.extents.x1 < rect.extents.x2)
	{
	    rect.extents.x1 += priv->attrib.x;
	    rect.extents.y1 += priv->attrib.y;
	    rect.extents.x2 += priv->attrib.x;
	    rect.extents.y2 += priv->attrib.y;

	    XUnionRegion (&rect, priv->region, priv->region);
	}
    }

    if (shapeRects)
	XFree (shapeRects);

    window->updateFrameRegion ();
}

bool
PrivateWindow::updateStruts ()
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    bool	  hasOld, hasNew;
    CompStruts    old, c_new;

#define MIN_EMPTY 76

    if (priv->struts)
    {
	hasOld = true;

	old.left   = priv->struts->left;
	old.right  = priv->struts->right;
	old.top    = priv->struts->top;
	old.bottom = priv->struts->bottom;
    }
    else
    {
	hasOld = false;
    }

    hasNew = true;

    c_new.left.x	    = 0;
    c_new.left.y	    = 0;
    c_new.left.width  = 0;
    c_new.left.height = screen->size().height ();

    c_new.right.x      = screen->size().width ();
    c_new.right.y      = 0;
    c_new.right.width  = 0;
    c_new.right.height = screen->size().height ();

    c_new.top.x	   = 0;
    c_new.top.y	   = 0;
    c_new.top.width  = screen->size().width ();
    c_new.top.height = 0;

    c_new.bottom.x      = 0;
    c_new.bottom.y      = screen->size().height ();
    c_new.bottom.width  = screen->size().width ();
    c_new.bottom.height = 0;

    result = XGetWindowProperty (screen->dpy (), priv->id,
				 Atoms::wmStrutPartial,
				 0L, 12L, FALSE, XA_CARDINAL, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	unsigned long *struts = (unsigned long *) data;

	if (n == 12)
	{
	    int gap;

	    hasNew = true;

	    gap = screen->size().width () - struts[0] - struts[1];
	    gap -= MIN_EMPTY;

	    c_new.left.width  = (int) struts[0] + MIN (0, gap / 2);
	    c_new.right.width = (int) struts[1] + MIN (0, gap / 2);

	    gap = screen->size().height () - struts[2] - struts[3];
	    gap -= MIN_EMPTY;

	    c_new.top.height    = (int) struts[2] + MIN (0, gap / 2);
	    c_new.bottom.height = (int) struts[3] + MIN (0, gap / 2);

	    c_new.right.x  = screen->size().width () - c_new.right.width;
	    c_new.bottom.y = screen->size().height () -
			     c_new.bottom.height;

	    c_new.left.y       = struts[4];
	    c_new.left.height  = struts[5] - c_new.left.y + 1;
	    c_new.right.y      = struts[6];
	    c_new.right.height = struts[7] - c_new.right.y + 1;

	    c_new.top.x        = struts[8];
	    c_new.top.width    = struts[9] - c_new.top.x + 1;
	    c_new.bottom.x     = struts[10];
	    c_new.bottom.width = struts[11] - c_new.bottom.x + 1;
	}

	XFree (data);
    }

    if (!hasNew)
    {
	result = XGetWindowProperty (screen->dpy (), priv->id,
				     Atoms::wmStrut,
				     0L, 4L, FALSE, XA_CARDINAL,
				     &actual, &format, &n, &left, &data);

	if (result == Success && n && data)
	{
	    unsigned long *struts = (unsigned long *) data;

	    if (n == 4)
	    {
		int gap;

		hasNew = true;

		gap = screen->size().width () - struts[0] - struts[1];
		gap -= MIN_EMPTY;

		c_new.left.width  = (int) struts[0] + MIN (0, gap / 2);
		c_new.right.width = (int) struts[1] + MIN (0, gap / 2);

		gap = screen->size().height () - struts[2] - struts[3];
		gap -= MIN_EMPTY;

		c_new.top.height    = (int) struts[2] + MIN (0, gap / 2);
		c_new.bottom.height = (int) struts[3] + MIN (0, gap / 2);

		c_new.left.x  = 0;
		c_new.right.x = screen->size().width () -
				c_new.right.width;

		c_new.top.y    = 0;
		c_new.bottom.y = screen->size().height () -
				 c_new.bottom.height;
	    }

	    XFree (data);
	}
    }

    if (hasNew)
    {
	int strutX1, strutY1, strutX2, strutY2;
	int x1, y1, x2, y2;

	/* applications expect us to clip struts to xinerama edges */
	for (unsigned int i = 0;
	     i < screen->screenInfo ().size (); i++)
	{
	    x1 = screen->screenInfo ()[i].x_org;
	    y1 = screen->screenInfo ()[i].y_org;
	    x2 = x1 + screen->screenInfo ()[i].width;
	    y2 = y1 + screen->screenInfo ()[i].height;

	    strutX1 = c_new.left.x;
	    strutX2 = strutX1 + c_new.left.width;

	    if (strutX2 > x1 && strutX2 <= x2)
	    {
		c_new.left.x     = x1;
		c_new.left.width = strutX2 - x1;
	    }

	    strutX1 = c_new.right.x;
	    strutX2 = strutX1 + c_new.right.width;

	    if (strutX1 > x1 && strutX1 <= x2)
	    {
		c_new.right.x     = strutX1;
		c_new.right.width = x2 - strutX1;
	    }

	    strutY1 = c_new.top.y;
	    strutY2 = strutY1 + c_new.top.height;

	    if (strutY2 > y1 && strutY2 <= y2)
	    {
		c_new.top.y      = y1;
		c_new.top.height = strutY2 - y1;
	    }

	    strutY1 = c_new.bottom.y;
	    strutY2 = strutY1 + c_new.bottom.height;

	    if (strutY1 > y1 && strutY1 <= y2)
	    {
		c_new.bottom.y      = strutY1;
		c_new.bottom.height = y2 - strutY1;
	    }
	}
    }

    if (hasOld != hasNew || (hasNew && hasOld &&
			     memcmp (&c_new, &old, sizeof (CompStruts))))
    {
	if (hasNew)
	{
	    if (!priv->struts)
	    {
		priv->struts = (CompStruts *) malloc (sizeof (CompStruts));
		if (!priv->struts)
		    return false;
	    }

	    *priv->struts = c_new;
	}
	else
	{
	    free (priv->struts);
	    priv->struts = NULL;
	}

	return true;
    }

    return false;
}

static void
setDefaultWindowAttributes (XWindowAttributes *wa)
{
    wa->x		      = 0;
    wa->y		      = 0;
    wa->width		      = 1;
    wa->height		      = 1;
    wa->border_width	      = 0;
    wa->depth		      = 0;
    wa->visual		      = NULL;
    wa->root		      = None;
    wa->c_class		      = InputOnly;
    wa->bit_gravity	      = NorthWestGravity;
    wa->win_gravity	      = NorthWestGravity;
    wa->backing_store	      = NotUseful;
    wa->backing_planes	      = 0;
    wa->backing_pixel	      = 0;
    wa->save_under	      = FALSE;
    wa->colormap	      = None;
    wa->map_installed	      = FALSE;
    wa->map_state	      = IsUnviewable;
    wa->all_event_masks	      = 0;
    wa->your_event_mask	      = 0;
    wa->do_not_propagate_mask = 0;
    wa->override_redirect     = TRUE;
    wa->screen		      = NULL;
}



void
CompWindow::destroy ()
{
    screen->priv->eraseWindowFromMap (id ());

    priv->id = 1;
    priv->mapNum = 0;

    priv->destroyRefCnt--;
    if (priv->destroyRefCnt)
	return;

    if (priv->frame)
	priv->unreparent ();

    if (!priv->destroyed)
    {
	priv->destroyed = true;
	screen->priv->pendingDestroys++;
    }
}

void
CompWindow::sendConfigureNotify ()
{
    XConfigureEvent xev;

    xev.type   = ConfigureNotify;
    xev.event  = priv->id;
    xev.window = priv->id;

    /* normally we should never send configure notify events to override
       redirect windows but if they support the _NET_WM_SYNC_REQUEST
       protocol we need to do this when the window is mapped. however the
       only way we can make sure that the attributes we send are correct
       and is to grab the server. */
    if (priv->attrib.override_redirect)
    {
	XWindowAttributes attrib;

	XGrabServer (screen->dpy ());

	if (XGetWindowAttributes (screen->dpy (), priv->id, &attrib))
	{
	    xev.x	     = attrib.x;
	    xev.y	     = attrib.y;
	    xev.width	     = attrib.width;
	    xev.height	     = attrib.height;
	    xev.border_width = attrib.border_width;

	    xev.above		  = (prev) ? prev->priv->id : None;
	    xev.override_redirect = TRUE;

	    XSendEvent (screen->dpy (), priv->id, FALSE,
			StructureNotifyMask, (XEvent *) &xev);
	}

	XUngrabServer (screen->dpy ());
    }
    else
    {
	xev.x		 = priv->serverGeometry.x ();
	xev.y		 = priv->serverGeometry.y ();
	xev.width	 = priv->serverGeometry.width ();
	xev.height	 = priv->serverGeometry.height ();
	xev.border_width = priv->serverGeometry.border ();

	xev.above	      = (prev) ? prev->priv->id : None;
	xev.override_redirect = priv->attrib.override_redirect;

	XSendEvent (screen->dpy (), priv->id, FALSE,
		    StructureNotifyMask, (XEvent *) &xev);
    }
}

void
CompWindow::map ()
{
    if (isViewable ())
	return;

    priv->pendingMaps--;

    priv->mapNum = screen->priv->mapNum++;

    if (priv->struts)
	screen->priv->updateWorkarea ();

    if (windowClass () == InputOnly)
	return;

    priv->unmapRefCnt = 1;

    priv->attrib.map_state = IsViewable;

    if (!overrideRedirect ())
	screen->priv->setWmState (NormalState, priv->id);

    priv->invisible  = true;
    priv->alive      = true;

    priv->lastPong = screen->priv->lastPing;

    priv->updateRegion ();
    priv->updateSize ();

    if (priv->frame)
    {
	XMapWindow (screen->dpy (), priv->frame);
	XMapWindow (screen->dpy (), priv->wrapper);
    }

    screen->priv->updateClientList ();

    if (priv->type & CompWindowTypeDesktopMask)
	screen->priv->desktopWindowCount++;

    if (priv->protocols & CompWindowProtocolSyncRequestMask)
    {
	sendSyncRequest ();
	sendConfigureNotify ();
    }

    if (!overrideRedirect ())
    {
	/* been shaded */
	if (!priv->height)
	    resize (priv->attrib.x, priv->attrib.y, priv->attrib.width,
		    ++priv->attrib.height - 1, priv->attrib.border_width);
    }

    windowNotify (CompWindowNotifyMap);
}

void
CompWindow::unmap ()
{
    if (priv->mapNum)
	priv->mapNum = 0;

    priv->unmapRefCnt--;
    if (priv->unmapRefCnt > 0)
	return;

    if (priv->struts)
	screen->priv->updateWorkarea ();

    if (priv->attrib.map_state != IsViewable)
	return;

    if (priv->type == CompWindowTypeDesktopMask)
	screen->priv->desktopWindowCount--;

    priv->attrib.map_state = IsUnmapped;

    priv->invisible = true;

    if (priv->shaded && priv->height)
	resize (priv->attrib.x, priv->attrib.y,
		priv->attrib.width, ++priv->attrib.height - 1,
		priv->attrib.border_width);

    screen->priv->updateClientList ();

    windowNotify (CompWindowNotifyUnmap);
}

bool
PrivateWindow::restack (Window aboveId)
{
    if (window->prev)
    {
	if (aboveId && (aboveId == window->prev->id () ||
		        aboveId == window->prev->frame ()))
	    return false;
    }
    else if (aboveId == None && !window->next)
	return false;

    if (aboveId && !screen->findTopLevelWindow (aboveId))
        return false;

    screen->unhookWindow (window);
    screen->insertWindow (window, aboveId);

    screen->priv->updateClientList ();

    window->windowNotify (CompWindowNotifyRestack);

    return true;
}

bool
CompWindow::resize (XWindowAttributes attr)
{
    return resize (Geometry (attr.x, attr.y, attr.width, attr.height,
			     attr.border_width));
}

bool
CompWindow::resize (int x, int y, unsigned int width, unsigned int height,
		    unsigned int border)
{
    return resize (Geometry (x, y, width, height, border));
}

bool
CompWindow::resize (CompWindow::Geometry gm)
{
    if (priv->attrib.width        != (int) gm.width ()  ||
	priv->attrib.height       != (int) gm.height () ||
	priv->attrib.border_width != (int) gm.border ())
    {
	unsigned int pw, ph;
	int	     dx, dy, dwidth, dheight;

	pw = gm.width () + gm.border () * 2;
	ph = gm.height () + gm.border () * 2;

	if (priv->shaded)
	{
	    ph = 0;
	}

	dx      = gm.x () - priv->attrib.x;
	dy      = gm.y () - priv->attrib.y;
	dwidth  = gm.width () - priv->attrib.width;
	dheight = gm.height () - priv->attrib.height;

	priv->attrib.x	          = gm.x ();
	priv->attrib.y	          = gm.y ();
	priv->attrib.width	  = gm.width ();
	priv->attrib.height       = gm.height ();
	priv->attrib.border_width = gm.border ();

	priv->geometry.set (priv->attrib.x, priv->attrib.y,
			    priv->attrib.width, priv->attrib.height,
			    priv->attrib.border_width);

	priv->width  = pw;
	priv->height = ph;

	if (priv->mapNum)
	    priv->updateRegion ();

	resizeNotify (dx, dy, dwidth, dheight);

	priv->invisible = WINDOW_INVISIBLE (priv);
    }
    else if (priv->attrib.x != gm.x () || priv->attrib.y != gm.y ())
    {
	int dx, dy;

	dx = gm.x () - priv->attrib.x;
	dy = gm.y () - priv->attrib.y;

	move (dx, dy);
    }

    return true;
}

static void
syncValueIncrement (XSyncValue *value)
{
    XSyncValue one;
    int	       overflow;

    XSyncIntToValue (&one, 1);
    XSyncValueAdd (value, *value, one, &overflow);
}

bool
PrivateWindow::initializeSyncCounter ()
{
    XSyncAlarmAttributes values;
    Atom		 actual;
    int			 result, format;
    unsigned long	 n, left;
    unsigned char	 *data;

    if (syncCounter)
	return syncAlarm != None;

    if (!(protocols & CompWindowProtocolSyncRequestMask))
	return false;

    result = XGetWindowProperty (screen->dpy (), id,
				 Atoms::wmSyncRequestCounter,
				 0L, 1L, FALSE, XA_CARDINAL, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	unsigned long *counter = (unsigned long *) data;

	syncCounter = *counter;

	XFree (data);

	XSyncIntsToValue (&syncValue, (unsigned int) rand (), 0);
	XSyncSetCounter (screen->dpy (),
			 syncCounter,
			 syncValue);

	syncValueIncrement (&syncValue);

	values.events = TRUE;

	values.trigger.counter    = syncCounter;
	values.trigger.wait_value = syncValue;

	values.trigger.value_type = XSyncAbsolute;
	values.trigger.test_type  = XSyncPositiveComparison;

	XSyncIntToValue (&values.delta, 1);

	values.events = TRUE;

	CompScreen::checkForError (screen->dpy ());

	/* Note that by default, the alarm increments the trigger value
	 * when it fires until the condition (counter.value < trigger.value)
	 * is FALSE again.
	 */
	syncAlarm = XSyncCreateAlarm (screen->dpy (),
				      XSyncCACounter   |
				      XSyncCAValue     |
				      XSyncCAValueType |
				      XSyncCATestType  |
				      XSyncCADelta     |
				      XSyncCAEvents,
				      &values);

	if (CompScreen::checkForError (screen->dpy ()))
	    return true;

	XSyncDestroyAlarm (screen->dpy (), syncAlarm);
	syncAlarm = None;
    }

    return false;
}

void
CompWindow::sendSyncRequest ()
{
    XClientMessageEvent xev;

    if (priv->syncWait)
	return;

    if (!priv->initializeSyncCounter ())
	return;

    xev.type	     = ClientMessage;
    xev.window	     = priv->id;
    xev.message_type = Atoms::wmProtocols;
    xev.format	     = 32;
    xev.data.l[0]    = Atoms::wmSyncRequest;
    xev.data.l[1]    = CurrentTime;
    xev.data.l[2]    = XSyncValueLow32 (priv->syncValue);
    xev.data.l[3]    = XSyncValueHigh32 (priv->syncValue);
    xev.data.l[4]    = 0;

    syncValueIncrement (&priv->syncValue);

    XSendEvent (screen->dpy (), priv->id, FALSE, 0,
		(XEvent *) &xev);

    priv->syncWait     = TRUE;
    priv->syncGeometry = priv->serverGeometry;

    if (!priv->syncWaitTimer.active ())
	priv->syncWaitTimer.start (
	    boost::bind (&PrivateWindow::handleSyncAlarm, priv), 1000, 1200);
}

void
PrivateWindow::configure (XConfigureEvent *ce)
{
    if (priv->frame)
	return;

    priv->attrib.override_redirect = ce->override_redirect;

    if (priv->syncWait)
    {
	priv->syncGeometry.set (ce->x, ce->y, ce->width, ce->height,
				ce->border_width);
    }
    else
    {
	if (ce->override_redirect)
	{
	    priv->serverGeometry.set (ce->x, ce->y, ce->width, ce->height,
				      ce->border_width);
	}

	window->resize (ce->x, ce->y, ce->width, ce->height, ce->border_width);
    }

    if (ce->event == screen->root ())
	priv->restack (ce->above);
}

void
PrivateWindow::configureFrame (XConfigureEvent *ce)
{
    int x, y, width, height;

    if (!priv->frame)
	return;

    x      = ce->x + priv->input.left;
    y      = ce->y + priv->input.top;
    width  = ce->width - priv->input.left - priv->input.right;
    height = ce->height - priv->input.top - priv->input.bottom;

    if (priv->syncWait)
    {
	priv->syncGeometry.set (x, y, width, height, ce->border_width);
    }
    else
    {
	if (ce->override_redirect)
	{
	    priv->serverGeometry.set (x, y, width, height, ce->border_width);
	}

	window->resize (x, y, width, height, ce->border_width);
    }

    priv->restack (ce->above);
}

void
PrivateWindow::circulate (XCirculateEvent *ce)
{
    Window newAboveId;

    if (ce->place == PlaceOnTop)
	newAboveId = screen->priv->getTopWindow ();
    else
	newAboveId = 0;

    priv->restack (newAboveId);
}

void
CompWindow::move (int dx, int dy, bool immediate)
{
    if (dx || dy)
    {
	priv->attrib.x += dx;
	priv->attrib.y += dy;

	priv->geometry.setX (priv->attrib.x);
	priv->geometry.setY (priv->attrib.y);

	XOffsetRegion (priv->region, dx, dy);

	priv->invisible = WINDOW_INVISIBLE (priv);

	moveNotify (dx, dy, immediate);
    }
}

void
CompWindow::syncPosition ()
{
    priv->serverGeometry.setX (priv->attrib.x);
    priv->serverGeometry.setY (priv->attrib.y);

    XMoveWindow (screen->dpy (), ROOTPARENT (this),
		 priv->attrib.x - priv->input.left,
		 priv->attrib.y - priv->input.top);

    if (priv->frame)
    {
	XMoveWindow (screen->dpy (), priv->wrapper,
		     priv->input.left, priv->input.top);
	sendConfigureNotify ();
    }
}

bool
CompWindow::focus ()
{
    WRAPABLE_HND_FUNC_RETURN(2, bool, focus)

    if (overrideRedirect ())
	return false;

    if (!priv->managed)
	return false;

    if (!onCurrentDesktop ())
	return false;

    if (!priv->shaded && (priv->state & CompWindowStateHiddenMask))
	return false;

    if (priv->attrib.x + priv->width  <= 0	||
	priv->attrib.y + priv->height <= 0	||
	priv->attrib.x >= (int) screen->size().width ()||
	priv->attrib.y >= (int) screen->size().height ())
	return false;

    return true;
}

bool
CompWindow::place (int        x,
		   int        y,
		   int        *newX,
		   int        *newY)
{
    WRAPABLE_HND_FUNC_RETURN(4, bool, place, x, y, newX, newY)
    return false;
}

void
CompWindow::validateResizeRequest (unsigned int   *mask,
				   XWindowChanges *xwc)
    WRAPABLE_HND_FUNC(5, validateResizeRequest, mask, xwc)

void
CompWindow::resizeNotify (int dx,
			  int dy,
			  int dwidth,
			  int dheight)
    WRAPABLE_HND_FUNC(6, resizeNotify, dx, dy, dwidth, dheight)

void
CompWindow::moveNotify (int  dx,
			int  dy,
			bool immediate)
    WRAPABLE_HND_FUNC(7, moveNotify, dx, dy, immediate)

void
CompWindow::windowNotify (CompWindowNotify n)
    WRAPABLE_HND_FUNC (8, windowNotify, n)

void
CompWindow::grabNotify (int	       x,
			int	       y,
			unsigned int state,
			unsigned int mask)
{
    WRAPABLE_HND_FUNC(9, grabNotify, x, y, state, mask)
    priv->grabbed = true;
}

void
CompWindow::ungrabNotify ()
{
    WRAPABLE_HND_FUNC(10, ungrabNotify)
    priv->grabbed = false;
}

void
CompWindow::stateChangeNotify (unsigned int lastState)
    WRAPABLE_HND_FUNC(11, stateChangeNotify, lastState);

bool
PrivateWindow::isGroupTransient (Window clientLeader)
{
    if (!clientLeader)
	return false;

    if (transientFor == None || transientFor == screen->root ())
    {
	if (type & (CompWindowTypeDialogMask |
		       CompWindowTypeModalDialogMask))
	{
	    if (this->clientLeader == clientLeader)
		return true;
	}
    }

    return false;
}

CompWindow *
PrivateWindow::getModalTransient ()
{
    CompWindow *w, *modalTransient;

    modalTransient = window;

    for (w = screen->windows ().back (); w; w = w->prev)
    {
	if (w == modalTransient || w->priv->mapNum == 0)
	    continue;

	if (w->priv->transientFor == modalTransient->priv->id)
	{
	    if (w->priv->state & CompWindowStateModalMask)
	    {
		modalTransient = w;
		w = screen->windows ().back ();
	    }
	}
    }

    if (modalTransient == window)
    {
	/* don't look for group transients with modal state if current window
	   has modal state */
	if (state & CompWindowStateModalMask)
	    return NULL;

	for (w = screen->windows ().back (); w; w = w->prev)
	{
	    if (w == modalTransient || w->priv->mapNum == 0)
		continue;

	    if (isAncestorTo (modalTransient, w))
		continue;

	    if (w->priv->isGroupTransient (modalTransient->priv->clientLeader))
	    {
		if (w->priv->state & CompWindowStateModalMask)
		{
		    modalTransient = w;
		    w = w->priv->getModalTransient ();
		    if (w)
			modalTransient = w;

		    break;
		}
	    }
	}
    }

    if (modalTransient == window)
	modalTransient = NULL;

    return modalTransient;
}

void
CompWindow::moveInputFocusTo ()
{
    CompScreen  *s = screen;
    CompWindow  *modalTransient;

    modalTransient = priv->getModalTransient ();
    if (modalTransient)
	return modalTransient->moveInputFocusTo ();

    if (priv->state & CompWindowStateHiddenMask)
    {
	XSetInputFocus (s->dpy (), priv->frame, RevertToPointerRoot, CurrentTime);
	XChangeProperty (s->dpy (), s->root (), Atoms::winActive,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) &priv->id, 1);
    }
    else
    {
	bool setFocus = false;

	if (priv->inputHint)
	{
	    XSetInputFocus (s->dpy (), priv->id, RevertToPointerRoot,
			    CurrentTime);
	    setFocus = true;
	}

	if (priv->protocols & CompWindowProtocolTakeFocusMask)
	{
	    XEvent ev;

	    ev.type		    = ClientMessage;
	    ev.xclient.window	    = priv->id;
	    ev.xclient.message_type = Atoms::wmProtocols;
	    ev.xclient.format	    = 32;
	    ev.xclient.data.l[0]    = Atoms::wmTakeFocus;
	    ev.xclient.data.l[1]    = s->getCurrentTime ();
	    ev.xclient.data.l[2]    = 0;
	    ev.xclient.data.l[3]    = 0;
	    ev.xclient.data.l[4]    = 0;

	    XSendEvent (s->dpy (), priv->id, FALSE, NoEventMask, &ev);

	    setFocus = true;
	}

	if (!setFocus && !modalTransient)
	{
	    CompWindow *ancestor;

	    /* move input to closest ancestor */
	    for (ancestor = s->windows ().front (); ancestor;
		 ancestor = ancestor->next)
	    {
		if (PrivateWindow::isAncestorTo (this, ancestor))
		{
		    ancestor->moveInputFocusTo ();
		    break;
		}
	    }
	}
    }
}

void
CompWindow::moveInputFocusToOtherWindow ()
{
    if (priv->id == screen->activeWindow ())
    {
	CompWindow *ancestor;

	if (priv->transientFor && priv->transientFor != screen->root ())
	{
	    ancestor = screen->findWindow (priv->transientFor);
	    if (ancestor &&
		!(ancestor->priv->type & (CompWindowTypeDesktopMask |
					  CompWindowTypeDockMask)))
	    {
		ancestor->moveInputFocusTo ();
	    }
	    else
		screen->focusDefaultWindow ();
	}
	else if (priv->type & (CompWindowTypeDialogMask |
			       CompWindowTypeModalDialogMask))
	{
	    CompWindow *a, *focus = NULL;

	    for (a = screen->windows ().back (); a; a = a->prev)
	    {
		if (a->priv->clientLeader == priv->clientLeader)
		{
		    if (a->focus ())
		    {
			if (focus)
			{
			    if (a->priv->type & (CompWindowTypeNormalMask |
						 CompWindowTypeDialogMask |
						 CompWindowTypeModalDialogMask))
			    {
				if (priv->compareWindowActiveness (focus, a) < 0)
				    focus = a;
			    }
			}
			else
			    focus = a;
		    }
		}
	    }

	    if (focus && !(focus->priv->type & (CompWindowTypeDesktopMask |
						CompWindowTypeDockMask)))
	    {
		focus->moveInputFocusTo ();
	    }
	    else
		screen->focusDefaultWindow ();
	}
	else
	    screen->focusDefaultWindow ();
    }
}


bool
PrivateWindow::stackLayerCheck (CompWindow *w,
				Window	    clientLeader,
				CompWindow *below)
{
    if (isAncestorTo (w, below))
	return true;

    if (isAncestorTo (below, w))
	return false;

    if (clientLeader && below->priv->clientLeader == clientLeader)
	if (below->priv->isGroupTransient (clientLeader))
	    return false;

    if (w->priv->state & CompWindowStateAboveMask)
    {
	return true;
    }
    else if (w->priv->state & CompWindowStateBelowMask)
    {
	if (below->priv->state & CompWindowStateBelowMask)
	    return true;
    }
    else if (!(below->priv->state & CompWindowStateAboveMask))
    {
	return true;
    }

    return false;
}

bool
PrivateWindow::avoidStackingRelativeTo (CompWindow *w)
{
    if (w->overrideRedirect ())
	return true;

    if (!w->priv->shaded && !w->priv->pendingMaps)
    {
	if (w->priv->attrib.map_state != IsViewable || w->priv->mapNum == 0)
	    return true;
    }

    return false;
}

/* goes through the stack, top-down until we find a window we should
   stack above, normal windows can be stacked above fullscreen windows
   if aboveFs is TRUE. */
CompWindow *
PrivateWindow::findSiblingBelow (CompWindow *w,
				 bool       aboveFs)
{
    CompWindow   *below;
    Window	 clientLeader = w->priv->clientLeader;
    unsigned int type = w->priv->type;
    unsigned int belowMask;

    if (aboveFs)
	belowMask = CompWindowTypeDockMask;
    else
	belowMask = CompWindowTypeDockMask | CompWindowTypeFullscreenMask;

    /* normal stacking of fullscreen windows with below state */
    if ((type & CompWindowTypeFullscreenMask) &&
	(w->priv->state & CompWindowStateBelowMask))
	type = CompWindowTypeNormalMask;

    if (w->priv->transientFor || w->priv->isGroupTransient (clientLeader))
	clientLeader = None;

    for (below = screen->windows ().back (); below;
	 below = below->prev)
    {
	if (below == w || avoidStackingRelativeTo (below))
	    continue;

	/* always above desktop windows */
	if (below->priv->type & CompWindowTypeDesktopMask)
	    return below;

	switch (type) {
	case CompWindowTypeDesktopMask:
	    /* desktop window layer */
	    break;
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeDockMask:
	    /* fullscreen and dock layer */
	    if (below->priv->type & (CompWindowTypeFullscreenMask |
			       CompWindowTypeDockMask))
	    {
		if (stackLayerCheck (w, clientLeader, below))
		    return below;
	    }
	    else
	    {
		return below;
	    }
	    break;
	default:
	    /* fullscreen and normal layer */
	    if (!(below->priv->type & belowMask))
	    {
		if (stackLayerCheck (w, clientLeader, below))
		    return below;
	    }
	    break;
	}
    }

    return NULL;
}

/* goes through the stack, top-down and returns the lowest window we
   can stack above. */
CompWindow *
PrivateWindow::findLowestSiblingBelow (CompWindow *w)
{
    CompWindow   *below, *lowest = screen->windows ().back ();
    Window	 clientLeader = w->priv->clientLeader;
    unsigned int type = w->priv->type;

    /* normal stacking fullscreen windows with below state */
    if ((type & CompWindowTypeFullscreenMask) &&
	(w->priv->state & CompWindowStateBelowMask))
	type = CompWindowTypeNormalMask;

    if (w->priv->transientFor || w->priv->isGroupTransient (clientLeader))
	clientLeader = None;

    for (below = screen->windows ().back (); below;
	 below = below->prev)
    {
	if (below == w || avoidStackingRelativeTo (below))
	    continue;

	/* always above desktop windows */
	if (below->priv->type & CompWindowTypeDesktopMask)
	    return below;

	switch (type) {
	case CompWindowTypeDesktopMask:
	    /* desktop window layer - desktop windows always should be
	       stacked at the bottom; no other window should be below them */
	    return NULL;
	    break;
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeDockMask:
	    /* fullscreen and dock layer */
	    if (below->priv->type & (CompWindowTypeFullscreenMask |
			       CompWindowTypeDockMask))
	    {
		if (!stackLayerCheck (below, clientLeader, w))
		    return lowest;
	    }
	    else
	    {
		return lowest;
	    }
	    break;
	default:
	    /* fullscreen and normal layer */
	    if (!(below->priv->type & CompWindowTypeDockMask))
	    {
		if (!stackLayerCheck (below, clientLeader, w))
		    return lowest;
	    }
	    break;
	}

	lowest = below;
    }

    return lowest;
}

bool
PrivateWindow::validSiblingBelow (CompWindow *w,
				  CompWindow *sibling)
{
    Window	 clientLeader = w->priv->clientLeader;
    unsigned int type = w->priv->type;

    /* normal stacking fullscreen windows with below state */
    if ((type & CompWindowTypeFullscreenMask) &&
	(w->priv->state & CompWindowStateBelowMask))
	type = CompWindowTypeNormalMask;

    if (w->priv->transientFor || w->priv->isGroupTransient (clientLeader))
	clientLeader = None;

    if (sibling == w || avoidStackingRelativeTo (sibling))
	return false;

    /* always above desktop windows */
    if (sibling->priv->type & CompWindowTypeDesktopMask)
	return true;

    switch (type) {
    case CompWindowTypeDesktopMask:
	/* desktop window layer */
	break;
    case CompWindowTypeFullscreenMask:
    case CompWindowTypeDockMask:
	/* fullscreen and dock layer */
	if (sibling->priv->type & (CompWindowTypeFullscreenMask |
			     CompWindowTypeDockMask))
	{
	    if (stackLayerCheck (w, clientLeader, sibling))
		return true;
	}
	else
	{
	    return true;
	}
	break;
    default:
	/* fullscreen and normal layer */
	if (!(sibling->priv->type & CompWindowTypeDockMask))
	{
	    if (stackLayerCheck (w, clientLeader, sibling))
		return true;
	}
	break;
    }

    return false;
}

void
PrivateWindow::saveGeometry (int mask)
{
    int m = mask & ~saveMask;

    /* only save geometry if window has been placed */
    if (!placed)
	return;

    if (m & CWX)
	saveWc.x = serverGeometry.x ();

    if (m & CWY)
	saveWc.y = serverGeometry.y ();

    if (m & CWWidth)
	saveWc.width = serverGeometry.width ();

    if (m & CWHeight)
	saveWc.height = serverGeometry.height ();

    if (m & CWBorderWidth)
	saveWc.border_width = serverGeometry.border ();

    saveMask |= m;
}

int
PrivateWindow::restoreGeometry (XWindowChanges *xwc,
				int            mask)
{
    int m = mask & saveMask;

    if (m & CWX)
	xwc->x = saveWc.x;

    if (m & CWY)
	xwc->y = saveWc.y;

    if (m & CWWidth)
    {
	xwc->width = saveWc.width;

	/* This is not perfect but it works OK for now. If the saved width is
	   the same as the current width then make it a little be smaller so
	   the user can see that it changed and it also makes sure that
	   windowResizeNotify is called and plugins are notified. */
	if (xwc->width == (int) serverGeometry.width ())
	{
	    xwc->width -= 10;
	    if (m & CWX)
		xwc->x += 5;
	}
    }

    if (m & CWHeight)
    {
	xwc->height = saveWc.height;

	/* As above, if the saved height is the same as the current height
	   then make it a little be smaller. */
	if (xwc->height == (int) serverGeometry.height ())
	{
	    xwc->height -= 10;
	    if (m & CWY)
		xwc->y += 5;
	}
    }

    if (m & CWBorderWidth)
	xwc->border_width = saveWc.border_width;

    saveMask &= ~mask;

    return m;
}

void
PrivateWindow::reconfigureXWindow (unsigned int   valueMask,
				   XWindowChanges *xwc)
{
    if (valueMask & CWX)
	serverGeometry.setX (xwc->x);

    if (valueMask & CWY)
	serverGeometry.setY (xwc->y);

    if (valueMask & CWWidth)
	serverGeometry.setWidth (xwc->width);

    if (valueMask & CWHeight)
	serverGeometry.setHeight (xwc->height);

    if (valueMask & CWBorderWidth)
	serverGeometry.setBorder (xwc->border_width);

    if (frame)
    {
	XWindowChanges wc = *xwc;
	wc.x      -= input.left;
	wc.y      -= input.top;
	wc.width  += input.left + input.right;
	wc.height += input.top + input.bottom;
	
	XConfigureWindow (screen->dpy (), frame,
			  valueMask, &wc);
	valueMask &= ~(CWSibling | CWStackMode);
	xwc->x = input.left;
	xwc->y = input.top;
	XConfigureWindow (screen->dpy (), wrapper,
			  valueMask, xwc);
	xwc->x = 0;
	xwc->y = 0;
    }

    XConfigureWindow (screen->dpy (), id, valueMask, xwc);
}

bool
PrivateWindow::stackTransients (CompWindow	*w,
				CompWindow	*avoid,
				XWindowChanges *xwc)
{
    CompWindow *t;
    Window     clientLeader = w->priv->clientLeader;

    if (w->priv->transientFor || w->priv->isGroupTransient (clientLeader))
	clientLeader = None;

    for (t = screen->windows ().back (); t; t = t->prev)
    {
	if (t == w || t == avoid)
	    continue;

	if (t->priv->transientFor == w->priv->id ||
	    t->priv->isGroupTransient (clientLeader))
	{
	    if (w->priv->type & CompWindowTypeDockMask)
		if (!(t->priv->type & CompWindowTypeDockMask))
		    return false;

	    if (!stackTransients (t, avoid, xwc))
		return false;

	    if (xwc->sibling == t->priv->id ||
		(t->priv->frame && xwc->sibling == t->priv->frame))
		return false;

	    if (t->priv->mapNum || t->priv->pendingMaps)
		t->priv->reconfigureXWindow (CWSibling | CWStackMode, xwc);
	}
    }

    return true;
}

void
PrivateWindow::stackAncestors (CompWindow     *w,
			       XWindowChanges *xwc)
{
    CompWindow *transient = NULL;
    if (w->priv->transientFor)
	transient = screen->findWindow (w->priv->transientFor);
    if (transient && (xwc->sibling != transient->priv->id &&
	(!transient->priv->frame || xwc->sibling != transient->priv->frame)))
    {
	CompWindow *ancestor;

	ancestor = screen->findWindow (w->priv->transientFor);
	if (ancestor)
	{
	    if (!stackTransients (ancestor, w, xwc))
		return;

	    if (ancestor->priv->type & CompWindowTypeDesktopMask)
		return;

	    if (ancestor->priv->type & CompWindowTypeDockMask)
		if (!(w->priv->type & CompWindowTypeDockMask))
		    return;

	    if (ancestor->priv->mapNum || ancestor->priv->pendingMaps)
		ancestor->priv->reconfigureXWindow (CWSibling | CWStackMode,
				    		    xwc);

	    stackAncestors (ancestor, xwc);
	}
    }
    else if (w->priv->isGroupTransient (w->priv->clientLeader))
    {
	CompWindow *a;

	for (a = screen->windows ().back (); a; a = a->prev)
	{
	    if (a->priv->clientLeader == w->priv->clientLeader &&
		a->priv->transientFor == None		   &&
		!a->priv->isGroupTransient (w->priv->clientLeader))
	    {
		if (xwc->sibling == a->priv->id ||
		    (a->priv->frame && xwc->sibling == a->priv->frame))
		    break;

		if (!stackTransients (a, w, xwc))
		    break;

		if (a->priv->type & CompWindowTypeDesktopMask)
		    continue;

		if (a->priv->type & CompWindowTypeDockMask)
		    if (!(w->priv->type & CompWindowTypeDockMask))
			break;

		if (a->priv->mapNum || a->priv->pendingMaps)
		    a->priv->reconfigureXWindow (CWSibling | CWStackMode,
						 xwc);
	    }
	}
    }
}

void
CompWindow::configureXWindow (unsigned int valueMask,
			      XWindowChanges *xwc)
{
    if (priv->managed && (valueMask & (CWSibling | CWStackMode)))
    {
	/* transient children above */
	if (PrivateWindow::stackTransients (this, NULL, xwc))
	{
	    priv->reconfigureXWindow (valueMask, xwc);

	    /* ancestors, siblings and sibling transients below */
	    PrivateWindow::stackAncestors (this, xwc);
	}
    }
    else
    {
	priv->reconfigureXWindow (valueMask, xwc);
    }
}

int
PrivateWindow::addWindowSizeChanges (XWindowChanges *xwc,
				     CompWindow::Geometry old)
{
    XRectangle workArea;
    int	       mask = 0;
    int	       x, y;
    int	       vx, vy;
    int	       output;

    screen->viewportForGeometry (old, &vx, &vy);

    x = (vx - screen->vp ().x ()) * screen->size().width ();
    y = (vy - screen->vp ().y ()) * screen->size().height ();

    output = screen->outputDeviceForGeometry (old);
    screen->getWorkareaForOutput (output, &workArea);

    if (type & CompWindowTypeFullscreenMask)
    {
	saveGeometry (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

	xwc->width	  = screen->outputDevs ()[output].width ();
	xwc->height	  = screen->outputDevs ()[output].height ();
	xwc->border_width = 0;

	mask |= CWWidth | CWHeight | CWBorderWidth;
    }
    else
    {
	mask |= restoreGeometry (xwc, CWBorderWidth);

	if (state & CompWindowStateMaximizedVertMask)
	{
	    saveGeometry (CWY | CWHeight);

	    xwc->height = workArea.height - input.top -
		input.bottom - old.border () * 2;

	    mask |= CWHeight;
	}
	else
	{
	    mask |= restoreGeometry (xwc, CWY | CWHeight);
	}

	if (state & CompWindowStateMaximizedHorzMask)
	{
	    saveGeometry (CWX | CWWidth);

	    xwc->width = workArea.width - input.left -
		input.right - old.border () * 2;

	    mask |= CWWidth;
	}
	else
	{
	    mask |= restoreGeometry (xwc, CWX | CWWidth);
	}

	/* constrain window width if smaller than minimum width */
	if (!(mask & CWWidth) && (int) old.width () < sizeHints.min_width)
	{
	    xwc->width = sizeHints.min_width;
	    mask |= CWWidth;
	}

	/* constrain window width if greater than maximum width */
	if (!(mask & CWWidth) && (int) old.width () > sizeHints.max_width)
	{
	    xwc->width = sizeHints.max_width;
	    mask |= CWWidth;
	}

	/* constrain window height if smaller than minimum height */
	if (!(mask & CWHeight) && (int) old.height () < sizeHints.min_height)
	{
	    xwc->height = sizeHints.min_height;
	    mask |= CWHeight;
	}

	/* constrain window height if greater than maximum height */
	if (!(mask & CWHeight) && (int) old.height () > sizeHints.max_height)
	{
	    xwc->height = sizeHints.max_height;
	    mask |= CWHeight;
	}
    }

    if (mask & (CWWidth | CWHeight))
    {
	if (type & CompWindowTypeFullscreenMask)
	{
	    xwc->x = x + screen->outputDevs ()[output].x1 ();
	    xwc->y = y + screen->outputDevs ()[output].y1 ();

	    mask |= CWX | CWY;
	}
	else
	{
	    int width, height, max;

	    width  = (mask & CWWidth)  ? xwc->width  : old.width ();
	    height = (mask & CWHeight) ? xwc->height : old.height ();

	    xwc->width  = old.width ();
	    xwc->height = old.height ();

	    window->constrainNewWindowSize (width, height, &width, &height);

	    if (width != (int) old.width ())
	    {
		mask |= CWWidth;
		xwc->width = width;
	    }
	    else
		mask &= ~CWWidth;

	    if (height != (int) old.height ())
	    {
		mask |= CWHeight;
		xwc->height = height;
	    }
	    else
		mask &= ~CWHeight;

	    if (state & CompWindowStateMaximizedVertMask)
	    {
		if (old.y () < y + workArea.y + input.top)
		{
		    xwc->y = y + workArea.y + input.top;
		    mask |= CWY;
		}
		else
		{
		    height = xwc->height + old.border () * 2;

		    max = y + workArea.y + workArea.height;
		    if (old.y () + (int) old.height () + input.bottom > max)
		    {
			xwc->y = max - height - input.bottom;
			mask |= CWY;
		    }
		    else if (old.y () + height + input.bottom > max)
		    {
			xwc->y = y + workArea.y +
			    (workArea.height - input.top - height -
			     input.bottom) / 2 + input.top;
			mask |= CWY;
		    }
		}
	    }

	    if (state & CompWindowStateMaximizedHorzMask)
	    {
		if (old.x () < x + workArea.x + input.left)
		{
		    xwc->x = x + workArea.x + input.left;
		    mask |= CWX;
		}
		else
		{
		    width = xwc->width + old.border () * 2;

		    max = x + workArea.x + workArea.width;
		    if (old.x () + (int) old.width () + input.right > max)
		    {
			xwc->x = max - width - input.right;
			mask |= CWX;
		    }
		    else if (old.x () + width + input.right > max)
		    {
			xwc->x = x + workArea.x +
			    (workArea.width - input.left - width -
			     input.right) / 2 + input.left;
			mask |= CWX;
		    }
		}
	    }
	}
    }

    if ((mask & CWX) && (xwc->x == old.x ()))
	mask &= ~CWX;

    if ((mask & CWY) && (xwc->y == old.y ()))
	mask &= ~CWY;

    if ((mask & CWWidth) && (xwc->width == (int) old.width ()))
	mask &= ~CWWidth;

    if ((mask & CWHeight) && (xwc->height == (int) old.height ()))
	mask &= ~CWHeight;

    return mask;
}

unsigned int
PrivateWindow::adjustConfigureRequestForGravity (XWindowChanges *xwc,
						 unsigned int   xwcm,
						 int            gravity)
{
    int          newX, newY;
    unsigned int mask = 0;

    newX = xwc->x;
    newY = xwc->y;

    if (xwcm & (CWX | CWWidth))
    {
	switch (gravity) {
	case NorthWestGravity:
	case WestGravity:
	case SouthWestGravity:
	    if (xwcm & CWX)
		newX += priv->input.left;
	    break;

	case NorthGravity:
	case CenterGravity:
	case SouthGravity:
	    if (!(xwcm & CWX))
		newX += (priv->serverGeometry.width () - xwc->width) / 2;
	    break;

	case NorthEastGravity:
	case EastGravity:
	case SouthEastGravity:
	    if (xwcm & CWX)
		newX -= priv->input.right;
	    else
		newX += priv->serverGeometry.width () - xwc->width;
	    break;

	case StaticGravity:
	default:
	    break;
	}
    }

    if (xwcm & (CWY | CWHeight))
    {
	switch (gravity) {
	case NorthWestGravity:
	case NorthGravity:
	case NorthEastGravity:
	    if (xwcm & CWY)
		newY += priv->input.top;
	    break;

	case WestGravity:
	case CenterGravity:
	case EastGravity:
	    if (!(xwcm & CWY))
		newY += (priv->serverGeometry.height () - xwc->height) / 2;
	    break;

	case SouthWestGravity:
	case SouthGravity:
	case SouthEastGravity:
	    if (xwcm & CWY)
		newY -= priv->input.bottom;
	    else
		newY += priv->serverGeometry.height () - xwc->height;
	    break;

	case StaticGravity:
	default:
	    break;
	}
    }

    if (newX != xwc->x)
    {
	xwc->x = newX;
	mask |= CWX;
    }

    if (newY != xwc->y)
    {
	xwc->y = newY;
	mask |= CWY;
    }

    return mask;
}

void
CompWindow::moveResize (XWindowChanges *xwc,
			unsigned int   xwcm,
			int            gravity)
{
    Bool placed = xwcm & (CWX | CWY);

    xwcm &= (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

    if (gravity == 0)
	gravity = priv->sizeHints.win_gravity;

    if (!(xwcm & CWX))
	xwc->x = priv->serverGeometry.x ();
    if (!(xwcm & CWY))
	xwc->y = priv->serverGeometry.y ();
    if (!(xwcm & CWWidth))
	xwc->width = priv->serverGeometry.width ();
    if (!(xwcm & CWHeight))
	xwc->height = priv->serverGeometry.height ();

    if (xwcm & (CWWidth | CWHeight))
    {
	int width, height;

	if (constrainNewWindowSize (xwc->width, xwc->height, &width, &height))
	{
	    if (width != xwc->width)
		xwcm |= CWWidth;

	    if (height != xwc->height)
		xwcm |= CWHeight;

	    xwc->width = width;
	    xwc->height = height;
	}
    }

    xwcm |= priv->adjustConfigureRequestForGravity (xwc, xwcm, gravity);

    if (!(priv->type & (CompWindowTypeDockMask       |
		     CompWindowTypeFullscreenMask |
		     CompWindowTypeUnknownMask)))
    {
	if (xwcm & CWY)
	{
	    int min, max;

	    min = screen->workArea ().y + priv->input.top;
	    max = screen->workArea ().y +
		  screen->workArea ().height;

	    min -= screen->vp ().y () * screen->size().height ();
	    max += (screen->vpSize ().height () -
		   screen->vp ().y () - 1) *
		   screen->size().height ();

	    if (xwc->y < min)
		xwc->y = min;
	    else if (xwc->y > max)
		xwc->y = max;
	}

	if (xwcm & CWX)
	{
	    int min, max;

	    min = screen->workArea ().x + priv->input.left;
	    max = screen->workArea ().x + screen->workArea ().width;

	    min -= screen->vp ().x () * screen->size().width ();
	    max += (screen->vpSize ().width () -
		   screen->vp ().x () - 1) *
		   screen->size().width ();

	    if (xwc->x < min)
		xwc->x = min;
	    else if (xwc->x > max)
		xwc->x = max;
	}
    }

    validateResizeRequest (&xwcm, xwc);

    /* when horizontally maximized only allow width changes added by
       addWindowSizeChanges */
    if (priv->state & CompWindowStateMaximizedHorzMask)
	xwcm &= ~CWWidth;

    /* when vertically maximized only allow height changes added by
       addWindowSizeChanges */
    if (priv->state & CompWindowStateMaximizedVertMask)
	xwcm &= ~CWHeight;

    xwcm |= priv->addWindowSizeChanges (xwc, Geometry (xwc->x, xwc->y,
					xwc->width, xwc->height,
					xwc->border_width));

    /* check if the new coordinates are useful and valid (different
       to current size); if not, we have to clear them to make sure
       we send a synthetic ConfigureNotify event if all coordinates
       match the server coordinates */
    if (xwc->x == priv->serverGeometry.x ())
	xwcm &= ~CWX;

    if (xwc->y == priv->serverGeometry.y ())
	xwcm &= ~CWY;

    if (xwc->width == (int) priv->serverGeometry.width ())
	xwcm &= ~CWWidth;

    if (xwc->height == (int) priv->serverGeometry.height ())
	xwcm &= ~CWHeight;

    if (xwc->border_width == (int) priv->serverGeometry.border ())
	xwcm &= ~CWBorderWidth;

    /* update saved window coordinates - if CWX or CWY is set for fullscreen
       or maximized windows after addWindowSizeChanges, it should be pretty
       safe to assume that the saved coordinates should be updated too, e.g.
       because the window was moved to another viewport by some client */
    if ((xwcm & CWX) && (priv->saveMask & CWX))
    	priv->saveWc.x += (xwc->x - priv->serverGeometry.x ());

    if ((xwcm & CWY) && (priv->saveMask & CWY))
	priv->saveWc.y += (xwc->y - priv->serverGeometry.y ());

    if (priv->mapNum && (xwcm & (CWWidth | CWHeight)))
	sendSyncRequest ();

    if (xwcm)
	configureXWindow (xwcm, xwc);
    else
    {
	/* we have to send a configure notify on ConfigureRequest events if
	   we decide not to do anything according to ICCCM 4.1.5 */
	sendConfigureNotify ();
    }

    if (placed)
	priv->placed = true;
}

void
PrivateWindow::updateSize ()
{
    XWindowChanges xwc;
    int		   mask;

    if (window->overrideRedirect () || !managed)
	return;

    mask = priv->addWindowSizeChanges (&xwc, priv->serverGeometry);
    if (mask)
    {
	if (priv->mapNum && (mask & (CWWidth | CWHeight)))
	    window->sendSyncRequest ();

	window->configureXWindow (mask, &xwc);
    }
}

int
PrivateWindow::addWindowStackChanges (XWindowChanges *xwc,
				      CompWindow     *sibling)
{
    int	mask = 0;

    if (!sibling || sibling->priv->id != id)
    {
	if (window->prev)
	{
	    if (!sibling)
	    {
		XLowerWindow (screen->dpy (), id);
		if (frame)
		    XLowerWindow (screen->dpy (), frame);
	    }
	    else if (sibling->priv->id != window->prev->priv->id)
	    {
		mask |= CWSibling | CWStackMode;

		xwc->stack_mode = Above;
		xwc->sibling    = ROOTPARENT (sibling);
	    }
	}
	else if (sibling)
	{
	    mask |= CWSibling | CWStackMode;

	    xwc->stack_mode = Above;
	    xwc->sibling    = ROOTPARENT (sibling);
	}
    }

    if (sibling && mask)
    {
	/* a normal window can be stacked above fullscreen windows but we
	   don't want normal windows to be stacked above dock window so if
	   the sibling we're stacking above is a fullscreen window we also
	   update all dock windows. */
	if ((sibling->priv->type & CompWindowTypeFullscreenMask) &&
	    (!(type & (CompWindowTypeFullscreenMask |
			  CompWindowTypeDockMask))) &&
	    !isAncestorTo (window, sibling))
	{
	    CompWindow *dw;

	    for (dw = screen->windows ().back (); dw; dw = dw->prev)
		if (dw == sibling)
		    break;

	    for (; dw; dw = dw->prev)
		if (dw->priv->type & CompWindowTypeDockMask)
		    dw->configureXWindow (mask, xwc);
	}
    }

    return mask;
}

void
CompWindow::raise ()
{
    XWindowChanges xwc;
    int		   mask;

    mask = priv->addWindowStackChanges (&xwc,
	PrivateWindow::findSiblingBelow (this, false));
    if (mask)
	configureXWindow (mask, &xwc);
}

void
CompWindow::lower ()
{
    XWindowChanges xwc;
    int		   mask;

    mask = priv->addWindowStackChanges (&xwc,
	PrivateWindow::findLowestSiblingBelow (this));
    if (mask)
	configureXWindow (mask, &xwc);
}

void
CompWindow::restackAbove (CompWindow *sibling)
{
    for (; sibling; sibling = sibling->next)
	if (PrivateWindow::validSiblingBelow (this, sibling))
	    break;

    if (sibling)
    {
	XWindowChanges xwc;
	int	       mask;

	mask = priv->addWindowStackChanges (&xwc, sibling);
	if (mask)
	    configureXWindow (mask, &xwc);
    }
}

/* finds the highest window under sibling we can stack above */
CompWindow *
PrivateWindow::findValidStackSiblingBelow (CompWindow *w,
					   CompWindow *sibling)
{
    CompWindow *lowest, *last, *p;

    /* get lowest sibling we're allowed to stack above */
    lowest = last = findLowestSiblingBelow (w);

    /* walk from bottom up */
    for (p = screen->windows ().front (); p; p = p->next)
    {
	/* stop walking when we reach the sibling we should try to stack
	   below */
	if (p == sibling)
	    return lowest;

	/* skip windows that we should avoid */
	if (w == p || avoidStackingRelativeTo (p))
	    continue;

	if (validSiblingBelow (w, p))
	{
	    /* update lowest as we find windows below sibling that we're
	       allowed to stack above. last window must be equal to the
	       lowest as we shouldn't update lowest if we passed an
	       invalid window */
	    if (last == lowest)
		lowest = p;
	}

	/* update last pointer */
	last = p;
    }

    return lowest;
}

void
CompWindow::restackBelow (CompWindow *sibling)
{
    XWindowChanges xwc;
    unsigned int   mask;

    mask = priv->addWindowStackChanges (&xwc,
	PrivateWindow::findValidStackSiblingBelow (this, sibling));

    if (mask)
	configureXWindow (mask, &xwc);
}

void
CompWindow::updateAttributes (CompStackingUpdateMode stackingMode)
{
    XWindowChanges xwc;
    int		   mask = 0;

    if (overrideRedirect () || !priv->managed)
	return;

    if (priv->state & CompWindowStateShadedMask)
    {
	hide ();
    }
    else if (priv->shaded)
    {
	show ();
    }

    if (stackingMode != CompStackingUpdateModeNone)
    {
	Bool       aboveFs;
	CompWindow *sibling;

	aboveFs = (stackingMode == CompStackingUpdateModeAboveFullscreen);
	sibling = PrivateWindow::findSiblingBelow (this, aboveFs);

	if (sibling &&
	    (stackingMode == CompStackingUpdateModeInitialMapDeniedFocus))
	{
	    CompWindow *p;

	    for (p = sibling; p; p = p->prev)
		if (p->priv->id == screen->activeWindow ())
		    break;

	    /* window is above active window so we should lower it */
	    if (p)
		p = PrivateWindow::findValidStackSiblingBelow (sibling, p);

	    /* if we found a valid sibling under the active window, it's
	       our new sibling we want to stack above */
	    if (p)
		sibling = p;
	}

	mask |= priv->addWindowStackChanges (&xwc, sibling);
    }

    if ((stackingMode == CompStackingUpdateModeInitialMap) ||
	(stackingMode == CompStackingUpdateModeInitialMapDeniedFocus))
    {
	/* If we are called from the MapRequest handler, we have to
	   immediately update the internal stack. If we don't do that,
	   the internal stacking order is invalid until the ConfigureNotify
	   arrives because we put the window at the top of the stack when
	   it was created */
	if (mask & CWStackMode)
	{
	    Window above = (mask & CWSibling) ? xwc.sibling : 0;
	    priv->restack (above);
	}
    }

    mask |= priv->addWindowSizeChanges (&xwc, priv->serverGeometry);

    if (priv->mapNum && (mask & (CWWidth | CWHeight)))
	sendSyncRequest ();

    if (mask)
	configureXWindow (mask, &xwc);
}

void
PrivateWindow::ensureWindowVisibility ()
{
    int x1, y1, x2, y2;
    int	width = serverGeometry.width () + serverGeometry.border () * 2;
    int	height = serverGeometry.height () + serverGeometry.border () * 2;
    int dx = 0;
    int dy = 0;

    if (struts || attrib.override_redirect)
	return;

    if (type & (CompWindowTypeDockMask	|
		   CompWindowTypeFullscreenMask |
		   CompWindowTypeUnknownMask))
	return;

    x1 = screen->workArea ().x - screen->size().width () * screen->vp ().x ();
    y1 = screen->workArea ().y - screen->size().height () * screen->vp ().y ();
    x2 = x1 + screen->workArea ().width + screen->vpSize ().width () *
	 screen->size().width ();
    y2 = y1 + screen->workArea ().height + screen->vpSize ().height () *
	 screen->size().height ();

    if (serverGeometry.x () - input.left >= x2)
	dx = (x2 - 25) - serverGeometry.x ();
    else if (serverGeometry.x () + width + input.right <= x1)
	dx = (x1 + 25) - (serverGeometry.x () + width);

    if (serverGeometry.y () - input.top >= y2)
	dy = (y2 - 25) - serverGeometry.y ();
    else if (serverGeometry.y () + height + input.bottom <= y1)
	dy = (y1 + 25) - (serverGeometry.y () + height);

    if (dx || dy)
    {
	XWindowChanges xwc;

	xwc.x = serverGeometry.x () + dx;
	xwc.y = serverGeometry.y () + dy;

	window->configureXWindow (CWX | CWY, &xwc);
    }
}

void
PrivateWindow::reveal ()
{
    if (minimized)
	window->unminimize ();

    screen->leaveShowDesktopMode (window);
}

void
PrivateWindow::revealAncestors (CompWindow *w,
				CompWindow *transient)
{
    if (isAncestorTo (transient, w))
    {
	screen->forEachWindow (boost::bind (revealAncestors, _1, w));
	w->priv->reveal ();
    }
}

void
CompWindow::activate ()
{
    WRAPABLE_HND_FUNC(3, activate)

    screen->priv->setCurrentDesktop (priv->desktop);

    screen->forEachWindow (
	boost::bind (PrivateWindow::revealAncestors, _1, this));
    priv->reveal ();

    if (priv->state & CompWindowStateHiddenMask)
    {
	priv->state &= ~CompWindowStateShadedMask;
	if (priv->shaded)
	    show ();
    }

    if (priv->state & CompWindowStateHiddenMask)
	return;

    if (!onCurrentDesktop ())
	return;

    priv->ensureWindowVisibility ();
    updateAttributes (CompStackingUpdateModeAboveFullscreen);
    moveInputFocusTo ();
}


#define PVertResizeInc (1 << 0)
#define PHorzResizeInc (1 << 1)

bool
CompWindow::constrainNewWindowSize (int        width,
				    int        height,
				    int        *newWidth,
				    int        *newHeight)
{
    const XSizeHints *hints = &priv->sizeHints;
    int              oldWidth = width;
    int              oldHeight = height;
    int		     min_width = 0;
    int		     min_height = 0;
    int		     base_width = 0;
    int		     base_height = 0;
    int		     xinc = 1;
    int		     yinc = 1;
    int		     max_width = MAXSHORT;
    int		     max_height = MAXSHORT;
    long	     flags = hints->flags;
    long	     resizeIncFlags = (flags & PResizeInc) ? ~0 : 0;

    if (screen->getOption ("ignore_hints_when_maximized")->value ().b ())
    {
	if (priv->state & MAXIMIZE_STATE)
	{
	    flags &= ~PAspect;

	    if (priv->state & CompWindowStateMaximizedHorzMask)
		resizeIncFlags &= ~PHorzResizeInc;

	    if (priv->state & CompWindowStateMaximizedVertMask)
		resizeIncFlags &= ~PVertResizeInc;
	}
    }

    /* Ater gdk_window_constrain_size(), which is partially borrowed from fvwm.
     *
     * Copyright 1993, Robert Nation
     *     You may use this code for any purpose, as long as the original
     *     copyright remains in the source code and all documentation
     *
     * which in turn borrows parts of the algorithm from uwm
     */

#define FLOOR(value, base) (((int) ((value) / (base))) * (base))
#define FLOOR64(value, base) (((uint64_t) ((value) / (base))) * (base))
#define CLAMP(v, min, max) ((v) <= (min) ? (min) : (v) >= (max) ? (max) : (v))

    if ((flags & PBaseSize) && (flags & PMinSize))
    {
	base_width = hints->base_width;
	base_height = hints->base_height;
	min_width = hints->min_width;
	min_height = hints->min_height;
    }
    else if (flags & PBaseSize)
    {
	base_width = hints->base_width;
	base_height = hints->base_height;
	min_width = hints->base_width;
	min_height = hints->base_height;
    }
    else if (flags & PMinSize)
    {
	base_width = hints->min_width;
	base_height = hints->min_height;
	min_width = hints->min_width;
	min_height = hints->min_height;
    }

    if (flags & PMaxSize)
    {
	max_width = hints->max_width;
	max_height = hints->max_height;
    }

    if (resizeIncFlags & PHorzResizeInc)
	xinc = MAX (xinc, hints->width_inc);

    if (resizeIncFlags & PVertResizeInc)
	yinc = MAX (yinc, hints->height_inc);

    /* clamp width and height to min and max values */
    width  = CLAMP (width, min_width, max_width);
    height = CLAMP (height, min_height, max_height);

    /* shrink to base + N * inc */
    width  = base_width + FLOOR (width - base_width, xinc);
    height = base_height + FLOOR (height - base_height, yinc);

    /* constrain aspect ratio, according to:
     *
     * min_aspect.x       width      max_aspect.x
     * ------------  <= -------- <=  -----------
     * min_aspect.y       height     max_aspect.y
     */
    if ((flags & PAspect) && hints->min_aspect.y > 0 && hints->max_aspect.x > 0)
    {
	/* Use 64 bit arithmetic to prevent overflow */

	uint64_t min_aspect_x = hints->min_aspect.x;
	uint64_t min_aspect_y = hints->min_aspect.y;
	uint64_t max_aspect_x = hints->max_aspect.x;
	uint64_t max_aspect_y = hints->max_aspect.y;
	uint64_t delta;

	if (min_aspect_x * height > width * min_aspect_y)
	{
	    delta = FLOOR64 (height - width * min_aspect_y / min_aspect_x,
			     yinc);
	    if (height - (int) delta >= min_height)
		height -= delta;
	    else
	    {
		delta = FLOOR64 (height * min_aspect_x / min_aspect_y - width,
				 xinc);
		if (width + (int) delta <= max_width)
		    width += delta;
	    }
	}

	if (width * max_aspect_y > max_aspect_x * height)
	{
	    delta = FLOOR64 (width - height * max_aspect_x / max_aspect_y,
			     xinc);
	    if (width - (int) delta >= min_width)
		width -= delta;
	    else
	    {
		delta = FLOOR64 (width * min_aspect_y / min_aspect_x - height,
				 yinc);
		if (height + (int) delta <= max_height)
		    height += delta;
	    }
	}
    }

#undef CLAMP
#undef FLOOR64
#undef FLOOR

    if (width != oldWidth || height != oldHeight)
    {
	*newWidth  = width;
	*newHeight = height;

	return true;
    }

    return false;
}

void
CompWindow::hide ()
{
    bool onDesktop = onCurrentDesktop ();

    if (!priv->managed)
	return;

    if (!priv->minimized && !priv->inShowDesktopMode &&
	!priv->hidden && onDesktop)
    {
	if (priv->state & CompWindowStateShadedMask)
	{
	    priv->shaded = true;
	}
	else
	{
	    return;
	}
    }
    else
    {
	priv->shaded = false;

	if ((priv->state & CompWindowStateShadedMask) && priv->frame)
	    XUnmapWindow (screen->dpy (), priv->frame);
    }

    windowNotify (CompWindowNotifyHide);

    if (!priv->pendingMaps && priv->attrib.map_state != IsViewable)
	return;

    priv->pendingUnmaps++;

    if (priv->frame && !priv->shaded)
        XUnmapWindow (screen->dpy (), priv->frame);

    XUnmapWindow (screen->dpy (), priv->id);

    if (priv->minimized || priv->inShowDesktopMode ||
	priv->hidden || priv->shaded)
	changeState (priv->state | CompWindowStateHiddenMask);

    if (priv->shaded && priv->id == screen->activeWindow ())
	moveInputFocusTo ();
}

void
CompWindow::show ()
{
    Bool onDesktop = onCurrentDesktop ();

    if (!priv->managed)
	return;

    if (priv->minimized || priv->inShowDesktopMode ||
	priv->hidden || !onDesktop)
    {
	/* no longer hidden but not on current desktop */
	if (!priv->minimized && !priv->inShowDesktopMode && !priv->hidden)
	    changeState (priv->state & ~CompWindowStateHiddenMask);

	return;
    }

    /* transition from minimized to shaded */
    if (priv->state & CompWindowStateShadedMask)
    {
	priv->shaded = true;

	if (priv->frame)
	    XMapWindow (screen->dpy (), priv->frame);

	if (priv->height)
	    resize (priv->attrib.x, priv->attrib.y,
		    priv->attrib.width, ++priv->attrib.height - 1,
		    priv->attrib.border_width);

	return;
    }
    else
    {
	priv->shaded = false;
    }

    windowNotify (CompWindowNotifyShow);

    priv->pendingMaps++;

    XMapWindow (screen->dpy (), priv->id);

    changeState (priv->state & ~CompWindowStateHiddenMask);
}

void
PrivateWindow::minimizeTransients (CompWindow *w,
				   CompWindow *ancestor)
{
    if (w->priv->transientFor == ancestor->priv->id ||
	w->priv->isGroupTransient (ancestor->priv->clientLeader))
    {
	w->minimize ();
    }
}

void
CompWindow::minimize ()
{
    if (!priv->managed)
	return;

    if (!priv->minimized)
    {
	priv->minimized = true;

	screen->forEachWindow (
	    boost::bind (PrivateWindow::minimizeTransients, _1, this));

	hide ();
    }
}

void
PrivateWindow::unminimizeTransients (CompWindow *w,
				     CompWindow *ancestor)
{
    if (w->priv->transientFor == ancestor->priv->id ||
	w->priv->isGroupTransient (ancestor->priv->clientLeader))
	w->unminimize ();
}

void
CompWindow::unminimize ()
{
    if (priv->minimized)
    {
	priv->minimized = false;

	show ();

	screen->forEachWindow (
	    boost::bind (PrivateWindow::unminimizeTransients, _1, this));
    }
}

void
CompWindow::maximize (unsigned int state)
{
    if (overrideRedirect ())
	return;

    state = constrainWindowState (state, priv->actions);

    state &= MAXIMIZE_STATE;

    if (state == (priv->state & MAXIMIZE_STATE))
	return;

    state |= (priv->state & ~MAXIMIZE_STATE);

    changeState (state);
    updateAttributes (CompStackingUpdateModeNone);
}

bool
PrivateWindow::getUserTime (Time *time)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    result = XGetWindowProperty (screen->dpy (), priv->id,
				 Atoms::wmUserTime,
				 0L, 1L, False, XA_CARDINAL, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	CARD32 value;

	memcpy (&value, data, sizeof (CARD32));
	XFree ((void *) data);

	*time = (Time) value;
	return true;
    }

    return false;
}

void
PrivateWindow::setUserTime (Time time)
{
    CARD32 value = (CARD32) time;

    XChangeProperty (screen->dpy (), priv->id,
		     Atoms::wmUserTime,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &value, 1);
}

/*
 * Macros from metacity
 *
 * Xserver time can wraparound, thus comparing two timestamps needs to
 * take this into account.  Here's a little macro to help out.  If no
 * wraparound has occurred, this is equivalent to
 *   time1 < time2
 * Of course, the rest of the ugliness of this macro comes from
 * accounting for the fact that wraparound can occur and the fact that
 * a timestamp of 0 must be special-cased since it means older than
 * anything else.
 *
 * Note that this is NOT an equivalent for time1 <= time2; if that's
 * what you need then you'll need to swap the order of the arguments
 * and negate the result.
 */
#define XSERVER_TIME_IS_BEFORE_ASSUMING_REAL_TIMESTAMPS(time1, time2) \
    ( (( (time1) < (time2) ) &&					      \
       ( (time2) - (time1) < ((unsigned long) -1) / 2 )) ||	      \
      (( (time1) > (time2) ) &&					      \
       ( (time1) - (time2) > ((unsigned long) -1) / 2 ))	      \
	)
#define XSERVER_TIME_IS_BEFORE(time1, time2)				 \
    ( (time1) == 0 ||							 \
      (XSERVER_TIME_IS_BEFORE_ASSUMING_REAL_TIMESTAMPS (time1, time2) && \
       (time2) != 0)							 \
	)

bool
PrivateWindow::getUsageTimestamp (Time *timestamp)
{
    if (getUserTime (timestamp))
	return true;

    if (initialTimestampSet)
    {
	*timestamp = initialTimestamp;
	return true;
    }

    return false;
}

bool
PrivateWindow::isWindowFocusAllowed (Time timestamp)
{
    CompScreen   *s = screen;
    CompWindow   *active;
    Time	 wUserTime, aUserTime;
    bool         gotTimestamp = false;
    int          level;
    CompPoint    dvp;

    level = s->getOption ("focus_prevention_level")->value ().i ();

    if (level == FOCUS_PREVENTION_LEVEL_NONE)
	return true;

    if (timestamp)
    {
	/* the caller passed a timestamp, so use that
	   instead of the window's user time */
	wUserTime = timestamp;
	gotTimestamp = true;
    }
    else
    {
	gotTimestamp = getUsageTimestamp (&wUserTime);
    }

    /* if we got no timestamp for the window, try to get at least a timestamp
       for its transient parent, if any */
    if (!gotTimestamp && transientFor)
    {
	CompWindow *parent;

	parent = screen->findWindow (transientFor);
	if (parent)
	    gotTimestamp =
		parent->priv->getUsageTimestamp (&wUserTime);
    }

    if (gotTimestamp && !wUserTime)
    {
	/* window explicitly requested no focus */
	return false;
    }

    /* allow focus for excluded windows */
    CompMatch &match =
	s->getOption ("focus_prevention_match")->value ().match ();
    if (!match.evaluate (window))
	return true;

    if (level == FOCUS_PREVENTION_LEVEL_VERYHIGH)
	return false;

    /* not in current viewport */
    dvp = window->defaultViewport ();
    if (dvp.x () != s->vp ().x () || dvp.y () != s->vp ().y ())
	return false;

    if (!gotTimestamp)
    {
	/* unsure as we have nothing to compare - allow focus in low level,
	   don't allow in high level */
	if (level == FOCUS_PREVENTION_LEVEL_HIGH)
	    return false;

	return true;
    }

    /* can't get user time for active window */
    active = s->findWindow (s->activeWindow ());
    if (!active || !active->priv->getUserTime (&aUserTime))
	return true;

    if (XSERVER_TIME_IS_BEFORE (wUserTime, aUserTime))
	return false;

    return true;
}

bool
PrivateWindow::allowWindowFocus (unsigned int noFocusMask,
				 Time         timestamp)
{
    bool retval;

    if (priv->id == screen->activeWindow ())
	return true;

    /* do not focus windows of these types */
    if (priv->type & noFocusMask)
	return false;

    /* window doesn't take focus */
    if (!priv->inputHint && !(priv->protocols & CompWindowProtocolTakeFocusMask))
	return false;

    retval = priv->isWindowFocusAllowed (timestamp);

    if (!retval)
    {
	/* add demands attention state if focus was prevented */
	window->changeState (priv->state | CompWindowStateDemandsAttentionMask);
    }

    return retval;
}

CompPoint
CompWindow::defaultViewport ()
{
    int vx, vy;
    screen->viewportForGeometry (priv->serverGeometry, &vx, &vy);
    return CompPoint (vx, vy);
}

/* returns icon with dimensions as close as possible to width and height
   but never greater. */
CompIcon *
CompWindow::getIcon (int width, int height)
{
    CompIcon     *icon;
    int          wh, diff, oldDiff;
    unsigned int i;

    /* need to fetch icon property */
    if (priv->icons.size () == 0 && !priv->noIcons)
    {
	Atom	      actual;
	int	      result, format;
	unsigned long n, left;
	unsigned char *data;

	result = XGetWindowProperty (screen->dpy (), priv->id,
				     Atoms::wmIcon,
				     0L, 65536L,
				     FALSE, XA_CARDINAL,
				     &actual, &format, &n,
				     &left, &data);

	if (result == Success && n && data)
	{
	    CARD32   *p;
	    CARD32   alpha, red, green, blue;
	    int      iw, ih, j;

	    for (i = 0; i + 2 < n; i += iw * ih + 2)
	    {
		unsigned long *idata = (unsigned long *) data;

		iw = idata[i];
		ih = idata[i + 1];

		if (iw * ih + 2 > (int) (n - i))
		    break;

		if (iw && ih)
		{
		    icon = new CompIcon (screen, iw, ih);
		    if (!icon)
			continue;

		    priv->icons.push_back (icon);

		    p = (CARD32 *) (icon->data ());

		    /* EWMH doesn't say if icon data is premultiplied or
		       not but most applications seem to assume data should
		       be unpremultiplied. */
		    for (j = 0; j < iw * ih; j++)
		    {
			alpha = (idata[i + j + 2] >> 24) & 0xff;
			red   = (idata[i + j + 2] >> 16) & 0xff;
			green = (idata[i + j + 2] >>  8) & 0xff;
			blue  = (idata[i + j + 2] >>  0) & 0xff;

			red   = (red   * alpha) >> 8;
			green = (green * alpha) >> 8;
			blue  = (blue  * alpha) >> 8;

			p[j] =
			    (alpha << 24) |
			    (red   << 16) |
			    (green <<  8) |
			    (blue  <<  0);
		    }
		}
	    }

	    XFree (data);
	}

	/* don't fetch property again */
	if (priv->icons.size() == 0)
	    priv->noIcons = true;
    }

    /* no icons available for this window */
    if (priv->noIcons)
	return NULL;

    icon = NULL;
    wh   = width + height;

    for (i = 0; i < priv->icons.size (); i++)
    {
	if ((int) priv->icons[i]->width () > width ||
	    (int) priv->icons[i]->height () > height)
	    continue;

	if (icon)
	{
	    diff    = wh - (priv->icons[i]->width () +
		      priv->icons[i]->height ());
	    oldDiff = wh - (icon->width () + icon->height ());

	    if (diff < oldDiff)
		icon = priv->icons[i];
	}
	else
	    icon = priv->icons[i];
    }

    return icon;
}

void
PrivateWindow::freeIcons ()
{
    for (unsigned int i = 0; i < priv->icons.size (); i++)
    {
	delete priv->icons[i];
    }

    priv->icons.resize (0);
    priv->noIcons = false;
}

int
CompWindow::outputDevice ()
{
    return screen->outputDeviceForGeometry (priv->serverGeometry);
}

bool
CompWindow::onCurrentDesktop ()
{
    if (priv->desktop == 0xffffffff || priv->desktop ==
	screen->currentDesktop ())
	return true;

    return false;
}

void
PrivateWindow::setDesktop (unsigned int desktop)
{
    if (desktop != 0xffffffff)
    {
	if (priv->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
	    return;

	if (desktop >= screen->nDesktop ())
	    return;
    }

    if (desktop == priv->desktop)
	return;

    priv->desktop = desktop;

    if (desktop == 0xffffffff || desktop == screen->currentDesktop ())
	window->show ();
    else
	window->hide ();

    screen->setWindowProp (priv->id, Atoms::winDesktop, priv->desktop);
}

/* The compareWindowActiveness function compares the two windows 'w1'
   and 'w2'. It returns an integer less than, equal to, or greater
   than zero if 'w1' is found, respectively, to activated longer time
   ago than, to be activated at the same time, or be activated more
   recently than 'w2'. */
int
PrivateWindow::compareWindowActiveness (CompWindow *w1,
					CompWindow *w2)
{
    CompActiveWindowHistory *history = screen->currentHistory ();
    int			    i;

    /* check current window history first */
    for (i = 0; i < ACTIVE_WINDOW_HISTORY_SIZE; i++)
    {
	if (history->id[i] == w1->priv->id)
	    return 1;

	if (history->id[i] == w2->priv->id)
	    return -1;

	if (!history->id[i])
	    break;
    }

    return w1->priv->activeNum - w2->priv->activeNum;
}

bool
CompWindow::onAllViewports ()
{
    if (overrideRedirect ())
	return true;

    if (!priv->managed && priv->attrib.map_state != IsViewable)
	return true;

    if (priv->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
	return true;

    if (priv->state & CompWindowStateStickyMask)
	return true;

    return false;
}

CompPoint
CompWindow::getMovementForOffset (CompPoint offset)
{
    CompScreen *s = screen;
    int         m, vWidth, vHeight;
    int         offX = offset.x (), offY = offset.y ();
    CompPoint   rv;

    vWidth = s->size().width () * s->vpSize ().width ();
    vHeight = s->size().height () * s->vpSize ().height ();

    offX %= vWidth;
    offY %= vHeight;

    /* x */
    if (s->vpSize ().width () == 1)
    {
	rv.setX (offX);
    }
    else
    {
	m = priv->attrib.x + offX;
	if (m - priv->input.left < (int) s->size().width () - vWidth)
	    rv.setX (offX + vWidth);
	else if (m + priv->width + priv->input.right > vWidth)
	    rv.setX (offX - vWidth);
	else
	    rv.setX (offX);
    }

    if (s->vpSize ().height () == 1)
    {
	rv.setY (offY);
    }
    else
    {
	m = priv->attrib.y + offY;
	if (m - priv->input.top < (int) s->size().height () - vHeight)
	    rv.setY (offY + vHeight);
	else if (m + priv->height + priv->input.bottom > vHeight)
	    rv.setY (offY - vHeight);
	else
	    rv.setY (offY);
    }

    return rv;
}

void
WindowInterface::getOutputExtents (CompWindowExtents *output)
    WRAPABLE_DEF (getOutputExtents, output)

void
WindowInterface::getAllowedActions (unsigned int *setActions,
				    unsigned int *clearActions)
    WRAPABLE_DEF (getAllowedActions, setActions, clearActions)

bool
WindowInterface::focus ()
    WRAPABLE_DEF (focus)

void
WindowInterface::activate ()
    WRAPABLE_DEF (activate)

bool
WindowInterface::place (int x, int y, int *newX, int *newY)
    WRAPABLE_DEF (place, x, y, newX, newY)

void
WindowInterface::validateResizeRequest (unsigned int   *mask,
					XWindowChanges *xwc)
    WRAPABLE_DEF (validateResizeRequest, mask, xwc)

void
WindowInterface::resizeNotify (int dx, int dy, int dwidth, int dheight)
    WRAPABLE_DEF (resizeNotify, dx, dy, dwidth, dheight)

void
WindowInterface::moveNotify (int dx, int dy, bool immediate)
    WRAPABLE_DEF (moveNotify, dx, dy, immediate)

void
WindowInterface::windowNotify (CompWindowNotify n)
    WRAPABLE_DEF (windowNotify, n)

		
void
WindowInterface::grabNotify (int x,
			     int y,
			     unsigned int state,
			     unsigned int mask)
    WRAPABLE_DEF (grabNotify, x, y, state, mask)

void
WindowInterface::ungrabNotify ()
    WRAPABLE_DEF (ungrabNotify)

void
WindowInterface::stateChangeNotify (unsigned int lastState)
    WRAPABLE_DEF (stateChangeNotify, lastState)

void
WindowInterface::updateFrameRegion (Region region)
    WRAPABLE_DEF (updateFrameRegion, region)

Window
CompWindow::id ()
{
    return priv->id;
}

unsigned int
CompWindow::type ()
{
    return priv->type;
}
	
unsigned int &
CompWindow::state ()
{
    return priv->state;
}
	
unsigned int
CompWindow::actions ()
{
    return priv->actions;
}
	
unsigned int &
CompWindow::protocols ()
{
    return priv->protocols;
}

void
CompWindow::close (Time serverTime)
{
    if (serverTime == 0)
	serverTime = screen->getCurrentTime ();

    if (priv->alive)
    {
	if (priv->protocols & CompWindowProtocolDeleteMask)
	{
	    XEvent ev;

	    ev.type		    = ClientMessage;
	    ev.xclient.window	    = priv->id;
	    ev.xclient.message_type = Atoms::wmProtocols;
	    ev.xclient.format	    = 32;
	    ev.xclient.data.l[0]    = Atoms::wmDeleteWindow;
	    ev.xclient.data.l[1]    = serverTime;
	    ev.xclient.data.l[2]    = 0;
	    ev.xclient.data.l[3]    = 0;
	    ev.xclient.data.l[4]    = 0;

	    XSendEvent (screen->dpy (), priv->id, false,
			NoEventMask, &ev);
	}
	else
	{
	    XKillClient (screen->dpy (), priv->id);
	}

	priv->closeRequests++;
    }
    else
    {
	screen->toolkitAction (Atoms::toolkitActionForceQuitDialog,
				     serverTime, priv->id, true, 0, 0);
    }

    priv->lastCloseRequestTime = serverTime;
}

bool
PrivateWindow::handlePingTimeout (unsigned int lastPing)
{
    if (!window->isViewable ())
	return false;

    if (!(priv->type & CompWindowTypeNormalMask))
	return false;

    if (priv->protocols & CompWindowProtocolPingMask)
    {
	if (priv->transientFor)
	    return false;

	if (priv->lastPong < lastPing)
	{
	    if (priv->alive)
	    {
		priv->alive = false;

		window->windowNotify (CompWindowNotifyAliveChanged);

		if (priv->closeRequests)
		{
		    screen->toolkitAction (
			Atoms::toolkitActionForceQuitDialog,
			priv->lastCloseRequestTime,
			priv->id, true, 0, 0);

		    priv->closeRequests = 0;
		}
	    }
	}

	return true;
    }
    return false;
}

void
PrivateWindow::handlePing (int lastPing)
{
    if (!priv->alive)
    {
	priv->alive = true;

	window->windowNotify (CompWindowNotifyAliveChanged);

	if (priv->lastCloseRequestTime)
	{
	    screen->toolkitAction (
		Atoms::toolkitActionForceQuitDialog,
		priv->lastCloseRequestTime,
		priv->id, false, 0, 0);

	    priv->lastCloseRequestTime = 0;
	}
    }
    priv->lastPong = lastPing;
}

void
PrivateWindow::processMap ()
{
    bool                   allowFocus;
    CompStackingUpdateMode stackingMode;

    priv->initialViewport.setX (screen->vp ().x ());
    priv->initialViewport.setY (screen->vp ().y ());

    priv->initialTimestampSet = false;

    screen->priv->applyStartupProperties (window);

    if (!priv->placed)
    {
	int            newX, newY;
	int            gravity = priv->sizeHints.win_gravity;
	XWindowChanges xwc;
	unsigned int   xwcm;

	/* adjust for gravity */
	xwc.x      = priv->serverGeometry.x ();
	xwc.y      = priv->serverGeometry.y ();
	xwc.width  = priv->serverGeometry.width ();
	xwc.height = priv->serverGeometry.height ();

	xwcm = adjustConfigureRequestForGravity (&xwc, CWX | CWY, gravity);

	if (window->place (xwc.x, xwc.y, &newX, &newY))
	{
	    xwc.x = newX;
	    xwc.y = newY;
	    xwcm |= CWX | CWY;
	}

	if (xwcm)
	    window->configureXWindow (xwcm, &xwc);

	priv->placed = true;
    }

    allowFocus = allowWindowFocus (NO_FOCUS_MASK, 0);

    if (!allowFocus && (priv->type & ~NO_FOCUS_MASK))
	stackingMode = CompStackingUpdateModeInitialMapDeniedFocus;
    else
	stackingMode = CompStackingUpdateModeInitialMap;

    window->updateAttributes (stackingMode);

    if (priv->minimized)
	window->unminimize ();

    screen->leaveShowDesktopMode (window);

    if (!(priv->state & CompWindowStateHiddenMask))
    {
	priv->pendingMaps++;
	XMapWindow (screen->dpy (), priv->id);
    }

    if (allowFocus)
	window->moveInputFocusTo ();
}


Region
CompWindow::region ()
{
    return priv->region;
}

Region
CompWindow::frameRegion ()
{
    return priv->frameRegion;
}

bool
CompWindow::inShowDesktopMode ()
{
    return priv->inShowDesktopMode;
}

void
CompWindow::setShowDesktopMode (bool value)
{
    priv->inShowDesktopMode = value;
}

bool
CompWindow::managed ()
{
    return priv->managed;
}

bool
CompWindow::grabbed ()
{
    return priv->grabbed;
}

unsigned int &
CompWindow::wmType ()
{
    return priv->wmType;
}

unsigned int
CompWindow::activeNum ()
{
    return priv->activeNum;
}

Window
CompWindow::frame ()
{
    return priv->frame;
}

int
CompWindow::mapNum ()
{
    return priv->mapNum;
}

CompStruts *
CompWindow::struts ()
{
    return priv->struts;
}

int &
CompWindow::saveMask ()
{
    return priv->saveMask;
}

XWindowChanges &
CompWindow::saveWc ()
{
    return priv->saveWc;
}

void
CompWindow::moveToViewportPosition (int x, int y, bool sync)
{
    int	tx, vWidth = screen->size().width () *
		     screen->vpSize ().width ();
    int ty, vHeight = screen->size().height () *
		      screen->vpSize ().height ();

    if (screen->vpSize ().width () != 1)
    {
	x += screen->vp ().x () * screen->size().width ();
	x = MOD (x, vWidth);
	x -= screen->vp ().x () * screen->size().width ();
    }

    if (screen->vpSize ().height () != 1)
    {
	y += screen->vp ().y () * screen->size().height ();
	y = MOD (y, vHeight);
	y -= screen->vp ().y () * screen->size().height ();
    }

    tx = x - priv->attrib.x;
    ty = y - priv->attrib.y;

    if (tx || ty)
    {
	int m, wx, wy;

	if (!priv->managed)
	    return;

	if (priv->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
	    return;

	if (priv->state & CompWindowStateStickyMask)
	    return;

	wx = tx;
	wy = ty;

	if (screen->vpSize ().width ()!= 1)
	{
	    m = priv->attrib.x + tx;

	    if (m - priv->output.left <
		(int) screen->size().width () - vWidth)
		wx = tx + vWidth;
	    else if (m + priv->width + priv->output.right > vWidth)
		wx = tx - vWidth;
	}

	if (screen->vpSize ().height () != 1)
	{
	    m = priv->attrib.y + ty;

	    if (m - priv->output.top <
		(int) screen->size().height () - vHeight)
		wy = ty + vHeight;
	    else if (m + priv->height + priv->output.bottom > vHeight)
		wy = ty - vHeight;
	}

	if (priv->saveMask & CWX)
	    priv->saveWc.x += wx;

	if (priv->saveMask & CWY)
	    priv->saveWc.y += wy;

	move (wx, wy);

	if (sync)
	    syncPosition ();
    }
}

char *
CompWindow::startupId ()
{
     return priv->startupId;
}

void
PrivateWindow::applyStartupProperties (CompStartupSequence *s)
{
    int workspace;

    priv->initialViewport.setX (s->viewportX);
    priv->initialViewport.setY (s->viewportY);

    workspace = sn_startup_sequence_get_workspace (s->sequence);
    if (workspace >= 0)
	priv->desktop = workspace;

    priv->initialTimestamp    =
	sn_startup_sequence_get_timestamp (s->sequence);
    priv->initialTimestampSet = true;
}

unsigned int
CompWindow::desktop ()
{
    return priv->desktop;
}

Window
CompWindow::clientLeader ()
{
    return priv->clientLeader;
}

Window
CompWindow::transientFor ()
{
    return priv->transientFor;
}

int
CompWindow::pendingUnmaps ()
{
    return priv->pendingUnmaps;
}

bool
CompWindow::minimized ()
{
    return priv->minimized;
}

bool
CompWindow::placed ()
{
    return priv->placed;
}

bool
CompWindow::shaded ()
{
    return priv->shaded;
}

CompSize
CompWindow::size ()
{
    return CompSize (priv->width, priv->height);
}

CompWindowExtents
CompWindow::input ()
{
    return priv->input;
}

CompWindowExtents
CompWindow::output ()
{
    return priv->output;
}

XSizeHints
CompWindow::sizeHints ()
{
    return priv->sizeHints;
}

void
PrivateWindow::updateMwmHints ()
{
    screen->priv->getMwmHints (priv->id, &priv->mwmFunc,
				     &priv->mwmDecor);

    window->recalcActions ();
}

void
PrivateWindow::updateStartupId ()
{
    if (startupId)
	free (startupId);

    startupId = getStartupId ();
}

bool
CompWindow::destroyed ()
{
    return priv->destroyed;
}

bool
CompWindow::invisible ()
{
    return priv->invisible;
}

XSyncAlarm
CompWindow::syncAlarm ()
{
    return priv->syncAlarm;
}


CompWindow::CompWindow (Window     id,
			Window     aboveId) :
   CompPrivateStorage (&windowPrivateIndices, 1)
{
    priv = new PrivateWindow (this);
    assert (priv);

    priv->region = XCreateRegion ();
    assert (priv->region);
    priv->frameRegion = XCreateRegion ();
    assert (priv->frameRegion);



    /* Failure means that window has been destroyed. We still have to add the
       window to the window list as we might get configure requests which
       require us to stack other windows relative to it. Setting some default
       values if this is the case. */
    if (!XGetWindowAttributes (screen->dpy (), id, &priv->attrib))
	setDefaultWindowAttributes (&priv->attrib);


    priv->serverGeometry.set (priv->attrib.x, priv->attrib.y,
			      priv->attrib.width, priv->attrib.height,
			      priv->attrib.border_width);
    priv->syncGeometry.set (priv->attrib.x, priv->attrib.y,
			    priv->attrib.width, priv->attrib.height,
			    priv->attrib.border_width);
    priv->geometry.set (priv->attrib.x, priv->attrib.y,
			priv->attrib.width, priv->attrib.height,
			priv->attrib.border_width);

    priv->width  = priv->attrib.width  + priv->attrib.border_width * 2;
    priv->height = priv->attrib.height + priv->attrib.border_width * 2;

    priv->sizeHints.flags = 0;

    priv->recalcNormalHints ();

    priv->transientFor = None;
    priv->clientLeader = None;

    XSelectInput (screen->dpy (), id,
		  PropertyChangeMask |
		  EnterWindowMask    |
		  FocusChangeMask);

    priv->id = id;

    XGrabButton (screen->dpy (), AnyButton, AnyModifier, priv->id, TRUE,
		 ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
		 GrabModeSync, GrabModeSync, None, None);

    priv->alpha     = (depth () == 32);
    priv->lastPong  = screen->priv->lastPing;

    if (screen->XShape ())
	XShapeSelectInput (screen->dpy (), id, ShapeNotifyMask);

    screen->insertWindow (this, aboveId);

    EMPTY_REGION (priv->region);

    if (windowClass () != InputOnly)
    {
	REGION rect;

	rect.rects = &rect.extents;
	rect.numRects = rect.size = 1;

	rect.extents.x1 = priv->attrib.x;
	rect.extents.y1 = priv->attrib.y;
	rect.extents.x2 = priv->attrib.x + priv->width;
	rect.extents.y2 = priv->attrib.y + priv->height;

	XUnionRegion (&rect, priv->region, priv->region);

	/* need to check for DisplayModal state on all windows */
	priv->state = screen->priv->getWindowState (priv->id);

	priv->updateClassHints ();
    }
    else
    {
	priv->attrib.map_state = IsUnmapped;
    }

    priv->wmType    = screen->priv->getWindowType (priv->id);
    priv->protocols = screen->priv->getProtocols (priv->id);

    if (!overrideRedirect ())
    {
	priv->updateNormalHints ();
	priv->updateStruts ();
	priv->updateWmHints ();
	priv->updateTransientHint ();

	priv->clientLeader = priv->getClientLeader ();
	if (!priv->clientLeader)
	    priv->startupId = priv->getStartupId ();

	recalcType ();

	screen->priv->getMwmHints (priv->id, &priv->mwmFunc, &priv->mwmDecor);

	if (!(priv->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask)))
	{
	    priv->desktop = screen->getWindowProp (priv->id, Atoms::winDesktop,
					           priv->desktop);
	    if (priv->desktop != 0xffffffff)
	    {
		if (priv->desktop >= screen->nDesktop ())
		    priv->desktop = screen->currentDesktop ();
	    }
	}
    }
    else
    {
	recalcType ();
    }

    if (priv->attrib.map_state == IsViewable)
    {
	priv->placed = true;

	if (!overrideRedirect ())
	{
	    priv->managed = true;

	    if (screen->priv->getWmState (priv->id) == IconicState)
	    {
		if (priv->state & CompWindowStateShadedMask)
		    priv->shaded = true;
		else
		    priv->minimized = true;
	    }
	    else
	    {
		if (priv->wmType & (CompWindowTypeDockMask |
				 CompWindowTypeDesktopMask))
		{
		    priv->setDesktop (0xffffffff);
		}
		else
		{
		    if (priv->desktop != 0xffffffff)
			priv->desktop = screen->currentDesktop ();

		    screen->setWindowProp (priv->id, Atoms::winDesktop,
				           priv->desktop);
		}
	    }
	}

	priv->attrib.map_state = IsUnmapped;
	priv->pendingMaps++;

	map ();

	updateAttributes (CompStackingUpdateModeNormal);

	if (priv->minimized || priv->inShowDesktopMode ||
	    priv->hidden || priv->shaded)
	{
	    priv->state |= CompWindowStateHiddenMask;

	    priv->pendingUnmaps++;

	    XUnmapWindow (screen->dpy (), priv->id);

	    screen->priv->setWindowState (priv->state, priv->id);
	}
    }
    else if (!overrideRedirect ())
    {
	if (screen->priv->getWmState (priv->id) == IconicState)
	{
	    priv->managed = true;
	    priv->placed  = true;

	    if (priv->state & CompWindowStateHiddenMask)
	    {
		if (priv->state & CompWindowStateShadedMask)
		    priv->shaded = true;
		else
		    priv->minimized = true;
	    }
	}
    }

    /* TODO: bailout properly when objectInitPlugins fails */
    assert (CompPlugin::windowInitPlugins (this));

    recalcActions ();
    priv->updateIconGeometry ();

    if (priv->shaded)
	resize (priv->attrib.x, priv->attrib.y,
		priv->attrib.width, ++priv->attrib.height - 1,
		priv->attrib.border_width);

    if (priv->attrib.map_state == IsViewable)
    {
	priv->invisible = WINDOW_INVISIBLE (priv);
    }
}

CompWindow::~CompWindow ()
{
    screen->unhookWindow (this);

    if (!priv->destroyed)
    {
	if (priv->frame)
	    priv->unreparent ();

	/* restore saved geometry and map if hidden */
	if (!priv->attrib.override_redirect)
	{
	    if (priv->saveMask)
		XConfigureWindow (screen->dpy (), priv->id,
				  priv->saveMask, &priv->saveWc);

	    if (!priv->hidden)
	    {
		if (priv->state & CompWindowStateHiddenMask)
		    XMapWindow (screen->dpy (), priv->id);
	    }
	}

	if (screen->XShape ())
	    XShapeSelectInput (screen->dpy (), priv->id, NoEventMask);

	XSelectInput (screen->dpy (), priv->id, NoEventMask);

	XUngrabButton (screen->dpy (), AnyButton, AnyModifier, priv->id);
    }

    if (priv->attrib.map_state == IsViewable)
    {
	if (priv->type == CompWindowTypeDesktopMask)
	    screen->priv->desktopWindowCount--;

	if (priv->destroyed && priv->struts)
	    screen->priv->updateWorkarea ();
    }

    if (priv->destroyed)
	screen->priv->updateClientList ();

    CompPlugin::windowFiniPlugins (this);

    delete priv;
}

PrivateWindow::PrivateWindow (CompWindow *window) :
    priv (this),
    window (window),
    refcnt (1),
    id (None),
    frame (None),
    wrapper (None),
    mapNum (0),
    activeNum (0),
    transientFor (None),
    clientLeader (None),
    inputHint (true),
    alpha (false),
    width (0),
    height (0),
    region (0),
    wmType (0),
    type (CompWindowTypeUnknownMask),
    state (0),
    actions (0),
    protocols (0),
    mwmDecor (MwmDecorAll),
    mwmFunc (MwmFuncAll),
    invisible (true),
    destroyed (false),
    managed (false),
    destroyRefCnt (1),
    unmapRefCnt (1),

    initialViewport (0, 0),

    initialTimestamp (0),
    initialTimestampSet (false),

    placed (false),
    minimized (false),
    inShowDesktopMode (false),
    shaded (false),
    hidden (false),
    grabbed (false),

    desktop (0),

    pendingUnmaps (0),
    pendingMaps (0),

    startupId (0),
    resName (0),
    resClass (0),

    group (0),

    lastPong (0),
    alive (true),

    struts (0),

    icons (0),
    noIcons (false),

    iconGeometrySet (false),

    saveMask (0),
    syncCounter (0),
    syncAlarm (None),
    syncWaitTimer (),

    syncWait (false),
    closeRequests (false),
    lastCloseRequestTime (0)
{
    iconGeometry.x      = 0;
    iconGeometry.y      = 0;
    iconGeometry.width  = 0;
    iconGeometry.height = 0;
    iconGeometrySet     = FALSE;

    input.left   = 0;
    input.right  = 0;
    input.top    = 0;
    input.bottom = 0;

    output.left   = 0;
    output.right  = 0;
    output.top    = 0;
    output.bottom = 0;
}

PrivateWindow::~PrivateWindow ()
{
     if (syncAlarm)
	XSyncDestroyAlarm (screen->dpy (), syncAlarm);

    syncWaitTimer.stop ();

    if (frame)
	XDestroyWindow (screen->dpy (), frame);

    if (region)
	XDestroyRegion (region);

    if (frameRegion)
	XDestroyRegion (frameRegion);

    if (struts)
	free (struts);

    if (icons.size ())
	freeIcons ();

    if (startupId)
	free (startupId);

    if (resName)
	free (resName);

    if (resClass)
	free (resClass);
}

bool
CompWindow::syncWait ()
{
    return priv->syncWait;
}

bool
CompWindow::alpha ()
{
    return priv->alpha;
}

bool
CompWindow::overrideRedirect ()
{
    return priv->attrib.override_redirect;
}

void
PrivateWindow::setOverrideRedirect (bool overrideRedirect)
{
    if (overrideRedirect == window->overrideRedirect ())
	return;

    priv->attrib.override_redirect = overrideRedirect ? 1 : 0;
    window->recalcType ();
    window->recalcActions ();

    screen->matchPropertyChanged (window);
}

bool
CompWindow::isViewable ()
{
    return priv->attrib.map_state == IsViewable;
}

int
CompWindow::windowClass ()
{
    return priv->attrib.c_class;
}

unsigned int
CompWindow::depth ()
{
    return priv->attrib.depth;
}

bool
CompWindow::alive ()
{
    return priv->alive;
}

unsigned int
CompWindow::mwmDecor ()
{
    return priv->mwmDecor;
}

unsigned int
CompWindow::mwmFunc ()
{
    return priv->mwmFunc;
}

void
CompWindow::updateFrameRegion ()
{
    REGION r;
    int    x, y;

    if ((priv->input.left || priv->input.right ||
	priv->input.top || priv->input.bottom) && priv->frame)
    {

	XSubtractRegion (&emptyRegion, &emptyRegion, priv->frameRegion);

	updateFrameRegion (priv->frameRegion);

	r.numRects = 1;
	r.rects = &r.extents;
	r.extents = priv->region->extents;
	XSubtractRegion (priv->frameRegion, &r, priv->frameRegion);

	r.extents.x1 -= priv->input.left;
	r.extents.x2 += priv->input.right;
	r.extents.y1 -= priv->input.top;
	r.extents.y2 += priv->input.bottom;

	XIntersectRegion (priv->frameRegion, &r, priv->frameRegion);
	
	XUnionRegion (priv->frameRegion, priv->region, priv->frameRegion);

	x = priv->serverGeometry.x () - priv->input.left;
	y = priv->serverGeometry.y () - priv->input.top;


	XShapeCombineRegion (screen->dpy (), priv->frame,
			     ShapeBounding, -x, -y, priv->frameRegion,
			     ShapeSet);
    }
}

void
CompWindow::setWindowFrameExtents (CompWindowExtents *i)
{
    if (priv->input.left   != i->left  ||
	priv->input.right  != i->right ||
	priv->input.top    != i->top   ||
	priv->input.bottom != i->bottom)
    {
	unsigned long data[4];

	priv->input = *i;

	priv->updateSize ();
	priv->updateFrameWindow ();
	recalcActions ();

	data[0] = i->left;
	data[1] = i->right;
	data[2] = i->top;
	data[3] = i->bottom;

	XChangeProperty (screen->dpy (), priv->id,
			 Atoms::frameExtents,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) data, 4);
    }
}

void
CompWindow::updateFrameRegion (Region region)
    WRAPABLE_HND_FUNC(12, updateFrameRegion, region)

bool
PrivateWindow::reparent ()
{
    XSetWindowAttributes attr;
    XWindowChanges	 xwc;
    int                  mask;
    XEvent               e;

    Display              *dpy = screen->dpy ();

    if (frame)
	return false;

    XSync (dpy, FALSE);

    if (XCheckTypedWindowEvent (dpy, id, DestroyNotify, &e))
    {
        XPutBackEvent (dpy, &e);
        return false;
    }

    mask = CWOverrideRedirect | CWEventMask | CWColormap | CWBackPixmap;

    attr.background_pixmap = None;
    attr.border_pixel      = 0;
    mask |= CWBorderPixel;
    attr.colormap          = attrib.colormap;
    attr.override_redirect = FALSE;
    attr.event_mask        = SubstructureRedirectMask | StructureNotifyMask |
			     SubstructureNotifyMask | EnterWindowMask |
			     LeaveWindowMask;

    frame = XCreateWindow(dpy, screen->root (), attrib.x - input.left,
			  attrib.y - input.top,
			  attrib.width + input.left + input.right,
			  attrib.height + input.top + input.bottom,
			  0, attrib.depth,
			  InputOutput, attrib.visual, mask, &attr);

    wrapper = XCreateWindow(dpy, frame, input.left, input.top,
			    attrib.width, attrib.height, 0, attrib.depth,
			    InputOutput, attrib.visual, mask, &attr);

    attr.event_mask = PropertyChangeMask | FocusChangeMask |
		      EnterWindowMask | LeaveWindowMask;
    attr.do_not_propagate_mask = ButtonPressMask | ButtonReleaseMask |
				 ButtonMotionMask;

    XGrabButton (dpy, AnyButton, AnyModifier, frame, TRUE,
		 ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
		 GrabModeSync, GrabModeSync, None, None);

    xwc.stack_mode = Below;
    xwc.sibling    = id;

    XConfigureWindow (dpy, frame, CWSibling | CWStackMode, &xwc);

    XMapWindow (dpy, wrapper);

    XReparentWindow (dpy, id, wrapper, 0, 0);
    XChangeSaveSet (dpy, id, SetModeInsert);
    XFlush (dpy);

    XChangeWindowAttributes(dpy, id, CWEventMask | CWDontPropagate, &attr);


    if (mapNum || shaded)
    {
	XMapWindow (dpy, frame);
	XSync (dpy, FALSE);
	if (XCheckTypedWindowEvent (dpy, id, FocusIn, &e) ||
	    screen->activeWindow () == id)
	    window->moveInputFocusTo ();
	pendingUnmaps++;
	pendingMaps++;
    }

    window->windowNotify (CompWindowNotifyReparent);

    return true;
}

void
PrivateWindow::unreparent ()
{
    Display *dpy = screen->dpy ();
    XEvent         e;
    XWindowChanges xwc;

    bool alive = true;

    if (!frame)
	return;

    XSync (dpy, FALSE);

    if (XCheckTypedWindowEvent (dpy, id, DestroyNotify, &e))
    {
        XPutBackEvent (dpy, &e);
        alive = false;
    }

    if ((!destroyed) && alive)
    {
        XChangeSaveSet (dpy, id, SetModeDelete);
        XSelectInput (dpy, frame, NoEventMask);
        XReparentWindow (dpy, id, screen->root (), attrib.x, attrib.y);
	xwc.stack_mode = Above;
        xwc.sibling    = frame;
        XConfigureWindow (dpy, id, CWSibling | CWStackMode, &xwc);
        XUnmapWindow (dpy, frame);
	XFlush (dpy);
	
	if (mapNum || shaded)
	{
	    if (XCheckTypedWindowEvent (dpy, id, FocusIn, &e) ||
		screen->activeWindow () == id)
		window->moveInputFocusTo ();
	    pendingUnmaps++;
	    pendingMaps++;
	}
    }

    XDestroyWindow (dpy, frame);
    XDestroyWindow (dpy, wrapper);
    wrapper = None;
    frame = None;

    window->windowNotify (CompWindowNotifyUnreparent);
}