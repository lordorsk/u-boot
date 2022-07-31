/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *  (C) Copyright 2010,2011
 *  NVIDIA Corporation <www.nvidia.com>
 */

#ifndef _TEGRA_SPL_H_
#define _TEGRA_SPL_H_

/* I2C addr is in 8 bit */
#define TPS65911_I2C_ADDR		0x5A
#define TPS65911_VDDCTRL_OP_REG		0x28
#define TPS65911_VDDCTRL_SR_REG		0x27
#define TPS65911_VDDCTRL_OP_DATA	(0x2400 | TPS65911_VDDCTRL_OP_REG)
#define TPS65911_VDDCTRL_SR_DATA	(0x0100 | TPS65911_VDDCTRL_SR_REG)
#define I2C_SEND_2_BYTES		0x0A02

/* Tegra30-specific CPU init code */
void tegra_i2c_ll_write_addr(uint addr, uint config);
void tegra_i2c_ll_write_data(uint data, uint config);

/* Allow board setup PMIC for CPU */
void enable_pmic_cpu_power(void);

#endif	/* TEGRA_SPL_H */
