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

extern const KdCardFuncs wsfbFuncs;
extern const KdKeyboardFuncs wsKeyboardFuncs;
extern const KdMouseFuncs wsMouseFuncs;

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
	ErrorF("\nXwsfb Device Usage:\n");
	ErrorF
	    ("-fb path         wscons device to use. Defaults to /dev/tty\n");
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
	}

	return KdProcessArgument(argc, argv, i);
}

static const KdOsFuncs BSDFuncs = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0
};

void OsVendorInit(void)
{
	KdOsInit(&BSDFuncs);
}
