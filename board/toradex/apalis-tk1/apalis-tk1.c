/*
 * Copyright (c) 2016-2017 Toradex, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch-tegra/ap.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pinmux.h>
#include <fdt_support.h>
#include <pci_tegra.h>
#include <power/as3722.h>

#include "../common/tdx-common.h"
#include "pinmux-config-apalis-tk1.h"

#define FAN_EN		TEGRA_GPIO(DD, 2) /* Apalis GPIO8 */
#define LAN_DEV_OFF_N	TEGRA_GPIO(O, 6)
#define LAN_RESET_N	TEGRA_GPIO(S, 2)
#define LAN_WAKE_N	TEGRA_GPIO(O, 5)
#ifdef CONFIG_APALIS_TK1_PCIE_EVALBOARD_INIT
#define PEX_PERST_N	TEGRA_GPIO(DD, 1) /* Apalis GPIO7 */
#define RESET_MOCI_CTRL	TEGRA_GPIO(U, 4)
#endif /* CONFIG_APALIS_TK1_PCIE_EVALBOARD_INIT */
#define VCC_USBH	TEGRA_GPIO(T, 6)
#define VCC_USBH_V1_0	TEGRA_GPIO(N, 5)
#define VCC_USBO1	TEGRA_GPIO(T, 5)
#define VCC_USBO1_V1_0	TEGRA_GPIO(N, 4)

int arch_misc_init(void)
{
	if (readl(NV_PA_BASE_SRAM + NVBOOTINFOTABLE_BOOTTYPE) ==
	    NVBOOTTYPE_RECOVERY) {
		printf("USB recovery mode, disabled autoboot\n");
#ifdef CONFIG_TDX_EASY_INSTALLER
		setenv("bootdelay", "-2");
		setenv("defargs", "pcie_aspm=off user_debug=30");
		setenv("fdt_high", "");
		setenv("initrd_high", "");
		setenv("setup", "setenv setupargs igb_mac=${ethaddr} " \
			"consoleblank=0 no_console_suspend=1 " \
			"console=${console},${baudrate}n8 ${memargs}");
		setenv("teziargs", "rootfstype=squashfs root=/dev/ram quiet " \
			"autoinstall");
		setenv("vidargs", "video=HDMI-A-1:640x480-16@60 hotplugfb");
		setenv("bootcmd", "run setup; setenv bootargs ${defargs} " \
			"${setupargs} ${vidargs} ${teziargs}; " \
			"bootm 0x80208000#config@${fdt_module}");
#else /* CONFIG_TDX_EASY_INSTALLER */
		setenv("bootdelay", "-1");
#endif /* CONFIG_TDX_EASY_INSTALLER */
	}

	/* PCB Version Indication: V1.2 and later have GPIO_PV0 wired to GND */
	gpio_request(TEGRA_GPIO(V, 0), "PCB Version Indication");
	gpio_direction_input(TEGRA_GPIO(V, 0));
	if (gpio_get_value(TEGRA_GPIO(V, 0))) {
		/*
		 * if using the default device tree for new V1.2 and later HW,
		 * use version for older V1.0 and V1.1 HW
		 */
		char *fdt_env = getenv("fdt_module");
		if ((fdt_env != NULL) && (strcmp(FDT_MODULE, fdt_env) == 0)) {
			setenv("fdt_module", FDT_MODULE_V1_0);
			printf("patching fdt_module to " FDT_MODULE_V1_0
			       " for older V1.0 and V1.1 HW\n");
#ifndef CONFIG_ENV_IS_NOWHERE
			saveenv();
#endif
		}

		/* activate USB power enable GPIOs */
		gpio_request(VCC_USBH_V1_0, "VCC_USBH");
		gpio_direction_output(VCC_USBH_V1_0, 1);
		gpio_request(VCC_USBO1_V1_0, "VCC_USBO1");
		gpio_direction_output(VCC_USBO1_V1_0, 1);
	} else {
		/* activate USB power enable GPIOs */
		gpio_request(VCC_USBH, "VCC_USBH");
		gpio_direction_output(VCC_USBH, 1);
		gpio_request(VCC_USBO1, "VCC_USBO1");
		gpio_direction_output(VCC_USBO1, 1);
	}

	return 0;
}

int checkboard(void)
{
	puts("Model: Toradex Apalis TK1 2GB\n");

	return 0;
}

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, bd_t *bd)
{
	uint8_t enetaddr[6];

	/* MAC addr */
	if (eth_getenv_enetaddr("ethaddr", enetaddr)) {
		int err = fdt_find_and_setprop(blob,
				     "/pcie@1003000/pci@2,0/pcie@0",
				     "local-mac-address", enetaddr, 6, 0);

		if (err >= 0)
			puts("   MAC address updated...\n");
	}

	return ft_common_board_setup(blob, bd);
}
#endif

/*
 * Routine: pinmux_init
 * Description: Do individual peripheral pinmux configs
 */
void pinmux_init(void)
{
	pinmux_clear_tristate_input_clamping();

	gpio_config_table(apalis_tk1_gpio_inits,
			  ARRAY_SIZE(apalis_tk1_gpio_inits));

	pinmux_config_pingrp_table(apalis_tk1_pingrps,
				   ARRAY_SIZE(apalis_tk1_pingrps));

	pinmux_config_drvgrp_table(apalis_tk1_drvgrps,
				   ARRAY_SIZE(apalis_tk1_drvgrps));
}

#ifdef CONFIG_PCI_TEGRA
int tegra_pcie_board_init(void)
{
	struct udevice *pmic;
	int err;

	err = as3722_init(&pmic);
	if (err) {
		error("failed to initialize AS3722 PMIC: %d\n", err);
		return err;
	}

	err = as3722_sd_enable(pmic, 4);
	if (err < 0) {
		error("failed to enable SD4: %d\n", err);
		return err;
	}

	err = as3722_sd_set_voltage(pmic, 4, 0x24);
	if (err < 0) {
		error("failed to set SD4 voltage: %d\n", err);
		return err;
	}

	err = as3722_gpio_configure(pmic, 1, AS3722_GPIO_OUTPUT_VDDH |
					     AS3722_GPIO_INVERT);
	if (err < 0) {
		error("failed to configure GPIO#1 as output: %d\n", err);
		return err;
	}

	err = as3722_gpio_direction_output(pmic, 2, 1);
	if (err < 0) {
		error("failed to set GPIO#2 high: %d\n", err);
		return err;
	}

	gpio_request(LAN_DEV_OFF_N, "LAN_DEV_OFF_N");
	gpio_request(LAN_RESET_N, "LAN_RESET_N");
	gpio_request(LAN_WAKE_N, "LAN_WAKE_N");

#ifdef CONFIG_APALIS_TK1_PCIE_EVALBOARD_INIT
	gpio_request(PEX_PERST_N, "PEX_PERST_N");
	gpio_request(RESET_MOCI_CTRL, "RESET_MOCI_CTRL");
#endif /* CONFIG_APALIS_TK1_PCIE_EVALBOARD_INIT */

	return 0;
}

void tegra_pcie_board_port_reset(struct tegra_pcie_port *port)
{
	int index = tegra_pcie_port_index_of_port(port);
	if (index == 1) { /* I210 Gigabit Ethernet Controller (On-module) */
		struct udevice *pmic;
		int err;

		err = as3722_init(&pmic);
		if (err) {
			error("failed to initialize AS3722 PMIC: %d\n", err);
			return;
		}

		/* Reset I210 Gigabit Ethernet Controller */
		gpio_direction_output(LAN_RESET_N, 0);

		/*
		 * Make sure we don't get any back feeding from DEV_OFF_N resp.
		 * LAN_WAKE_N
		 */
		gpio_direction_output(LAN_DEV_OFF_N, 0);
		gpio_direction_output(LAN_WAKE_N, 0);

		/* Make sure LDO9 and LDO10 are initially enabled @ 0V */
		err = as3722_ldo_enable(pmic, 9);
		if (err < 0) {
			error("failed to enable LDO9: %d\n", err);
			return;
		}
		err = as3722_ldo_enable(pmic, 10);
		if (err < 0) {
			error("failed to enable LDO10: %d\n", err);
			return;
		}
		err = as3722_ldo_set_voltage(pmic, 9, 0x80);
		if (err < 0) {
			error("failed to set LDO9 voltage: %d\n", err);
			return;
		}
		err = as3722_ldo_set_voltage(pmic, 10, 0x80);
		if (err < 0) {
			error("failed to set LDO10 voltage: %d\n", err);
			return;
		}

		/* Make sure controller gets enabled by disabling DEV_OFF_N */
		gpio_set_value(LAN_DEV_OFF_N, 1);

		/*
		 * Enable LDO9 and LDO10 for +V3.3_ETH on patched prototype
		 * V1.0A and sample V1.0B and newer modules
		 */
		err = as3722_ldo_set_voltage(pmic, 9, 0xff);
		if (err < 0) {
			error("failed to set LDO9 voltage: %d\n", err);
			return;
		}
		err = as3722_ldo_set_voltage(pmic, 10, 0xff);
		if (err < 0) {
			error("failed to set LDO10 voltage: %d\n", err);
			return;
		}

		/*
		 * Must be asserted for 100 ms after power and clocks are stable
		 */
		mdelay(100);

		gpio_set_value(LAN_RESET_N, 1);
	} else if (index == 0) { /* Apalis PCIe */
#ifdef CONFIG_APALIS_TK1_PCIE_EVALBOARD_INIT
		/*
		 * Reset PLX PEX 8605 PCIe Switch plus PCIe devices on Apalis
		 * Evaluation Board
		 */
		gpio_direction_output(PEX_PERST_N, 0);
		gpio_direction_output(RESET_MOCI_CTRL, 0);

		/*
		 * Must be asserted for 100 ms after power and clocks are stable
		 */
		mdelay(100);

		gpio_set_value(PEX_PERST_N, 1);
		/*
		 * Err_5: PEX_REFCLK_OUTpx/nx Clock Outputs is not Guaranteed
		 * Until 900 us After PEX_PERST# De-assertion
		 */
		mdelay(1);
		gpio_set_value(RESET_MOCI_CTRL, 1);
#endif /* CONFIG_APALIS_TK1_PCIE_EVALBOARD_INIT */
	}
}
#endif /* CONFIG_PCI_TEGRA */

void start_cpu_fan(void)
{
	gpio_request(FAN_EN, "FAN_EN");
	gpio_direction_output(FAN_EN, 1);
}

/*
 * Backlight off before OS handover
 */
void board_preboot_os(void)
{
	gpio_request(TEGRA_GPIO(BB, 5), "BL_ON");
	gpio_direction_output(TEGRA_GPIO(BB, 5), 0);
}
