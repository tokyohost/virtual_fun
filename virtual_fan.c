#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

// 1. 必须定义宏
#define NUM_FANS 3

// 2. 结构体成员必须定义为数组
struct virtual_fan_data {
    long pwm_value[NUM_FANS];   // 数组：保存每个风扇的 PWM
    long enabled[NUM_FANS];     // 数组：保存每个风扇的使能状态
    long fan_speed[NUM_FANS];   // 数组：保存来自 Go 的真实 RPM
};

// 属性可见性逻辑
static umode_t virtual_fan_is_visible(const void *data, enum hwmon_sensor_types type,
                                      u32 attr, int channel) {
    // 基础越界检查
    if (channel >= NUM_FANS) return 0;

    if (type == hwmon_pwm) {
        switch (attr) {
            case hwmon_pwm_input:
            case hwmon_pwm_enable:
            case hwmon_pwm_mode:
                return 0644;
            default: return 0;
        }
    } else if (type == hwmon_fan) {
        if (attr == hwmon_fan_input) {
            return 0644; // 允许 Go 写入 RPM 数据
        }
    }
    return 0;
}

// 读取函数：利用 channel 索引
static int virtual_fan_read(struct device *dev, enum hwmon_sensor_types type,
                            u32 attr, int channel, long *val) {
    struct virtual_fan_data *data = dev_get_drvdata(dev);

    if (channel < 0 || channel >= NUM_FANS) return -EINVAL;

    if (type == hwmon_fan && attr == hwmon_fan_input) {
        *val = data->fan_speed[channel];
        return 0;
    }

    if (type == hwmon_pwm) {
        if (attr == hwmon_pwm_input) {
            *val = data->pwm_value[channel];
            return 0;
        }
        if (attr == hwmon_pwm_enable) {
            *val = data->enabled[channel];
            return 0;
        }
    }
    return -EOPNOTSUPP;
}

// 写入函数：利用 channel 索引
static int virtual_fan_write(struct device *dev, enum hwmon_sensor_types type,
                             u32 attr, int channel, long val) {
    struct virtual_fan_data *data = dev_get_drvdata(dev);

    if (channel < 0 || channel >= NUM_FANS) return -EINVAL;

    if (type == hwmon_fan && attr == hwmon_fan_input) {
        data->fan_speed[channel] = val; // 接收来自 Go 的 RPM
        return 0;
    }

    if (type == hwmon_pwm) {
        switch (attr) {
            case hwmon_pwm_enable:
                if (val != 0 && val != 1) return -EINVAL;
                data->enabled[channel] = val;
                return 0;
            case hwmon_pwm_input:
                if (!data->enabled[channel]) return -EACCES;
                if (val < 0 || val > 255) return -EINVAL;
                data->pwm_value[channel] = val;
                return 0;
        }
    }
    return -EINVAL;
}

static const struct hwmon_ops virtual_fan_hwmon_ops = {
    .is_visible = virtual_fan_is_visible,
    .read = virtual_fan_read,
    .write = virtual_fan_write,
};

// 3. 定义 3 个通道的信息
static const struct hwmon_channel_info *virtual_fan_info[] = {
    HWMON_CHANNEL_INFO(pwm,
                       HWMON_PWM_INPUT | HWMON_PWM_ENABLE | HWMON_PWM_MODE, // ch 0
                       HWMON_PWM_INPUT | HWMON_PWM_ENABLE | HWMON_PWM_MODE, // ch 1
                       HWMON_PWM_INPUT | HWMON_PWM_ENABLE | HWMON_PWM_MODE),// ch 2
    HWMON_CHANNEL_INFO(fan,
                       HWMON_F_INPUT,  // ch 0
                       HWMON_F_INPUT,  // ch 1
                       HWMON_F_INPUT), // ch 2
    NULL
};

static const struct hwmon_chip_info virtual_fan_chip_info = {
    .ops = &virtual_fan_hwmon_ops,
    .info = virtual_fan_info,
};

// 基础 Probe 函数
static int virtual_fan_probe(struct platform_device *pdev) {
    struct device *hwmon_dev;
    struct virtual_fan_data *data;
    int i;

    data = devm_kzalloc(&pdev->dev, sizeof(struct virtual_fan_data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    // 4. 初始化所有通道的默认值
    for (i = 0; i < NUM_FANS; i++) {
        data->pwm_value[i] = 100;
        data->enabled[i] = 1;
        data->fan_speed[i] = 0;
    }

    hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev, "virtual_pwm_fan",
                                                     data, &virtual_fan_chip_info, NULL);
    if (IS_ERR(hwmon_dev)) return PTR_ERR(hwmon_dev);

    platform_set_drvdata(pdev, data);
    return 0;
}

static struct platform_driver virtual_fan_driver = {
    .driver = { .name = "virtual_fan_driver" },
    .probe = virtual_fan_probe,
};

static struct platform_device *v_pdev;

static int __init virtual_fan_init(void) {
    int ret;
    pr_info("Virtual Fan: Module loading...\n");

    ret = platform_driver_register(&virtual_fan_driver);
    if (ret) {
        pr_err("Virtual Fan: Failed to register driver\n");
        return ret;
    }

    v_pdev = platform_device_register_simple("virtual_fan_driver", -1, NULL, 0);
    if (IS_ERR(v_pdev)) {
        pr_err("Virtual Fan: Failed to register device\n");
        platform_driver_unregister(&virtual_fan_driver);
        return PTR_ERR(v_pdev);
    }

    pr_info("Virtual Fan: Device registered successfully!\n");
    return 0;
}

static void __exit virtual_fan_exit(void) {
    platform_device_unregister(v_pdev);
    platform_driver_unregister(&virtual_fan_driver);
}

module_init(virtual_fan_init);
module_exit(virtual_fan_exit);
MODULE_LICENSE("GPL");