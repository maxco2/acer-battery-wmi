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

# Check or update battery health mode

If health mode is 1, the charging threshold limit is activated. Otherwise, it means the charging threshold limit is deactivated. 
You can update `health_mode` by `echo`.

````
cd /sys/module/acer_battery_wmi/drivers/platform:acer_battery_wmi/acer_battery_wmi/acer_battery/
cat health_mode
# echo 1 >> health_mode 
````
