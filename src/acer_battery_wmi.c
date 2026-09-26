#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/version.h>

MODULE_DESCRIPTION("Support Acer Battery Health Control");
MODULE_AUTHOR("Copyright 2021 maxc@stateoftheart.pw");
MODULE_LICENSE("GPL v2");

#define ACER_BATTERY_GUID "79772EC5-04B1-4BFD-843C-61E7F77B6CC9"
MODULE_ALIAS("wmi:79772EC5-04B1-4BFD-843C-61E7F77B6CC9");

struct SetBatteryControlIn {
  u8 uBatteryNo;
  u8 uFunctionMask;
  u8 uFunctionStatus;
  u8 uReserved[5];
} __attribute__((packed));

struct SetBatteryControlOut {
  u16 uReturn;
  u16 uReserved;
} __attribute__((packed));

struct GetBatteryControlIn {
  u8 uBatteryNo;
  u8 uFunctionQuery;
  u16 uReserved;
} __attribute__((packed));

struct GetBatteryControlOut {
  u8 uFunctionList;
  u8 uReturn[2];
  u8 uFunctionStatus[5];
} __attribute__((packed));

static struct platform_driver platform_driver;
static struct platform_device *platform_device;

/*
 * Cached health mode, re-applied after resume because the EC may drop the
 * setting over a sleep cycle. 0xFF means "unknown", set on first read, write
 * or probe.
 */
static u8 health_mode_val = 0xFF;

static acpi_status acer_battery_wmi_exec_method(u8 val) {
  acpi_status status;
  union acpi_object *obj;
  struct SetBatteryControlIn in;
  struct SetBatteryControlOut *out;
  in.uBatteryNo = 1;
  in.uFunctionMask = 1;
  in.uFunctionStatus = val;
  in.uReserved[0] = 0;
  in.uReserved[1] = 0;
  in.uReserved[2] = 0;
  in.uReserved[3] = 0;
  in.uReserved[4] = 0;
  struct acpi_buffer input = {sizeof(struct SetBatteryControlIn), &in};
  struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
  status = wmi_evaluate_method(ACER_BATTERY_GUID, 0, 21, &input, &output);
  if (ACPI_FAILURE(status)) {
    pr_err("Error setting battery health status: %s\n",
           acpi_format_exception(status));
    return AE_ERROR;
  }
  obj = output.pointer;
  if (!obj || obj->type != ACPI_TYPE_BUFFER || obj->buffer.length != 4) {
    kfree(output.pointer);
    pr_err("Unexpected output format setting battery health status, buffer "
           "length:%d\n",
           obj->buffer.length);
    return AE_ERROR;
  }
  out = (struct SetBatteryControlOut *)(obj->buffer.pointer);
  printk("Setting battery health status success! Status was %d ret was 0x%x "
         "(%d)\n",
         status, out->uReturn, out->uReturn);
  kfree(output.pointer);
  return AE_OK;
}

static int acer_battery_wmi_query_health_status(u8 *health_status) {
  acpi_status status;
  union acpi_object *obj;
  struct GetBatteryControlIn in;
  struct GetBatteryControlOut *out;
  int ret = -ENODEV;
  in.uBatteryNo = 1;
  in.uFunctionQuery = 1;
  in.uReserved = 0;
  struct acpi_buffer input = {sizeof(struct GetBatteryControlIn), &in};
  struct acpi_buffer output = {ACPI_ALLOCATE_BUFFER, NULL};
  status = wmi_evaluate_method(ACER_BATTERY_GUID, 0, 20, &input, &output);
  if (ACPI_FAILURE(status)) {
    pr_err("Error getting battery health status: %s\n",
           acpi_format_exception(status));
    return -ENODEV;
  }
  obj = output.pointer;
  if (!obj || obj->type != ACPI_TYPE_BUFFER || obj->buffer.length != 8) {
    pr_err("Unexpected output format getting battery health status, buffer "
           "length:%d\n",
           obj ? obj->buffer.length : 0);
    goto out;
  }
  out = (struct GetBatteryControlOut *)(obj->buffer.pointer);
  if (out->uReturn[0] > 0)
    goto out;
  switch (out->uFunctionList) {
  case 1:
  case 3:
    *health_status = out->uFunctionStatus[0] > 0;
    ret = 0;
    break;
  case 2:
    /* Only calibration mode is available, no battery health control. */
  default:
    break;
  }
out:
  kfree(output.pointer);
  if (ret == 0)
    printk("Getting battery health status success!\n");
  return ret;
}

static ssize_t acer_battery_show_save_mode(struct device *dev,
                                           struct device_attribute *attr,
                                           char *buf) {
  u8 healthStatus;
  if (acer_battery_wmi_query_health_status(&healthStatus) != 0)
    return -ENODEV;
  health_mode_val = healthStatus;
  return sprintf(buf, "%d\n", healthStatus);
}

static ssize_t acer_battery_store_save_mode(struct device *dev,
                                            struct device_attribute *attr,
                                            const char *buf, size_t count) {
  u8 val;
  if (sscanf(buf, "%hhd", &val) != 1)
    return -EINVAL;
  if ((val != 0) && (val != 1))
    return -EINVAL;
  if (acer_battery_wmi_exec_method(val) != AE_OK)
    return -ENODEV;
  health_mode_val = val;
  return count;
}

struct device_attribute acer_battery_health =
    __ATTR(health_mode, 0664, acer_battery_show_save_mode,
           acer_battery_store_save_mode);

static struct attribute *acer_battery_attrs[] = {&acer_battery_health.attr,
                                                 NULL};

static const struct attribute_group platform_attribute_group = {
    .name = "acer_battery", .attrs = acer_battery_attrs};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void acer_battery_wmi_remove(struct platform_device *device) {
#else
static int acer_battery_wmi_remove(struct platform_device *device) {
#endif
  sysfs_remove_group(&platform_device->dev.kobj, &platform_attribute_group);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
  return 0;
#endif
}

static int __init acer_battery_wmi_probe(struct platform_device *pdev) {
  u8 health_status;
  if (!wmi_has_guid(ACER_BATTERY_GUID)) {
    pr_err("no acer battery WMI guid\n");
    return -ENODEV;
  }

  /* Cache the current health mode so it can be restored on resume. */
  if (acer_battery_wmi_query_health_status(&health_status) == 0)
    health_mode_val = health_status;

  return sysfs_create_group(&pdev->dev.kobj, &platform_attribute_group);
}

static int acer_battery_wmi_resume(struct device *dev) {
  /* The EC may not keep the health mode over a sleep cycle, re-apply it. */
  if (health_mode_val != 0xFF) {
    if (acer_battery_wmi_exec_method(health_mode_val) != AE_OK)
      pr_err("Failed to re-apply battery health mode after resume\n");
    else
      printk("Battery health mode %d re-applied after resume\n",
             health_mode_val);
  }
  return 0;
}

static const struct dev_pm_ops acer_battery_wmi_pm_ops = {
    .resume = acer_battery_wmi_resume};

static int __init acer_battery_wmi_init(void) {
  platform_driver.remove = acer_battery_wmi_remove;
  platform_driver.driver.owner = THIS_MODULE;
  platform_driver.driver.name = "acer_battery_wmi";
  platform_driver.driver.pm = &acer_battery_wmi_pm_ops;
  platform_device = platform_create_bundle(
      &platform_driver, acer_battery_wmi_probe, NULL, 0, NULL, 0);
  if (IS_ERR(platform_device))
    return PTR_ERR(platform_device);
  pr_info("Acer battery WMI driver loaded\n");
  return 0;
}

static void __exit acer_battery_wmi_exit(void) {
  platform_device_unregister(platform_device);
  platform_driver_unregister(&platform_driver);

  pr_info("Acer battery WMI driver unloaded\n");
}
module_init(acer_battery_wmi_init);
module_exit(acer_battery_wmi_exit);
