/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *  (C) Copyright 2010,2012
 *  NVIDIA Corporation <www.nvidia.com>
 *
 *  (C) Copyright 2022
 *  Svyatoslav Ryhel <clamor95@gmail.com>
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#include "tegra30-common.h"

/* High-level configuration options */
#define CONFIG_TEGRA_BOARD_STRING	"HTC One X"

#define BOARD_EXTRA_ENV_SETTINGS \
	"kernel_file=vmlinuz\0" \
	"ramdisk_file=uInitrd\0" \
	"bootkernel=bootz ${kernel_addr_r} - ${fdt_addr_r}\0" \
	"bootrdkernel=bootz ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr_r}\0"

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"setenv volup_button 144;" \
	"if gpio input ${volup_button}; test $? -eq 0;" \
	"then echo Starting Fastboot protocol ...; fastboot usb 0;" \
	"else echo Loading from eMMC...;" \
		"setenv bootargs console=ttyS0,115200n8 root=/dev/mmcblk0p15 rw gpt;" \
		"echo Loading Kernel;" \
		"load mmc 0:14 ${kernel_addr_r} ${kernel_file};" \
		"echo Loading DTB;" \
		"load mmc 0:14 ${fdt_addr_r} ${fdtfile};" \
		"echo Loading Initramfs;" \
		"if load mmc 0:14 ${ramdisk_addr_r} ${ramdisk_file};" \
		"then echo Booting Kernel;" \
			"run bootrdkernel;" \
		"else echo Booting Kernel;" \
			"run bootkernel; fi;" \
	"fi;"

/* Board-specific serial config */
#define CONFIG_TEGRA_ENABLE_UARTA
#define CONFIG_SYS_NS16550_COM1		NV_PA_APB_UARTA_BASE

/* Used for downstream */
#define CONFIG_MACH_TYPE		MACH_TYPE_EDGE_U

#include "tegra-common-post.h"

#endif /* __CONFIG_H */
