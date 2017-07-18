// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <exports.h>

/*
 * add at least one variable. Without this at least in some cases the
 * standalone application sometimes freezes, sometimes it prints 'random'
 * stuff with printf ...
 * I assume that this forces alignment of some linker sections.
 */
int dummy_var_in_text = 1;

/*
 * Make thumb work by compiling the entry function for arm.
 * Only do this if the target CPU is able to execute arm code.
 * Note that code which calls back into the U-Boot binary
 * must be compiled for thumb.
 */
#if defined(__thumb__) && defined(__ARM_ARCH_ISA_ARM)
static int _hello_world(int argc, char * const argv[]);

__attribute__((target("arm")))
int hello_world(int argc, char * const argv[])
{
	return _hello_world(argc, argv);
}

static noinline int _hello_world(int argc, char * const argv[])
#else

int hello_world (int argc, char * const argv[])
#endif
{
	int i;

	/* Print the ABI version */
	app_startup(argv);
	printf ("Example expects ABI version %d\n", XF_VERSION);
	printf ("Actual U-Boot ABI version %d\n", (int)get_version());

	printf ("Hello World\n");

	printf ("argc = %d\n", argc);

	for (i=0; i<=argc; ++i) {
		printf ("argv[%d] = \"%s\"\n",
			i,
			argv[i] ? argv[i] : "<NULL>");
	}

	printf ("Hit any key to exit ... ");
	while (!tstc())
		;
	/* consume input */
	(void) getc();

	printf ("\n\n");
	return (0);
}
