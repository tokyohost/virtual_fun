package main

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"github.com/tarm/serial"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

const homePath = "/sys/class/hwmon/"
const isSerialRunning = false

func main() {
	fmt.Println("=== Pico 虚拟风扇已启动 ===")

	for {
		// 1. 尝试初始化：查找串口和 HWMON 路径
		s, hwmonPath, err := initializeHardware()
		if err != nil {
			log.Printf("等待硬件就绪: %v", err)
			time.Sleep(3 * time.Second) // 探测频率
			continue
		}

		fmt.Printf("成功连接！串口已打开，驱动路径: %s\n", hwmonPath)

		// 2. 创建上下文，用于管理这一轮连接的协程生命周期
		ctx, cancel := context.WithCancel(context.Background())

		// 3. 启动桥接业务逻辑
		// 我们传一个 done channel，用来知道业务协程什么时候因为错误退出了
		done := make(chan struct{})
		go func() {
			startBridge(ctx, s, hwmonPath)
			close(done)
		}()

		// 4. 等待信号：要么是业务报错退出，要么是手动关掉
		<-done
		fmt.Println("硬件连接断开，尝试重新恢复...")

		// 清理资源
		cancel()
		s.Close()
		time.Sleep(2 * time.Second)
	}
}

// 初始化硬件：同时找到串口和驱动路径才算成功
func initializeHardware() (*serial.Port, string, error) {
	// 找到串口路径 (by-id)
	s, err := OpenPico() // 使用你之前的 OpenPico
	if err != nil {
		return nil, "", err
	}

	// 找到 C 驱动路径
	path, err := findHwmonPath() // 使用你之前的 findHwmonPath
	if err != nil {
		s.Close()
		return nil, "", err
	}

	return s, path, nil
}

// fileExists 检查文件是否存在
func fileExists(filename string) bool {
	_, err := os.Stat(filename)
	return !os.IsNotExist(err)
}
func startBridge(ctx context.Context, s *serial.Port, hwmonPath string) {
	pwmFile := filepath.Join(hwmonPath, "pwm1")
	rpmFile := filepath.Join(hwmonPath, "fan1_input")

	// 协程 1: 监听驱动 PWM -> 发给 Pico
	go func() {
		lastPwm := -1
		for {
			select {
			case <-ctx.Done(): // 收到退出信号
				return
			default:
				val := readIntFromFile(pwmFile)
				if val != lastPwm {
					if err := SetFanSpeed(s, val); err != nil {
						log.Printf("写入串口失败，可能已拔出: %v", err)
						return // 报错退出，触发重连逻辑
					}
					lastPwm = val
				}
				time.Sleep(200 * time.Millisecond)
			}
		}
	}()

	// 协程 2: 监听 Pico 串口 -> 写回驱动
	// 这里不加 go，让它在当前协程运行，阻塞 startBridge
	reader := bufio.NewReader(s)
	for {
		select {
		case <-ctx.Done():
			return
		default:
			line, err := reader.ReadString('\n')
			if err != nil {
				log.Printf("读取串口失败: %v", err)
				return // 触发重连
			}

			line = strings.TrimSpace(line)
			if strings.HasPrefix(line, "{") {
				var data PicoData
				if err := json.Unmarshal([]byte(line), &data); err == nil {
					// 写入 RPM 驱动文件
					os.WriteFile(rpmFile, []byte(strconv.Itoa(data.RPM)), 0644)
				}
			}
		}
	}
}

func findHwmonPath() (string, error) {
	var foundPath string
	hwmonDir := homePath

	err := filepath.Walk(hwmonDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}

		// 我们只对 hwmonX 目录感兴趣
		if strings.HasPrefix(info.Name(), "hwmon") {
			markerFile := filepath.Join(path, "device", "marker")

			if fileExists(markerFile) {
				content, err := os.ReadFile(markerFile)
				if err == nil && strings.Contains(string(content), "vFanByTk") {
					foundPath = path        // 捕获路径
					return filepath.SkipDir // 找到了，停止继续扫描该目录
				}
			}
		}
		return nil
	})

	if foundPath == "" && err == nil {
		return "", fmt.Errorf("未找到带有 marker 的硬件驱动")
	}
	return foundPath, err
}

// 示例：向 Pico 发送设置转速的 JSON 指令
func SetFanSpeed(s *serial.Port, pwmValue int) error {
	// 将 0-255 的 hwmon 值转换为 0-100 的百分比
	percent := int((float64(pwmValue) / 255.0) * 100)
	cmd := fmt.Sprintf("{\"set_duty\": %d}\n", percent)
	_, err := s.Write([]byte(cmd))
	if err != nil {
		log.Printf("发送指令失败: %v", err)
		return err
	}
	return nil
}

func readIntFromFile(filePath string) int {
	// 1. 读取文件全部内容
	data, err := os.ReadFile(filePath)
	if err != nil {
		// 如果读取失败（比如驱动还没加载），返回 0 或 -1
		return 0
	}

	// 2. 去除字符串两端的空白字符（如 \n \t 或空格）
	content := strings.TrimSpace(string(data))

	// 3. 将字符串转换为整数
	val, err := strconv.Atoi(content)
	if err != nil {
		return 0
	}

	return val
}
