#!/usr/bin/env bash
set -e

echo "== DKMS status =="
dkms status | grep virtual-fan || echo "❌ DKMS not installed"

echo
echo "== Module info =="
modinfo virtual_fan || echo "❌ Module file not found"

echo
echo "== Load test =="
modprobe virtual_fan && echo "✔ modprobe ok"

echo
echo "== lsmod =="
lsmod | grep virtual_fan || echo "❌ Module not loaded"

echo
echo "== dmesg =="
dmesg | tail -20

echo
echo "== hwmon =="
ls /sys/class/hwmon/ || true
