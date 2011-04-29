/*
 * Copyright (C) 2009 Texas Instruments Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/board-edp1.h>

#include <plat/common.h>
#include <plat/board.h>

#include "mux.h"
#include "sdram-hynix-h8mbx00u0mer-0em.h"
#include "omap3-opp.h"

/* Added for FlexST */
#include "board-connectivity.h"

#include "board-zoom2-wifi.h"

static void __init omap_edp1_map_io(void)
{
	omap2_set_globals_343x();
	omap2_map_common_io();
}

static struct omap_board_config_kernel edp1_config[] __initdata = {
};

static void __init omap_edp1_init_irq(void)
{
	omap_board_config = edp1_config;
	omap_board_config_size = ARRAY_SIZE(edp1_config);
	omap_init_irq();
	omap2_init_common_hw(h8mbx00u0mer0em_sdrc_params,
			     h8mbx00u0mer0em_sdrc_params,
			     omap3630_mpu_rate_table,
			     omap3630_dsp_rate_table,
			     omap3621_l3_rate_table);
}

static void __init omap_edp1_init(void)
{
	edp1_peripherals_init();
	conn_board_init(); /* wl127x VIO leakage fix*/
	conn_add_plat_device(); /* Added for FlexST */
	conn_config_gpios(); /* configure BT,FM and GPS gpios */
}

MACHINE_START(OMAP3621_EDP1, "OMAP 3621 EDP1 board")
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xfa000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_edp1_map_io,
	.init_irq	= omap_edp1_init_irq,
	.init_machine	= omap_edp1_init,
	.timer		= &omap_timer,
MACHINE_END
