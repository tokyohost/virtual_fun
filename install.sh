#!/bin/bash

# 配置区
MODULE_NAME="vfan"
MODULE_VERSION="1.0"
SRC_DIR="/usr/src/${MODULE_NAME}-${MODULE_VERSION}"
GO_BIN_DEST="/usr/local/bin/pico-fan-bridge"

# 必须以 root 权限运行
if [[ $EUID -ne 0 ]]; then
   echo "错误：必须使用 sudo 运行此脚本"
   exit 1
fi

echo "--- 1. 安装系统依赖 ---"
apt update
apt install -y dkms linux-headers-$(uname -r) build-essential

echo "--- 2. 配置 DKMS (内核驱动) ---"
# 清理旧版本
dkms remove -m ${MODULE_NAME} -v ${MODULE_VERSION} --all 2>/dev/null

# 创建源码目录并拷贝
mkdir -p ${SRC_DIR}
cp ./driver/* ${SRC_DIR}/

# 创建 dkms.conf
cat <<EOF > ${SRC_DIR}/dkms.conf
PACKAGE_NAME="${MODULE_NAME}"
PACKAGE_VERSION="${MODULE_VERSION}"
BUILT_MODULE_NAME[0]="virtual_fan_driver"
DEST_MODULE_LOCATION[0]="/kernel/drivers/hwmon"
AUTOINSTALL="yes"
EOF

# 注册并构建
dkms add -m ${MODULE_NAME} -v ${MODULE_VERSION}
dkms build -m ${MODULE_NAME} -v ${MODULE_VERSION}
dkms install -m ${MODULE_NAME} -v ${MODULE_VERSION}

# 设置开机自动加载模块
if ! grep -q "virtual_fan_driver" /etc/modules; then
    echo "virtual_fan_driver" >> /etc/modules
    echo "已添加驱动到 /etc/modules"
fi

echo "--- 3. 部署 Go Bridge 服务 ---"
echo "--- 检查 Go 环境 ---"
if ! command -v go &> /dev/null; then
    echo "未发现 Go 环境，正在安装..."
    apt install -y golang
fi

echo "--- 现场编译 Go Bridge ---"
cd ./go_bridge
go mod tidy  # 自动下载依赖
go build -o pico-fan-bridge .
cd ..


# 拷贝二进制文件
if [ -f "./go_bridge/pico-fan-bridge" ]; then
    cp ./go_bridge/pico-fan-bridge ${GO_BIN_DEST}
    chmod +x ${GO_BIN_DEST}
else
    echo "警告：未找到 Go 二进制文件，请确认路径"
fi

# 拷贝并启动 Systemd 服务
if [ -f "./pico-fan.service" ]; then
    cp ./pico-fan.service /etc/systemd/system/
    systemctl daemon-reload
    systemctl enable pico-fan.service
    systemctl restart pico-fan.service
    echo "Systemd 服务已启动"
fi

echo "--- 4. 验证安装 ---"
lsmod | grep virtual_fan_driver && echo "内核模块：已加载"
systemctl is-active pico-fan.service && echo "Go 服务：正在运行"

echo "安装完成！"