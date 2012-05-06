/*
 * Encore Modem Manager library
 *
 * Copyright (C) 2010 Barnes & Noble, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>


/*****************************************************************************
 * Configuration
 *****************************************************************************/

#define EMM_GPIO_MODEM_RST             50   /* active low  */
#define EMM_GPIO_MODEM_ON              34   /* active high */
#define EMM_GPIO_MODEM_NDISABLE        21   /* active high */

#define EMM_DEFAULT_DIR_CHANGE_DELAY    1   /* Delay in ms */

#define EMM_MIN_PULSE_MODEM_ON       5000   /* Pulse Width in ms */
#define EMM_MIN_PULSE_MODEM_OFF       500   /* Pulse Width in ms */
#define EMM_MIN_PULSE_MODEM_RST        10   /* Pulse Width in ms */

#define EMM_DEFAULT_PULSE_MODEM_ON   (EMM_MIN_PULSE_MODEM_ON + (EMM_MIN_PULSE_MODEM_ON / 10))
#define EMM_DEFAULT_PULSE_MODEM_OFF  (EMM_MIN_PULSE_MODEM_OFF + (EMM_MIN_PULSE_MODEM_OFF / 10))

#define EMM_MODEM_INITIALIZATION_DELAY  500   /* .5 seconds !!! */
#define EMM_MODEM_RESET_INIT_DELAY      500   /* .5 seconds !!! */

#define EMM_T_PDSU                      150   /* power down setup time from Sierra Wireless */
#define EMM_T_PD                        550   /* power down pulse width from Sierra Wireless */

/*****************************************************************************
 * Logging/Debugging Configuration
 *****************************************************************************/

#define ENCORE_MODEM_MGR_DEBUG          1   /* Regular debug    */
#define ENCORE_MODEM_MGR_DEBUG_VERBOSE  1   /* Extra debug      */

#if ENCORE_MODEM_MGR_DEBUG
#define DEBUGPRINT(x...) printk(x)
#else  /* ENCORE_MODEM_MGR_DEBUG */
#define DEBUGPRINT(x...)
#endif /* ENCORE_MODEM_MGR_DEBUG */


/*****************************************************************************
 * Global Variables
 *****************************************************************************/

/* Pulse MODEM_ON to 1 for 5000+ ms to turn on the modem, and 500+ ms to turn off the modem. */
static unsigned long int g_pulse_width_ON  = EMM_DEFAULT_PULSE_MODEM_ON;

/* Pulse MODEM_RST to 0 for 10 - 30 ms to reset the modem. */
static unsigned long int g_pulse_width_RST = EMM_MIN_PULSE_MODEM_RST;


/*****************************************************************************
 * Logging/Debugging Helpers
 *****************************************************************************/

static char * getGpioLabel(unsigned int GpioID)
{
    switch(GpioID)
    {
        case EMM_GPIO_MODEM_ON:
            return "Modem ON";

        case EMM_GPIO_MODEM_NDISABLE:
            return "Modem nDISABLE";

        case EMM_GPIO_MODEM_RST:
            return "Modem RST";

        default:
            return "Unknown";
    }
}


/*****************************************************************************
 * Helpers - GPIO
 *****************************************************************************/

/*
 * Returns 0 if the given GPIO line is Low.
 * Returns 1 if the given GPIO line is High.
 */
static int GetGpioValue(unsigned int GpioID)
{
    int value = gpio_get_value(GpioID);

#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: GPIO %02d (%s) state: %s\n", GpioID, getGpioLabel(GpioID), ((0 == value) ? "LOW" : "HIGH") );
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

    return ((0 == value) ? 0 : 1);
}

/*
 * Sets a GPIO Line to the given value.
 */
static void SetGpioValue(unsigned int GpioID, int newValue)
{
    int actualNewValue = ((0 == newValue) ? 0 : 1);

#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Setting GPIO %02d (%s) state to %s...\n", GpioID, getGpioLabel(GpioID), ((0 == actualNewValue) ? "LOW" : "HIGH") );
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

    switch(GpioID)
    {
        /* Active-High Signals */
        case EMM_GPIO_MODEM_ON:
            gpio_set_value(GpioID, actualNewValue);
            break;

        /* Active-Low Signals */
        case EMM_GPIO_MODEM_NDISABLE:
        case EMM_GPIO_MODEM_RST:
            gpio_direction_output(GpioID, actualNewValue);
            gpio_set_value(GpioID, actualNewValue);

            if (1 == newValue)
            {
                msleep(EMM_DEFAULT_DIR_CHANGE_DELAY);
                gpio_direction_input(GpioID);
            }
            break;

        default:
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
            DEBUGPRINT(KERN_INFO "encore_modem_mgr: Ignoring request to set GPIO %02d (%s) state to %s...\n", GpioID, getGpioLabel(GpioID), ((0 == actualNewValue) ? "LOW" : "HIGH") );
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */
            break;
    }
}

/*
 * Toggles the value of the given GPIO for the specified amount of time.
 */
static void PulseGpioValue(unsigned int GpioID, int pulseValue, unsigned long int numMillisecs)
{
    int gpioValue = ((0 == pulseValue) ? 0 : 1);

    if (0 == numMillisecs)
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: Ignoring pulse request for GPIO %02d (%s) since the given duration is zero milliseconds.\n", GpioID, getGpioLabel(GpioID) );
    }
    else
    {
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: Pulsing GPIO %02d (%s) %s for %ld milliseconds...\n", GpioID, getGpioLabel(GpioID), ((0 == gpioValue) ? "LOW" : "HIGH"), numMillisecs );
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

        SetGpioValue(GpioID, gpioValue);
        msleep(numMillisecs);
        gpioValue = ((0 == gpioValue) ? 1 : 0);
        SetGpioValue(GpioID, gpioValue);
    }
}


/*****************************************************************************
 * Helpers - Modem Functions
 *****************************************************************************/

static void ResetModem(void)
{
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Resetting the modem...\n");
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

    SetGpioValue(EMM_GPIO_MODEM_RST, 1);
    SetGpioValue(EMM_GPIO_MODEM_ON, 1);

    msleep(50);

    PulseGpioValue(EMM_GPIO_MODEM_RST, 0, EMM_MIN_PULSE_MODEM_RST);

    msleep(EMM_MODEM_RESET_INIT_DELAY);
}

static void TurnModemOn(void)
{
    int modemState;

    /* If the modem is on and we are driving MODEM_RST low for some reason
     * then we can be fooled into thinking that the modem is actually off,
     * since we use the MODEM_RST signal to determine the modem state.
     * We avoid that situation here (at the risk of modifying the state of
     * the MODEM_RST signal in the middle of a reset, which is unlikely since
     * we shouldn't be trying to turn on the modem in the middle of a modem
     * reset anyway) by making the MODEM_RST GPIO an input so we can accurately
     * see whether the modem is actually on.
     */

#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Setting GPIO %02d (%s) state to INPUT...\n", EMM_GPIO_MODEM_RST, getGpioLabel(EMM_GPIO_MODEM_RST));
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

    gpio_direction_input(EMM_GPIO_MODEM_RST);
    modemState = GetGpioValue(EMM_GPIO_MODEM_RST);

    /* Only turn on the modem if the modem is currently off. */
    /* Note that the modem will pull MODEM_RST high if it is on. */

    if (0 == modemState)
    {
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: Turning the modem ON...\n");
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

        // assert MODEM_ON to turn on the modem
        SetGpioValue(EMM_GPIO_MODEM_ON, 1);
    }
    else
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: modem is already on - do nothing\n");
    }

}

static void TurnModemOff(void)
{
    int modemState;

    /* If the modem is on and we are driving MODEM_RST low for some reason
     * then we can be fooled into thinking that the modem is actually off,
     * since we use the MODEM_RST signal to determine the modem state.
     * We avoid that situation here (at the risk of modifying the state of
     * the MODEM_RST signal in the middle of a reset, which is unlikely since
     * we shouldn't be trying to turn off the modem in the middle of a modem
     * reset anyway) by making the MODEM_RST GPIO an input so we can accurately
     * see whether the modem is actually on.
     * Note that it doesn't really matter anyway, since if the modem is off,
     * the MODEM_RST GPIO should already be an input and if the modem is on,
     * then we would be switching it to an input before turning off the modem.
     */

#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Setting GPIO %02d (%s) state to INPUT...\n", EMM_GPIO_MODEM_RST, getGpioLabel(EMM_GPIO_MODEM_RST));
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

    gpio_direction_input(EMM_GPIO_MODEM_RST);
    modemState = GetGpioValue(EMM_GPIO_MODEM_RST);

    /* Only turn off the modem if the modem is currently on. */
    /* Note that the modem will pull MODEM_RST high if it is on. */

    if (1 == modemState)
    {
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: Turning the modem OFF...\n");
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

        // de-assert MODEM_ON for t_pdsu as specified by Sierra Wireless
        SetGpioValue(EMM_GPIO_MODEM_ON, 0);
        msleep(EMM_T_PDSU);

        // assert MODEM_ON for t_pd ms as specified by Sierra Wireless
        PulseGpioValue(EMM_GPIO_MODEM_ON, 1, EMM_T_PD);
        SetGpioValue(EMM_GPIO_MODEM_ON, 0);
    }
    else
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: modem was already off - do nothing.\n");
    }

}

static void EnableFlightMode(void)
{
    int currentFlightModeState = GetGpioValue(EMM_GPIO_MODEM_NDISABLE);

    /* Only turn on Flight Mode if Flight Mode is currently off. */
    /* Note that the modem will automatically pull MODEM_NDISABLE high unless we drive it to 0. */

    if (1 == currentFlightModeState)
    {
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: Enabling Flight Mode...\n");
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

        SetGpioValue(EMM_GPIO_MODEM_NDISABLE, 0);
    }
}

static void DisableFlightMode(void)
{
    int currentFlightModeState = GetGpioValue(EMM_GPIO_MODEM_NDISABLE);

    /* Only turn off Flight Mode if Flight Mode is currently on. */
    /* Note that the modem will automatically pull MODEM_NDISABLE high unless we drive it to 0. */

    if (0 == currentFlightModeState)
    {
#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: Disabling Flight Mode...\n");
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

        SetGpioValue(EMM_GPIO_MODEM_NDISABLE, 1);
    }
}


/*****************************************************************************
 * SysFS Handlers
 *****************************************************************************/

/****** modem_state ******/

static ssize_t modem_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int modemState;

    /* If the modem is on and we are driving MODEM_RST low for some reason
     * then we can be fooled into thinking that the modem is actually off,
     * since we use the MODEM_RST signal to determine the modem state.
     * We avoid that situation here (at the risk of modifying the state of
     * the MODEM_RST signal in the middle of a reset) by making the MODEM_RST
     * GPIO an input so we can accurately see whether the modem is actually on.
     */

#if ENCORE_MODEM_MGR_DEBUG_VERBOSE
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Setting GPIO %02d (%s) state to INPUT...\n", EMM_GPIO_MODEM_RST, getGpioLabel(EMM_GPIO_MODEM_RST));
#endif /* ENCORE_MODEM_MGR_DEBUG_VERBOSE */

    gpio_direction_input(EMM_GPIO_MODEM_RST);
    modemState = GetGpioValue(EMM_GPIO_MODEM_RST);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Modem is currently %s\n", ((0 == modemState) ? "OFF" : "ON") );

	return snprintf(buf, PAGE_SIZE, "%d\n", modemState);
}

static ssize_t modem_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    if ('0' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Turning modem OFF...\n");
        TurnModemOff();
    }
    else if ('1' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Turning modem ON...\n");
        TurnModemOn();
    }
    else
    {
        printk(KERN_ERR "encore_modem_mgr: Invalid value specified for Modem State: \"%s\"\n", buf);
        retval = -EINVAL;
    }

	return retval;
}

static DEVICE_ATTR(modem_state, S_IWUGO | S_IRUGO, modem_state_show, modem_state_store);

/****** modem_on ******/

static ssize_t modem_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int val = GetGpioValue(EMM_GPIO_MODEM_ON);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem ON) is currently %s\n", EMM_GPIO_MODEM_ON, ((0 == val) ? "LOW" : "HIGH") );

	return snprintf(buf, PAGE_SIZE, "%d\n", ((0 == val) ? 0 : 1));
}

static ssize_t modem_on_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    if ('0' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Setting GPIO %d (Modem ON) state to OUTPUT LOW...\n", EMM_GPIO_MODEM_ON);
        SetGpioValue(EMM_GPIO_MODEM_ON, 0);
    }
    else if ('1' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Setting GPIO %d (Modem ON) state to OUTPUT HIGH...\n", EMM_GPIO_MODEM_ON);
        SetGpioValue(EMM_GPIO_MODEM_ON, 1);
    }
    else
    {
        printk(KERN_ERR "encore_modem_mgr: Invalid value specified for GPIO %d (Modem ON): \"%s\"\n", EMM_GPIO_MODEM_ON, buf);
        retval = -EINVAL;
    }

	return retval;
}

static DEVICE_ATTR(modem_on, S_IWUGO | S_IRUGO, modem_on_show, modem_on_store);


/****** modem_ndisable ******/

static ssize_t modem_ndisable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int val = GetGpioValue(EMM_GPIO_MODEM_NDISABLE);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem nDISABLE) is currently %s\n", EMM_GPIO_MODEM_NDISABLE, ((0 == val) ? "LOW" : "HIGH") );

	return snprintf(buf, PAGE_SIZE, "%d\n", ((0 == val) ? 0 : 1));
}

static ssize_t modem_ndisable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    if ('0' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Setting GPIO %d (Modem nDISABLE) state to OUTPUT LOW (Enabling Flight Mode)...\n", EMM_GPIO_MODEM_NDISABLE);
        SetGpioValue(EMM_GPIO_MODEM_NDISABLE, 0);
    }
    else if ('1' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Setting GPIO %d (Modem nDISABLE) state to OUTPUT HIGH (Disabling Flight Mode)...\n", EMM_GPIO_MODEM_NDISABLE);
        SetGpioValue(EMM_GPIO_MODEM_NDISABLE, 1);            
    }
    else
    {
        printk(KERN_ERR "encore_modem_mgr: Invalid value specified for GPIO %d (Modem nDISABLE): \"%s\"\n", EMM_GPIO_MODEM_NDISABLE, buf);
        retval = -EINVAL;
    }

	return retval;
}

static DEVICE_ATTR(modem_ndisable, S_IWUGO | S_IRUGO, modem_ndisable_show, modem_ndisable_store);


/****** modem_rst ******/

static ssize_t modem_rst_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int val = GetGpioValue(EMM_GPIO_MODEM_RST);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem RST) is currently %s\n", EMM_GPIO_MODEM_RST, ((0 == val) ? "LOW" : "HIGH") );

	return snprintf(buf, PAGE_SIZE, "%d\n", ((0 == val) ? 0 : 1));
}

static ssize_t modem_rst_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    if ('0' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Setting GPIO %d (Modem RST) state to OUTPUT LOW...\n", EMM_GPIO_MODEM_RST);
        SetGpioValue(EMM_GPIO_MODEM_RST, 0);
    }
    else if ('1' == buf[0])
    {
        DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - Setting GPIO %d (Modem RST) state to OUTPUT HIGH...\n", EMM_GPIO_MODEM_RST);
        SetGpioValue(EMM_GPIO_MODEM_RST, 1);
    }
    else
    {
        printk(KERN_ERR "encore_modem_mgr: Invalid value specified for GPIO %d (Modem RST): \"%s\"\n", EMM_GPIO_MODEM_RST, buf);
        retval = -EINVAL;
    }

	return retval;
}

static DEVICE_ATTR(modem_rst, S_IWUGO | S_IRUGO, modem_rst_show, modem_rst_store);


/****** modem_on_pulse_width ******/

static ssize_t modem_on_pulse_width_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem ON) Pulse Width is currently %ld ms.\n", EMM_GPIO_MODEM_ON, g_pulse_width_ON);

	return snprintf(buf, PAGE_SIZE, "%ld\n", g_pulse_width_ON);
}

static ssize_t modem_on_pulse_width_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    /* We assume simple_strtoul gives us something useable */
    g_pulse_width_ON = simple_strtoul(buf, NULL, 10);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem ON) Pulse Width is now %ld ms.\n", EMM_GPIO_MODEM_ON, g_pulse_width_ON);

	return retval;
}

static DEVICE_ATTR(modem_on_pulse_width, S_IWUGO | S_IRUGO, modem_on_pulse_width_show, modem_on_pulse_width_store);


/****** modem_on_pulse ******/

static ssize_t modem_on_pulse_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem ON) Pulse - Nothing to show.\n", EMM_GPIO_MODEM_ON );

	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t modem_on_pulse_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    if ('1' == buf[0])
    {
        PulseGpioValue(EMM_GPIO_MODEM_ON, 1, g_pulse_width_ON);
    }
    else
    {
        printk(KERN_ERR "encore_modem_mgr: Invalid value specified for GPIO %d (Modem ON) Pulse: \"%s\"\n", EMM_GPIO_MODEM_ON, buf);
        retval = -EINVAL;
    }

	return retval;
}

static DEVICE_ATTR(modem_on_pulse, S_IWUGO | S_IRUGO, modem_on_pulse_show, modem_on_pulse_store);


/****** modem_rst_pulse_width ******/

static ssize_t modem_rst_pulse_width_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem RST) Pulse Width is currently %ld ms.\n", EMM_GPIO_MODEM_RST, g_pulse_width_RST);

	return snprintf(buf, PAGE_SIZE, "%ld\n", g_pulse_width_RST);
}

static ssize_t modem_rst_pulse_width_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    /* We assume simple_strtoul gives us something useable */
    g_pulse_width_RST = simple_strtoul(buf, NULL, 10);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem RST) Pulse Width is now %ld ms.\n", EMM_GPIO_MODEM_RST, g_pulse_width_RST);

	return retval;
}

static DEVICE_ATTR(modem_rst_pulse_width, S_IWUGO | S_IRUGO, modem_rst_pulse_width_show, modem_rst_pulse_width_store);


/****** modem_rst_pulse ******/

static ssize_t modem_rst_pulse_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: SYSFS - GPIO %d (Modem RST) Pulse - Nothing to show.\n", EMM_GPIO_MODEM_RST );

	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t modem_rst_pulse_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    ssize_t retval = count;

    if ('1' == buf[0])
    {
        PulseGpioValue(EMM_GPIO_MODEM_RST, 0, g_pulse_width_RST);
    }
    else
    {
        printk(KERN_ERR "encore_modem_mgr: Invalid value specified for GPIO %d (Modem RST) Pulse: \"%s\"\n", EMM_GPIO_MODEM_RST, buf);
        retval = -EINVAL;
    }

	return retval;
}

static DEVICE_ATTR(modem_rst_pulse, S_IWUGO | S_IRUGO, modem_rst_pulse_show, modem_rst_pulse_store);


/****** SysFS table entries ******/

static struct device_attribute* const g_encore_modem_mgr_attributes[] =
{
    &dev_attr_modem_state,
    &dev_attr_modem_on,
    &dev_attr_modem_ndisable,
    &dev_attr_modem_rst,
    &dev_attr_modem_on_pulse_width,
    &dev_attr_modem_on_pulse,
    &dev_attr_modem_rst_pulse_width,
    &dev_attr_modem_rst_pulse,
}; 


/*****************************************************************************
 * Power Management
 *****************************************************************************/

#ifdef CONFIG_PM
static int encore_modem_mgr_suspend(struct platform_device *pdev, pm_message_t state)
{
    /* Do nothing, for now */
    return 0;
}

static int encore_modem_mgr_resume(struct platform_device *pdef)
{
    /* Do nothing, for now */
    return 0;
}
#else  /* CONFIG_PM */
#define encore_modem_mgr_suspend NULL
#define encore_modem_mgr_resume  NULL
#endif /* CONFIG_PM */


/*****************************************************************************
 * Module initialization/deinitialization
 *****************************************************************************/

static int __init encore_modem_mgr_probe(struct platform_device *pdev)
{
    int err = 0;
    int i;

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Configuring Encore Modem Manager\n");

    /* Reserve the necessary GPIOs */

    err = gpio_request(EMM_GPIO_MODEM_RST, "encore_modem_mgr");
    if (0 > err)
    {
        printk(KERN_ERR "encore_modem_mgr: ERROR - Request for GPIO %d (Modem RST) FAILED.\n", EMM_GPIO_MODEM_RST);
        goto ERROR;
    }

    err = gpio_request(EMM_GPIO_MODEM_ON, "encore_modem_mgr");
    if (0 > err)
    {
        printk(KERN_ERR "encore_modem_mgr: ERROR - Request for GPIO %d (Modem ON) FAILED.\n", EMM_GPIO_MODEM_ON);
        goto ERROR;
    }

    err = gpio_request(EMM_GPIO_MODEM_NDISABLE, "encore_modem_mgr");
    if (0 > err)
    {
        printk(KERN_ERR "encore_modem_mgr: ERROR - Request for GPIO %d (Modem nDISABLE) FAILED.\n", EMM_GPIO_MODEM_NDISABLE);
        goto ERROR;
    }

    /* Configure the necessary GPIOs */

    gpio_direction_input(EMM_GPIO_MODEM_RST);
    gpio_direction_input(EMM_GPIO_MODEM_NDISABLE);
    gpio_direction_output(EMM_GPIO_MODEM_ON, 1);

    SetGpioValue(EMM_GPIO_MODEM_ON, 1);

    /* Make sure we sleep long enough for the modem to do its thing */
    msleep(EMM_MODEM_INITIALIZATION_DELAY);

    /* Make sure Flight Mode is enabled */

    /* EnableFlightMode(); */

    /* Reset the modem to ensure modem is in a known good state */
    ResetModem();

    /* Configure SysFS entries */

    for (i = 0; i < ARRAY_SIZE(g_encore_modem_mgr_attributes); i++)
    {
        err = device_create_file(&(pdev->dev), g_encore_modem_mgr_attributes[i]);
        if (0 > err)
        {
            while (i--)
            {
                device_remove_file(&(pdev->dev), g_encore_modem_mgr_attributes[i]);
            }
            printk(KERN_ERR "encore_modem_mgr: ERROR - failed to register SYSFS\n");
            goto ERROR;
        }
    }

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Encore Modem Manager configuration complete\n");

    return 0;

ERROR:
    gpio_free(EMM_GPIO_MODEM_RST);
    gpio_free(EMM_GPIO_MODEM_ON);
    gpio_free(EMM_GPIO_MODEM_NDISABLE);

    return err;
}

static void encore_modem_mgr_shutdown(struct platform_device *pdef)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Shutting down Encore Modem Manager\n");

    TurnModemOff();

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Encore Modem Manager shut down complete\n");
}

static int __devexit encore_modem_mgr_remove(struct platform_device *pdev)
{
    int i;

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Removing Encore Modem Manager\n");

    encore_modem_mgr_shutdown(pdev);

    /* Remove SysFS entries */
    for (i = 0; i < ARRAY_SIZE(g_encore_modem_mgr_attributes); i++)
    {
        device_remove_file(&(pdev->dev), g_encore_modem_mgr_attributes[i]);
	}

    /* Release the requested GPIOs */

    gpio_free(EMM_GPIO_MODEM_RST);
    gpio_free(EMM_GPIO_MODEM_ON);
    gpio_free(EMM_GPIO_MODEM_NDISABLE);

    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Encore Modem Manager removal complete\n");
    return 0;
}

static struct platform_driver g_encore_modem_mgr_driver =
{
    .probe    = encore_modem_mgr_probe,
    .remove   = __devexit_p(encore_modem_mgr_remove),
    .shutdown = encore_modem_mgr_shutdown,
    .suspend  = encore_modem_mgr_suspend,
    .resume   = encore_modem_mgr_resume,
    .driver   =
                {
                    .name   = "encore_modem_mgr",
                    .bus    = &platform_bus_type,
                    .owner  = THIS_MODULE,
                },
};


/*****************************************************************************
 * Module init/cleanup
 *****************************************************************************/

static int __init encore_modem_mgr_init(void)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Initializing Encore Modem Manager\n");

    return platform_driver_register(&g_encore_modem_mgr_driver);
}

static void __exit encore_modem_mgr_cleanup(void)
{
    DEBUGPRINT(KERN_INFO "encore_modem_mgr: Cleaning Up Encore Modem Manager\n");

    platform_driver_unregister(&g_encore_modem_mgr_driver);
}

module_init(encore_modem_mgr_init);
module_exit(encore_modem_mgr_cleanup);


/*****************************************************************************
 * Final Administrivia
 *****************************************************************************/

MODULE_DESCRIPTION("Encore Modem Manager");
MODULE_LICENSE("GPL");

