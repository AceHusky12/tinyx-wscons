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
  * Derived from linux/keyboard.c
  */

#ifdef HAVE_CONFIG_H
#include <kdrive-config.h>
#endif

#include <sys/ioctl.h>
#include <dev/wscons/wsconsio.h>
#include <termios.h>
#include <errno.h>

#include "kdrive.h"
#include "kkeymap.h"
#include "wskeymap.h"

static int wsConsoleFd = -1;
static struct termios wsTermios;
static int wsKbdType;
static u_int kbd_type;
static KeySym *scancodeMap;
static unsigned int mapsize;

static void readKernelMapping(void);

static void wsKeyboardLoad(void)
{
	if ((wsConsoleFd = open("/dev/wskbd", O_RDWR | O_NONBLOCK))
								< 0) {
		fprintf(stderr, "Error opening keyboard %s: %s\n",
		       "/dev/wskbd", strerror(errno));
		wsConsoleFd = -1;
	}
       if (wsConsoleFd > 0 && ioctl(wsConsoleFd, WSKBDIO_GTYPE, &kbd_type) == -1) {
		perror("Cannot get keyboard type");
		close(wsConsoleFd);
		wsConsoleFd = -1;
		return;
       }
       switch (kbd_type) {
	       case WSKBD_TYPE_PC_XT:
	       case WSKBD_TYPE_PC_AT:
                    scancodeMap = wsXtMap;
		    mapsize = __arraycount(wsXtMap);
                    break;
	       case WSKBD_TYPE_USB:
	       case WSKBD_TYPE_MAPLE:
                    scancodeMap = wsUsbMap;
		    mapsize = __arraycount(wsUsbMap);
                    break;
	       case WSKBD_TYPE_ADB:
                    scancodeMap = wsAdbMap; 
		    mapsize = __arraycount(wsAdbMap);
                    break;
	       case WSKBD_TYPE_AMIGA:
                    scancodeMap = wsAmigaMap; 
		    mapsize = __arraycount(wsAmigaMap);
                    break;
	       case WSKBD_TYPE_LK201:
                    scancodeMap = wsLk201Map;
		    mapsize = __arraycount(wsLk201Map);
                    break;
	       case WSKBD_TYPE_SUN5:
	       case WSKBD_TYPE_SUN:
                    scancodeMap = wsSunMap;
		    mapsize = __arraycount(wsSunMap);
                    break;
	       default:
           		perror("Unknown keyboard type");
           		close(wsConsoleFd);
			wsConsoleFd = -1;
           		return;
	}

	readKernelMapping();
}

static void wsKeyboardRead(int fd, void *closure)
{
	struct wscons_event events[64];
	int n, i, type;


	if ((n = read(fd, events, sizeof(events))) > 0) {
		n /=  sizeof(struct wscons_event);
		for (i = 0; i < n; i++) {
			type = events[i].type;
			if (type == WSCONS_EVENT_KEY_UP ||
			    type == WSCONS_EVENT_KEY_DOWN) {
				KdEnqueueKeyboardEvent(events[i].value,
				    type == WSCONS_EVENT_KEY_DOWN ? 0x0 : 0x80);
			}
		}
	}
}

static int wasDisabled = 0;
static int wsKeyboardEnable(int fd, void *closure)
{
	struct termios nTty;
	unsigned char buf[256];
	int option, n;

	if (wasDisabled == 1 &&
	    (wsConsoleFd = open("/dev/wskbd", O_RDWR | O_NONBLOCK))
								< 0) {
		fprintf(stderr, "Error opening keyboard %s: %s\n",
		       "/dev/wskbd", strerror(errno));
		wsConsoleFd = -1;

		return FALSE;
	}
	wasDisabled = 0;
	fd = wsConsoleFd;
	tcgetattr(fd, &wsTermios);


	nTty = wsTermios;
	nTty.c_iflag = (IGNPAR | IGNBRK) & (~PARMRK) & (~ISTRIP);
	nTty.c_oflag = 0;
	nTty.c_cflag = CREAD | CS8;
	nTty.c_lflag = 0;
	nTty.c_cc[VTIME] = 0;
	nTty.c_cc[VMIN] = 1;
	cfsetispeed(&nTty, 9600);
	cfsetospeed(&nTty, 9600);
	tcsetattr(fd, TCSANOW, &nTty);

	option = WSKBDIO_EVENT_VERSION;
	if (ioctl(fd, WSKBDIO_SETVERSION, &option) == -1) {
		perror("cannot set wskbd version\n");
		return -1;
	}

#if 0
 	option = WSKBD_RAW;
	if (ioctl(fd, WSKBDIO_SETMODE, &option) == -1) {
		perror("Cant get raw keyboad");
	}
#endif

	/*
	 * Flush any pending keystrokes
	 */
	while ((n = read(fd, buf, sizeof(buf))) > 0) ;

	return fd;
}

#define NR_KEYS 256

static void readKernelMapping(void)
{
	KeySym *k;
	int i;
	int minKeyCode, maxKeyCode;
	int row;

	minKeyCode = NR_KEYS;
	maxKeyCode = 0;
	row = 0;
	for (i = 0; i < mapsize && row < KD_MAX_LENGTH; ++i) {
		k = kdKeymap + row * KD_MAX_WIDTH;

		k[0] = scancodeMap[i];	
		switch (scancodeMap[i]) {
		case XK_1:
			k[1] = XK_exclam;
			break;
		case XK_2:
			k[1] = XK_at;
			break;
		case XK_3:
			k[1] = XK_numbersign;
			break;
		case XK_4:
			k[1] = XK_dollar;
			break;
		case XK_5:
			k[1] = XK_percent;
			break;
		case XK_6:
			k[1] = XK_asciicircum;
			break;
		case XK_7:
			k[1] = XK_ampersand;
			break;
		case XK_8:
			k[1] = XK_asterisk;
			break;
		case XK_9:
			k[1] = XK_parenleft;
			break;
		case XK_0:
			k[1] = XK_parenright;
			break;
		case XK_minus:
			k[1] = XK_underscore;
			break;
		case XK_equal:
			k[1] = XK_plus;
			break;
		case XK_grave:
			k[1] = XK_asciitilde;
			break;
		case XK_bracketleft:
			k[1] = XK_braceleft;
			break;
		case XK_bracketright:
			k[1] = XK_braceright;
			break;
		case XK_backslash:
			k[1] = XK_bar;
			break;
		case XK_semicolon:
			k[1] = XK_colon;
			break;
		case XK_apostrophe:
			k[1] = XK_quotedbl;
			break;
		case XK_comma:
			k[1] = XK_less;
			break;
		case XK_period:
			k[1] = XK_greater;
			break;
		case XK_slash:
			k[1] = XK_question;
			break;
		case XK_Alt_L:
			k[1] = XK_Meta_L;
			break;
		case XK_Alt_R:
			k[1] = XK_Meta_R;
			break;
		case XK_Print:
			k[1] = XK_Sys_Req;
			break;
		default:
			k[1] = NoSymbol;
			break;
		}
		k[2] = k[3] = NoSymbol;


		if (i < minKeyCode)
			minKeyCode = i;
		if (i > maxKeyCode)
			maxKeyCode = i;

		if (k[3] == k[2])
			k[3] = NoSymbol;
		if (k[2] == k[1])
			k[2] = NoSymbol;
		if (k[1] == k[0])
			k[1] = NoSymbol;
		if (k[0] == k[2] && k[1] == k[3])
			k[2] = k[3] = NoSymbol;
		if (k[3] == k[0] && k[2] == k[1] && k[2] == NoSymbol)
			k[3] = NoSymbol;
		row++;
	}
	kdMinScanCode = minKeyCode;
	kdMaxScanCode = maxKeyCode;
}

static void wsKeyboardDisable(int fd, void *closure)
{
	int option;

 	option = WSKBD_TRANSLATED;
	ioctl(fd, WSKBDIO_SETMODE, &option);

	tcsetattr(fd, TCSANOW, &wsTermios);

	close(fd);

	wasDisabled = 1;
}

static int wsKeyboardInit(void)
{
	if (!wsKbdType)
		wsKbdType = KdAllocInputType();
	if (wsConsoleFd == -1)
		return FALSE;

	KdRegisterFd(wsKbdType, wsConsoleFd, wsKeyboardRead, 0);
	wsKeyboardEnable(wsConsoleFd, 0);
	KdRegisterFdEnableDisable(wsConsoleFd,
				  wsKeyboardEnable, wsKeyboardDisable);
	return 1;
}

static void wsKeyboardFini(void)
{
	wsKeyboardDisable(wsConsoleFd, 0);
	KdUnregisterFds(wsKbdType, FALSE);
}

static void wsKeyboardLeds(int leds)
{
	ioctl(wsConsoleFd, WSKBDIO_SETLEDS, leds & 7);
}

static void wsKeyboardBell(int volume, int pitch, int duration)
{
	struct wskbd_bell_data wsb;

	wsb.which = WSKBD_BELL_DOALL;
	wsb.pitch = pitch;
	wsb.period = duration;
	wsb.volume = volume;

	if (volume && pitch)
		ioctl(wsConsoleFd, WSKBDIO_COMPLEXBELL, &wsb);
}


const KdKeyboardFuncs wsKeyboardFuncs = {
	wsKeyboardLoad,
	wsKeyboardInit,
	wsKeyboardLeds,
	wsKeyboardBell,
	wsKeyboardFini,
	3,
};

