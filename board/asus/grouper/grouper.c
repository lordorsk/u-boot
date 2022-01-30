// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  (C) Copyright 2010-2013
 *  NVIDIA Corporation <www.nvidia.com>
 *
 *  (C) Copyright 2021
 *  Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <dm.h>
#include <i2c.h>
#include <log.h>
#include <asm/arch/pinmux.h>
#include <asm/arch/gp_padctrl.h>
#include <asm/arch/gpio.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include "pinmux-config-grouper.h"

#define TPS65911_I2C_ADDRESS	0x2D
#define MAX_I2C_RETRY		3

#define TPS65911_LDO1		0x30
#define TPS65911_DEVCTRL	0x3F

#define DEVCTRL_PWR_OFF_MASK	0x80
#define DEVCTRL_DEV_OFF_MASK	0x01
#define DEVCTRL_DEV_ON_MASK	0x04

#define MAX77663_I2C_ADDRESS	0x3C
#define MAX77620_REG_ONOFFCNFG1	0x41
#define MAX77620_ONOFFCNFG1_SFT_RST 0x40

#ifdef CONFIG_CMD_POWEROFF
#ifdef CONFIG_GROUPER_TPS65911
int do_poweroff(struct cmd_tbl *cmdtp,
		       int flag, int argc, char *const argv[])
{
	struct udevice *dev;
	uchar data_buffer[1];
	int ret;

	ret = i2c_get_chip_for_busnum(0, TPS65911_I2C_ADDRESS, 1, &dev);
	if (ret) {
		debug("%s: Cannot find PMIC I2C chip\n", __func__);
		return 0;
	}

	ret = dm_i2c_read(dev, TPS65911_DEVCTRL, data_buffer, 1);
	if (ret)
		return ret;

	data_buffer[0] |= DEVCTRL_PWR_OFF_MASK;

	ret = dm_i2c_write(dev, TPS65911_DEVCTRL, data_buffer, 1);
	if (ret)
		return ret;

	data_buffer[0] |= DEVCTRL_DEV_OFF_MASK;
	data_buffer[0] &= ~DEVCTRL_DEV_ON_MASK;

	ret = dm_i2c_write(dev, TPS65911_DEVCTRL, data_buffer, 1);
	if (ret)
		return ret;

	// wait some time and then print error
	mdelay(5000);
	printf("Failed to power off!!!\n");
	return 1;
}
#endif /* CONFIG_GROUPER_TPS65911 */

#ifdef CONFIG_GROUPER_MAX77663
int do_poweroff(struct cmd_tbl *cmdtp,
		       int flag, int argc, char *const argv[])
{
	struct udevice *dev;
	uchar data_buffer[1];
	int ret;

	ret = i2c_get_chip_for_busnum(0, MAX77663_I2C_ADDRESS, 1, &dev);
	if (ret) {
		debug("%s: Cannot find PMIC I2C chip\n", __func__);
		return 0;
	}

	ret = dm_i2c_read(dev, MAX77620_REG_ONOFFCNFG1, data_buffer, 1);
	if (ret)
		return ret;

	data_buffer[0] |= MAX77620_ONOFFCNFG1_SFT_RST;

	ret = dm_i2c_write(dev, MAX77620_REG_ONOFFCNFG1, data_buffer, 1);
	if (ret)
		return ret;

	// wait some time and then print error
	mdelay(5000);
	printf("Failed to power off!!!\n");
	return 1;
}
#endif /* CONFIG_GROUPER_MAX77663 */
#endif /* CONFIG_CMD_POWEROFF */

/*
 * Routine: pinmux_init
 * Description: Do individual peripheral pinmux configs
 */
void pinmux_init(void)
{
	pinmux_config_pingrp_table(tegra3_pinmux_common,
		ARRAY_SIZE(tegra3_pinmux_common));

	pinmux_config_pingrp_table(unused_pins_lowpower,
		ARRAY_SIZE(unused_pins_lowpower));

	/* Initialize any non-default pad configs (APB_MISC_GP regs) */
	pinmux_config_drvgrp_table(cardhu_padctrl, ARRAY_SIZE(cardhu_padctrl));
}

#ifdef CONFIG_MMC_SDHCI_TEGRA
/*
 * Do I2C/PMU writes to bring up SD card and eMMC bus power
 */
void board_sdmmc_tps65911_voltage_init(void)
{
	struct udevice *dev;
	uchar data_buffer[1];
	int ret;
	int i;

	ret = i2c_get_chip_for_busnum(0, TPS65911_I2C_ADDRESS, 1, &dev);
	if (ret) {
		debug("%s: Cannot find PMIC I2C chip\n", __func__);
		return;
	}

	/* TPS659110: LDO1_REG = 3.3v, ACTIVE to SDMMC4 */
	data_buffer[0] = 0xc9;

	for (i = 0; i < MAX_I2C_RETRY; ++i) {
		if (dm_i2c_write(dev, TPS65911_LDO1, data_buffer, 1))
			udelay(100);
	}
}

/*
 * Routine: pin_mux_mmc
 * Description: setup the MMC muxes, power rails, etc.
 */
void pin_mux_mmc(void)
{
	/*
	 * NOTE: We don't do mmc-specific pin muxes here.
	 * They were done globally in pinmux_init().
	 */

#ifdef CONFIG_GROUPER_TPS65911
	/* Bring up eMMC power on TI PMIC*/
	board_sdmmc_tps65911_voltage_init();
#endif
}
#endif	/* MMC */
