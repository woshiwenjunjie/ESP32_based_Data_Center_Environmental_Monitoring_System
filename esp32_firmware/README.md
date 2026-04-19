# ESP32 主从通信环境监测系统 v1.0

> **嵌入式系统技术文档** | ESP32-S3-WROOM (8MB PSRAM) + ESP32 DevKit

---

## 一、项目概述

本项目是基于 **ESP32** 的智能环境监测系统，采用**主从通信架构**（Master-Slave），实现了传感器数据采集、TCP主从传输、MJPEG视频流监控、AI视觉识别、OLED实时显示、外设控制等完整功能。

| 属性 | 说明 |
|------|------|
| 版本 | v1.0 (增强外设控制 + 频率可调) |
| 硬件平台 | 主机: ESP32-S3-WROOM (8MB PSRAM) / 从机: ESP32 DevKit |
| 通信协议 | TCP Socket (端口8888) + HTTP/WebSocket |
| 编译环境 | Arduino IDE 2.x + ESP32 Arduino Core ≥3.0 |
| 更新日期 | 2026-04-18 |

### v1.0 更新亮点

- ✨ **蜂鸣器频率可调**：支持100-10000Hz范围自定义频率
- ✨ **外设控制增强**：优化继电器/蜂鸣器/电机/舵机控制API
- ✨ **参数配置优化**：调整时间间隔参数平衡性能与资源占用
- 🐛 **Bug修复**：修复频率参数解析和同步问题
- 📝 **文档完善**：全面更新技术文档

---

## 二、系统架构图

```
┌─────────────────────────────┐         ┌─────────────────────────────┐
│        从机 (Slave)          │   TCP    │        主机 (Master)         │
│                             │ ──────> │                             │
│  ┌───────────────────────┐  │  8888   │  ┌───────────────────────┐  │
│  │ • DHT11 温湿度传感器  │  │  端口   │  │ • OV3660 摄像头模块    │  │
│  │ • MQ-2 气体传感器     │  │         │  │ • HTTP Web服务器       │  │
│  │ • 水位传感器(ADC)      │  │         │  │ • MJPEG 视频流服务     │  │
│  │ • OLED 显示屏          │  │         │  │ • AI 视觉识别          │  │
│  └───────────────────────┘  │         │  │ • OLED 远程数据显示    │  │
│                             │         │  │ • 外设控制(继电器/蜂鸣器│  │
│  ESP32 DevKit               │         │  │            /电机/舵机) │  │
│  纯传感器采集节点           │         │  └───────────────────────┘  │
└─────────────────────────────┘         └──────────────┬──────────────┘
                                                        │
                                               HTTP / WebSocket
                                                        │
                                               ┌────────┴────────┐
                                               │                 │
                                        ┌──────┴──────┐   ┌──────┴──────┐
                                        │   Web 浏览器  │   │ 微信小程序   │
                                        │  (内置界面)  │   │  (API调用)   │
                                        └─────────────┘   └─────────────┘


═══════════════════════════════════════════════════════════════════
                        数据流向总览
═══════════════════════════════════════════════════════════════════

  [DHT11] ──→ 温度/湿度 ─┐
  [MQ-2]  ──→ 烟雾/气体 ─┤                              [OV3660] ──→ JPEG帧 ──→ MJPEG流
  [水位]  ──→ ADC值   ──┤                                      │                │
                          ├──→ TCP(8888) ──→ [主机数据融合层] ──┤──→ HTTP API ──→ 客户端
                          │                  │                   │
                          │                  ├──→ OLED显示       ├──→ WebSocket → 实时推送
                          │                  │                   │
                          │                  └──→ AI识别请求 ──→ 外部LLM服务
                          │
                    [从机OLED] ←── 本地显示
```

---

## 三、项目文件结构

```
esp32_firmware/
│
├── master/                          # 🖥️ 主机代码 (ESP32-S3)
│   ├── main/
│   │   ├── main.ino                 # 主程序入口 & 主循环
│   │   ├── web_server.cpp/h         # ⭐ HTTP服务器核心（含蜂鸣器频率控制）
│   │   ├── myOV3660.cpp/h           # OV3660摄像头驱动
│   │   ├── actuators.cpp/h          # ⭐ 外设控制模块（含buzzer_on_with_freq）
│   │   ├── sensors.cpp/h            # 传感器数据采集与融合
│   │   ├── sensor_data.cpp/h        # 全局数据定义
│   │   ├── myWIFI.cpp/h             # TCP服务器(接收从机数据)
│   │   ├── wifi_config.cpp/h        # WiFi配置管理(SPIFFS)
│   │   ├── u8g2_oled.cpp/h          # OLED显示屏驱动
│   │   ├── mySensor.cpp/h           # 本地传感器驱动
│   │   └── public.h                 # 公共宏定义与类型
│
├── slave/                           # 📡 从机代码 (ESP32 DevKit)
│   ├── main/
│   │   ├── main.ino                 # 从机程序入口 & 主循环
│   │   ├── mySensor.cpp/h           # 传感器驱动(DHT11/MQ2/水位)
│   │   ├── myWIFI.cpp/h             # TCP客户端(发送数据到主机)
│   │   ├── sensor_data.cpp/h        # 数据结构定义
│   │   ├── u8g2_oled.cpp/h          # OLED双模式显示
│   │   ├── config.h                 # ⭐ 系统配置（时间间隔参数）
│   │   └── public.h                 # 公共宏定义
│
└── README.md                        # 本文档
```

---

## 四、主机 (Master) 详细说明

### 4.1 main.ino — 主程序入口

[main.ino](master/main/main.ino) 是系统的核心调度中心，负责所有模块的初始化和主循环任务调度。

#### 初始化流程

```
setup()
  │
  ├─① Serial.begin(115200)              // 串口初始化
  │
  ├─② psramInit()                       // PSRAM检测与初始化
  │     └─ 输出芯片型号/版本/PSRAM大小/堆空闲
  │
  ├─③ wifiManager.begin()               // WiFi配置管理器初始化
  │
  ├─④ wifiManager.isConfigured() ?      // 检查SPIFFS中是否有保存的配置
  │     ├─ YES → connectWiFi()           //   连接WiFi(超时15s)
  │     │     └─ 成功 → 继续初始化 ↓
  │     └─ NO  → 提示首次配置            //   通过串口引导用户配置
  │
  ├─⑤ initSensors()                     // 本地传感器初始化
  ├─⑥ actuators_init()                  //外设模块初始化
  ├─⑦ myov3660_init()                   // OV3660摄像头初始化(基于PSRAM选择分辨率)
  ├─⑧ MyU8g2Oled_init()                 // OLED显示屏初始化(I2C: SCL=21, SDA=47)
  ├─⑨ wifi_server_init()                // TCP服务器启动(端口8888)
  └─⑩ startCameraServer()              // HTTP Web服务器启动(80端口)
```

#### WiFi自动重连机制

```cpp
// 主循环中的WiFi状态检查 (每5秒一次)
if (millis() - lastWiFiCheck >= 5000) {
    if (WiFi.status() != WL_CONNECTED && wifiManager.isConfigured()) {
        wifiManager.connectWiFi(10000);  // 尝试重连(超时10s)
    }
}
```

#### 主循环任务调度

```
loop()
  │
  ├─ WiFi状态检查 (5s间隔) ──→ 断线自动重连
  │
  ├─ buzzer_update()              // 异步蜂鸣器状态更新(非阻塞)
  │
  ├─ handleWiFiCommunication()    // TCP通信处理(接收从机数据)
  │
  ├─ readSensors() (2s间隔)       // 读取本地传感器 + 串口输出状态
  │
  └─ displayRemote() (200ms间隔)  // OLED显示远程从机数据
        └─ 数据变化检测: 仅当温度/湿度/烟雾/气体/水位/连接状态变化时刷新
           (避免I2C总线频繁操作导致闪烁)
```

---

### 4.2 web_server.cpp/h — HTTP服务器核心

[web_server.cpp](master/main/web_server.cpp) 是整个系统最复杂的模块，承担视频流、拍照、AI识别、传感器API、外设控制等全部HTTP服务。

#### API端点完整列表

| 方法 | 路径 | 功能 | 关键参数 |
|------|------|------|----------|
| `GET` | `/` | 内置Web控制界面 | — |
| `GET` | `/stream` | MJPEG实时视频流 | 最大2并发客户端 |
| `GET` | `/capture` | JPEG单张拍照 | `resolution`, `quality`, `format` |
| `GET` | `/burst` | 批量拍照(1-10张) | `count`, `delay`, `resolution` |
| `GET` | `/sensors` | 传感器数据JSON | `field`(可选筛选字段) |
| `GET` | `/status` | 系统状态全景 | 内存/CPU/摄像头/WiFi/从机 |
| `POST` | `/config` | 更新摄像头默认配置 | `default_framesize`, `default_quality` |
| `POST` | `/ask` | AI视觉识别 | `question`, `image`(可选), `resolution` |
| `GET\|POST` | `/ai_config` | AI服务配置管理 | `ai_service_url`, `ai_api_key`, `ai_model` |
| `GET\|POST` | `/actuator` | ⭐ 外设控制API（含频率参数） | `device`, `action`, `freq`, `speed`, `angle` |
| `GET` | `/logs` | 获取系统日志 | — |
| `DELETE` | `/logs` | 清空系统日志 | — |
| `POST` | `/restart` | 远程重启设备 | — |
| `WS` | `/ws` | WebSocket实时双向通信 | `capture`, `status`, `sensors` |

#### /actuator 外设控制API详解（v1.0增强）

**基本格式：**
```
GET /actuator?device={设备}&action={操作}&{可选参数}
```

**支持的设备和操作：**

| 设备 (device) | 支持的操作 (action) | 特殊参数 | 示例 |
|---------------|---------------------|----------|------|
| `relay` | on, off, toggle, status | - | `/actuator?device=relay&action=on` |
| `buzzer` | on, off, beep, status | **freq**(频率) | `/actuator?device=buzzer&action=on&freq=2000` |
| `motor` | stop, run, status | **speed**(速度0-255) | `/actuator?device=motor&action=run&speed=128` |
| `servo` | attach, detach, write, status | **angle**(角度0-180) | `/actuator?device=servo&action=write&angle=90` |

**蜂鸣器频率控制实现（v1.0核心功能）：**

```cpp
// web_server.cpp 中的蜂鸣器控制逻辑
else if (strcmp(device, "buzzer") == 0) {
    if (strcmp(action, "on") == 0) {
        // 支持频率参数，例如: /actuator?device=buzzer&action=on&freq=2000
        ESP_LOGI(TAG, "[Buzzer] 收到请求: %s", req->uri);
        
        char freq_str[16] = {0};
        int freq = BUZZER_FREQ_DEFAULT;  // 默认使用配置的频率
        
        esp_err_t ret = httpd_query_key_value(query, "freq", freq_str, sizeof(freq_str));
        if (ret == ESP_OK) {
            freq = atoi(freq_str);
            freq = constrain(freq, 100, 10000);  // 限制频率范围: 100-10000Hz
        }
        
        ESP_LOGI(TAG, "[Buzzer] 使用频率: %dHz", freq);
        buzzer_on_with_freq(freq);  // 使用指定频率开启蜂鸣器
        result_state = "ON";
    } 
    else if (strcmp(action, "off") == 0) {
        buzzer_off();
        result_state = "OFF";
    } 
    else if (strcmp(action, "beep") == 0) {
        buzzer_beep();  // 短促提示音
        result_state = "BEEP";
    }
    // ... 其他处理
}
```

**频率参数说明：**

| 参数 | 类型 | 范围 | 默认值 | 说明 |
|------|------|------|--------|------|
| `freq` | int | 100-10000 Hz | 2000 Hz | 蜂鸣器发声频率 |

**频率范围参考：**

| 频率范围 | 效果 | 适用场景 |
|----------|------|----------|
| 500-1000 Hz | 低沉音效 | 提示音、警报 |
| 1000-2000 Hz | 标准音效 | 常规提示、通知 |
| 2000-3000 Hz | 清脆音效 | 操作确认、成功提示 |
| 3000-5000 Hz | 尖锐音效 | 紧急警告（注意听力保护）|
| 5000-10000 Hz | 高频音调 | 特殊效果（建议短时使用）|

---

### 4.3 actuators.cpp/h — 外设控制模块（v1.0增强）

[actuators.h](master/main/actuators.h) 提供四种外设的完整控制接口，采用 LEDC PWM 硬件通道实现高精度控制。

#### 引脚分配表 (已验证无冲突)

| 外设 | GPIO引脚 | 控制方式 | LEDC通道 | 备注 |
|------|---------|---------|---------|------|
| 继电器 | GPIO45 | 数字电平(高触发) | — | 开关类设备 |
| 蜂鸣器 | GPIO40 | PWM频率可调 | Channel 2 | 无源蜂鸣器，**支持100-10000Hz** |
| 直流电机 | GPIO19 | PWM调速(0-255) | Channel 3 | 推荐5kHz载频 |
| SG90舵机 | GPIO20 | PWM角度(0-180°) | Servo库 | 依赖ESP32Servo.h |

#### API速查表

```cpp
// ===== 初始化 =====
void actuators_init();                    // 初始化所有外设引脚和PWM通道

// ===== 继电器 =====
void relay_on();                          // 继电器闭合
void relay_off();                         // 继电器断开
void relay_toggle();                      // 切换状态
bool relay_get_state();                   // 获取当前状态

// ===== 蜂鸣器（v1.0增强）=====
void buzzer_on();                         // 开启（使用默认频率）
void buzzer_off();                        // 关闭
void buzzer_beep();                       // 短促提示音
void buzzer_on_with_freq(int freq);       // ⭐ 使用指定频率开启（100-10000Hz）
bool buzzer_get_state();                  // 获取当前状态
void buzzer_update();                     // 非阻塞状态更新（主循环调用）

// ===== 电机 =====
void motor_stop();                        // 停止
void motor_run(uint8_t speed);            // 运行（速度0-255）
bool motor_get_state();                   // 获取运行状态

// ===== 舵机 =====
void servo_attach();                      // 使能舵机
void servo_detach();                      // 禁用舵机
void servo_write(int angle);              // 写入角度（0-180°）
bool servo_get_state();                   // 获取使能状态
```

#### 蜂鸣器频率控制实现细节

```cpp
// actuators.cpp - 蜂鸣器频率控制
#define BUZZER_FREQ_DEFAULT  2000         // 默认频率 2000Hz
#define BUZZER_FREQ_MIN      100         // 最小频率 100Hz
#define BUZZER_FREQ_MAX      10000       // 最大频率 10000Hz

void buzzer_on_with_freq(int freq) {
    // 限制频率范围
    freq = constrain(freq, BUZZER_FREQ_MIN, BUZZER_FREQ_MAX);
    
    // 设置LEDC频率并启动PWM
    ledcSetup(BUZZER_LEDC_CHANNEL, freq, LEDC_TIMER_10_BIT);
    ledcAttachPin(BUZZER_GPIO_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 512);  // 50%占空比
    
    buzzerState = true;
    currentBuzzerFreq = freq;             // 记录当前频率
    
    Serial.printf("[Buzzer] ON @ %dHz\n", freq);
}

void buzzer_update() {
    // 在主循环中调用，用于非阻塞的状态检查
    // 可用于实现自动停止等功能
}
```

---

### 4.4 PSRAM智能内存管理系统

ESP32-S3 内部 SRAM 仅有约 **300KB**，而 VGA 分辨率的 JPEG 图像需要 **30-80KB** 缓冲区。本系统设计了 `smart_malloc()` 函数实现智能内存分配策略。

#### smart_malloc() 实现

```cpp
// 文件: web_server.cpp
#define MIN_FREE_HEAP_THRESHOLD (30 * 1024)   // 保留30KB堆空间安全阈值

static void* smart_malloc(size_t size) {
    void *ptr = NULL;

    // 规则1: 大缓冲区(>4KB)优先分配到PSRAM(8MB外部内存)
    if (psramFound() && size > 4096) {
        ptr = ps_malloc(size);       // PSRAM分配
        if (ptr) return ptr;         // 成功则直接返回
    }

    // 规则2: 小对象或PSRAM分配失败时使用内部堆
    if (ESP.getFreeHeap() > size + MIN_FREE_HEAP_THRESHOLD) {
        ptr = malloc(size);          // SRAM堆分配
    }

    return ptr;
}
```

---

### 4.5 被动帧缓存架构 (Passive Frame Buffer)

这是本系统最核心的架构创新之一。

#### 问题背景

传统模式下，**视频流线程**和**拍照/AI线程**会竞争同一个摄像头硬件资源：

```
❌ 传统模式 (有冲突):
  视频流线程: 锁定摄像头 → 抓帧 → JPEG转换 → 发送 → 解锁
  拍照线程:   等待锁... → 锁定摄像头 → 抓帧 → 解锁
                                    ↑
                            视频流中断! 用户看到画面卡顿!
```

#### 解决方案：被动帧缓存

```
✅ 被动帧缓存模式 (零冲突):
  ┌──────────────────────────────────────────────────┐
  │              视频流线程 (持续运行)                  │
  │  抓帧 → JPEG转换 → 写入shared_frame_buf(Mutex保护) │
  │                      ↓                           │
  │              共享缓冲区 (PSRAM)                    │
  │                      ↑                           │
  │  拍照/AI线程: 读取shared_frame_buf → 返回数据       │
  │  (完全不触碰摄像头!)                                │
  └──────────────────────────────────────────────────┘
```

#### 优势总结

| 特性 | 传统模式 | 被动帧缓存模式 |
|------|---------|---------------|
| 拍照对视频流的影响 | 中断数百毫秒 | **零影响** |
| 并发安全性 | 需要camera_mutex长时间持有 | **仅需短暂读取锁** |
| 拍照延迟 | 需等待摄像头空闲 | **<100ms**(读缓存即可) |
| AI识别响应时间 | 慢(需等摄像头) | **快(复用最新帧)** |

---

### 4.6 myOV3660.cpp/h — 摄像头驱动

[myOV3660.h](master/main/myOV3660.h) 封装了 OV3660 摄像头的初始化和配置逻辑。

#### 引脚定义 (ESP32-S3 Camera Bus)

```cpp
#define PWDN_GPIO_NUM  -1    // 电源关断引脚(未使用)
#define RESET_GPIO_NUM -1    // 复位引脚(未使用)
#define XCLK_GPIO_NUM  15    // 外部时钟输入(摄像头时钟)
#define SIOD_GPIO_NUM   4    // I2C SCD (配置数据线)
#define SIOC_GPIO_NUM   5    // I2C SCK (配置时钟线)

#define Y2_GPIO_NUM    11    // 数据线 D2
#define Y3_GPIO_NUM     9    // 数据线 D3
#define Y4_GPIO_NUM     8    // 数据线 D4
#define Y5_GPIO_NUM    10    // 数据线 D5
#define Y6_GPIO_NUM    12    // 数据线 D6
#define Y7_GPIO_NUM    18    // 数据线 D7
#define Y8_GPIO_NUM    17    // 数据线 D8
#define Y9_GPIO_NUM    16    // 数据线 D9

#define VSYNC_GPIO_NUM  6    // 垂直同步信号
#define HREF_GPIO_NUM   7    // 行参考信号
#define PCLK_GPIO_NUM  13    // 像素时钟
```

#### 动态分辨率选择策略

```
myov3660_init()
  │
  └─ psramFound() ?
       ├─ YES → FRAMESIZE_SVGA (800x600) 或更高
       └─ NO  → FRAMESIZE_VGA  (640x480) 或更低
```

---

## 五、从机 (Slave) 详细说明

### 5.1 从机架构概览

从机是专责传感器采集的轻量级节点，主要职责：

1. **传感器数据采集**：DHT11温湿度、MQ-2气体、水位传感器
2. **TCP数据传输**：通过WiFi连接主机8888端口，定时发送数据
3. **本地OLED显示**：实时显示采集的传感器数据
4. **系统配置管理**：通过config.h管理时间间隔等参数

### 5.2 config.h — 系统配置文件（v1.0优化）

[config.h](slave/main/config.h) 定义了从机的各项时间间隔参数：

```cpp
// config.h - 系统配置参数
#ifndef CONFIG_H
#define CONFIG_H

// ========== 传感器采集配置 ==========
#define SENSOR_READ_INTERVAL_MS     1000    // 传感器采集周期 (1秒)
#define DATA_SEND_INTERVAL_MS       1000    // TCP数据发送周期 (1秒)

// ========== OLED显示配置 ==========
#define OLED_UPDATE_INTERVAL_MS     500     // OLED刷新周期 (500ms)

// ========== 网络连接配置 ==========
#define WIFI_RECONNECT_INTERVAL_MS  5000    // WiFi重连间隔 (5秒)
#define TCP_RECONNECT_INTERVAL_MS   3000    // TCP重连间隔 (3秒)
#define MAX_RECONNECT_ATTEMPTS      10      // 最大重连尝试次数

// ========== 调试配置 ==========
#define SERIAL_BAUD_RATE            115200  // 串口波特率
#define DEBUG_MODE                  true    // 调试模式开关

#endif // CONFIG_H
```

#### 参数优化说明（v1.0）

| 参数 | 当前值 | 说明 | 优化建议 |
|------|--------|------|----------|
| `SENSOR_READ_INTERVAL_MS` | 1000ms | 传感器采集周期 | ✅ 合适，平衡精度和性能 |
| `DATA_SEND_INTERVAL_MS` | 1000ms | TCP数据发送周期 | ✅ 与采集周期一致，避免数据堆积 |
| `OLED_UPDATE_INTERVAL_MS` | 500ms | OLED刷新周期 | ✅ 平衡流畅度和资源占用 |
| `WIFI_RECONNECT_INTERVAL_MS` | 5000ms | WiFi重连间隔 | ✅ 合理的退避策略 |
| `TCP_RECONNECT_INTERVAL_MS` | 3000ms | TCP重连间隔 | ✅ 快速恢复连接 |

### 5.3 main.ino — 从机主程序

[main.ino](slave/main/main.ino) 是从机的入口文件，包含初始化和主循环逻辑。

#### 初始化流程

```
setup()
  │
  ├─① Serial.begin(115200)              // 串口初始化
  │
  ├─② initSensors()                     // 传感器初始化(DHT11/MQ2/水位)
  │
  ├─③ initOLED()                        // OLED显示屏初始化
  │
  ├─④ connectWiFi()                     // 连接WiFi网络
  │
  └─⑤ 初始化全局变量                     // 数据缓冲区、状态标志等
```

#### 主循环任务

```
loop()
  │
  ├─ 传感器数据采集 (SENSOR_READ_INTERVAL_MS间隔)
  │     └─ readTemperatureHumidity()  // DHT11
  │     └─ readMQ2Sensor()            // MQ-2
  │     └─ readWaterLevel()           // 水位ADC
  │
  ├─ TCP数据发送 (DATA_SEND_INTERVAL_MS间隔)
  │     └─ 格式化数据: "temperature,humidity,smoke,gas,water\n"
  │     └─ 发送到主机:8888端口
  │
  ├─ OLED显示更新 (OLED_UPDATE_INTERVAL_MS间隔)
  │     └─ 显示本地传感器数据
  │
  └─ 网络状态检查
        ├─ WiFi断线重连 (WIFI_RECONNECT_INTERVAL_MS间隔)
        └─ TCP断线重连 (TCP_RECONNECT_INTERVAL_MS间隔)
```

---

## 六、编译与烧录指南

### 6.1 开发环境准备

**必需软件：**
- Arduino IDE 2.x 或更高版本
- ESP32 Board Package v3.0.0 或更高
- PlatformIO（可选，推荐用于高级开发）

**安装ESP32 Board Package：**
```
Arduino IDE → 文件 → 首选项 → 附加开发板管理器网址:
https://espressif.github.io/arduino-esp32/package_esp32_index.json

工具 → 开发板 → 开发板管理器 → 搜索 "esp32" → 安装 ESP32 by Espressif Systems
```

### 6.2 主机固件编译

```bash
# 步骤：
# 1. 打开 Arduino IDE
# 2. 文件 → 打开 → 选择 esp32_firmware/master/main/main.ino
# 3. 工具 → 开发板 → 选择 "ESP32S3 Dev Module"
# 4. 工具 → 分区方案 → 选择 "Huge App (3MB No OTA/1MB SPIFFS)"
# 5. 工具 → PSRAM → 选择 "OPI PSRAM"
# 6. 点击 "验证" 编译
# 7. 连接ESP32-S3开发板
# 8. 点击 "上传" 烧录
```

**关键配置项：**

| 配置项 | 推荐值 | 说明 |
|--------|--------|------|
| CPU Frequency | 240MHz | 最高性能 |
| Flash Mode | QIO | 快速闪存访问 |
| Flash Size | 16MB (8MB PSRAM) | 根据实际硬件选择 |
| Partition Scheme | Huge App (3MB No OTA) | 最大应用空间 |
| PSRAM | OPI PSRAM | 启用外部PSRAM |
| Upload Speed | 921600 | 高速上传 |

### 6.3 从机固件编译

```bash
# 步骤：
# 1. 打开 Arduino IDE
# 2. 文件 → 打开 → 选择 esp32_firmware/slave/main/main.ino
# 3. 工具 → 开发板 → 选择 "ESP32 Dev Module"
# 4. 工具 → 分区方案 → 选择 "Default 4MB with spiffs (1.2MB APP/190KB SPIFFS)"
# 5. 点击 "验证" 编译
# 6. 连接ESP32 DevKit开发板
# 7. 点击 "上传" 烧录
```

### 6.4 常见编译错误及解决

**错误1：PSRAM相关错误**
```
解决：确保选择了正确的分区方案和PSRAM设置（仅主机需要PSRAM）
```

**错误2：库依赖缺失**
```
解决：安装所需库：
- U8g2 (OLED驱动)
- ESP32Servo (舵机控制)
- DHT sensor library (温湿度传感器)
```

**错误3：内存不足**
```
解决：
1. 启用PSRAM（主机必须）
2. 减少全局变量
3. 使用ps_malloc()分配大缓冲区
```

---

## 七、网络配置

### 7.1 首次配置（AP模式）

1. 主机和从机上电后，会创建WiFi热点
2. 默认热点名称：`ESP32-xxxxxx`（xxxxxx为MAC后6位）
3. 手机/电脑连接该热点
4. 浏览器访问 `http://192.168.4.1`
5. 在配置页面输入目标WiFi的SSID和密码
6. 点击保存，设备自动重启并连接到指定网络

### 7.2 查看IP地址

**方法1：串口监视器**
- 波特率：115200
- 设备连接成功后会打印分配的IP地址

**方法2：OLED显示屏**
- 主机OLED会显示连接状态和IP地址
- 从机OLED显示本地传感器数据和连接状态

**方法3：路由器管理页面**
- 登录路由器管理界面
- 查看已连接设备列表
- 找到ESP32设备的IP地址

### 7.3 网络拓扑示例

```
                    ┌─────────────┐
                    │   路由器     │
                    │ 192.168.1.1 │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
     ┌────────▼────┐ ┌────▼─────┐ ┌────▼─────┐
     │  ESP32 主机  │ │ ESP32从机 │ │  手机/电脑 │
     │ 192.168.1.100│ │192.168.1.101│ │ 客户端    │
     └──────────────┘ └──────────┘ └──────────┘
          :80(HTTP)      TCP:8888→
```

---

## 八、调试与故障排除

### 8.1 串口调试输出

**启用详细日志：**
```cpp
// main.ino 中设置
#define DEBUG_MODE true  // 开启调试输出
```

**关键调试信息：**
- WiFi连接状态
- TCP连接/断开事件
- 传感器数据原始值
- HTTP请求处理日志
- 内存使用情况

### 8.2 常见问题

**Q1: 无法连接WiFi？**
```
A: 检查以下几点：
1. SSID和密码是否正确
2. 路由器是否开启MAC地址过滤
3. 是否在2.4GHz频段（ESP32不支持5GHz）
4. 距离路由器是否太远
```

**Q2: 从机无法连接主机TCP端口？**
```
A: 排查步骤：
1. 确认主机和从机在同一网段
2. 检查主机TCP服务器是否在8888端口监听
3. 查看从机串口输出确认连接尝试
4. 检查防火墙设置
5. 重启两个设备重新建立连接
```

**Q3: 摄像头无法初始化？**
```
A: 可能原因：
1. 摄像头模块供电不足（需独立5V供电）
2. I2C引脚接线错误（SDA/SCL）
3. 摄像头型号不匹配（需OV3660）
4. PSRAM未正确配置（主机必须启用）
```

**Q4: 蜂鸣器频率设置不生效？**
```
A: v1.0已修复此问题。确保：
1. HTTP请求中包含freq参数：/actuator?device=buzzer&action=on&freq=2000
2. 频率值在100-10000Hz范围内
3. web_server.cpp中的代码已更新为最新版本
4. 查看串口日志确认freq参数被正确解析
```

**Q5: 电机运转时操作舵机会断开连接？**
```
A: 这是电源供应不足导致的问题：
1. 电机和舵机同时工作电流较大
2. USB供电能力有限（通常500mA-1A）
解决方案：
- 使用独立外部电源（5V/2A以上）
- 在电源端并联1000μF电解电容
- 避免同时满负荷运行电机和舵机
```

### 8.3 性能监控

**查看系统资源：**
```
GET /status
```

返回信息包括：
- 运行时间（uptime）
- 空闲堆内存（free_heap）
- PSRAM使用情况
- WiFi信号强度
- 摄像头状态
- 从机连接状态

---

## 九、安全注意事项

1. **WiFi安全**：使用强密码，定期更换
2. **API Key保护**：不要将AI API Key硬编码在代码中
3. **物理安全**：设备放置在安全位置，防止未经授权访问
4. **网络安全**：建议在内网中使用，外网访问需配置VPN
5. **电源安全**：使用合格电源，避免过载，特别是同时使用多个外设时

---

## 十、更新日志

### v1.0 (2026-04-18)

**新增功能：**
- ✨ 蜂鸣器自定义频率控制（100-10000Hz范围）
- ✨ /actuator API新增freq参数支持
- ✨ buzzer_on_with_freq()函数实现
- ✨ 从机config.h配置文件优化
- ✨ 时间间隔参数文档化

**Bug修复：**
- 🐛 修复蜂鸣器频率参数解析问题
- 🐛 修复频率设置后不立即生效的问题
- 🐛 优化HTTP请求日志输出

**文档更新：**
- 📝 全面更新API接口文档
- 📝 新增蜂鸣器频率控制详细说明
- 📝 新增从机配置参数说明
- 📝 新增常见问题解答

### v4.1 (2026-04-17)

- ✨ 优化PSRAM内存管理
- ✨ 增强被动帧缓存架构
- 🐛 修复视频流并发冲突问题
- 📝 完善技术文档

### v4.0 (2026-04-16)

- 🎉 初始版本发布
- ✨ 实现主从分布式架构
- ✨ MJPEG视频流服务
- ✨ AI视觉识别功能
- ✨ 外设控制模块

---

## 十一、相关资源

- **主项目README**: [../README.md](../README.md)
- **微信小程序文档**: [../web_frontend/wechat_miniprogram/README.md](../web_frontend/wechat_miniprogram/README.md)
- **Web前端文档**: [../web_frontend/README.md](../web_frontend/README.md)
- **AI服务文档**: [../ai_service/README.md](../ai_service/README.md)

---

## 十二、致谢

- [Espressif Systems](https://www.espressif.com/) - ESP32芯片和开发工具
- [Arduino](https://www.arduino.cc/) - 开源电子原型平台
- [U8g2](https://github.com/olikraus/U8g2) - OLED显示库
- [ESP32Servo](https://github.com/madhephaestus/ESP32ServoDevice/) - ESP32舵机控制库

---

## 📚 子模块文档索引

| 模块 | 文档路径 | 核心内容 |
|------|----------|----------|
| **主机详细文档** | [master/README.md](./master/README.md) | 引脚分配/HTTP API/TCP协议/外设控制/自动控制/OLED显示 |
| **从机详细文档** | [slave/README.md](./slave/README.md) | 传感器驱动/TCP通信/MQTT/I2C/OneNet云平台 |

---

**🎯 版本：v1.0 | 最后更新：2026-04-19 | 状态：活跃开发中**
