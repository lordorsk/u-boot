// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <i2c.h>
#include <log.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <panel.h>
#include <power/regulator.h>

#include <asm/gpio.h>

/* TF700T uses Toshiba TC358768 MIPI DSI bridge */

#define usleep_range(a, b) udelay((b))
#define msleep(a) udelay(a * 1000)

struct bridge_register_set {
	uint16_t addr;
	uint16_t data;
};

static const struct bridge_register_set display_table[] = {
	/* Software Reset */
	{0x0002, 0x0001}, // SYSctl, S/W Reset
	{0x0000, 0x0005}, // Delay time
	{0x0002, 0x0000}, // SYSctl, S/W Reset release

	/* PLL, Clock Setting */
	{0x0016, 0x309F}, // PLL Control Register 0 (PLL_PRD,PLL_FBD)
	{0x0018, 0x0203}, // PLL_FRS,PLL_LBWS, PLL oscillation enable
	{0x0000, 0x0005}, // Delay time
	{0x0018, 0x0213}, // PLL_FRS,PLL_LBWS, PLL clock out enable

	/* DPI Input Control */
	{0x0006, 0x012C}, // FIFO Control Register

	/* D-PHY Setting */
	{0x0140, 0x0000}, // D-PHY Clock Lane enable
	{0x0142, 0x0000},
	{0x0144, 0x0000}, // D-PHY Data Lane0 enable
	{0x0146, 0x0000},
	{0x0148, 0x0000}, // D-PHY Data Lane1 enable
	{0x014A, 0x0000},
	{0x014C, 0x0000}, // D-PHY Data Lane2 enable
	{0x014E, 0x0000},
	{0x0150, 0x0000}, // D-PHY Data Lane3 enable
	{0x0152, 0x0000},

	{0x0100, 0x0203}, // D-PHY Clock Lane Control TX
	{0x0102, 0x0000},
	{0x0104, 0x0203}, // D-PHY Data Lane0 Control TX
	{0x0106, 0x0000},
	{0x0108, 0x0203}, // D-PHY Data Lane1 Control TX
	{0x010A, 0x0000},
	{0x010C, 0x0203}, // D-PHY Data Lane2 Control TX
	{0x010E, 0x0000},
	{0x0110, 0x0203}, // D-PHY Data Lane3 Control TX
	{0x0112, 0x0000},

	/* DSI-TX PPI Control */
	{0x0210, 0x1964}, // LINEINITCNT
	{0x0212, 0x0000},
	{0x0214, 0x0005}, // LPTXTIMECNT
	{0x0216, 0x0000},
	{0x0218, 0x2801}, // TCLK_HEADERCNT
	{0x021A, 0x0000},
	{0x021C, 0x0000}, // TCLK_TRAILCNT
	{0x021E, 0x0000},
	{0x0220, 0x0C06}, // THS_HEADERCNT
	{0x0222, 0x0000},
	{0x0224, 0x4E20}, // TWAKEUPCNT
	{0x0226, 0x0000},
	{0x0228, 0x000B}, // TCLK_POSTCNT
	{0x022A, 0x0000},
	{0x022C, 0x0005}, // THS_TRAILCNT
	{0x022E, 0x0000},
	{0x0230, 0x0005}, // HSTXVREGCNT
	{0x0232, 0x0000},
	{0x0234, 0x001F}, // HSTXVREGEN enable
	{0x0236, 0x0000},
	{0x0238, 0x0001}, // DSI clock enable/disable during LP
	{0x023A, 0x0000},
	{0x023C, 0x0005}, // BTACNTRL1
	{0x023E, 0x0005},
	{0x0204, 0x0001}, // STARTCNTRL
	{0x0206, 0x0000},

	/* DSI-TX Timing Control */
	{0x0620, 0x0001}, // Sync Pulse/Sync Event mode setting
	{0x0622, 0x0020}, // V Control Register1
	{0x0624, 0x001A}, // V Control Register2
	{0x0626, 0x04B0}, // V Control Register3
	{0x0628, 0x015E}, // H Control Register1
	{0x062A, 0x00FA}, // H Control Register2
	{0x062C, 0x1680}, // H Control Register3

	{0x0518, 0x0001}, // DSI Start
	{0x051A, 0x0000},

	/* Set to HS mode */
	{0x0500, 0x0086}, // DSI lane setting, DSI mode=HS
	{0x0502, 0xA300}, // bit set
	{0x0500, 0x8000}, // Switch to DSI mode
	{0x0502, 0xC300},

	/* Host: RGB(DPI) input start */
	{0x0008, 0x0037}, // DSI-TX Format setting
	{0x0050, 0x003E}, // DSI-TX Pixel Stream packet Data Type setting
	{0x0032, 0x0001}, // HSYNC polarity
	{0x0004, 0x0064}, // Configuration Control Register
};

struct tc358768_priv {
	struct udevice *panel;

	struct udevice *vddc;
	struct udevice *vddmipi;
	struct udevice *vddio;

	struct clk *refclk;

	struct gpio_desc gate_gpio;
	struct gpio_desc reset_gpio;
};

static int tc358768_write_reg(struct udevice *dev, unsigned char addr,
			      unsigned char data)
{
	struct dm_i2c_chip *chip = dev_get_parent_plat(dev);
	unsigned char buf[4];
	struct i2c_msg msg[1];
	int err;

	buf[0] = (addr >> 8);
	buf[1] = (addr & 0xFF);
	buf[2] = (data >> 8);
	buf[3] = (data & 0xFF);

	msg->addr = chip->chip_addr;
	msg->flags = 0;
	msg->len = ARRAY_SIZE(buf);
	msg->buf = buf;

	err = dm_i2c_xfer(dev, msg, ARRAY_SIZE(msg));
	if (err) {
		printf("%s: write failed, err = %d, addr = %#x, data = %#x\n",
			__func__, err, addr, data);
		return err;
	}

	return 0;
}

static int tc358768_write_table(struct udevice *dev,
				const struct bridge_register_set *regs, int nregs)
{
	int err, i;

	for (i = 0; i < nregs; i++) {
		if (!regs[i].addr)
			mdelay(regs[i].data);

		err = tc358768_write_reg(dev, regs[i].addr, regs[i].data);
		if (err)
			break;
	}

	return err;
}

static int tc358768_set_backlight(struct udevice *dev, int percent)
{
	return 0;
}

static int tc358768_hw_enable(struct udevice *dev)
{
	struct tc358768_priv *uc_priv = dev_get_priv(dev);
	int ret;

	ret = dm_gpio_set_value(&uc_priv->gate_gpio, true);
	if (ret) {
		printf("%s: error changing gate-gpio (%d)\n", __func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(uc_priv->refclk);
	if (ret) {
		printf("%s: error enabling refclk (%d)\n", __func__, ret);
		return ret;
	}

	ret = regulator_set_enable_if_allowed(uc_priv->vddc, true);
	if (ret) {
		printf("%s: error enabling vddc (%d)\n", __func__, ret);
		return ret;
	}

	ret = regulator_set_enable_if_allowed(uc_priv->vddmipi, true);
	if (ret) {
		printf("%s: error enabling vddmipi (%d)\n", __func__, ret);
		return ret;
	}

	msleep(10);

	ret = regulator_set_enable_if_allowed(uc_priv->vddio, true);
	if (ret) {
		printf("%s: error enabling vddio (%d)\n", __func__, ret);
		return ret;
	}

	usleep_range(200, 300);

	/*
	 * The RESX is active low (GPIO_ACTIVE_LOW).
	 * DEASSERT (value = 0) the reset_gpio to enable the chip
	 */
	ret = dm_gpio_set_value(&uc_priv->reset_gpio, false);
	if (ret)
		printf("%s: error changing reset-gpio (%d)\n", __func__, ret);

	/* wait for encoder clocks to stabilize */
	usleep_range(1000, 2000);

	return 0;
}

static int tc358768_attach(struct udevice *dev)
{
	struct tc358768_priv *uc_priv = dev_get_priv(dev);
	struct dm_i2c_chip *chip = dev_get_parent_plat(dev);
	struct i2c_msg msg[2];
	unsigned char buf[4] = { 0, 0, 0, 0 };
	int ret;

	/* Enable sequence */
	ret = tc358768_hw_enable(dev);
	if (ret)
		return ret;

	/* First msg */
	msg[0].addr = chip->chip_addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	/* high byte goes out first */
	buf[0] = 0;
	buf[1] = 0;

	msg[1].addr = chip->chip_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = buf + 2;

	ret = dm_i2c_xfer(dev, msg, 2);
	if (ret) {
		printf("%s: read failed with %d\n", __func__, ret);
		return ret;
	}

	ret = tc358768_write_table(dev, display_table, ARRAY_SIZE(display_table));
	if (ret) {
		printf("%s: table write failed\n", __func__);
		return ret;
	}

	mdelay(35);

	ret = panel_enable_backlight(uc_priv->panel);
	if (ret)
		return ret;

	return 0;
}

static int tc358768_setup(struct udevice *dev)
{
	struct tc358768_priv *uc_priv = dev_get_priv(dev);
	int ret;

	ret = uclass_get_device_by_phandle(UCLASS_PANEL, dev,
					   "panel", &uc_priv->panel);
	if (ret) {
		printf("%s: Cannot get panel: ret=%d\n", __func__, ret);
		return log_ret(ret);
	}

	/* get regulators */
	ret = device_get_supply_regulator(dev, "vddc-supply", &uc_priv->vddc);
	if (ret) {
		printf("%s: vddc regulator error: %d\n", __func__, ret);
		return ret;
	}

	ret = device_get_supply_regulator(dev, "vddmipi-supply", &uc_priv->vddmipi);
	if (ret) {
		printf("%s: vddmipi regulator error: %d\n", __func__, ret);
		return ret;
	}

	ret = device_get_supply_regulator(dev, "vddio-supply", &uc_priv->vddio);
	if (ret) {
		printf("%s: vddio regulator error: %d\n", __func__, ret);
		return ret;
	}

	/* get clk */
	uc_priv->refclk = devm_clk_get_optional(dev, "refclk");
	if (IS_ERR(uc_priv->refclk)) {
		printf("%s: Could not get refclk: %ld\n",
			__func__, PTR_ERR(uc_priv->refclk));
		return PTR_ERR(uc_priv->refclk);
	}

	/* get gpios */
	ret = gpio_request_by_name(dev, "gate-gpios", 0,
				   &uc_priv->gate_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("%s: Could not decode gate-gpios (%d)\n", __func__, ret);
		return ret;
	}

	/* get gpios */
	ret = gpio_request_by_name(dev, "reset-gpios", 0,
				   &uc_priv->reset_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("%s: Could not decode reset-gpios (%d)\n", __func__, ret);
		return ret;
	}

	/*
	 * RESX is low active, to disable tc358768 initially (keep in reset)
	 * the gpio line must be LOW. This is the ASSERTED state of
	 * GPIO_ACTIVE_LOW (GPIOD_OUT_HIGH == ASSERTED).
	 */
	ret = dm_gpio_set_value(&uc_priv->reset_gpio, true);
	if (ret)
		printf("%s: error changing reset-gpio (%d)\n", __func__, ret);

	return 0;
}

static int tc358768_probe(struct udevice *dev)
{
	if (device_get_uclass_id(dev->parent) != UCLASS_I2C)
		return -EPROTONOSUPPORT;

	return tc358768_setup(dev);
}

struct panel_ops tc358768_ops = {
	.enable_backlight	= tc358768_attach,
	.set_backlight		= tc358768_set_backlight,
};

static const struct udevice_id tc358768_ids[] = {
	{ .compatible = "toshiba,tc358768" },
	{ }
};

U_BOOT_DRIVER(tc358768) = {
	.name		= "tc358768",
	.id		= UCLASS_PANEL,
	.of_match 	= tc358768_ids,
	.probe		= tc358768_probe,
	.ops		= &tc358768_ops,
	.priv_auto	= sizeof(struct tc358768_priv),
};
