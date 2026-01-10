# build
make
# install to kernel
insmod virtual_fan.ko

#unmound
sudo rmmod virtual_fan;

#test
make && sudo rmmod virtual_fan; sudo insmod virtual_fan.ko

#show kernel log
dmesg -wH


#show device
at /sys/class/hwmon/hwmon*/name