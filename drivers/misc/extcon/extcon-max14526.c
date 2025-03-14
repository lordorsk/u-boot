// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Svyatoslav Ryhel <clamor95@gmail.com>
 *
 * U-boot lacks extcon DM.
 */

#include <common.h>
#include <dm.h>
#include <i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <log.h>
#include <misc.h>
#include <asm/gpio.h>

#define CONTROL_1	0x01
#define SW_CONTROL	0x03

#define ID_200		0x10
#define ADC_EN		0x02
#define CP_EN		0x01

#define DP_USB		0x00
#define DP_UART		0x08
#define DP_AUDIO	0x10
#define DP_OPEN		0x38

#define DM_USB		0x00
#define DM_UART		0x01
#define DM_AUDIO	0x02
#define DM_OPEN		0x07

#define AP_USB		BIT(0)
#define CP_USB		BIT(1)
#define CP_UART		BIT(2)

struct max14526_priv {
	struct gpio_desc usif_gpio;
	struct gpio_desc dp2t_gpio;
	struct gpio_desc ifx_usb_vbus_gpio;
};

static void max14526_set_mode(struct udevice *dev, int mode)
{
	struct max14526_priv *priv = dev_get_priv(dev);
	uchar val;
	int ret;

	if ((mode & AP_USB) || (mode & CP_USB)) {
		/* Connect CP UART signals to AP */
		ret = dm_gpio_set_value(&priv->usif_gpio, 0);
		if (ret)
			printf("%s: error changing usif-gpio (%d)\n", __func__, ret);
	}

	if (mode & CP_UART) {
		/* Connect CP UART signals to DP2T */
		ret = dm_gpio_set_value(&priv->usif_gpio, 1);
		if (ret)
			printf("%s: error changing usif-gpio (%d)\n", __func__, ret);
	}

	if (mode & CP_USB) {
		/* Connect CP USB to MUIC UART */
		ret = dm_gpio_set_value(&priv->ifx_usb_vbus_gpio, 1);
		if (ret)
			printf("%s: error changing usb-vbus-gpio (%d)\n", __func__, ret);

		ret = dm_gpio_set_value(&priv->dp2t_gpio, 1);
		if (ret)
			printf("%s: error changing dp2t-gpio (%d)\n", __func__, ret);
	}

	if ((mode & AP_USB) || (mode & CP_UART)) {
		/* Connect CP UART to MUIC UART */
		ret = dm_gpio_set_value(&priv->dp2t_gpio, 0);
		if (ret)
			printf("%s: error changing dp2t-gpio (%d)\n", __func__, ret);
	}

	if (mode & AP_USB) {
		/* Enables USB Path */
		val = DP_USB | DM_USB;
		ret = dm_i2c_write(dev, SW_CONTROL, &val, 1);
		if (ret)
			printf("USB Path set failed: %d\n", ret);
	}

	if ((mode & CP_USB) || (mode & CP_UART)) {
		/* Enables UART Path */
		val = DP_UART | DM_UART;
		ret = dm_i2c_write(dev, SW_CONTROL, &val, 1);
		if (ret)
			printf("USB Path set failed: %d\n", ret);
	}

	/* Enables 200K, Charger Pump, and ADC */
	val = ID_200 | ADC_EN | CP_EN;
	ret = dm_i2c_write(dev, CONTROL_1, &val, 1);
	if (ret)
		printf("200K, Charger Pump, and ADC set failed: %d\n", ret);
}

static int max14526_probe(struct udevice *dev)
{
	struct max14526_priv *priv = dev_get_priv(dev);
	int ret, mode = 0;

	ret = gpio_request_by_name(dev, "usif-gpios", 0,
				   &priv->usif_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("%s: Could not decode usif-gpios (%d)\n", __func__, ret);
		return ret;
	}

	ret = gpio_request_by_name(dev, "dp2t-gpios", 0,
				   &priv->dp2t_gpio, GPIOD_IS_OUT);
	if (ret) {
		printf("%s: Could not decode dp2t-gpios (%d)\n", __func__, ret);
		return ret;
	}

	if (dev_read_bool(dev, "maxim,ap-usb"))
		mode |= AP_USB;

	if (dev_read_bool(dev, "maxim,cp-usb")) {
		mode |= CP_USB;

		ret = gpio_request_by_name(dev, "usb-vbus-gpios", 0,
					   &priv->ifx_usb_vbus_gpio, GPIOD_IS_OUT);
		if (ret) {
			printf("%s: Could not decode dp2t-gpios (%d)\n", __func__, ret);
			return ret;
		}
	}

	if (dev_read_bool(dev, "maxim,cp-uart"))
		mode |= CP_UART;

	max14526_set_mode(dev, mode);

	return 0;
}

static const struct udevice_id max14526_ids[] = {
	{ .compatible = "maxim,max14526-muic" },
	{ }
};

U_BOOT_DRIVER(extcon_max14526) = {
	.name		= "extcon_max14526",
	.id		= UCLASS_MISC,
	.of_match	= max14526_ids,
	.probe		= max14526_probe,
	.priv_auto	= sizeof(struct max14526_priv),
};
