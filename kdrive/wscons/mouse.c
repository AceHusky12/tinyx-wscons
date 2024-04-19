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
  * Derived from linux/mouse.c
  */

/*
 *
 * Copyright © 2001 Keith Packard
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

#include "kdrive.h"
#include <sys/ioctl.h>
#include <dev/wscons/wsconsio.h>
#include <errno.h>

typedef enum _kmouseStage {
	MouseBroken, MouseTesting, MouseWorking
} KmouseStage;

typedef struct _kmouse {
	KmouseStage stage;	/* protocol verification stage */
} Kmouse;


static int MouseInputType;

const char * const kdefaultMouse[] = {
	"/dev/wsmouse",
};

#define NUM_DEFAULT_MOUSE    (sizeof (kdefaultMouse) / sizeof (kdefaultMouse[0]))

static Bool wsmouseParse(KdMouseInfo * mi, struct wscons_event *ev)
{
	Kmouse *km = mi->driver;
	int dx, dy;
	static unsigned long flags = 0;

	flags |= KD_MOUSE_DELTA;
	dx = dy = 0;
#if 0
	/* Absolute events. */
	if (event.type == WSCONS_EVENT_MOUSE_ABSOLUTE_X)
		printf("ABSOLUTE_X: %d\n", event.value);

	if (event.type == WSCONS_EVENT_MOUSE_ABSOLUTE_Y)
		printf("ABSOLUTE_Y: %d\n", event.value);
#endif

	/* Relative events. */
	if (ev->type == WSCONS_EVENT_MOUSE_DELTA_X) {
	//	printf("ABSOLUTE_X: %d\n", ev->value);
		dx = ev->value;
	}

	if (ev->type == WSCONS_EVENT_MOUSE_DELTA_Y) {
	//	printf("REL_Y: %d\n", ev->value);
		dy = -ev->value;
	}

	if (ev->type == WSCONS_EVENT_MOUSE_DOWN) {
		flags |= (ev->value == 0) ? KD_BUTTON_1 : 0;
		flags |= (ev->value == 1) ? KD_BUTTON_2 : 0;
		flags |= (ev->value == 2) ? KD_BUTTON_3 : 0;
	
	}
	if (ev->type == WSCONS_EVENT_MOUSE_UP) {
		flags &= ~((ev->value == 0) ? KD_BUTTON_1 : 0);
		flags &= ~((ev->value == 1) ? KD_BUTTON_2 : 0);
		flags &= ~((ev->value == 2) ? KD_BUTTON_3 : 0);
	}
	
	if (km->stage == MouseWorking)
		KdEnqueueMouseEvent(mi, flags, dx, dy);

	return TRUE;
}
static void MouseRead(int mousePort, void *closure)
{
	KdMouseInfo *mi = closure;
	Kmouse *km = mi->driver;
	int errno, bytesRead;
	struct wscons_event event;

	for (;;) {
		bytesRead = read(mousePort, &event, sizeof(event));

		if (bytesRead == sizeof(event)) {
			km->stage = MouseWorking;
			wsmouseParse(mi, &event);
			break;
		}
		if (bytesRead == -1) {
			km->stage = MouseBroken;
			break;
		}
	}
}

static Bool MouseInit(void)
{
	int i;
	int fd = -1;
	Kmouse *km;
	KdMouseInfo *mi, *next;
	int n = 0;

	if (!MouseInputType)
		MouseInputType = KdAllocInputType();

	for (mi = kdMouseInfo; mi; mi = next) {
		next = mi->next;
		if (mi->inputType)
			continue;
		if (!mi->name) {
			for (i = 0; i < NUM_DEFAULT_MOUSE; i++) {
				fd = open(kdefaultMouse[i], 2);
				if (fd >= 0) {
					mi->name = strdup(kdefaultMouse[i]);
					break;
				}
			}
		} else
			fd = open(mi->name, 2);

		if (fd >= 0) {
			km = malloc(sizeof(Kmouse));
			if (km) {
				mi->driver = km;
				mi->inputType = MouseInputType;
				if (KdRegisterFd
				    (MouseInputType, fd, MouseRead, (void *)mi))
					n++;
			} else
				close(fd);
		}
	}
	return TRUE;
}

static void MouseFini(void)
{
	KdMouseInfo *mi;

	KdUnregisterFds(MouseInputType, TRUE);
	for (mi = kdMouseInfo; mi; mi = mi->next) {
		if (mi->inputType == MouseInputType) {
			free(mi->driver);
			mi->driver = 0;
			mi->inputType = 0;
		}
	}
}

const KdMouseFuncs wsMouseFuncs = {
	MouseInit,
	MouseFini,
};

