# install pak
sudo apt-get install linux-headers-$(uname -r)

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
cat /sys/class/hwmon/hwmon*/name