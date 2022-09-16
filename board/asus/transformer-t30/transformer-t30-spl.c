// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  T30 Transformers SPL stage configuration
 *
 *  (C) Copyright 2010-2013
 *  NVIDIA Corporation <www.nvidia.com>
 *
 *  (C) Copyright 2021
 *  Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <asm/arch-tegra/tegra_spl.h>
#include <linux/delay.h>

/* I2C addr is in 8 bit */
#define TPS62361B_I2C_ADDR		0xC0
#define TPS62361B_SET3_REG		0x03
#define TPS62361B_SET3_DATA		(0x4600 | TPS62361B_SET3_REG)

void enable_pmic_cpu_power(void)
{
	/* Set VDD_CORE to 1.200V. */
	tegra_i2c_ll_write_addr(TPS62361B_I2C_ADDR, 2);
	tegra_i2c_ll_write_data(TPS62361B_SET3_DATA, I2C_SEND_2_BYTES);

	udelay(1000);

	/*
	 * Bring up CPU VDD via the TPS65911x PMIC on the DVC I2C bus.
	 * First set VDD to 1.0125V, then enable the VDD regulator.
	 */
	tegra_i2c_ll_write_addr(TPS65911_I2C_ADDR, 2);
	tegra_i2c_ll_write_data(TPS65911_VDDCTRL_OP_DATA, I2C_SEND_2_BYTES);
	udelay(1000);
	tegra_i2c_ll_write_data(TPS65911_VDDCTRL_SR_DATA, I2C_SEND_2_BYTES);
	udelay(10 * 1000);
}
