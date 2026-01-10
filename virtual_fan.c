#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>

// 模拟数据结构
struct virtual_fan_data {
    long pwm_value;   // 0-255
    long enabled;   // 0 = disabled, 1 = manual
    long fan_speed;   // RPM
};

// 1. 定义哪些属性是可见的
static umode_t virtual_fan_is_visible(const void *data, enum hwmon_sensor_types type,
                                     u32 attr, int channel) {
    if (type == hwmon_pwm) {
        // 此时 channel 应该是 0，对应的文件名会是 pwm1
        switch (attr) {
            case hwmon_pwm_input:
            case hwmon_pwm_enable:
            case hwmon_pwm_mode:
                return 0644;
            default: return 0;
        }
    } else if (type == hwmon_fan) {
        if (attr == hwmon_fan_input) return 0444;
    }
    return 0;
}

// 2. 读取数据 (cat 访问时触发)
static int virtual_fan_read(struct device *dev,
                            enum hwmon_sensor_types type,
                            u32 attr, int channel, long *val)
{
    struct virtual_fan_data *data = dev_get_drvdata(dev);

    if (type == hwmon_pwm) {
        if (attr == hwmon_pwm_input) {
            *val = data->pwm_value;
            return 0;
        }
        if (attr == hwmon_pwm_enable) {
            *val = data->enabled;   // 必须返回 0 / 1
            return 0;
        }
        return -EOPNOTSUPP;
    }

    if (type == hwmon_fan) {
        if (attr == hwmon_fan_input) {
            *val = data->pwm_value * 20;
            return 0;
        }
        return -EOPNOTSUPP;
    }

    return -EOPNOTSUPP;
}

// 3. 写入数据 (echo 访问时触发)
static int virtual_fan_write(struct device *dev,
                             enum hwmon_sensor_types type,
                             u32 attr, int channel, long val)
{
    struct virtual_fan_data *data = dev_get_drvdata(dev);

    /* 仅支持 pwm1 */
    if (channel != 0)
        return -EINVAL;

    if (type != hwmon_pwm)
        return -EINVAL;

    switch (attr) {

        case hwmon_pwm_enable:
            /* fancontrol 只要求 0 / 1 */
            if (val != 0 && val != 1)
                return -EINVAL;

            data->enabled = val;
            return 0;

        case hwmon_pwm_input:
            if (!data->enabled)
                return -EACCES;

            if (val < 0 || val > 255)
                return -EINVAL;

            data->pwm_value = val;
            return 0;
    }

    return -EINVAL;
}

// 4. 组装 hwmon 结构体
static const struct hwmon_ops virtual_fan_hwmon_ops = {
    .is_visible = virtual_fan_is_visible,
    .read = virtual_fan_read,
    .write = virtual_fan_write,
};

static const struct hwmon_channel_info *virtual_fan_info[] = {
    /* * 这里的第一个参数是类型(pwm)，
     * 后面的参数是该类型下的多个属性。
     * 内核会将它们全部归类为索引 1 (pwm1, pwm1_enable, pwm1_mode)
     */
    HWMON_CHANNEL_INFO(pwm,
                       HWMON_PWM_INPUT|    /* 对应 pwm1 */
                       HWMON_PWM_ENABLE|   /* 对应 pwm1_enable */
                       HWMON_PWM_MODE),    /* 对应 pwm1_mode */

    HWMON_CHANNEL_INFO(fan,
                       HWMON_F_INPUT),     /* 对应 fan1_input */
    NULL
};

static const struct hwmon_chip_info virtual_fan_chip_info = {
    .ops = &virtual_fan_hwmon_ops,
    .info = virtual_fan_info,
};

// --- 平台驱动生命周期 ---

static struct platform_device *v_pdev;

static int virtual_fan_probe(struct platform_device *pdev) {
    struct device *hwmon_dev;
    struct virtual_fan_data *data;

    data = devm_kzalloc(&pdev->dev, sizeof(struct virtual_fan_data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    data->pwm_value = 100; // 默认初始值

    hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev, "virtual_pwm_fan",
                                                     data, &virtual_fan_chip_info, NULL);
    return PTR_ERR_OR_ZERO(hwmon_dev);
}

static struct platform_driver virtual_fan_driver = {
    .driver = { .name = "virtual_fan_driver" },
    .probe = virtual_fan_probe,
};

static int __init virtual_fan_init(void) {
    int ret;
    pr_info("Virtual Fan: Module loading...\n"); // 打印加载信息

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
MODULE_AUTHOR("Gemini Partner");
MODULE_DESCRIPTION("A simple virtual PWM fan driver for hwmon demo");