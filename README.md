# acer-battery-wmi

This repo is a Linux Acer battery health control driver. This kernel module brings windows Acer Care Center battery health control mode to Linux.

**Please confirm that you have a matching WMI method on your device. Take a look [issue#1](https://github.com/maxco2/acer-battery-wmi/issues/1) for further information.**

# Install kernel modules

For general Linux users:

````bash
git clone https://github.com/maxco2/acer-battery-wmi.git
cd acer-battery-wmi/src
make -j6
sudo make install
````

For Arch Linux users:

````
git clone https://github.com/maxco2/acer-battery-wmi-dkms.git
makepkg
sudo pacman -U acer-battery-wmi-0.1-1-x86_64.pkg.tar.zst
# reboot
````

To package local changes with the updated DKMS PKGBUILD, set the absolute
path to this checkout (otherwise the PKGBUILD downloads GitHub sources):

```bash
cd /path/to/acer-battery-wmi-dkms
ACER_BATTERY_WMI_SRC=/path/to/acer-battery-wmi makepkg -f
sudo pacman -U acer-battery-wmi-0.1-3-x86_64.pkg.tar.zst
```

Install the headers matching your target kernel before installing the package.
The driver supports both the old platform remove callback and the void callback
used by Linux 6.11 and newer.

# Check or update battery health mode

If health mode is 1, the charging threshold limit is activated. Otherwise, it means the charging threshold limit is deactivated. 
You can update `health_mode` by `echo`.

````
cd /sys/module/acer_battery_wmi/drivers/platform:acer_battery_wmi/acer_battery_wmi/acer_battery/
cat health_mode
# echo 1 >> health_mode 
````
