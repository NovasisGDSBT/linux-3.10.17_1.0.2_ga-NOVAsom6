/*
 * arch/arm/mach-imx/devices/novasom-rfkill.c
 *
 * Copyright (C) 2015 Fil <filippo.visocchi@novasis.it>
 *
 * based on arch/arm/mach-imx/devices/wand-rfkill.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>


struct novasom_rfkill_data {
	struct rfkill *rfkill_dev;
	int shutdown_gpio;
	const char *shutdown_name;
};

static int novasom_rfkill_set_block(void *data, bool blocked)
{
struct novasom_rfkill_data *rfkill = data;

	pr_debug("novasomboard-rfkill: set block %d\n", blocked);

	if (blocked) {
		if (gpio_is_valid(rfkill->shutdown_gpio))
			gpio_direction_output(rfkill->shutdown_gpio, 0);
	} else {
		if (gpio_is_valid(rfkill->shutdown_gpio))
			gpio_direction_output(rfkill->shutdown_gpio, 1);
	}

	return 0;
}

static const struct rfkill_ops novasom_rfkill_ops = {
	.set_block = novasom_rfkill_set_block,
};

static int novasom_rfkill_wifi_probe(struct device *dev, struct device_node *np, struct novasom_rfkill_data *rfkill)
{
int ret;
int wl_ref_on, wl_rst_n, wl_reg_on, wl_wake, wl_host_wake;

	wl_reg_on = of_get_named_gpio(np, "wifi-reg-on", 0);
	wl_wake = of_get_named_gpio(np, "wifi-wake", 0);
	wl_host_wake = of_get_named_gpio(np, "wifi-host-wake", 0);

	wl_ref_on = of_get_named_gpio(np, "wifi-ref-on", 0);
	wl_rst_n = of_get_named_gpio(np, "wifi-rst-n", 0);

	if (!gpio_is_valid(wl_rst_n) || !gpio_is_valid(wl_ref_on) ||
			!gpio_is_valid(wl_reg_on) || !gpio_is_valid(wl_wake) ||
			!gpio_is_valid(wl_host_wake)) {

		printk("%s : Incorrect wifi gpios (%d %d %d %d %d)\n",__FUNCTION__,
				wl_rst_n, wl_ref_on, wl_reg_on, wl_wake, wl_host_wake);
		return -EINVAL;
	}

	printk("%s : Initialize wifi chip\n",__FUNCTION__);

	gpio_request(wl_rst_n, "wl_rst_n");
	gpio_direction_output(wl_rst_n, 0);
	msleep(25);
	gpio_set_value(wl_rst_n, 1);
	msleep(25);

	gpio_request(wl_ref_on, "wl_ref_on");
	gpio_direction_output(wl_ref_on, 1);

	gpio_request(wl_reg_on, "wl_reg_on");
	gpio_direction_output(wl_reg_on, 1);

	gpio_request(wl_wake, "wl_wake");
	gpio_direction_output(wl_wake, 1);

	gpio_request(wl_host_wake, "wl_host_wake");
	gpio_direction_input(wl_host_wake);

	rfkill->shutdown_name = "wifi_shutdown";
	rfkill->shutdown_gpio = wl_reg_on;

	rfkill->rfkill_dev = rfkill_alloc("wifi-rfkill", dev, RFKILL_TYPE_WLAN,
			&novasom_rfkill_ops, rfkill);
	if (!rfkill->rfkill_dev) {
		ret = -ENOMEM;
		goto wifi_fail_free_gpio;
	}

	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		goto wifi_fail_unregister;

	dev_info(dev, "wifi-rfkill registered.\n");

	return 0;

wifi_fail_unregister:
	rfkill_destroy(rfkill->rfkill_dev);
wifi_fail_free_gpio:
	if (gpio_is_valid(wl_rst_n))     gpio_free(wl_rst_n);
	if (gpio_is_valid(wl_ref_on))    gpio_free(wl_ref_on);
	if (gpio_is_valid(wl_reg_on))    gpio_free(wl_reg_on);
	if (gpio_is_valid(wl_wake))      gpio_free(wl_wake);
	if (gpio_is_valid(wl_host_wake)) gpio_free(wl_host_wake);

	return ret;
}

static int novasom_rfkill_bt_probe(struct device *dev, struct device_node *np, struct novasom_rfkill_data *rfkill)
{
int ret;
int bt_on, bt_wake, bt_host_wake;

	bt_on = of_get_named_gpio(np, "bluetooth-on", 0);
	bt_wake = of_get_named_gpio(np, "bluetooth-wake", 0);
	bt_host_wake = of_get_named_gpio(np, "bluetooth-host-wake", 0);

	if (!gpio_is_valid(bt_on) || !gpio_is_valid(bt_wake) || !gpio_is_valid(bt_host_wake)) {

		dev_err(dev, "incorrect bt gpios (%d %d %d)\n",
				bt_on, bt_wake, bt_host_wake);
		return -EINVAL;
	}

	dev_info(dev, "initialize bluetooth chip\n");

	gpio_request(bt_on, "bt_on");
	gpio_direction_output(bt_on, 0);
	msleep(11);
	gpio_set_value(bt_on, 1);

	gpio_request(bt_wake, "bt_wake");
	gpio_direction_output(bt_wake, 1);

	gpio_request(bt_host_wake, "bt_host_wake");
	gpio_direction_input(bt_host_wake);

	rfkill->shutdown_name = "bluetooth_shutdown";
	rfkill->shutdown_gpio = bt_on;

	rfkill->rfkill_dev = rfkill_alloc("bluetooth-rfkill", dev, RFKILL_TYPE_BLUETOOTH,
			&novasom_rfkill_ops, rfkill);
	if (!rfkill->rfkill_dev) {
		ret = -ENOMEM;
		goto bt_fail_free_gpio;
	}

	ret = rfkill_register(rfkill->rfkill_dev);
	if (ret < 0)
		goto bt_fail_unregister;

	dev_info(dev, "bluetooth-rfkill registered.\n");

	return 0;

bt_fail_unregister:
	rfkill_destroy(rfkill->rfkill_dev);
bt_fail_free_gpio:
	if (gpio_is_valid(bt_on))        gpio_free(bt_on);
	if (gpio_is_valid(bt_wake))      gpio_free(bt_wake);
	if (gpio_is_valid(bt_host_wake)) gpio_free(bt_host_wake);

	return ret;
}

static int novasom_rfkill_probe(struct platform_device *pdev)
{
struct novasom_rfkill_data *rfkill;
struct pinctrl *pinctrl;
int ret;

	dev_info(&pdev->dev, "NOVAsom rfkill initialization\n");

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no device tree node\n");
		return -ENODEV;
	}

	rfkill = kzalloc(sizeof(*rfkill) * 2, GFP_KERNEL);
	if (!rfkill)
		return -ENOMEM;

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		int ret = PTR_ERR(pinctrl);
		dev_err(&pdev->dev, "failed to get default pinctrl: %d\n", ret);
		return ret;
	}

	/* setup WiFi */
	ret = novasom_rfkill_wifi_probe(&pdev->dev, pdev->dev.of_node, &rfkill[0]);
	if (ret < 0)
		goto fail_free_rfkill;

	/* setup bluetooth */
	ret = novasom_rfkill_bt_probe(&pdev->dev, pdev->dev.of_node, &rfkill[1]);
	if (ret < 0)
		goto fail_unregister_wifi;

	platform_set_drvdata(pdev, rfkill);

	return 0;

fail_unregister_wifi:
	if (rfkill[1].rfkill_dev) {
		rfkill_unregister(rfkill[1].rfkill_dev);
		rfkill_destroy(rfkill[1].rfkill_dev);
	}

	/* TODO free gpio */

fail_free_rfkill:
	kfree(rfkill);

	return ret;
}

static int novasom_rfkill_remove(struct platform_device *pdev)
{
struct novasom_rfkill_data *rfkill = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Module unloading\n");

	if (!rfkill)
		return 0;

	/* WiFi */
	if (gpio_is_valid(rfkill[0].shutdown_gpio))
		gpio_free(rfkill[0].shutdown_gpio);

	rfkill_unregister(rfkill[0].rfkill_dev);
	rfkill_destroy(rfkill[0].rfkill_dev);

	/* Bt */
	if (gpio_is_valid(rfkill[1].shutdown_gpio))
		gpio_free(rfkill[1].shutdown_gpio);

	rfkill_unregister(rfkill[1].rfkill_dev);
	rfkill_destroy(rfkill[1].rfkill_dev);

	kfree(rfkill);

	return 0;
}

static struct of_device_id novasom_rfkill_match[] = {
	{ .compatible = "novasom,imx6q-novasomboard-rfkill", },
	{ .compatible = "novasom,imx6dl-novasomboard-rfkill", },
	{ .compatible = "novasom,imx6qdl-novasomboard-rfkill", },
	{}
};

static struct platform_driver novasom_rfkill_driver = {
	.driver = {
		.name = "novasomboard-rfkill",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(novasom_rfkill_match),
	},
	.probe = novasom_rfkill_probe,
	.remove = novasom_rfkill_remove
};

module_platform_driver(novasom_rfkill_driver);

MODULE_AUTHOR("Fil <filippo.visocchi@novasis.it>");
MODULE_DESCRIPTION("Wandboard rfkill driver, basically stolen from Vladimir Ermakov <vooon341@gmail.com>");
MODULE_LICENSE("GPL v2");
