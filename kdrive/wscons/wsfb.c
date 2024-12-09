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
  * Derived from fbdev/fbdev.c
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

#ifdef HAVE_CONFIG_H
#include <kdrive-config.h>
#endif
#include "wsdev.h"
#include <sys/ioctl.h>

#include <termios.h>
#include <errno.h>

#include "apple_cmap.h"

extern int KdTsPhyScreen;

const char *wsfbDevicePath = NULL;

const KdCardFuncs wsfbFuncs = {
	wsfbCardInit,		/* cardinit */
	wsfbScreenInit,	/* scrinit */
	wsfbInitScreen,	/* initScreen */
	wsfbFinishInitScreen,	/* finishInitScreen */
	wsfbCreateResources,	/* createRes */
	wsfbPreserve,		/* preserve */
	NULL,		/* enable */
	wsfbDPMS,	/* dpms */
	wsfbDisable,		/* disable */
	wsfbRestore,		/* restore */
	wsfbScreenFini,	/* scrfini */
	wsfbCardFini,		/* cardfini */

	0,			/* initCursor */
	0,			/* enableCursor */
	0,			/* disableCursor */
	0,			/* finiCursor */
	0,			/* recolorCursor */

	0,			/* initAccel */
	0,			/* enableAccel */
	0,			/* disableAccel */
	0,			/* finiAccel */

	wsfbGetColors,		/* getColors */
	wsfbPutColors,		/* putColors */
};

static Bool wsfbMapFramebuffer(KdScreenInfo * screen);
static int wsfbUpdateFbColormap(FbdevPriv * priv, int minidx, int maxidx);
static void wsfbDefaultColormap(KdScreenInfo * screen);
static void wsfbDefaultColormapMono(KdScreenInfo * screen);

static Bool wsfbInitialize(KdCardInfo * card, FbdevPriv * priv)
{
	unsigned long off;

	if (wsfbDevicePath == NULL)
		wsfbDevicePath = "/dev/tty";

	if ((priv->fd = open(wsfbDevicePath, O_RDWR)) < 0) {
		ErrorF("Error opening framebuffer %s: %s\n",
		       wsfbDevicePath, strerror(errno));
		return FALSE;
	}


	/* quiet valgrind */
	memset(&priv->info, '\0', sizeof(priv->info));
	if (ioctl(priv->fd, WSDISPLAYIO_GET_FBINFO, &priv->info) < 0) {
		perror("Error: WSDISPLAYIO_GET_FBINFO");
		close(priv->fd);
		return FALSE;
	}

	int wsmode = WSDISPLAYIO_MODE_DUMBFB;

	if (ioctl(priv->fd, WSDISPLAYIO_SMODE, &wsmode) == -1)
		return FALSE;

	caddr_t addr = 0;
	priv->fb_base = (char *) mmap(addr,
				     priv->info.fbi_fbsize,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED, priv->fd, 0);


	if (priv->fb_base == (char *) -1) {
		perror("Error: mmap framebuffer fails!");
		close(priv->fd);
		return FALSE;
	}
	off = (unsigned long) priv->info.fbi_fboffset % (unsigned long) getpagesize();
	priv->fb = priv->fb_base + off;

	return TRUE;
}

Bool wsfbCardInit(KdCardInfo * card)
{
	FbdevPriv *priv;

	priv = malloc(sizeof(FbdevPriv));
	if (!priv)
		return FALSE;

	if (!wsfbInitialize(card, priv)) {
		free(priv);
		return FALSE;
	}
	card->driver = priv;

	return TRUE;
}

static Pixel wsfbMakeContig(Pixel orig, Pixel others)
{
	Pixel low;

	low = lowbit(orig) >> 1;
	while (low && (others & low) == 0) {
		orig |= low;
		low >>= 1;
	}
	return orig;
}

static Bool wsfbModeSupported(KdScreenInfo * screen, const KdMonitorTiming * t)
{
	return TRUE;
}

static Bool wsfbScreenInitialize(KdScreenInfo * screen, FbdevScrPriv * scrpriv)
{
	FbdevPriv *priv = screen->card->driver;
	int depth;
	struct wsdisplayio_fbinfo info;
	const KdMonitorTiming *t;
	int k;

	memset(&info, '\0', sizeof(info));
	k = ioctl(priv->fd, WSDISPLAYIO_GET_FBINFO, &info);

	if (!screen->width || !screen->height) {
		if (k >= 0) {
			screen->width = info.fbi_width;
			screen->height = info.fbi_height;
		} else {
			screen->width = 1024;
			screen->height = 768;
		}
		screen->rate = 103;	/* FIXME: should get proper value from fb driver */
	}
	if (!screen->fb.depth) {
		if (k >= 0)
			screen->fb.depth = info.fbi_bitsperpixel;
		else
			screen->fb.depth = 16;
	}

	if ((screen->width != info.fbi_width) || (screen->height != info.fbi_height)) {
		t = KdFindMode(screen, wsfbModeSupported);
		screen->rate = t->rate;
		screen->width = t->horizontal;
		screen->height = t->vertical;

	}

	int wstype;


	k = ioctl(priv->fd, WSDISPLAYIO_GTYPE, &wstype);

	if (k < 0) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		return FALSE;
	}

	memset(&priv->info, '\0', sizeof(priv->info));
	/* Re-get the "infoed" parameters since they might have changed */
	k = ioctl(priv->fd, WSDISPLAYIO_GET_FBINFO, &priv->info);
	if (k < 0)
		perror("Error: WSDISPLAYIO_GET_FBINFO");

	/* Now get the new screeninfo */
	ioctl(priv->fd, WSDISPLAYIO_GINFO, &priv->var);
	depth = priv->info.fbi_bitsperpixel;

	/* Calculate info.fbi_stride if it's zero */
	if (!priv->info.fbi_stride)
		priv->info.fbi_stride = (priv->var.width * depth + 7) / 8;

	screen->fb.visuals = 0;
	switch (priv->info.fbi_bitsperpixel) {
	case 32:
	case 24:
	case 16:
		screen->fb.visuals |= (1 << TrueColor);
		break;
	case 8:
		if (gray) {
			if (staticmap)
				screen->fb.visuals |= (1 << StaticGray);
			else
				screen->fb.visuals |= (1 << GrayScale);
		} else {
			if (staticmap)
				screen->fb.visuals |= (1 << StaticColor);
			else
				screen->fb.visuals |= (1 << PseudoColor);
		}
		break;
	case 4:
		if (gray)
			screen->fb.visuals |= (1 << StaticGray);
		else
			screen->fb.visuals |= (1 << StaticColor);
		break;
	case 2:
	case 1:
		screen->fb.visuals |= (1 << StaticGray);
		break;
	default:
		return FALSE;
		break;
	}
#define Mask(o,l)   ((((1 << l) - 1) << o))
		       
	screen->fb.redMask =
	    Mask(priv->info.fbi_subtype.fbi_rgbmasks.red_offset,
		priv->info.fbi_subtype.fbi_rgbmasks.red_size);
	screen->fb.greenMask =
	    Mask(priv->info.fbi_subtype.fbi_rgbmasks.green_offset,
		priv->info.fbi_subtype.fbi_rgbmasks.green_size);
	screen->fb.blueMask =
	    Mask(priv->info.fbi_subtype.fbi_rgbmasks.blue_offset,
		priv->info.fbi_subtype.fbi_rgbmasks.blue_size);

	/*
	 * This is a kludge so that Render will work -- fill in the gaps
	 * in the pixel
	 */
	screen->fb.redMask = wsfbMakeContig(screen->fb.redMask,
	    screen->fb.greenMask | screen->fb.blueMask);

	screen->fb.greenMask = wsfbMakeContig(screen->fb.greenMask,
	    screen->fb.redMask | screen->fb.blueMask);

	screen->fb.blueMask = wsfbMakeContig(screen->fb.blueMask,
	    screen->fb.redMask | screen->fb.greenMask);

#ifdef notyet
	allbits =
	    screen->fb.redMask | screen->fb.greenMask | screen->fb.blueMask;
	while (depth && !(allbits & (1 << (depth - 1))))
		depth--;
#endif
	screen->fb.depth = depth;
	screen->fb.bitsPerPixel = priv->info.fbi_bitsperpixel;
	screen->randr = RR_Rotate_0;

	scrpriv->randr = screen->randr;

	return wsfbMapFramebuffer(screen);
}

Bool wsfbScreenInit(KdScreenInfo * screen)
{
	FbdevScrPriv *scrpriv;

	scrpriv = calloc(1, sizeof(FbdevScrPriv));
	if (!scrpriv)
		return FALSE;
	screen->driver = scrpriv;
	if (!wsfbScreenInitialize(screen, scrpriv)) {
		screen->driver = 0;
		free(scrpriv);
		return FALSE;
	}
	return TRUE;
}

static void *wsfbWindowLinear(ScreenPtr pScreen,
			CARD32 row,
			CARD32 offset, int mode, CARD32 * size, void *closure)
{
	KdScreenPriv(pScreen);
	FbdevPriv *priv = pScreenPriv->card->driver;

	if (!pScreenPriv->enabled)
		return 0;
	*size = priv->info.fbi_stride;
	return (CARD8 *) priv->fb + row * priv->info.fbi_stride + offset;
}

static Bool wsfbMapFramebuffer(KdScreenInfo * screen)
{
	FbdevScrPriv *scrpriv = screen->driver;
	KdMouseMatrix m;
	FbdevPriv *priv = screen->card->driver;

	if (scrpriv->randr != RR_Rotate_0)
		scrpriv->shadow = TRUE;
	else
		scrpriv->shadow = FALSE;

	KdComputeMouseMatrix(&m, scrpriv->randr, screen->width, screen->height);

	KdSetMouseMatrix(&m);

	screen->width = priv->var.width;
	screen->height = priv->var.height;
	screen->memory_base = (CARD8 *) (priv->fb);
	screen->memory_size = priv->info.fbi_fbsize;

	if (scrpriv->shadow) {
		if (!KdShadowFbAlloc(screen,
				     scrpriv->
				     randr & (RR_Rotate_90 | RR_Rotate_270)))
			return FALSE;
		screen->off_screen_base = screen->memory_size;
	} else {
		screen->fb.byteStride = priv->info.fbi_stride;
		screen->fb.pixelStride = (priv->info.fbi_stride * 8 /
					     priv->var.depth);
		screen->fb.frameBuffer = (CARD8 *) (priv->fb);
		screen->off_screen_base =
		    screen->fb.byteStride * screen->height;
	}

	return TRUE;
}

static void wsfbSetScreenSizes(ScreenPtr pScreen)
{
	KdScreenPriv(pScreen);
	KdScreenInfo *screen = pScreenPriv->screen;
	FbdevScrPriv *scrpriv = screen->driver;
	FbdevPriv *priv = screen->card->driver;

	if (scrpriv->randr & (RR_Rotate_0 | RR_Rotate_180)) {
		pScreen->width = priv->var.width;
		pScreen->height = priv->var.height;
		pScreen->mmWidth = screen->width_mm;
		pScreen->mmHeight = screen->height_mm;
	} else {
		pScreen->width = priv->var.height;
		pScreen->height = priv->var.width;
		pScreen->mmWidth = screen->height_mm;
		pScreen->mmHeight = screen->width_mm;
	}
}

static Bool wsfbUnmapFramebuffer(KdScreenInfo * screen)
{
	KdShadowFbFree(screen);
	return TRUE;
}

static Bool wsfbSetShadow(ScreenPtr pScreen)
{
	KdScreenPriv(pScreen);
	KdScreenInfo *screen = pScreenPriv->screen;
	FbdevScrPriv *scrpriv = screen->driver;
	FbdevPriv *priv = screen->card->driver;
	ShadowUpdateProc update;
	ShadowWindowProc window;
	int useYX = 0;

#ifdef __arm__
	/* Use variant copy routines that always read left to right in the
	   shadow framebuffer.  Reading vertical strips is exceptionally
	   slow on XScale due to cache effects.  */
	useYX = 1;
#endif

	window = wsfbWindowLinear;
	update = 0;

	if (scrpriv->randr)
		if (priv->info.fbi_bitsperpixel == 16) {
			switch (scrpriv->randr) {
			case RR_Rotate_90:
				if (useYX)
					update = shadowUpdateRotate16_90YX;
				else
					update = shadowUpdateRotate16_90;
				break;
			case RR_Rotate_180:
				update = shadowUpdateRotate16_180;
				break;
			case RR_Rotate_270:
				if (useYX)
					update = shadowUpdateRotate16_270YX;
				else
					update = shadowUpdateRotate16_270;
				break;
			default:
				update = shadowUpdateRotate16;
				break;
			}
		} else
			update = shadowUpdateRotatePacked;
	else
		update = shadowUpdatePacked;
	return KdShadowSet(pScreen, scrpriv->randr, update, window);
}

static Bool wsfbRandRGetInfo(ScreenPtr pScreen, Rotation * rotations)
{
	KdScreenPriv(pScreen);
	KdScreenInfo *screen = pScreenPriv->screen;
	FbdevScrPriv *scrpriv = screen->driver;
	RRScreenSizePtr pSize;
	Rotation randr;
	int n;

	*rotations = RR_Rotate_All | RR_Reflect_All;

	for (n = 0; n < pScreen->numDepths; n++)
		if (pScreen->allowedDepths[n].numVids)
			break;
	if (n == pScreen->numDepths)
		return FALSE;

	pSize = RRRegisterSize(pScreen,
			       screen->width,
			       screen->height,
			       screen->width_mm, screen->height_mm);

	randr = KdSubRotation(scrpriv->randr, screen->randr);

	RRSetCurrentConfig(pScreen, randr, 0, pSize);

	return TRUE;
}

static Bool
wsfbRandRSetConfig(ScreenPtr pScreen,
		    Rotation randr, int rate, RRScreenSizePtr pSize)
{
	KdScreenPriv(pScreen);
	KdScreenInfo *screen = pScreenPriv->screen;
	FbdevScrPriv *scrpriv = screen->driver;
	Bool wasEnabled = pScreenPriv->enabled;
	FbdevScrPriv oldscr;
	int oldwidth;
	int oldheight;
	int oldmmwidth;
	int oldmmheight;
	int newwidth, newheight, newmmwidth, newmmheight;

	if (screen->randr & (RR_Rotate_0 | RR_Rotate_180)) {
		newwidth = pSize->width;
		newheight = pSize->height;
		newmmwidth = pSize->mmWidth;
		newmmheight = pSize->mmHeight;
	} else {
		newwidth = pSize->height;
		newheight = pSize->width;
		newmmwidth = pSize->mmHeight;
		newmmheight = pSize->mmWidth;
	}

	if (wasEnabled)
		KdDisableScreen(pScreen);

	oldscr = *scrpriv;

	oldwidth = screen->width;
	oldheight = screen->height;
	oldmmwidth = pScreen->mmWidth;
	oldmmheight = pScreen->mmHeight;

	/*
	 * Set new configuration
	 */

	scrpriv->randr = KdAddRotation(screen->randr, randr);

	pScreen->width = newwidth;
	pScreen->height = newheight;
	pScreen->mmWidth = newmmwidth;
	pScreen->mmHeight = newmmheight;

	wsfbUnmapFramebuffer(screen);

	if (!wsfbMapFramebuffer(screen))
		goto bail4;

	KdShadowUnset(screen->pScreen);

	if (!wsfbSetShadow(screen->pScreen))
		goto bail4;

	wsfbSetScreenSizes(screen->pScreen);

	/*
	 * Set frame buffer mapping
	 */
	(*pScreen->ModifyPixmapHeader) (fbGetScreenPixmap(pScreen),
					pScreen->width,
					pScreen->height,
					screen->fb.depth,
					screen->fb.bitsPerPixel,
					screen->fb.byteStride,
					screen->fb.frameBuffer);

	/* set the subpixel order */

	KdSetSubpixelOrder(pScreen, scrpriv->randr);
	if (wasEnabled)
		KdEnableScreen(pScreen);

	return TRUE;

 bail4:
	wsfbUnmapFramebuffer(screen);
	*scrpriv = oldscr;
	wsfbMapFramebuffer(screen);
	pScreen->width = oldwidth;
	pScreen->height = oldheight;
	pScreen->mmWidth = oldmmwidth;
	pScreen->mmHeight = oldmmheight;

	if (wasEnabled)
		KdEnableScreen(pScreen);
	return FALSE;
}

static Bool wsfbRandRInit(ScreenPtr pScreen)
{
	rrScrPrivPtr pScrPriv;

	if (!RRScreenInit(pScreen))
		return FALSE;

	pScrPriv = rrGetScrPriv(pScreen);
	pScrPriv->rrGetInfo = wsfbRandRGetInfo;
	pScrPriv->rrSetConfig = wsfbRandRSetConfig;
	return TRUE;
}

static Bool wsfbCreateColormap(ColormapPtr pmap)
{
	ScreenPtr pScreen = pmap->pScreen;
	KdScreenPriv(pScreen);
	KdScreenInfo *screen = pScreenPriv->screen;
	VisualPtr pVisual;
	int i;
	int nent;
	xColorItem *pdefs;

	Bool result;
	pVisual = pmap->pVisual;
	nent = pVisual->ColormapEntries;

	switch (screen->fb.depth) {
	case 1:
	case 2:
	case 4:
	case 8:
		pdefs = malloc(nent * sizeof(xColorItem));
		if (!pdefs)
			return FALSE;
		for (i = 0; i < nent; i++)
			pdefs[i].pixel = i;
		if (gray)
			wsfbDefaultColormapMono(screen);
		else
			wsfbDefaultColormap(screen);
		wsfbGetColors(pScreen, nent, pdefs);
		for (i = 0; i < nent; i++) {
			pmap->red[i].co.local.red = (uint16_t)pdefs[i].red;
			pmap->red[i].co.local.green = (uint16_t)pdefs[i].green;
			pmap->red[i].co.local.blue = (uint16_t)pdefs[i].blue;
		}
		free(pdefs);
		result = TRUE;
		break;
	default:
		result = fbInitializeColormap(pmap);
		break;
	}

	pScreen->blackPixel = nent - 1;
	pScreen->whitePixel = 0;

	if (result && revcolors) {
		nent--;
		pScreen->whitePixel = nent;
		pScreen->blackPixel = 0;
		for (i = 0; i <= nent / 2; i++) {
			uint32_t red, green, blue;

			red = pmap->red[i].co.local.red;
			blue = pmap->red[i].co.local.blue;
			green = pmap->red[i].co.local.green;

			pmap->red[i].co.local.red =
			    pmap->red[nent - i].co.local.red;
			pmap->red[i].co.local.blue =
			    pmap->red[nent - i].co.local.blue;
			pmap->red[i].co.local.green =
			    pmap->red[nent - i].co.local.green;

			pmap->red[nent - i].co.local.red = red;
			pmap->red[nent - i].co.local.blue = blue;
			pmap->red[nent - i].co.local.green = green;
		}
	}
	return result;
}

Bool wsfbInitScreen(ScreenPtr pScreen)
{

	pScreen->CreateColormap = wsfbCreateColormap;
	return TRUE;
}

Bool wsfbFinishInitScreen(ScreenPtr pScreen)
{
	if (!shadowSetup(pScreen))
		return FALSE;

	if (!wsfbRandRInit(pScreen))
		return FALSE;

	return TRUE;
}

Bool wsfbCreateResources(ScreenPtr pScreen)
{
	return wsfbSetShadow(pScreen);
}

void wsfbPreserve(KdCardInfo * card)
{
}

static void wsfbDefaultColormap(KdScreenInfo *screen)
{
	FbdevPriv *priv = screen->card->driver;
	int i, j;


	if (apple) {
		if (screen->fb.depth == 8) {
			j = 0;
			for (i = 0; i < 256; i++) {
				priv->red[i] = 255 - apple8_cmap[j];
				priv->green[i] = 255 - apple8_cmap[j + 1];
				priv->blue[i] = 255 - apple8_cmap[j + 2];
				j += 3;
			}
		} else {
			j = 0;
			for (i = 0; i < 16; i++) {
				priv->red[i] = apple4_cmap[j];
				priv->green[i] = apple4_cmap[j + 1];
				priv->blue[i] = apple4_cmap[j + 2];
				j += 3;
			}
		}
		i--;
		wsfbUpdateFbColormap(priv, 0, i);
	} else {
		/* Accept the kernel colormap */

		return;
	}
}

static void wsfbDefaultColormapMono(KdScreenInfo *screen)
{
	FbdevPriv *priv = screen->card->driver;
	int i, n;
	int mapcolors = pow(2, priv->info.fbi_bitsperpixel);

	n = 0;
	for (i = mapcolors - 1; i >= 0; i--) {
			priv->red[n] =
			    (i * 255 / (mapcolors - 1));
			priv->green[n] =
			    (i * 255 / (mapcolors - 1));
			priv->blue[n] =
			    (i * 255 / (mapcolors - 1));
			n++;
	}
	wsfbUpdateFbColormap(priv, 0, mapcolors - 1);
}

static int wsfbUpdateFbColormap(FbdevPriv *priv, int minidx, int maxidx)
{
	struct wsdisplay_cmap cmap;
	int result;

	if (!priv->inited) {
		cmap.index = 0;
		cmap.count = 256;

		cmap.red=&priv->orig_red[0];
		cmap.green=&priv->orig_green[0];
		cmap.blue=&priv->orig_blue[0];
		ioctl(priv->fd, WSDISPLAYIO_GETCMAP, &cmap);

		priv->inited = TRUE;
	}

	cmap.index = minidx;
	cmap.count = maxidx - minidx + 1;

	cmap.red = &priv->red[minidx];
	cmap.green = &priv->green[minidx];
	cmap.blue = &priv->blue[minidx];

	if ((result = ioctl(priv->fd, WSDISPLAYIO_PUTCMAP, &cmap)) < 0)
		perror("Error: Cannot put colormap");
	return result;
}

Bool wsfbEnable(ScreenPtr pScreen)
{
	KdScreenPriv(pScreen);
	KdScreenInfo *screen = pScreenPriv->screen;
	FbdevPriv *priv = pScreenPriv->card->driver;
	int mode;

	mode = WSDISPLAYIO_MODE_DUMBFB;
	if (ioctl(priv->fd, WSDISPLAYIO_SMODE, &mode) == -1)
		return FALSE;

	if (gray)
		wsfbDefaultColormapMono(screen);
	else
		wsfbDefaultColormap(screen);
#if 0
	if (priv->info.visual == FB_VISUAL_DIRECTCOLOR) {
		int i;

		for (i = 0;
		     i < (1 << priv->var.red.length) ||
		     i < (1 << priv->info.green.size) ||
		     i < (1 << priv->var.blue.length); i++) {
			priv->red[i] =
			    i * 65535 / ((1 << priv->var.red.length) - 1);
			priv->green[i] =
			    i * 65535 / ((1 << priv->info.green.size) - 1);
			priv->blue[i] =
			    i * 65535 / ((1 << priv->var.blue.length) - 1);
		}
		i--;
		wsfbUpdateFbColormap(priv, 0, i);
	}
#endif
	return TRUE;
}

Bool wsfbDPMS(ScreenPtr pScreen, int mode)
{
	KdScreenPriv(pScreen);
	FbdevPriv *priv = pScreenPriv->card->driver;
	static int oldmode = -1;
	int value;

	if (mode == oldmode)
		return TRUE;
	value = mode ? WSDISPLAYIO_VIDEO_ON : WSDISPLAYIO_VIDEO_OFF;
	if (ioctl(priv->fd, WSDISPLAYIO_SVIDEO, &value) != -1) {
		oldmode = mode;
		return TRUE;
	}
	return FALSE;

}

void wsfbDisable(ScreenPtr pScreen)
{
	KdScreenPriv(pScreen);
	FbdevPriv *priv = pScreenPriv->card->driver;
	int mode;

	mode = WSDISPLAYIO_MODE_EMUL;
	(void)ioctl(priv->fd, WSDISPLAYIO_SMODE, &mode);
}

void wsfbRestore(KdCardInfo * card)
{
}

void wsfbScreenFini(KdScreenInfo * screen)
{
	FbdevPriv *priv = screen->card->driver;
	struct wsdisplay_cmap cmap;

	if (screen->fb.depth <= 8 && priv->inited) {
		cmap.index = 0;
		cmap.count = 256;

		cmap.red=&priv->orig_red[0];
		cmap.green=&priv->orig_green[0];
		cmap.blue=&priv->orig_blue[0];
		ioctl(priv->fd, WSDISPLAYIO_PUTCMAP, &cmap);
	}

	int wsmode = WSDISPLAYIO_MODE_EMUL;

	(void)ioctl(priv->fd, WSDISPLAYIO_SMODE, &wsmode);
}

void wsfbCardFini(KdCardInfo * card)
{
	FbdevPriv *priv = card->driver;

	munmap(priv->fb_base, priv->info.fbi_fbsize);
	close(priv->fd);
	free(priv);
}

/*
 * Retrieve actual colormap and return selected n entries in pdefs.
 */

void wsfbGetColors(ScreenPtr pScreen, int n, xColorItem * pdefs)
{
	KdScreenPriv(pScreen);
	FbdevPriv *priv = pScreenPriv->card->driver;
	struct wsdisplay_cmap cmap;
	int p;
	int k;
	int min, max;

	min = 256;
	max = 0;
	for (k = 0; k < n; k++) {
		if (pdefs[k].pixel < min)
			min = pdefs[k].pixel;
		if (pdefs[k].pixel > max)
			max = pdefs[k].pixel;
	}
	cmap.index = min;
	cmap.count = max - min + 1;
	cmap.red = &priv->red[min];
	cmap.green = &priv->green[min];
	cmap.blue = &priv->blue[min];
	k = ioctl(priv->fd, WSDISPLAYIO_GETCMAP, &cmap);
	if (k < 0) {
		perror("Error: Can't get colormap");
		return;
	}
	while (n--) {
		p = pdefs->pixel;
		pdefs->red = (uint16_t)priv->red[p] << 8;
		pdefs->green = (uint16_t)priv->green[p] << 8;
		pdefs->blue = (uint16_t)priv->blue[p] << 8;
		pdefs++;
	}
}

/*
 * Change colormap by updating n entries described in pdefs.
 */
void wsfbPutColors(ScreenPtr pScreen, int n, xColorItem * pdefs)
{
	KdScreenPriv(pScreen);
	FbdevPriv *priv = pScreenPriv->card->driver;
	int p;
	int min, max;

	min = 256;
	max = 0;
	while (n--) {
		p = pdefs->pixel;
		priv->red[p] = pdefs->red >> 8;
		priv->green[p] = pdefs->green >> 8;
		priv->blue[p] = pdefs->blue >> 8;
		if (p < min)
			min = p;
		if (p > max)
			max = p;
		pdefs++;
	}

	wsfbUpdateFbColormap(priv, min, max);
}

