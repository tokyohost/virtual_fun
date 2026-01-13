package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"strings"

	"github.com/tarm/serial" // 需要安装此库
)

var currentPicoRPM int // 全局变量存储 Pico 传回的转速
// 定义与 Pico 输出匹配的结构体
type PicoData struct {
	RPM  int `json:"rpm"`
	Duty int `json:"duty"`
}

var currentPicoData PicoData

// 协程：监听 Pico 串口
func ReadPicoSerial(s *serial.Port) {

	reader := bufio.NewReader(s)
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			log.Printf("读取串口错误: %v", err)
			break
		}
		//  Pico 发送格式为 “{"rpm": 0, "duty": 60}”
		//line = strings.TrimSpace(line)
		//if strings.HasPrefix(line, "RPM:") {
		//	fmt.Sscanf(line, "RPM:%d", &currentPicoRPM)
		//}
		// 检查是否为 JSON 格式 (简单校验以防 Pico 启动时的杂讯)
		if strings.HasPrefix(line, "{") && strings.HasSuffix(line, "}") {
			var data PicoData
			err := json.Unmarshal([]byte(line), &data)
			if err != nil {
				log.Printf("JSON 解析失败: %v | 内容: %s", err, line)
				continue
			}

			// 更新全局变量或进行逻辑处理
			currentPicoData = data
			fmt.Printf("收到数据 -> 转速: %d RPM, 占空比: %d%%\n", data.RPM, data.Duty)
			currentPicoRPM := data.RPM
			fmt.Printf("当前 Pico 转速: %d RPM\n", currentPicoRPM)

		} else {
			// 打印非 JSON 的调试信息（比如 Pico 启动时的报错）
			log.Printf("Pico Log: %s", line)
		}

	}
}
func OpenPico() (*serial.Port, error) {
	portName := FindPicoPort()
	if portName == "" {
		return nil, fmt.Errorf("未发现 Pico 设备")
	}

	// 配置串口参数
	config := &serial.Config{
		Name: portName,
		Baud: 115200,
		// tarm/serial 默认是 8N1 模式，这与 MicroPython 匹配
	}

	// 真正打开串口，返回 *serial.Port
	s, err := serial.OpenPort(config)
	if err != nil {
		return nil, fmt.Errorf("打开串口失败: %v", err)
	}

	return s, nil
}

func FindPicoPort() string {
	// 方法 A: 扫描 by-id 目录
	idPath := "/dev/serial/by-id/"
	files, err := ioutil.ReadDir(idPath)
	if err == nil {
		for _, f := range files {
			if strings.Contains(f.Name(), "Pico") || strings.Contains(f.Name(), "Raspberry_Pi") {
				print("找到串口" + f.Name())
				return idPath + f.Name()
			}
		}
	}

	// 方法 B: 如果 by-id 不存在，尝试默认值
	return ""
}
