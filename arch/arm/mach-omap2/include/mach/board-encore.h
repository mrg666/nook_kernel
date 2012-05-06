/*
 * Copyright (C) 2011 Barnes & Noble, Inc
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_OMAP_ENCORE_H
#define __ASM_ARCH_OMAP_ENCORE_H

#define ENCORE_RAM_CONSOLE_START   0x8E000000
#define ENCORE_RAM_CONSOLE_SIZE    0x20000

#define BOARD_FEATURE_3G	0x08
#define BOARD_FEATURE_1GHz	0x10
#define BOARD_FEATURE_EINK	0x20

extern int has_3G_support(void);
extern int has_1GHz_support(void);

#endif /* __ASM_ARCH_OMAP_ENCORE_H */
