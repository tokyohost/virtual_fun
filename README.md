# Virtual Fan Kernel Driver

A **virtual fan kernel driver** for Linux systems (Debian, Ubuntu, and more), designed to provide **software-based fan speed control** on hardware that does **not natively support fan PWM control or fan speed adjustment**.

This driver exposes a **virtual fan device** to the Linux `hwmon` subsystem, allowing user-space programs to read and set fan speed values even on motherboards without physical fan control support.

---

## ✨ Features

- Works on **Debian, Ubuntu**, and most modern Linux distributions
- Creates a **virtual fan (hwmon) device**
- Allows **fan speed control via software**
- Ideal for mini PCs, NAS devices, and industrial boards
- Can integrate with USB fan controllers or external MCUs

---
This project works together with the following components:
- `virtual_funPico` ( a firmware project running on the Raspberry Pi Pico (RP2040))  
  https://github.com/tokyohost/virtual_funPico
- `virtualFunGo` (Go-based bridge service)  
  https://github.com/tokyohost/virtualFunGo
## 🧠 How It Works

This kernel module registers a virtual `hwmon` device and exposes standard fan control interfaces under:

```
/sys/class/hwmon/
```

It enables user-space programs to define fan speed logic even if the motherboard lacks PWM or RPM support.

⚠️ This driver does **not** directly control physical fans.

---

## 📦 Supported Systems

- Debian 10 / 11 / 12
- Ubuntu 20.04 / 22.04 / 24.04
- Linux kernel ≥ 5.x (recommended)

---

## 🚀 Installation

```bash
curl -fsSL https://raw.githubusercontent.com/tokyohost/virtual_fun_kernel/master/install.sh | sudo bash
```

---

## 🔍 Check Installation Status

```bash
curl -fsSL https://raw.githubusercontent.com/tokyohost/virtual_fun_kernel/master/check.sh | sudo bash
```

---

## 🔄 Reboot

```bash
sudo reboot
```

---

## ✅ Verify Module

```bash
lsmod | grep virtual_fan
```

---

## 📜 License

MIT License