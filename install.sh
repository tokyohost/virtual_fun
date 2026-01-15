#!/usr/bin/env bash
set -e

REPO_URL="https://github.com/tokyohost/virtual_fun_kernel.git"
REPO_DIR="virtual_fun_kernel"

MODULE_NAME="virtual-fan"
MODULE_REAL_NAME="virtual_fan"
MODULE_VERSION="1.0"
SRC_SUBDIR="virtual_fan_v1"
DKMS_SRC="/usr/src/${MODULE_NAME}-${MODULE_VERSION}"

echo "==============================="
echo " Virtual Fan DKMS Installer"
echo "==============================="

# 必须 root
if [[ $EUID -ne 0 ]]; then
  echo "[ERROR] Please run as root"
  exit 1
fi

# 检查系统
if [ ! -f /etc/os-release ]; then
  echo "[ERROR] Cannot detect OS"
  exit 1
fi
. /etc/os-release

echo "[INFO] Detected OS: $ID"

# ---------- 工具检查 ----------
need_cmd() {
  command -v "$1" >/dev/null 2>&1
}

# ---------- 安装依赖 ----------
install_deps_debian() {
  apt update
  apt install -y git dkms build-essential linux-headers-$(uname -r)
}

install_deps_rhel() {
  yum install -y git gcc make dkms kernel-devel kernel-headers
}

echo "[INFO] Installing dependencies..."
case "$ID" in
  debian|ubuntu)
    install_deps_debian
    ;;
  centos|rhel|rocky|almalinux)
    install_deps_rhel
    ;;
  *)
    echo "[ERROR] Unsupported OS: $ID"
    exit 1
    ;;
esac

# ---------- 检查 git ----------
if ! need_cmd git; then
  echo "[ERROR] git not installed"
  exit 1
fi

# ---------- 获取源码 ----------
WORKDIR="/usr/local/src"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

if [ ! -d "$REPO_DIR" ]; then
  echo "[INFO] Cloning repository..."
  git config --global http.version HTTP/1.1
  git clone "$REPO_URL"
else
  echo "[INFO] Repository already exists, pulling latest..."
  cd "$REPO_DIR"
  git pull
  cd ..
fi

cd "$REPO_DIR"

if [ ! -d "$SRC_SUBDIR" ]; then
  echo "[ERROR] ${SRC_SUBDIR} not found in repository"
  exit 1
fi

# ---------- 安装到 /usr/src ----------
echo "[INFO] Installing source to ${DKMS_SRC}"
rm -rf "$DKMS_SRC"
mkdir -p "$DKMS_SRC"
cp -r "${SRC_SUBDIR}/"* "$DKMS_SRC/"

if [ ! -f "$DKMS_SRC/dkms.conf" ]; then
  echo "[ERROR] dkms.conf not found"
  exit 1
fi

# ---------- DKMS ----------
echo "[INFO] Registering DKMS module"

dkms remove -m "$MODULE_NAME" -v "$MODULE_VERSION" --all >/dev/null 2>&1 || true
dkms add -m "$MODULE_NAME" -v "$MODULE_VERSION"
dkms build -m "$MODULE_NAME" -v "$MODULE_VERSION"
dkms install -m "$MODULE_NAME" -v "$MODULE_VERSION"

# ---------- 开机自动加载 ----------
echo "[INFO] Enabling auto-load on boot"
echo "$MODULE_REAL_NAME" > /etc/modules-load.d/${MODULE_REAL_NAME}.conf

# ---------- 立即加载 ----------
echo "[INFO] Loading module now"
modprobe "$MODULE_REAL_NAME" || true

# ---------- 验证 ----------
echo
echo "========== DKMS STATUS =========="
dkms status
echo "================================="

if lsmod | grep -q "$MODULE_REAL_NAME"; then
  echo "[SUCCESS] Module ${MODULE_REAL_NAME} loaded successfully"
else
  echo "[WARNING] Module installed but not currently loaded"
fi

echo
echo "[DONE] virtual_fan DKMS installation complete"
