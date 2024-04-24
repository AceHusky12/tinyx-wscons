/*
 *
 * Copyright © 2024 Ace Husky <acehusky12@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Ace Husky not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Ace Husky makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * ACE HUSKY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ACE HUSKY BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

 /*
  * Derived from fbdev/fbdev.h
  */

/*
 *
 * Copyright © 1999 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WSCONSDEV_H_
#define _WSCONSDEV_H_
#include <stdio.h>
#include <dev/wscons/wsconsio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <kdrive.h>

#include "randrstr.h"

typedef struct _wsfbPriv {
	struct wsdisplay_fbinfo var;
	struct wsdisplayio_fbinfo info;
	struct wsdisplay_cmap cmap;
	u_char red[256];
	u_char blue[256];
	u_char green[256];
	int fd;
	char *fb;
	char *fb_base;
} FbdevPriv;

typedef struct _wsfbScrPriv {
	Rotation randr;
	Bool shadow;
} FbdevScrPriv;

const char *wsfbDevicePath;
Bool gray, revcolors;

Bool wsfbCardInit(KdCardInfo * card);

Bool wsfbScreenInit(KdScreenInfo * screen);

Bool wsfbInitScreen(ScreenPtr pScreen);

Bool wsfbFinishInitScreen(ScreenPtr pScreen);

Bool wsfbCreateResources(ScreenPtr pScreen);

void wsfbPreserve(KdCardInfo * card);

Bool wsfbEnable(ScreenPtr pScreen);

Bool wsfbDPMS(ScreenPtr pScreen, int mode);

void wsfbDisable(ScreenPtr pScreen);

void wsfbRestore(KdCardInfo * card);

void wsfbScreenFini(KdScreenInfo * screen);

void wsfbCardFini(KdCardInfo * card);

void wsfbGetColors(ScreenPtr pScreen, int n, xColorItem * pdefs);

void wsfbPutColors(ScreenPtr pScreen, int n, xColorItem * pdefs);

#endif				/* _WSCONSDEV_H_ */
