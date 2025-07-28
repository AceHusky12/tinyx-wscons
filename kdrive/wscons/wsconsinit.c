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
 * Derived from fbdev/fbinit.c
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

#if HAVE_CONFIG_H
#include <kdrive-config.h>
#endif
#include "wsdev.h"
#include <X11/keysym.h>

#include <sys/ioctl.h>
#include <sys/resource.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplay_usl_io.h>
#include <errno.h>
#include <signal.h>
int8_t priority;

extern const KdCardFuncs wsfbFuncs;
extern const KdKeyboardFuncs wsKeyboardFuncs;
extern const KdMouseFuncs wsMouseFuncs;

#define DEFDEV "/dev/ttyEcfg"

extern CARD32	keyboard_rate;

void InitCard(char *name)
{
	KdCardInfoAdd(&wsfbFuncs, 0);
}

void InitOutput(ScreenInfo * pScreenInfo, int argc, char **argv)
{
	KdInitOutput(pScreenInfo, argc, argv);
}

void InitInput(int argc, char **argv)
{
	KdInitInput(&wsMouseFuncs, &wsKeyboardFuncs);
}

void ddxUseMsg(void)
{
	KdUseMsg();
	ErrorF("\nXwscons Device Usage:\n");
	ErrorF
	    ("-fb path         wscons device to use. Defaults to /dev/tty.\n");
	ErrorF
	    ("-n #             Xwscons scheduling priority \n");
	ErrorF
	    ("-keyrate #       Sets autorepeat keys per second\n");
	ErrorF
	    ("-gray            Use grayscale palette. 16/256 colors only.\n");
	ErrorF
	    ("-revcolors       Reverse colors.\n");
	ErrorF
	    ("-staticmap [apple]\n");
	ErrorF
	    ("                 Use static colormap. [Install apple default]\n");
	ErrorF("\n");
}

int ddxProcessArgument(int argc, char **argv, int i)
{

	if (!strcmp(argv[i], "-version")) {
		kdVersion("Xwsfb");
		exit(0);
	}

	if (!strcmp(argv[i], "-fb")) {
		if (i + 1 < argc) {
			wsfbDevicePath = argv[i + 1];
			return 2;
		}
		UseMsg();
		exit(1);
	} else if (!strcmp(argv[i], "-gray")) {
		gray = TRUE;
		return 1;
	} else if (!strcmp(argv[i], "-revcolors")) {
		revcolors = TRUE;
		return 1;
	} else if (!strcmp(argv[i], "-n")) {
		if (i + 1 < argc) {
			priority = atoi(argv[i + 1]);
			if (priority > 20)
				priority = 20;
			if (priority < -19)
				priority = -19;
			setpriority(PRIO_PROCESS, 0, priority);

			return 2;
		}
		UseMsg();
		exit(1);
	} else if (!strcmp(argv[i], "-keyrate")) {
		if (i + 1 < argc) {
			if (*argv[i + 1] == '-') {
				UseMsg();
				exit(1);
			}
		  	keyboard_rate = atoi(argv[i + 1]);
			if (keyboard_rate <= 0)
				keyboard_rate = 20;

			return 2;
		}
		UseMsg();
		exit(1);
	} else if (!strcmp(argv[i], "-staticmap")) {
		if (i + 1 < argc) {
		  	if (!strcmp(argv[i + 1], "apple")) {
				apple = TRUE;
				staticmap = TRUE;
				return 2;
			} else if (*argv[i + 1] != '-') {
				UseMsg();
				exit(1);
			}
		}
		staticmap = TRUE;
		return 1;
	}

	return KdProcessArgument(argc, argv, i);
}

static int activeVT=-1;

static void wsVTRequest(int sig)
{
	kdSwitchPending = TRUE;
}

static void WsDisable(void)
{
	int wsfd;
	int mode;

	if ((wsfd = open(wsfbDevicePath, O_RDWR)) == -1)
		return;
	if (kdSwitchPending) {
		kdSwitchPending = FALSE;
		ioctl(wsfd, VT_RELDISP, 1);
	}

	mode = WSDISPLAYIO_MODE_EMUL;
	ioctl(wsfd, WSDISPLAYIO_SMODE, &mode);

	close(wsfd);

	return;
}

static void WsEnable(void)
{
	struct vt_stat vts;
	struct sigaction act;
	struct vt_mode VT;
	int wsfd;
	int mode;

	if ((wsfd = open(wsfbDevicePath, O_RDWR)) == -1)
		return;

	if (kdSwitchPending) {
		kdSwitchPending = FALSE;
		if (ioctl(wsfd, VT_RELDISP, VT_ACKACQ) < 0) {
			fprintf(stderr, "Enable RELDISP\n");
			close(wsfd);

			return;
		}
	}

	if (activeVT == -1) {
		act.sa_handler = wsVTRequest;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		sigaction(SIGUSR1, &act, 0);

		VT.mode = VT_PROCESS;
		VT.relsig = SIGUSR1;
		VT.acqsig = SIGUSR1;

		if (ioctl(wsfd, VT_SETMODE, &VT) < 0)
			fprintf(stderr, "wsfbEnable: VT_SETMODE failed\n");
	}
	if (ioctl(wsfd, VT_GETSTATE, &vts) == 0) {
		activeVT = vts.v_active;
	}

	mode = WSDISPLAYIO_MODE_DUMBFB;
	ioctl(wsfd, WSDISPLAYIO_SMODE, &mode);
	close(wsfd);

	return;
}

static Bool WsSpecialKey(KeySym sym)
{
	uint8_t idx;
	int wsfdcon;

	if (sym >= XK_F1 && sym <= XK_F12) {
		idx = sym - XK_F1;
		idx++;

		if (idx == activeVT)
			return TRUE;

		kdSwitchPending = TRUE;

		if ((wsfdcon = open(DEFDEV, O_RDWR)) == -1)
			return TRUE;

		if (ioctl(wsfdcon, VT_ACTIVATE, idx) == -1)
			fprintf(stderr, "Cannot switch to %d", idx);

		close(wsfdcon);

		return TRUE;
	}
	return FALSE;
}

static const KdOsFuncs BSDFuncs = {
	NULL,
	WsEnable,
	WsSpecialKey,
	WsDisable,
	NULL,
	0
};

void OsVendorInit(void)
{
	KdOsInit(&BSDFuncs);
}
