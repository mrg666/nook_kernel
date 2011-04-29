#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>

#include <plat/display.h>
#include <plat/mcspi.h>
#include <plat/omap-pm.h>
#include <linux/spi/spi.h>
#include "mux.h"


#define ENABLE_VAUX2_DEDICATED          0x09
#define ENABLE_VAUX2_DEV_GRP            0x20
#define ENABLE_VAUX3_DEDICATED          0x03
#define ENABLE_VAUX3_DEV_GRP            0x20

#define ENABLE_VPLL2_DEDICATED          0x05
#define ENABLE_VPLL2_DEV_GRP            0xE0
#define TWL4030_VPLL2_DEV_GRP           0x33
#define TWL4030_VPLL2_DEDICATED         0x36


#define EDP_LCD_PANEL_BACKLIGHT_GPIO 	58
#define EDP_LCD_PANEL_RESET_GPIO	38
#define EDP_LCD_PWR_EN_GPIO		37


extern unsigned get_last_off_on_transaction_id(struct device *dev);
static int edp1_panel_enable_lcd(struct omap_dss_device *dssdev);
static void edp1_panel_disable_lcd(struct omap_dss_device *dssdev);
/*--------------------------------------------------------------------------*/
static int edp1_panel_resume(struct omap_dss_device *dssdev)
{
	gpio_direction_output(EDP_LCD_PWR_EN_GPIO, 1);
	mdelay(5);
	gpio_direction_output(EDP_LCD_PANEL_RESET_GPIO, 0);
	mdelay(5);
	gpio_direction_output(EDP_LCD_PANEL_RESET_GPIO, 1);
	mdelay(5);

	return 0;
}

static int edp1_panel_suspend(struct omap_dss_device *dssdev)
{
	gpio_direction_output(EDP_LCD_PWR_EN_GPIO, 0);
	return 0;
}

static struct omap_dss_device edp1_lcd_device = {
	.name = "lcd",
	.driver_name = "NEC_panel",
	.type = OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines = 16,
	.resume = edp1_panel_resume,
	.suspend = edp1_panel_suspend,
	.platform_enable = edp1_panel_enable_lcd,
	.platform_disable = edp1_panel_disable_lcd,
};

static struct omap_dss_device *edp1_dss_devices[] = {
	&edp1_lcd_device,
};

static struct omap_dss_board_info edp1_dss_data = {
	.get_last_off_on_transaction_id = get_last_off_on_transaction_id,
	.num_devices = ARRAY_SIZE(edp1_dss_devices),
	.devices = edp1_dss_devices,
	.default_device = &edp1_lcd_device,
};

static struct platform_device edp1_dss_device = {
	.name          = "omapdss",
	.id            = -1,
	.dev            = {
		.platform_data = &edp1_dss_data,
	},
};

/*--------------------------------------------------------------------------*/
static struct regulator_consumer_supply edp1_vdda_dac_supply = {
	.supply         = "vdda_dac",
	.dev            = &edp1_dss_device.dev,
};

struct regulator_init_data edp1_vdac = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &edp1_vdda_dac_supply,
};


/*--------------------------------------------------------------------------*/
static int edp1_panel_enable_lcd(struct omap_dss_device *dssdev)
{
	gpio_direction_output(EDP_LCD_PANEL_BACKLIGHT_GPIO, 1);

	return 0;
}

static void edp1_panel_disable_lcd(struct omap_dss_device *dssdev)
{
	gpio_direction_output(EDP_LCD_PANEL_BACKLIGHT_GPIO, 0);
}

struct platform_device *edp1_devices[] __initdata = {
	&edp1_dss_device,
};

/*--------------------------------------------------------------------------*/
static struct omap2_mcspi_device_config edp1_lcd_mcspi_config = {
	.turbo_mode             = 0,
	.single_channel         = 1,  /* 0: slave, 1: master */

};

struct spi_board_info edp1_spi_board_info[] __initdata = {
	[0] = {
		.modalias               = "zoom_disp_spi",
		.bus_num                = 2,
		.chip_select            = 0,
		.max_speed_hz           = 375000,
		.controller_data        = &edp1_lcd_mcspi_config,
	},
};

/*--------------------------------------------------------------------------*/
void __init edp1_lcd_tv_panel_init(void)
{
	gpio_request(EDP_LCD_PANEL_RESET_GPIO, "lcd reset");
	gpio_request(EDP_LCD_PANEL_BACKLIGHT_GPIO, "lcd backlight");
	gpio_request(EDP_LCD_PWR_EN_GPIO, "lcd_panel_pwren");

	/* According NEC datasheet min delay of 1msec should be present between
	 * EN=1 and RST=0 and after that when RST=1
	 */
	gpio_direction_output(EDP_LCD_PWR_EN_GPIO, 1);
	mdelay(5);
	gpio_direction_output(EDP_LCD_PANEL_RESET_GPIO, 0);
	gpio_direction_output(EDP_LCD_PANEL_BACKLIGHT_GPIO, 0);
	mdelay(5);
	gpio_direction_output(EDP_LCD_PANEL_RESET_GPIO, 1);
	mdelay(5);

	spi_register_board_info(edp1_spi_board_info,
				ARRAY_SIZE(edp1_spi_board_info));
	platform_device_register(&edp1_dss_device);
}


