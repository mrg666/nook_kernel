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
#include <linux/bootmem.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <plat/common.h>
#include <plat/board.h>

#include <mach/board-encore.h>

#include "mux.h"
#include "omap3-opp.h"

#if defined(CONFIG_MACH_OMAP3621_EVT1A) && defined (CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM_OR_SAMSUNG_K4X4G303PB)
#include "sdram-samsung-k4x4g303pb-or-hynix-h8mbx00u0mer-0em.h"
#elif defined(CONFIG_MACH_OMAP3621_EVT1A) && defined (CONFIG_MACH_SDRAM_SAMSUNG_K4X4G303PB)
#include "sdram-samsung-k4x4g303pb.h"
#elif defined(CONFIG_MACH_OMAP3621_EVT1A) && defined(CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM)
#include "sdram-hynix-h8mbx00u0mer-0em.h"
#endif

void __init evt_peripherals_init(void);

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static inline void omap2_ramconsole_reserve_sdram(void)
{
    reserve_bootmem(ENCORE_RAM_CONSOLE_START, ENCORE_RAM_CONSOLE_SIZE, 0);
}
#else
static inline void omap2_ramconsole_reserve_sdram(void) {}
#endif /* CONFIG_ANDROID_RAM_CONSOLE */

static void __init omap_evt_map_io(void)
{
    omap2_ramconsole_reserve_sdram();
	omap2_set_globals_343x();
	omap2_map_common_io();
}

static struct omap_board_config_kernel evt_config[] __initdata = {
};

static void __init omap_evt_init_irq(void)
{
	struct omap_opp *l3_rate_table;

	omap_board_config = evt_config;
	omap_board_config_size = ARRAY_SIZE(evt_config);
	omap_init_irq();

	l3_rate_table = (cpu_is_omap3622() && has_1GHz_support()) ? omap3630_l3_rate_table
				: omap3621_l3_rate_table;

#if defined(CONFIG_MACH_OMAP3621_EVT1A) && defined (CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM_OR_SAMSUNG_K4X4G303PB)
	omap2_init_common_hw(	h8mbx00u0mer0em_K4X4G303PB_sdrc_params,
				h8mbx00u0mer0em_K4X4G303PB_sdrc_params,
				omap3630_mpu_rate_table,
				omap3630_dsp_rate_table,
				l3_rate_table);
#elif defined(CONFIG_MACH_OMAP3621_EVT1A) && defined(CONFIG_MACH_SDRAM_HYNIX_H8MBX00U0MER0EM)
	omap2_init_common_hw(	h8mbx00u0mer0em_sdrc_params,
				h8mbx00u0mer0em_sdrc_params,
				omap3630_mpu_rate_table,
				omap3630_dsp_rate_table,
				l3_rate_table);
#elif defined(CONFIG_MACH_OMAP3621_EVT1A) && defined (CONFIG_MACH_SDRAM_SAMSUNG_K4X4G303PB)
	omap2_init_common_hw(	samsung_k4x4g303pb_sdrc_params,
				samsung_k4x4g303pb_sdrc_params,
				omap3630_mpu_rate_table,
				omap3630_dsp_rate_table,
				l3_rate_table);
#else
  #error "Please select SDRAM chip."
#endif
}

#ifdef CONFIG_OMAP_MUX
  #error "EVT1A port relies on the bootloader for MUX configuration."
#endif

static void __init omap_evt_init(void)
{
	omap3_mux_init(NULL, OMAP_PACKAGE_CBP);
	evt_peripherals_init();

	pr_info("CPU variant: %s Board: %s\n",
	    cpu_is_omap3622() ? "OMAP3622" : "OMAP3621",
	    has_1GHz_support() ? "1GHz" : "800MHz only");
}


MACHINE_START(OMAP3621_EVT1A, "OMAP3621 EVT1A board")
	.phys_io        = L4_34XX_PHYS,
	.io_pg_offst    = ((L4_34XX_VIRT) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_evt_map_io,
	.init_irq	= omap_evt_init_irq,
	.init_machine	= omap_evt_init,
	.timer		= &omap_timer,
MACHINE_END
