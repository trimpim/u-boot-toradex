/*
 * Copyright 2017 Toradex AG
 *
 * Configuration settings for the Colibri iMX6ULL module.
 *
 * based on colibri_imx7.h:
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __COLIBRI_IMX6ULL_CONFIG_H
#define __COLIBRI_IMX6ULL_CONFIG_H

#include "mx6_common.h"
#define CONFIG_IOMUX_LPSR

#define CONFIG_SYS_THUMB_BUILD
#define CONFIG_USE_ARCH_MEMCPY
#define CONFIG_USE_ARCH_MEMSET

/* #define CONFIG_DBG_MONITOR*/
#define PHYS_SDRAM_SIZE			SZ_512M

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_LATE_INIT

#define CONFIG_DISPLAY_BOARDINFO_LATE	/* Calls show_board_info() */

#define CONFIG_ENV_VARS_UBOOT_CONFIG
#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(32 * SZ_1M)

#define CONFIG_CMD_BMODE
/* Network */
#define CONFIG_FEC_MXC
#define CONFIG_MII
#define CONFIG_FEC_XCV_TYPE             RMII
#define CONFIG_ETHPRIME                 "FEC"
#define CONFIG_FEC_MXC_PHYADDR          0

#define CONFIG_PHYLIB
#define CONFIG_PHY_MICREL
#define CONFIG_IP_DEFRAG
#define CONFIG_TFTP_BLOCKSIZE		16352
#define CONFIG_TFTP_TSIZE

/* ENET1 */
#define IMX_FEC_BASE			ENET2_BASE_ADDR

/* MMC Config*/
#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#define CONFIG_SYS_FSL_USDHC_NUM	1

#undef CONFIG_BOOTM_PLAN9
#undef CONFIG_BOOTM_RTEMS

/* I2C configs */
#define CONFIG_SYS_I2C_MXC
#define CONFIG_SYS_I2C_SPEED		100000

#define CONFIG_IPADDR			192.168.10.2
#define CONFIG_NETMASK			255.255.255.0
#define CONFIG_SERVERIP			192.168.10.1

#define FDT_FILE "imx6ull-colibri${variant}-${fdt_board}.dtb"

#define MEM_LAYOUT_ENV_SETTINGS \
	"bootm_size=0x10000000\0" \
	"fdt_addr_r=0x82100000\0" \
	"initrd_high=0xffffffff\0" \
	"kernel_addr_r=0x81000000\0" \
	"pxefile_addr_r=0x87100000\0" \
	"ramdisk_addr_r=0x82200000\0" \
	"scriptaddr=0x87000000\0"

#define NFS_BOOTCMD \
	"nfsargs=ip=:::::eth0: root=/dev/nfs\0" \
	"nfsboot=run setup; " \
		"setenv bootargs ${defargs} ${nfsargs} " \
		"${setupargs} ${vidargs}; echo Booting from NFS...;" \
		"dhcp ${kernel_addr_r} && " \
		"tftp ${fdt_addr_r} " FDT_FILE " && " \
		"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0"

#define SD_BOOTCMD \
	"sdargs=root=/dev/mmcblk0p2 ro rootwait\0" \
	"sdboot=run setup; setenv bootargs ${defargs} ${sdargs} ${setupargs} " \
		"${vidargs}; echo Booting from MMC/SD card...; " \
		"load mmc 0:1 ${kernel_addr_r} ${kernel_file} && " \
		"load mmc 0:1 ${fdt_addr_r} " FDT_FILE " && " \
		"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0"

#ifdef CONFIG_TDX_EASY_INSTALLER
#define UBI_BOOTCMD \
	"ubiargs=ubi.mtd=ubi root=ubi0:rootfs rw rootfstype=ubifs " \
		"ubi.fm_autoconvert=1\0" \
	"ubiboot=run setup; " \
		"setenv bootargs console=ttymxc0,115200 quiet " \
		"rootfstype=squashfs root=/dev/ram autoinstall " \
		"${teziargs}; echo Booting Toradex Easy Installer...; " \
		"ubi part ubi && " \
		"ubi read ${ramdisk_addr_r} rootfs && " \
		"bootm ${ramdisk_addr_r}\0"
#else /* CONFIG_TDX_EASY_INSTALLER */
#define UBI_BOOTCMD \
	"ubiargs=ubi.mtd=ubi root=ubi0:rootfs rw rootfstype=ubifs " \
		"ubi.fm_autoconvert=1\0" \
	"ubiboot=run setup; " \
		"setenv bootargs ${defargs} ${ubiargs} " \
		"${setupargs} ${vidargs}; echo Booting from NAND...; " \
		"ubi part ubi &&" \
		"ubi read ${kernel_addr_r} kernel && " \
		"ubi read ${fdt_addr_r} dtb && " \
		"run fdt_fixup && bootz ${kernel_addr_r} - ${fdt_addr_r}\0"
#endif /* CONFIG_TDX_EASY_INSTALLER */

#ifdef CONFIG_TDX_EASY_INSTALLER
#define CONFIG_BOOTCOMMAND "setenv fdtfile " FDT_FILE " && " \
	"run bootcmd_mmc0; run ubiboot; run distro_bootcmd"
#else
#define CONFIG_BOOTCOMMAND "run ubiboot; " \
	"setenv fdtfile " FDT_FILE " && run distro_bootcmd"
#endif

#define BOOTENV_RUN_NET_USB_START ""
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(USB, usb, 0) \
	func(DHCP, dhcp, na)
#include <config_distro_bootcmd.h>
#include <config_distro_defaults.h>

#undef CONFIG_ISO_PARTITION

#define DFU_ALT_NAND_INFO "imx6ull-bcb part 0,1;u-boot1 part 0,2;u-boot2 part 0,3;u-boot-env part 0,4;ubi partubi 0,5"

#define CONFIG_EXTRA_ENV_SETTINGS \
	BOOTENV \
	MEM_LAYOUT_ENV_SETTINGS \
	NFS_BOOTCMD \
	SD_BOOTCMD \
	UBI_BOOTCMD \
	"console=ttymxc0\0" \
	"defargs=user_debug=30\0" \
	"dfu_alt_info=" DFU_ALT_NAND_INFO "\0" \
	"fdt_board=eval-v3\0" \
	"fdt_fixup=;\0" \
	"ip_dyn=yes\0" \
	"kernel_file=zImage\0" \
	"mtdparts=" MTDPARTS_DEFAULT "\0" \
	"setethupdate=if env exists ethaddr; then; else setenv ethaddr " \
		"00:14:2d:00:00:00; fi; tftpboot ${loadaddr} " \
		"${board}/flash_eth.img && source ${loadaddr}\0" \
	"setsdupdate=mmc rescan && setenv interface mmc && " \
		"fatload ${interface} 0:1 ${loadaddr} " \
		"${board}/flash_blk.img && source ${loadaddr}\0" \
	"setup=setenv setupargs " \
		"console=tty1 console=${console}" \
		",${baudrate}n8 ${memargs} consoleblank=0\0" \
	"setupdate=run setsdupdate || run setusbupdate || run setethupdate\0" \
	"setusbupdate=usb start && setenv interface usb && " \
		"fatload ${interface} 0:1 ${loadaddr} " \
		"${board}/flash_blk.img && source ${loadaddr}\0" \
	"splashpos=m,m\0" \
	"videomode=video=ctfb:x:640,y:480,depth:18,pclk:39722,le:48,ri:16,up:33,lo:10,hs:96,vs:2,sync:0,vmode:0\0" \
	"updlevel=2\0" \
	"vidargs=video=mxsfb:640x480M-16@60"

/* Miscellaneous configurable options */
#define CONFIG_SYS_LONGHELP

#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + 0x08000000)

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_HZ			1000

#define CONFIG_STACKSIZE		SZ_128K

/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* FLASH and environment organization */
#define CONFIG_SYS_NO_FLASH
#ifdef CONFIG_TDX_EASY_INSTALLER
#define CONFIG_ENV_IS_NOWHERE
#else
#define CONFIG_ENV_IS_IN_NAND
#endif

#if defined(CONFIG_ENV_IS_IN_NAND)
#define CONFIG_ENV_SECT_SIZE		(128 * 1024)
#define CONFIG_ENV_OFFSET		(28 * CONFIG_ENV_SECT_SIZE)
#define CONFIG_ENV_SIZE			CONFIG_ENV_SECT_SIZE
#else
#define CONFIG_ENV_SIZE			(8 * 1024)
#endif

#define CONFIG_NAND_MXS
#define CONFIG_BCH
#define CONFIG_CMD_WRITEBCB_MX7
#define CONFIG_CMD_NAND_TRIMFFS

/* NAND stuff */
#define CONFIG_SYS_MAX_NAND_DEVICE	1
/* used to initialize CONFIG_SYS_NAND_BASE_LIST which is unused */
#define CONFIG_SYS_NAND_BASE		-1
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_NAND_USE_FLASH_BBT

/* UBI stuff */
#define CONFIG_RBTREE
#define CONFIG_LZO
#define CONFIG_CMD_UBIFS	/* increases size by almost 60 KB */

/* Dynamic MTD partition support */
#define CONFIG_CMD_MTDPARTS	/* Enable 'mtdparts' command line support */
#define CONFIG_MTD_PARTITIONS
#define CONFIG_MTD_DEVICE	/* needed for mtdparts commands */
#define MTDIDS_DEFAULT		"nand0=gpmi-nand"
#define MTDPARTS_DEFAULT	"mtdparts=gpmi-nand:"		\
				"512k(mx6ull-bcb),"		\
				"1536k(u-boot1)ro,"		\
				"1536k(u-boot2)ro,"		\
				"512k(u-boot-env),"		\
				"-(ubi)"

/* DMA stuff, needed for GPMI/MXS NAND support */
#define CONFIG_APBH_DMA
#define CONFIG_APBH_DMA_BURST
#define CONFIG_APBH_DMA_BURST8

/* USB Configs */
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET

#define CONFIG_MXC_USB_PORTSC		(PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS		0
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2

#define CONFIG_IMX_THERMAL

#define CONFIG_USBD_HS

#define CONFIG_USB_FUNCTION_MASS_STORAGE

/* USB Device Firmware Update support */
#define CONFIG_SYS_DFU_DATA_BUF_SIZE	SZ_16M
#define DFU_DEFAULT_POLL_TIMEOUT	300

#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_MXS
#define MXS_LCDIF_BASE MX6UL_LCDIF1_BASE_ADDR
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

/* TODO new from mx6ullevk.h */
#define CONFIG_MXC_GPIO

#endif
