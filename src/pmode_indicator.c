// SPDX-License-Identifier: GPL-2.0
/*
 * pmode_indicator - read-only monitor for the Sixunited AXB35-02 embedded
 * controller (GMKtec NucBox EVO-X2).
 *
 * Derived from ec-su_axb35-linux by cmetz (commit f62c2c2, GPL-2.0).
 *
 * This variant NEVER writes to the EC:
 *   - ec_write() is never called (the build verifies it is not even linked),
 *   - every sysfs attribute is 0444 and the hwmon ops have no .write,
 *   - there is no background work and nothing runs on unload,
 *   - it only loads on a DMI-matched EVO-X2.
 * The power mode can only be changed with the physical P-mode button.
 */

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#define DRV_NAME "pmode_indicator"

/* EC registers (upstream map; mode values verified on EVO-X2 BIOS 1.12) */
#define EC_REG_POWER_MODE 0x31
#define EC_REG_CPU_TEMP   0x70

struct axb35_fan {
	const char *label;
	u8 reg_hi;
	u8 reg_lo;
};

/*
 * Labels must not contain "cpu": btop (1.4.7) also parses fanN_input as a
 * temperature and auto-picks any sensor named *cpu*, showing rpm/1000 as °C.
 */
static const struct axb35_fan axb35_fans[] = {
	{ "APU fan 1",  0x35, 0x36 },
	{ "APU fan 2",  0x37, 0x38 },
	{ "System fan", 0x28, 0x29 },
};

static const struct dmi_system_id axb35_dmi_table[] = {
	{
		.ident = "GMKtec NucBox EVO-X2",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GMKtec"),
			DMI_MATCH(DMI_PRODUCT_NAME, "NucBox_EVO-X2"),
		},
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, axb35_dmi_table);

/* ---- hwmon: fan speeds and EC CPU temperature (shows up in `sensors`) ---- */

static umode_t axb35_is_visible(const void *data, enum hwmon_sensor_types type,
				u32 attr, int channel)
{
	return 0444;
}

static int axb35_read(struct device *dev, enum hwmon_sensor_types type,
		      u32 attr, int channel, long *val)
{
	u8 hi, lo, temp;
	int ret;

	switch (type) {
	case hwmon_fan:
		if (attr != hwmon_fan_input)
			return -EOPNOTSUPP;
		ret = ec_read(axb35_fans[channel].reg_hi, &hi);
		if (ret)
			return ret;
		ret = ec_read(axb35_fans[channel].reg_lo, &lo);
		if (ret)
			return ret;
		*val = (hi << 8) | lo;
		/* System fan reports 8000 briefly before settling at 0 (upstream quirk) */
		if (channel == 2 && *val == 8000)
			*val = 0;
		return 0;
	case hwmon_temp:
		if (attr != hwmon_temp_input)
			return -EOPNOTSUPP;
		ret = ec_read(EC_REG_CPU_TEMP, &temp);
		if (ret)
			return ret;
		*val = temp * 1000L;
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int axb35_read_string(struct device *dev, enum hwmon_sensor_types type,
			     u32 attr, int channel, const char **str)
{
	switch (type) {
	case hwmon_fan:
		*str = axb35_fans[channel].label;
		return 0;
	case hwmon_temp:
		*str = "EC";
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

/* No .write op: the hwmon core rejects every write. */
static const struct hwmon_ops axb35_hwmon_ops = {
	.is_visible  = axb35_is_visible,
	.read        = axb35_read,
	.read_string = axb35_read_string,
};

static const struct hwmon_channel_info * const axb35_hwmon_info[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_LABEL),
	NULL
};

static const struct hwmon_chip_info axb35_chip_info = {
	.ops  = &axb35_hwmon_ops,
	.info = axb35_hwmon_info,
};

/* ---- /sys/devices/platform/pmode_indicator/power_mode{,_raw} (read-only) ---- */

static ssize_t power_mode_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	u8 val;
	int ret;

	ret = ec_read(EC_REG_POWER_MODE, &val);
	if (ret)
		return ret;

	switch (val) {
	case 0x00:
		return sysfs_emit(buf, "balanced\n");
	case 0x01:
		return sysfs_emit(buf, "performance\n");
	case 0x02:
		return sysfs_emit(buf, "quiet\n");
	default:
		return sysfs_emit(buf, "unknown\n");
	}
}
static DEVICE_ATTR_RO(power_mode);

static ssize_t power_mode_raw_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	u8 val;
	int ret;

	ret = ec_read(EC_REG_POWER_MODE, &val);
	if (ret)
		return ret;
	return sysfs_emit(buf, "0x%02x\n", val);
}
static DEVICE_ATTR_RO(power_mode_raw);

static struct attribute *axb35_attrs[] = {
	&dev_attr_power_mode.attr,
	&dev_attr_power_mode_raw.attr,
	NULL
};
ATTRIBUTE_GROUPS(axb35);

/* ---- driver plumbing ---- */

static int axb35_probe(struct platform_device *pdev)
{
	struct device *hwmon;
	u8 val;
	int ret;

	ret = ec_read(EC_REG_POWER_MODE, &val);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "EC not readable\n");

	hwmon = devm_hwmon_device_register_with_info(&pdev->dev, DRV_NAME, NULL,
						     &axb35_chip_info, NULL);
	return PTR_ERR_OR_ZERO(hwmon);
}

static struct platform_driver axb35_driver = {
	.driver = {
		.name       = DRV_NAME,
		.dev_groups = axb35_groups,
	},
	.probe = axb35_probe,
};

static struct platform_device *axb35_pdev;

static int __init axb35_init(void)
{
	int ret;

	if (!dmi_check_system(axb35_dmi_table)) {
		pr_info(DRV_NAME ": not a GMKtec EVO-X2, not loading\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&axb35_driver);
	if (ret)
		return ret;

	axb35_pdev = platform_device_register_simple(DRV_NAME, PLATFORM_DEVID_NONE,
						     NULL, 0);
	if (IS_ERR(axb35_pdev)) {
		platform_driver_unregister(&axb35_driver);
		return PTR_ERR(axb35_pdev);
	}

	pr_info(DRV_NAME ": loaded (read-only)\n");
	return 0;
}

/* Unload only tears down sysfs; it touches nothing on the EC. */
static void __exit axb35_exit(void)
{
	platform_device_unregister(axb35_pdev);
	platform_driver_unregister(&axb35_driver);
}

module_init(axb35_init);
module_exit(axb35_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Read-only monitor for the Sixunited AXB35-02 EC (GMKtec EVO-X2)");
MODULE_INFO(derived_from, "github.com/cmetz/ec-su_axb35-linux@f62c2c2");
