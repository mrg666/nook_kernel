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

#ifndef __PDA2_POWER_H__
#define __PDA2_POWER_H__

struct pda2_power_pdata {
	char **supplied_to;
	size_t num_supplicants;

	unsigned long ac_max_uA; /* current to draw when on AC */
	unsigned long usb_max_uA; /* current to draw when on USB */
};

#endif /* __PDA2_POWER_H__ */
