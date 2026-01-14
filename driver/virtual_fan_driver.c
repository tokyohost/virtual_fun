#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

// 模拟数据结构
struct virtual_fan_data {
    long pwm_value;   // 0-255
    long enabled;     // 0 = disabled, 1 = manual
    long fan_speed;   // RPM
};

// 定义哪些属性是可见的
static umode_t virtual_fan_is_visible(const void *data, enum hwmon_sensor_types type,
                                      u32 attr, int channel) {
    if (type == hwmon_fan && attr == hwmon_fan_input) {
        return 0644; // 修改为 0644，允许 Go 写入真实 RPM
    }
    if (type == hwmon_pwm) {
        switch (attr) {
            case hwmon_pwm_input:
            case hwmon_pwm_enable:
            case hwmon_pwm_mode:
                return 0644;
            default:
                return 0;
        }
    } else if (type == hwmon_fan) {
        if (attr == hwmon_fan_input) {
            return 0444;
        }
    }
    return 0;
}

// 读取数据 (cat 访问时触发)
static int virtual_fan_read(struct device *dev, enum hwmon_sensor_types type,
                            u32 attr, int channel, long *val) {
    struct virtual_fan_data *data = dev_get_drvdata(dev);

    // 越界检查
    if (channel < 0 || channel >= NUM_FANS) return -EINVAL;

    if (type == hwmon_fan && attr == hwmon_fan_input) {
        *val = data->fan_speed[channel];
        return 0;
    }

    if (type == hwmon_pwm) {
        switch (attr) {
            case hwmon_pwm_input:
                *val = data->pwm_value[channel];
                return 0;
            case hwmon_pwm_enable:
                *val = data->enabled[channel];
                return 0;
            default:
                return -EOPNOTSUPP;
        }
    }
    return -EOPNOTSUPP;
}

// 写入数据 (echo 访问时触发)
static int virtual_fan_write(struct device *dev, enum hwmon_sensor_types type,
                             u32 attr, int channel, long val) {
    struct virtual_fan_data *data = dev_get_drvdata(dev);

    if (channel < 0 || channel >= NUM_FANS) return -EINVAL;

    if (type == hwmon_fan && attr == hwmon_fan_input) {
        data->fan_speed[channel] = val;
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

// 组装 hwmon 结构体
static const struct hwmon_ops virtual_fan_hwmon_ops = {
    .is_visible = virtual_fan_is_visible,
    .read = virtual_fan_read,
    .write = virtual_fan_write,
};

static const struct hwmon_channel_info *virtual_fan_info[] = {
    // 注册 3 个 PWM 通道
    HWMON_CHANNEL_INFO(pwm,
                       HWMON_PWM_INPUT | HWMON_PWM_ENABLE | HWMON_PWM_MODE, // PWM 1
                       HWMON_PWM_INPUT | HWMON_PWM_ENABLE | HWMON_PWM_MODE, // PWM 2
                       HWMON_PWM_INPUT | HWMON_PWM_ENABLE | HWMON_PWM_MODE),// PWM 3
    // 注册 3 个 Fan 通道
    HWMON_CHANNEL_INFO(fan,
                       HWMON_F_INPUT,  // Fan 1
                       HWMON_F_INPUT,  // Fan 2
                       HWMON_F_INPUT), // Fan 3
    NULL
};
static const struct hwmon_chip_info virtual_fan_chip_info = {
    .ops = &virtual_fan_hwmon_ops,
    .info = virtual_fan_info,
};

// 属性文件的显示函数
static ssize_t virtual_fan_marker_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "vFanByTk\n");
}

// 创建一个 sysfs 属性
static DEVICE_ATTR(marker, 0444, virtual_fan_marker_show, NULL);


static struct platform_device *v_pdev;

static int virtual_fan_probe(struct platform_device *pdev) {
    struct device *hwmon_dev;
    struct virtual_fan_data *data;
    char markerFilePath[256];
    int ret;
    struct kobject *kobj;


    data = devm_kzalloc(&pdev->dev, sizeof(struct virtual_fan_data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    data->pwm_value = 100; // 默认初始值

    hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev, "virtual_pwm_fan",
                                                     data, &virtual_fan_chip_info, NULL);
    kobj = &hwmon_dev->kobj;
    if (IS_ERR(hwmon_dev)) {
        pr_err("Virtual Fan: Failed to register hwmon device\n");
        return PTR_ERR(hwmon_dev);
    }
    v_pdev = pdev;
    // 获取设备路径
    if (!kobj) {
        pr_err("Virtual Fan: Failed to get kobject\n");
        return -EINVAL;
    }
    // 获取设备路径
    snprintf(markerFilePath, sizeof(markerFilePath), "%s%s","/sys/class/hwmon/", kobj->name);
    pr_info("device path %s",markerFilePath);
    // // 创建标记文件
    // ret = create_marker_file(markerFilePath);
    // if (ret) {
    //     pr_err("Virtual Fan: Failed to create marker file\n");
    // } else {
    //     pr_info("Virtual Fan: Marker file created successfully\n");
    // }
    // 创建 sysfs 属性文件
    ret = device_create_file(&pdev->dev, &dev_attr_marker);
    if (ret) {
        pr_err("Virtual Fan: Failed to create sysfs attribute\n");
    } else {
        pr_info("Virtual Fan: Sysfs attribute created successfully\n");
    }


    return 0;
}

static struct platform_driver virtual_fan_driver = {
    .driver = {
        .name = "virtual_fan_driver",
    },
    .probe = virtual_fan_probe,
};

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
    pr_info("Virtual Fan: Module unloading...\n");
    platform_device_unregister(v_pdev);
    platform_driver_unregister(&virtual_fan_driver);
}

module_init(virtual_fan_init);
module_exit(virtual_fan_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tokyohost");
MODULE_DESCRIPTION("A simple virtual PWM fan driver for hwmon demo");
