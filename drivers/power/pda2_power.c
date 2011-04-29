/*
 * Similar driver to pda_power with one small technical difference. Charger
 * and usb cable plug/unplug events are communicated via OTG notifications
 * instead of IRQ or polling.
 *
 * Based on pda_power.c by Anton Vorontsov <cbou@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/pda2_power.h>
#include <linux/regulator/consumer.h>
#include <linux/jiffies.h>
#include <linux/usb/otg.h>

struct pda2_power_data {
	struct device *dev;
	struct otg_transceiver *otg;
	struct pda2_power_pdata *pdata;
	struct regulator *vbus_draw;
	struct notifier_block nb;
	int ac_status;
	int usb_status;

	struct power_supply pda2_psy_ac;
	struct power_supply pda2_psy_usb;
};

static int pda2_power_ac_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct pda2_power_data *par;

	par = container_of(psy, struct pda2_power_data, pda2_psy_ac);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = par->ac_status;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int pda2_power_usb_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct pda2_power_data *par;

	par = container_of(psy, struct pda2_power_data, pda2_psy_usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = par->usb_status;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property pda2_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pda2_power_supplied_to[] = {
	"main-battery",
	"backup-battery",
};

static void update_charger(struct pda2_power_data *par)
{
	struct pda2_power_pdata *pdata = par->pdata;
	int ac_max_uA = pdata->ac_max_uA;
	int usb_max_uA = pdata->usb_max_uA;

	if (!par->vbus_draw)
		return;

	if (par->ac_status > 0) {
		regulator_set_current_limit(par->vbus_draw, ac_max_uA, ac_max_uA);
		if (!regulator_is_enabled(par->vbus_draw)) {
			dev_dbg(par->dev, "charger on (AC)\n");
			regulator_enable(par->vbus_draw);
		}
	} else if (par->usb_status > 0) {
		regulator_set_current_limit(par->vbus_draw, usb_max_uA, usb_max_uA);
		if (!regulator_is_enabled(par->vbus_draw)) {
			dev_dbg(par->dev, "charger on (USB)\n");
			regulator_enable(par->vbus_draw);
		}
	} else {
		if (regulator_is_enabled(par->vbus_draw)) {
			dev_dbg(par->dev, "charger off\n");
			regulator_disable(par->vbus_draw);
		}
	}
}

static int pda2_usb_notifier_call(struct notifier_block *nb,
					unsigned long event, void *unused)
{
	struct pda2_power_data *par = container_of(nb, struct pda2_power_data, nb);

	switch (event) {
	case USB_EVENT_CHARGER:
		dev_info(par->dev, "AC charger on\n");
		par->usb_status = 0;
		par->ac_status = 1;
		break;
	case USB_EVENT_VBUS:
		dev_info(par->dev, "USB host plugged\n");
		par->usb_status = 1;
		par->ac_status = 0;
		break;
	case USB_EVENT_NONE:
		dev_info(par->dev, "USB cable unplugged\n");
		par->usb_status = 0;
		par->ac_status = 0;
		break;
	/* TODO - handle USB_EVENT_ENUMERATED */
	default:
		return NOTIFY_DONE;
	}

	mb();

	dev_dbg(par->dev, "USB event: %lu\n", event);

	update_charger(par);
	power_supply_changed(&par->pda2_psy_ac);
	power_supply_changed(&par->pda2_psy_usb);

	return NOTIFY_OK;
}


/*--------------------------------------------------------------------------*/

static int pda2_power_probe(struct platform_device *pdev)
{
	struct pda2_power_pdata *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct pda2_power_data *par;
	int ret = 0;

	if (pdev->id != -1) {
		dev_err(dev, "it's meaningless to register several "
			"pda2_powers; use id = -1\n");
		ret = -EINVAL;
		goto out;
	}

	if (!pdata->ac_max_uA)
		pdata->ac_max_uA = 500000;

	if (!pdata->usb_max_uA)
		pdata->ac_max_uA = 100000;

	par = kzalloc(sizeof(*par), GFP_KERNEL);
	if (!par) {
		ret = -ENOMEM;
		goto out;
	}

	par->dev = &pdev->dev;
	par->pdata = pdata;
	par->otg = otg_get_transceiver();
	platform_set_drvdata(pdev, par);

	par->pda2_psy_ac.name = "ac";
	par->pda2_psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	par->pda2_psy_ac.supplied_to = pda2_power_supplied_to;
	par->pda2_psy_ac.num_supplicants = ARRAY_SIZE(pda2_power_supplied_to);
	par->pda2_psy_ac.properties = pda2_power_props;
	par->pda2_psy_ac.num_properties = ARRAY_SIZE(pda2_power_props);
	par->pda2_psy_ac.get_property = pda2_power_ac_get_property;
	par->pda2_psy_usb.name = "usb";
	par->pda2_psy_usb.type = POWER_SUPPLY_TYPE_USB;
	par->pda2_psy_usb.supplied_to = pda2_power_supplied_to;
	par->pda2_psy_usb.num_supplicants = ARRAY_SIZE(pda2_power_supplied_to);
	par->pda2_psy_usb.properties = pda2_power_props;
	par->pda2_psy_usb.num_properties = ARRAY_SIZE(pda2_power_props);
	par->pda2_psy_usb.get_property = pda2_power_usb_get_property;

	if (pdata->supplied_to) {
		par->pda2_psy_ac.supplied_to = pdata->supplied_to;
		par->pda2_psy_ac.num_supplicants = pdata->num_supplicants;
		par->pda2_psy_usb.supplied_to = pdata->supplied_to;
		par->pda2_psy_usb.num_supplicants = pdata->num_supplicants;
	}

	par->vbus_draw = regulator_get(dev, "vbus_draw");
	if (IS_ERR(par->vbus_draw)) {
		dev_dbg(dev, "couldn't get vbus_draw regulator\n");
		par->vbus_draw = NULL;
	}

	ret = power_supply_register(&pdev->dev, &par->pda2_psy_ac);
	if (ret) {
		dev_err(dev, "failed to register %s power supply\n",
			par->pda2_psy_ac.name);
		goto ac_supply_failed;
	}

	ret = power_supply_register(&pdev->dev, &par->pda2_psy_usb);
	if (ret) {
		dev_err(dev, "failed to register %s power supply\n",
			par->pda2_psy_usb.name);
		goto usb_supply_failed;
	}

	par->nb.notifier_call = pda2_usb_notifier_call;
	ret = otg_register_notifier(par->otg, &par->nb);
	if (ret)
		goto notifier_failed;

	/* update charger status */
	otg_get_last_event(par->otg);

	dev_info(par->dev, "pda2_power device registered\n");

	return 0;

notifier_failed:
	power_supply_unregister(&par->pda2_psy_usb);
usb_supply_failed:
	power_supply_unregister(&par->pda2_psy_ac);
ac_supply_failed:
	otg_put_transceiver(par->otg);
	if (par->vbus_draw) {
		regulator_put(par->vbus_draw);
		par->vbus_draw = NULL;
	}
	kfree(par);
out:
	return ret;
}

static int pda2_power_remove(struct platform_device *pdev)
{
	struct pda2_power_data *par = platform_get_drvdata(pdev);

	otg_unregister_notifier(par->otg, &par->nb);
	power_supply_unregister(&par->pda2_psy_usb);
	power_supply_unregister(&par->pda2_psy_ac);
	otg_put_transceiver(par->otg);
	if (par->vbus_draw) {
		regulator_put(par->vbus_draw);
		par->vbus_draw = NULL;
	}
	kfree(par);

	return 0;
}

MODULE_ALIAS("platform:pda2-power");

static struct platform_driver pda2_power_pdrv = {
	.driver = {
		.name = "pda2-power",
	},
	.probe = pda2_power_probe,
	.remove = pda2_power_remove,
};

static int __init pda2_power_init(void)
{
	return platform_driver_register(&pda2_power_pdrv);
}

static void __exit pda2_power_exit(void)
{
	platform_driver_unregister(&pda2_power_pdrv);
}

module_init(pda2_power_init);
module_exit(pda2_power_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dimitar Dimitrov <dddimitrov@mm-sol.com>");
