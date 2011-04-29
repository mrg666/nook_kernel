
/*
 * Copyright (C) 2009 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-zoom2.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/bq24073.h>
#include <linux/synaptics_i2c_rmi.h>
#include <linux/interrupt.h>
#include <linux/switch.h>
#include <linux/pda2_power.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/common.h>
#include <plat/usb.h>
#include <plat/control.h>
#include <plat/mux.h>

#include "mmc-twl4030.h"
#include "mux.h"
#include "twl4030.h"

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI) || \
    defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_MODULE)
#define OMAP_SYNAPTICS_GPIO	39
#endif /* CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI */

#include <media/v4l2-int-device.h>

#define AIC3111_NAME "tlv320aic3111"
#define AIC3111_I2CSLAVEADDRESS 0x18


extern struct regulator_init_data edp1_vdac;
extern void edp1_lcd_tv_panel_init(void);

struct init_gpios {
	u8 gpio_num;
	u8 gpio_val;
	char *gpio_name;
};

/* EDP1 has Qwerty keyboard */
static int board_keymap[] = {
	KEY(0, 0, KEY_E),
	KEY(0, 1, KEY_R),
	KEY(0, 2, KEY_T),
	KEY(0, 3, KEY_ESC), /* marked as HOME on the external case */
	KEY(0, 6, KEY_I),
	KEY(0, 7, KEY_F1),

	KEY(1, 0, KEY_D),
	KEY(1, 1, KEY_F),
	KEY(1, 2, KEY_G),
	KEY(1, 3, KEY_MENU),
	KEY(1, 6, KEY_K),
	KEY(1, 7, KEY_SELECT),

	KEY(2, 0, KEY_X),
	KEY(2, 1, KEY_C),
	KEY(2, 2, KEY_V),
	KEY(2, 3, KEY_DELETE),
	KEY(2, 6, KEY_M),
	KEY(2, 7, KEY_CAPSLOCK),

	KEY(3, 0, KEY_Z),
	KEY(3, 1, KEY_KPPLUS),
	KEY(3, 2, KEY_B),
	KEY(3, 3, KEY_LEFTALT),
	KEY(3, 6, KEY_O),
	KEY(3, 7, KEY_SPACE),

	KEY(4, 0, KEY_W),
	KEY(4, 1, KEY_Y),
	KEY(4, 2, KEY_U),
	KEY(4, 3, KEY_NEXT),
	KEY(4, 4, KEY_VOLUMEUP),
	KEY(4, 6, KEY_L),
	KEY(4, 7, KEY_LEFT),

	KEY(5, 0, KEY_S),
	KEY(5, 1, KEY_H),
	KEY(5, 2, KEY_J),
	KEY(5, 3, KEY_FN),
	KEY(5, 5, KEY_VOLUMEDOWN),
	KEY(5, 6, KEY_DOT),
	KEY(5, 7, KEY_RIGHT),

	KEY(6, 0, KEY_Q),
	KEY(6, 1, KEY_A),
	KEY(6, 2, KEY_N),
	KEY(6, 3, KEY_BACK),
	KEY(6, 6, KEY_P),
	KEY(6, 7, KEY_UP),

	KEY(7, 6, KEY_ENTER),
	KEY(7, 7, KEY_DOWN),
};

static struct matrix_keymap_data board_map_data = {
	.keymap			= board_keymap,
	.keymap_size		= ARRAY_SIZE(board_keymap),
};

static struct twl4030_keypad_data edp1_kp_twl4030_data = {
	.keymap_data	= &board_map_data,
	.rows		= 8,
	.cols		= 8,
	.rep		= 1,
};

static struct __initdata twl4030_power_data edp1_t2scripts_data;

static struct regulator_consumer_supply edp1_vmmc1_supply = {
	.supply		= "vmmc",
};

static struct regulator_consumer_supply edp1_vsim_supply = {
	.supply		= "vmmc_aux",
};

static struct regulator_consumer_supply edp1_vmmc2_supply = {
	.supply		= "vmmc",
};

/* VMMC1 for OMAP VDD_MMC1 (i/o) and MMC1 card */
static struct regulator_init_data edp1_vmmc1 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &edp1_vmmc1_supply,
};

/* VMMC2 for MMC2 card */
static struct regulator_init_data edp1_vmmc2 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 1850000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &edp1_vmmc2_supply,
};

/* VSIM for OMAP VDD_MMC1A (i/o for DAT4..DAT7) */
static struct regulator_init_data edp1_vsim = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &edp1_vsim_supply,
};

static struct fixed_voltage_config edp1_vmmc2_regulator_data = {
	.supply_name		= "vmmc",
	.microvolts		= 1850000,
	.gpio			= 101,
	.enable_high		= 0,
	.enabled_at_boot	= 0,
	.init_data		= &edp1_vmmc2,
};

static struct platform_device edp1_vmmc2_regulator_device = {
	.name   = "reg-fixed-voltage",
	.id     = -1,
	.dev    = {
		.platform_data = &edp1_vmmc2_regulator_data,
	},
};

static struct twl4030_hsmmc_info mmc[] __initdata = {
	{
		.name		= "external",
		.mmc		= 1,
		.wires		= 4,
		.gpio_wp	= -EINVAL,
		.power_saving	= true,
	},
#ifdef CONFIG_OMAP_HS_MMC2
	{
		.name		= "internal",
		.mmc		= 2,
		.wires		= 8,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable	= true,
		.power_saving	= true,
	},
#endif
	{
		.mmc		= 3,
		.wires		= 4,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
	},
	{}      /* Terminator */
};

static int edp1_twl_gpio_setup(struct device *dev,
		unsigned gpio, unsigned ngpio)
{
	/* gpio + 0 is "mmc0_cd" (input/IRQ) */
	mmc[0].gpio_cd = gpio + 0;

#ifdef CONFIG_MMC_EMBEDDED_SDIO
	/* The controller that is connected to the 128x device
	 * should have the card detect gpio disabled. This is
	 * achieved by initializing it with a negative value
	 */
	mmc[CONFIG_TIWLAN_MMC_CONTROLLER - 1].gpio_cd = -EINVAL;
#endif

	twl4030_mmc_init(mmc);

	/* link regulators to MMC adapters ... we "know" the
	 * regulators will be set up only *after* we return.
	*/
	edp1_vmmc1_supply.dev = mmc[0].dev;
	edp1_vsim_supply.dev = mmc[0].dev;
	edp1_vmmc2_supply.dev = mmc[1].dev;

	return 0;
}

static struct twl4030_usb_data edp1_usb_data = {
	.usb_mode	= T2_USB_MODE_ULPI,
};

static struct twl4030_gpio_platform_data edp1_gpio_data = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.setup		= edp1_twl_gpio_setup,
};

static struct twl4030_madc_platform_data edp1_madc_data = {
	.irq_line	= 1,
};

static struct twl4030_platform_data edp1_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.madc		= &edp1_madc_data,
	.usb		= &edp1_usb_data,
	.gpio		= &edp1_gpio_data,
	.keypad		= &edp1_kp_twl4030_data,
	.power		= &edp1_t2scripts_data,
	.vmmc1		= &edp1_vmmc1,
	.vmmc2		= &edp1_vmmc2,
	.vsim		= &edp1_vsim,
	.vdac		= &edp1_vdac,
};

static struct i2c_board_info __initdata edp1_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("tps65921", 0x48),
		.flags		= I2C_CLIENT_WAKE,
		.irq		= INT_34XX_SYS_NIRQ,
		.platform_data	= &edp1_twldata,
	},
};

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI) || \
    defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_MODULE)
static void synaptics_dev_init(void)
{
	if (gpio_request(OMAP_SYNAPTICS_GPIO, "touch") < 0) {
		printk(KERN_ERR "can't get synaptics pen down GPIO\n");
		return;
	}
	gpio_direction_input(OMAP_SYNAPTICS_GPIO);
	omap_set_gpio_debounce(OMAP_SYNAPTICS_GPIO, 1);
	omap_set_gpio_debounce_time(OMAP_SYNAPTICS_GPIO, 0xa);
}

static int synaptics_power(int power_state)
{
	/* TODO: synaptics is powered by vbatt */
	return 0;
}

static struct synaptics_i2c_rmi_platform_data synaptics_platform_data[] = {
	{
		.version	= 0x0,
		.power		= &synaptics_power,
		.flags		= SYNAPTICS_SWAP_XY,
		.irqflags	= IRQF_TRIGGER_LOW,
	}
};
#endif /* CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI */

static struct i2c_board_info __initdata edp1_i2c_boardinfo2[] = {
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI) || \
    defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_MODULE)
	{
		I2C_BOARD_INFO(SYNAPTICS_I2C_RMI_NAME,  0x20),
		.platform_data = &synaptics_platform_data,
		.irq = OMAP_GPIO_IRQ(OMAP_SYNAPTICS_GPIO),
	},
#endif /* CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI */
#if defined(CONFIG_SND_SOC_TLV320AIC3111) || \
    defined(CONFIG_SND_SOC_TLV320AIC3111_MODULE)
	{
		I2C_BOARD_INFO(AIC3111_NAME,  AIC3111_I2CSLAVEADDRESS),
	},
#endif /* CONFIG_SND_SOC_TLV320AIC3111 */
#if defined(CONFIG_BATTERY_BQ27510) || defined(CONFIG_BATTERY_BQ27510_MODULE)
	{
		I2C_BOARD_INFO("bq27510",  0x55),
	},
#endif /* CONFIG_BATTERY_BQ27510 */
};

static int __init omap_i2c_init(void)
{
	omap_register_i2c_bus(1, 100, NULL, edp1_i2c_boardinfo,
			ARRAY_SIZE(edp1_i2c_boardinfo));
	omap_register_i2c_bus(2, 100, NULL, edp1_i2c_boardinfo2,
			ARRAY_SIZE(edp1_i2c_boardinfo2));
	return 0;
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_ULPI,
	.mode			= MUSB_OTG,
	.power			= 500,
};


static struct init_gpios init_gpio_config[] = {
	{ .gpio_num = 14,  .gpio_val = 0, .gpio_name = "FM-EN" },       // Set GPIO_14 (FM-EN) In output (Bit 14 to "0")
	{ .gpio_num = 21,  .gpio_val = 0, .gpio_name = "MODEM-EN"  },   // Set GPIO_21 (MODEM-EN) In output (Bit 21 to "0")
	{ .gpio_num = 23,  .gpio_val = 0, .gpio_name = "HSUSB2_nRS"  }, // Set GPIO_23 (HSUSB2_nRST) In output (Bit 23 to "0")
#ifdef CONFIG_OMAP2_DSS
	{ .gpio_num = 85,  .gpio_val = 0, .gpio_name = "EN_CPLD"  },    // Set GPIO_85 (EN_CPLD_POW) In Output (Bit 21 to "0")
	{ .gpio_num = 87,  .gpio_val = 0, .gpio_name = "EPD_WAKEUP"  }, // Set GPIO_87 (EPD_WAKEUP) In Ouput (Bit 23 to "0")
#endif
	};

static void edp1_init_pin_mux(void)
{
	int ret, i;

	for ( i = 0 ; i < ARRAY_SIZE(init_gpio_config) ; i++ )
		if ( init_gpio_config[i].gpio_val == 0 )        {
			ret = gpio_request(init_gpio_config[i].gpio_num, init_gpio_config[i].gpio_name);
			if (ret < 0) {
				printk(KERN_ERR "%s: can't reserve GPIO: %d\n", __func__,
				       init_gpio_config[i].gpio_num);
				continue;
			}
			gpio_direction_output(init_gpio_config[i].gpio_num, 0);
		}
}

/*---------------------------- CHARGER -------------------------------------*/

static char *edp1_power_supplicants[] = {
	"bq27510-0",
};


static struct pda2_power_pdata power_supply_info = {
	.ac_max_uA		= 1500000,
	.usb_max_uA		= 500000,
	.supplied_to		= edp1_power_supplicants,
	.num_supplicants	= ARRAY_SIZE(edp1_power_supplicants),
};

static struct platform_device pda2_power_pdev = {
	.name = "pda2-power",
	.id   = -1,
	.dev  = {
		.platform_data = &power_supply_info,
	},
};


static struct bq24073_mach_info bq24073_init_dev_data = {
	.gpio_nce = 110,
	.gpio_en1 = 102,
	.gpio_en2 = 104,
	.gpio_nce_state = 1,
	.gpio_en1_state = 0,
	.gpio_en2_state = 0,
};

static struct regulator_consumer_supply bq24073_vcharge_supply[] = {
	{
	       .dev	= &pda2_power_pdev.dev,
	       .supply	= "vbus_draw",
	},
};

static struct regulator_init_data bq24073_init  = {
	.constraints = {
		.min_uV                 = 0,
		.max_uV                 = 5000000,
		.min_uA                 = 0,
		.max_uA                 = 1500000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					  | REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_CURRENT
					  | REGULATOR_CHANGE_MODE
					  | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = ARRAY_SIZE(bq24073_vcharge_supply),
	.consumer_supplies      = bq24073_vcharge_supply,

	.driver_data = &bq24073_init_dev_data,
};

/* GPIOS need to be in order of BQ24073 */
static struct platform_device edp1_bq24073_regulator_device = {
	.name           = "bq24073",
	.id             = -1,
	.dev		= {
		.platform_data = &bq24073_init,
	},
};


static struct platform_device *devices[] __initdata = {
	&edp1_bq24073_regulator_device,
	&edp1_vmmc2_regulator_device,
	&pda2_power_pdev,
};


void __init edp1_peripherals_init(void)
{
	/* we want charger regulator to be registered before TWL4030-usb */
	platform_add_devices(devices, ARRAY_SIZE(devices));

	twl4030_get_scripts(&edp1_t2scripts_data);
	omap_i2c_init();

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI) || \
    defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI_MODULE)
	synaptics_dev_init();
#endif /* CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI */
	omap_serial_init();
	edp1_init_pin_mux();
	edp1_lcd_tv_panel_init();
	usb_musb_init(&musb_board_data);
}
