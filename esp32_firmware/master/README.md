# ESP32-S3 主控系统 - 环境监测站主机

## 📋 项目简介

本系统为ESP32-S3主控节点，集成**OV3660摄像头**、**HTTP Web服务器**、**TCP服务器**，负责：
- 📹 **MJPEG视频流监控**
- 📸 **高清拍照捕获**
- 🤖 **AI视觉识别**（支持10+大模型）
- 📊 **传感器数据融合展示**
- 🎛️ **外设远程控制**
- 🔗 **从机数据汇聚**

| 属性 | 说明 |
|------|------|
| 版本 | v1.0 (自动控制 + 频率可调) |
| 芯片 | ESP32-S3-WROOM (8MB PSRAM) |
| 摄像头 | OV3660 (200万像素) |
| 通信端口 | HTTP:80 / TCP:8888 |
| 更新日期 | 2026-04-19 |

---

## 🔌 硬件接口图

```
                    ESP32-S3 主机引脚分配
    ┌─────────────────────────────────────────────────────┐
    │                                                     │
    │    ┌──────────────┐                                 │
    │    │   OV3660     │  200万像素摄像头                 │
    │    │   摄像头模块  │                                 │
    │    └──────┬───────┘                                 │
    │           │                                          │
    │     XCLK ─── GPIO 15   时钟信号                     │
    │     SIOD ─── GPIO 4    I2C数据                      │
    │     SIOC ─── GPIO 5    I2C时钟                      │
    │     VSYNC── GPIO 6    垂直同步                      │
    │     HREF ─── GPIO 7    行参考                       │
    │     PCLK ─── GPIO 13   像素时钟                      │
    │     Y2~Y9 ─ GPIO 8-12,16-18 数据线                │
    │           │                                          │
    │    ┌──────┴──────┐                                 │
    │    │ 外设控制模块  │                                 │
    │    └──────┬───────┘                                 │
    │           │                                          │
    │     继电器── GPIO 45   高电平触发                   │
    │     蜂鸣器── GPIO 40   无源蜂鸣器(LEDC PWM)         │
    │     电机  ── GPIO 19   直流电机(LEDC PWM)          │
    │     舵机  ── GPIO 20   SG90舵机(PWM)               │
    │           │                                          │
    │    ┌──────┴──────┐                                 │
    │    │ OLED 显示屏  │  I2C接口                        │
    │    └──────┬───────┘                                 │
    │           │                                          │
    │     SCL  ── GPIO 21   I2C时钟                       │
    │     SDA  ── GPIO 47   I2C数据                       │
    │     VCC  ── 3.3V                                    │
    │     GND  ── GND                                     │
    │                                                     │
    │    [本地传感器] 可选连接                             │
    │     DHT11 DATA ── GPIO 14                           │
    │     MQ-2 AO   ── GPIO 2(VN)                         │
    │     MQ-2 DO   ── GPIO 39                            │
    │     水位      ── GPIO 38                            │
    │                                                     │
    └─────────────────────────────────────────────────────┘
```

## 📍 引脚定义表

### OV3660摄像头引脚

| 功能 | GPIO | 说明 |
|------|------|------|
| XCLK | 15 | 外部时钟输入（24MHz） |
| SIOD | 4 | SCCB I2C数据线 |
| SIOC | 5 | SCCB I2C时钟线 |
| VSYNC | 6 | 垂直同步信号 |
| HREF | 7 | 行参考信号 |
| PCLK | 13 | 像素时钟输出 |
| Y2-Y9 | 8-12,16-18 | 8位像素数据总线 |

### 外设控制引脚

| 设备 | GPIO | 类型 | 说明 |
|------|------|------|------|
| **继电器** | 45 | 数字输出 | 高电平触发 |
| **蜂鸣器** | 40 | LEDC PWM | 100-10000Hz频率可控 |
| **电机** | 19 | LEDC PWM | 速度控制(0-255) |
| **舵机** | 20 | Servo PWM | 角度控制(0-180°) |

### OLED显示屏引脚

| 信号 | GPIO | 说明 |
|------|------|------|
| SCL | 21 | I2C时钟线 |
| SDA | 47 | I2C数据线 |
| VCC | 3.3V | 电源 |
| GND | GND | 地线 |
| 地址 | 0x3C | SSD1306默认地址 |

### 本地传感器引脚（可选）

| 传感器 | GPIO | 说明 |
|--------|------|------|
| DHT11 DATA | 14 | 温湿度传感器 |
| MQ-2 AO | 2 (VN) | 气体浓度模拟值 |
| MQ-2 DO | 39 | 烟雾报警数字值 |
| 水位 | 38 | 水位模拟值 |

---

## 🔧 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     ESP32-S3 主控系统架构                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────────────┐  │
│  │  OV3660     │   │  本地传感器  │   │  外设控制模块        │  │
│  │  摄像头     │   │  (可选)     │   │  继电器/蜂鸣器/      │  │
│  │             │   │  DHT/MQ2/水位│   │  电机/舵机          │  │
│  └──────┬──────┘   └──────┬──────┘   └──────────┬──────────┘  │
│         │                  │                      │            │
│         ▼                  ▼                      ▼            │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │              web_server.cpp (HTTP Server :80)           │  │
│  │                                                         │  │
│  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌─────┐ │  │
│  │  │/stream│ │/capture│ │/sensors│ │ /ask │ │/control│ │/status│ │  │
│  │  │MJPEG流│ │JPEG照片│ │传感器  │ │AI识别│ │外设控制│ │系统状态│ │  │
│  │  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └─────┘ │  │
│  └─────────────────────────────────────────────────────────┘  │
│                              │                                │
│  ┌───────────────────────────┴─────────────────────────────┐  │
│  │              tcp_server.cpp (TCP Server :8888)          │  │
│  │           接收从机数据 → 解析 → 存入 remoteData         │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │              auto_control.cpp (自动控制引擎)            │  │
│  │         传感器阈值 → 条件判断 → 触发外设动作            │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ⚡ 内存管理: Internal SRAM (~300KB) + PSRAM (8MB)            │
│  🖥️ OLED显示: 远程传感器数据 + 连接状态                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## 📁 文件结构

```
master/main/
├── main.ino              # 主程序入口 & 初始化流程
├── public.h              # 公共宏定义与类型
├── web_server.cpp/h      # HTTP服务器核心（视频流/API）
├── myOV3660.cpp/h        # OV3660摄像头驱动
├── actuators.cpp/h       # 外设控制模块（继电器/蜂鸣器/电机/舵机）
├── sensors.cpp/h         # 传感器数据采集与融合
├── sensor_data.cpp/h     # 全局数据定义（本地+远程）
├── myWIFI.cpp/h          # TCP服务器（接收从机数据）
├── wifi_config.cpp/h     # WiFi配置管理(SPIFFS)
├── u8g2_oled.cpp/h       # OLED显示屏驱动
├── mySensor.cpp/h        # 本地传感器驱动
└── auto_control.cpp/h    # 自动控制规则引擎
```

## ⚙️ 配置说明

### 编译选项 (public.h)

```cpp
// 功能开关（禁用可节省空间）
#define DEBUG_ENABLE 1           // 调试输出（~20KB）
#define ENABLE_WEBSOCKET 1       // WebSocket支持（~8KB）
#define ENABLE_AUTO_CONTROL 1    // 自动控制功能（~15KB）
#define ENABLE_YOLO_DETECT 1     // YOLO目标检测（~3KB）
```

### 内存优化策略

- **PSRAM优先**: >4KB的缓冲区自动使用PSRAM
- **被动帧缓存**: 视频流持续更新共享缓冲区，拍照直接读取
- **互斥锁保护**: Mutex确保线程安全

---

## 📡 HTTP API 接口文档

### API端点列表

| 方法 | 路径 | 功能 | Content-Type |
|------|------|------|--------------|
| GET | `/` | 主页HTML | text/html |
| GET | `/stream` | MJPEG视频流 | multipart/x-mixed-replace |
| GET | `/capture` | JPEG照片捕获 | image/jpeg |
| POST | `/ask` | AI视觉识别 | application/json |
| GET | `/sensors` | 传感器数据JSON | application/json |
| GET | `/status` | 系统状态JSON | application/json |
| POST | `/control` | 外设控制 | application/json |
| POST | `/config` | 配置更新 | application/json |

### 1. MJPEG视频流 `GET /stream`

**响应格式**:
```
Content-Type: multipart/x-mixed-replace;boundary=123456789000000000000987654321

--123456789000000000000987654321
Content-Type: image/jpeg
Content-Length: <size>

<JPEG binary data>
--123456789000000000000987654321
...
```

**特性**:
- 支持最多2个客户端并发
- 自动重连机制
- 分辨率：VGA(640x480) / SVGA(800x600) / QVGA(320x240)

### 2. 拍照捕获 `GET /capture`

**请求参数**:
| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| quality | int | 10 | JPEG质量(10-63) |
| framesize | string | VGA | 分辨率(VGA/SVGA/QVGA) |

**响应示例**:
```json
{
  "success": true,
  "width": 640,
  "height": 480,
  "quality": 10,
  "size": 25680,
  "capture_time_ms": 120,
  "timestamp": 1713545200000
}
```

### 3. AI视觉识别 `POST /ask`

**请求体**:
```json
{
  "image": "<base64_encoded_jpeg>",
  "question": "描述这张图片",
  "mode": "llm",           // llm/yolo/local
  "model": "gpt-4o-mini"   // 可选，指定AI模型
}
```

**响应示例**:
```json
{
  "success": true,
  "mode": "llm",
  "model": "gpt-4o-mini",
  "result": "图片中显示一个温度计读数为25°C...",
  "process_time_ms": 1500
}
```

**支持的AI服务商**:
| 服务商 | 模型 | API地址 |
|--------|------|---------|
| OpenAI | GPT-4o/GPT-4o-Mini | api.openai.com |
| 阿里云 | 通义千问 Qwen-Turbo/Plus/Max | dashscope.aliyuncs.com |
| 月之暗面 | Kimi | api.moonshot.cn |
| DeepSeek | Chat/Reasoner | api.deepseek.com |
| 火山引擎 | 豆包 Pro/Lite | ark.cn-beijing.volces.com |

### 4. 传感器数据 `GET /sensors`

**响应示例**:
```json
{
  "local": {
    "temperature": 25.5,
    "humidity": 60.0,
    "smoke": 1,
    "gas": 1200,
    "water": 800
  },
  "remote": {
    "temperature": 26.2,
    "humidity": 58.5,
    "smoke": 1,
    "gas": 1350,
    "water": 750,
    "online": true,
    "last_update": 1713545200000
  },
  "slave_connected": true
}
```

### 5. 系统状态 `GET /status`

**响应示例**:
```json
{
  "uptime_seconds": 3600,
  "wifi_rssi": -45,
  "wifi_ssid": "MyWiFi",
  "ip_address": "192.168.1.100",
  "free_heap": 150000,
  "free_psram": 7000000,
  "psram_found": true,
  "min_free_heap": 30000,
  "cpu_freq_mhz": 240,
  "flash_size_mb": 16,
  "sdk_version": "v4.4.5"
}
```

### 6. 外设控制 `POST /control`

**请求体**:
```json
{
  "device": "relay",        // relay/buzzer/motor/servo
  "action": "on",           // 具体动作
  "value": null             // 可选参数
}
```

#### 继电器控制
| action | 说明 |
|--------|------|
| on | 开启继电器 |
| off | 关闭继电器 |
| toggle | 切换状态 |

**请求示例**:
```json
{"device": "relay", "action": "on"}
```

#### 蜂鸣器控制
| action | value | 说明 |
|--------|-------|------|
| on | - | 开启（默认2kHz） |
| off | - | 关闭 |
| beep | {"freq":2000,"duration":500} | 单次鸣叫 |

**请求示例**:
```json
{"device": "buzzer", "action": "on", "value": {"freq": 2500}}
```

#### 电机控制
| action | value | 说明 |
|--------|-------|------|
| stop | - | 停止电机 |
| run | 0-255 或 0-100 | 设置速度 |

**请求示例**:
```json
{"device": "motor", "action": "run", "value": 50}
```

#### 舵机控制
| action | value | 说明 |
|--------|-------|------|
| write | 0-180 | 设置角度 |

**请求示例**:
```json
{"device": "servo", "action": "write", "value": 90}
```

**响应示例**:
```json
{
  "success": true,
  "device": "relay",
  "state": "ON"
}
```

### 7. 配置更新 `POST /config`

**请求体**:
```json
{
  "ai_service_url": "https://api.openai.com/v1/chat/completions",
  "ai_api_key": "sk-xxx",
  "ai_model": "gpt-4o-mini",
  "default_framesize": "VGA",
  "default_quality": 10
}
```

---

## 🔗 TCP服务器协议详解

### 通信角色

主机作为 **TCP Server**，监听端口 **8888**，等待从机连接：

```
从机(TCP Client) ──TCP:8888──▶ 主机(TCP Server)
```

### 数据格式

从机发送的数据格式（CSV）：
```
temperature,humidity,smoke,gas,water\n
```

示例：
```
25.5,60.0,1,1500,2000\n
```

### 字段定义

| 字段 | 类型 | 范围 | 说明 |
|------|------|------|------|
| temperature | float | -40~125°C | 温度 |
| humidity | float | 0~100% | 湿度 |
| smoke | int | 0或1 | 烟雾状态(1=正常,0=报警) |
| gas | int | 0~4095 | 气体浓度(ADC值) |
| water | int | 0~4095 | 水位(ADC值) |

### 连接管理

```cpp
// TCP配置
const uint16_t TCP_PORT = 8888;              // 监听端口
const unsigned long SEND_INTERVAL = 1500;     // 发送间隔(ms)
const long RECONNECT_INTERVAL = 5000;         // 重连间隔(ms)
const unsigned long REMOTE_DATA_TIMEOUT = 5000;// 数据超时(ms)
```

### 工作流程

```
1. 主机启动TCP服务器，监听8888端口
2. 从机连接后发送传感器数据
3. 主机解析数据并存储到remoteData变量
4. 通过HTTP /sensors API提供给客户端
5. 断连后自动等待重连
```

---

## 🎛️ 外设控制接口

### 继电器控制

```cpp
#include "actuators.h"

actuators_init();      // 初始化
relay_on();            // 开启
relay_off();           // 关闭
relay_toggle();        // 切换
bool state = relay_get_state();  // 获取状态
```

### 蜂鸣器控制（频率可调）

```cpp
buzzer_on();                          // 默认2kHz
buzzer_on_with_freq(2500);            // 指定频率(Hz)
buzzer_beep(2000, 500);              // 单次鸣叫(阻塞)
buzzer_beep_async(2000, 500);        // 非阻塞鸣叫
buzzer_off();                         // 关闭
// 频率范围: 100Hz ~ 10000Hz
```

### 电机PWM控制

```cpp
motor_stop();                          // 停止
motor_run(128);                       // 速度值(0-255)
motor_run_percent(50);                 // 百分比(0-100%)
int speed = motor_get_speed();         // 获取当前速度
```

### 舵机角度控制

```cpp
servo_attach();                        // 附加到引脚
servo_write_angle(90);                 // 设置角度(0-180°)
servo_detach();                        // 分离释放资源
int angle = servo_get_angle();          // 获取角度
```

### LEDC PWM配置

| 外设 | 通道 | 默认频率 | 分辨率 |
|------|------|----------|--------|
| 蜂鸣器 | Channel 2 | 2000 Hz | 8-bit (0-255) |
| 电机 | Channel 3 | 5000 Hz | 8-bit (0-255) |

---

## ⚙️ 自动控制规则引擎

### 功能概述

根据传感器数据**自动触发**外设动作，无需人工干预。

### 支持的传感器类型

| 枚举值 | 名称 | 数据类型 | 说明 |
|--------|------|----------|------|
| SENSOR_TEMPERATURE | 温度 | float (°C) | 本地或远程温度 |
| SENSOR_HUMIDITY | 湿度 | float (%) | 本地或远程湿度 |
| SENSOR_GAS | 气体浓度 | int (ADC) | MQ-2原始值 |
| SENSOR_SMOKE | 烟雾状态 | int (0/1) | 0=有烟雾 |
| SENSOR_WATER | 水位 | int (ADC) | 水位传感器值 |

### 触发条件（7种）

| 条件 | 说明 | 参数 |
|------|------|------|
| GREATER_THAN | 大于阈值 | threshold1 |
| LESS_THAN | 小于阈值 | threshold1 |
| EQUAL_TO | 等于阈值 | threshold1 |
| IN_RANGE | 在范围内 | threshold1 ~ threshold2 |
| OUT_OF_RANGE | 超出范围 | threshold1 ~ threshold2 |
| CHANGED | 数值变化 | 任意变化 |

### 动作类型（11种）

| 动作 | 说明 | 参数 |
|------|------|------|
| ACTION_RELAY_ON | 继电器开启 | - |
| ACTION_RELAY_OFF | 继电器关闭 | - |
| ACTION_RELAY_TOGGLE | 继电器切换 | - |
| ACTION_BUZZER_ON | 蜂鸣器开启 | - |
| ACTION_BUZZER_OFF | 蜂鸣器关闭 | - |
| ACTION_BUZZER_BEEP | 蜂鸣器短鸣 | duration_ms |
| ACTION_MOTOR_RUN | 电机运行 | speed(0-255) |
| ACTION_MOTOR_STOP | 电机停止 | - |
| ACTION_SERVO_WRITE | 舵机设置 | angle(0-180) |

### 规则结构体

```cpp
typedef struct {
    uint8_t id;                    // 规则ID
    char name[4];                  // 规则名称
    SensorType sensorType;         // 传感器类型
    TriggerCondition condition;    // 触发条件
    float threshold1;              // 阈值1
    float threshold2;              // 阈值2（范围条件用）
    ActionType actionType;         // 动作类型
    int16_t actionParam;           // 动作参数
    uint16_t debounceMs;           // 去抖动时间(ms)
    uint16_t actionDurationMs;     // 动作持续时间(ms)
    bool enabled;                  // 是否启用
} AutoControlRule;
```

### 使用示例

```cpp
#include "auto_control.h"

// 1. 初始化
auto_control_init();

// 2. 创建规则：温度>35°C时开启继电器
AutoControlRule rule;
memset(&rule, 0, sizeof(rule));
rule.id = 1;
strncpy(rule.name, "T-HI", 4);
rule.sensorType = SENSOR_TEMPERATURE;
rule.condition = CONDITION_GREATER_THAN;
rule.threshold1 = 35.0;
rule.actionType = ACTION_RELAY_ON;
rule.debounceMs = 5000;      // 5秒去抖动
rule.enabled = true;

add_auto_control_rule(rule);

// 3. 在loop()中周期性调用
check_and_execute_auto_control();
```

### 预设默认规则

| 规则名 | 条件 | 动作 | 说明 |
|--------|------|------|------|
| T-HI | 温度 > 35°C | 蜂鸣器鸣叫 | 高温报警 |
| H-HI | 湿度 > 80% | 继电器开启 | 启动风扇通风 |
| G-HI | 气体 > 2500 | 蜂鸣器+继电器 | 气体泄漏告警 |

### 持久化存储

规则保存在 **SPIFFS** 文件系统中：

```cpp
save_rules_to_spiffs();           // 保存到文件
load_rules_from_spiffs();         // 从文件加载
has_saved_rules();                // 检查是否有保存的规则
```

---

## 📊 数据流向总览

```
┌──────────┐ 采集  ┌──────────┐  TCP:8888  ┌──────────┐
│ 从机传感器 │──────▶│ 从机TCP   │──────────▶│ 主机TCP   │
│ DHT/MQ2/水位│      │ Client   │           │ Server   │
└──────────┘      └──────────┘           └────┬─────┘
                                               │
                                               ▼
                                         ┌──────────┐
                                         │ 数据融合层 │
                                         │ remoteData│
                                         └────┬─────┘
                                              │
                    ┌─────────────────────────┼────────────────┐
                    │                         │                │
                    ▼                         ▼                ▼
            ┌──────────┐              ┌──────────┐    ┌──────────┐
            │ OLED显示  │              │HTTP API  │    │ 自动控制 │
            │ 远程数据  │              │ /sensors │    │ 规则引擎  │
            └──────────┘              └──────────┘    └────┬─────┘
                                                          │
                                                          ▼
                                                   ┌──────────┐
                                                   │ 外设执行  │
                                                   │继电器/蜂鸣│
                                                   │  电机/舵机│
                                                   └──────────┘
```

## 🚀 启动流程

```
上电启动
    │
    ▼
Serial.begin(115200) ───────────▶ 串口初始化
    │
    ▼
内存信息打印 ───────────────────▶ Heap/PSRAM状态
    │
    ▼
OLED初始化 ──────────────────────▶ 显示初始状态
    │
    ▼
WiFi连接 ───────────────────────▶ AP模式或STA模式
    │
    ▼
传感器初始化 ──────────────────▶ DHT/MQ2/水位(可选)
    │
    ▼
外设初始化 ─────────────────────▶ 继电器/蜂鸣器/电机/舵机
    │
    ▼
自动控制初始化 ─────────────────▶ 加载规则/SPIFFS
    │
    ▼
摄像头初始化(OV3660) ───────────▶ JPEG/VGA配置
    │
    ▼
TCP服务器启动(:8888) ───────────▶ 等待从机连接
    │
    ▼
HTTP服务器启动(:80) ───────────▶ 提供Web服务
    │
    ▼
进入主循环 ─────────────────────▶ 处理请求/检查规则
```

## 🖥️ OLED显示内容

主机OLED显示**远程从机的传感器数据**和**连接状态**：

```
┌─────────────────┐
│   Slave Data    │
├─────────────────┤
│ T: 26.2°C       │
│ H: 58.5%        │
│ S: ✓  G: 1350   │
│ W: 750          │
│ Status: Online  │
└─────────────────┘
```

**显示逻辑**：
- 从机在线时：显示实际数据
- 从机离线时：显示 `--.-` 和 `Offline`
- 每3秒强制刷新一次

## 🔧 故障排查

### 摄像头问题
1. 检查OV3660接线（特别是XCLK/SIOD/SIOC）
2. 确认供电稳定（建议独立3.3V电源）
3. 尝试降低分辨率或质量

### 视频流卡顿
1. 减少并发客户端数（最多2个）
2. 降低分辨率至QVGA
3. 检查WiFi信号强度

### AI识别失败
1. 检查API Key是否正确
2. 检查网络连通性
3. 确认AI服务URL可用

### 外设无反应
1. 检查GPIO引脚接线
2. 确认调用了 `actuators_init()`
3. 检查电源供应（外设可能需要额外供电）

### 从机无法连接
1. 确认主机和从机在同一网段
2. 检查防火墙设置
3. 确认TCP端口8888未被占用

## 📦 依赖库

- `esp_camera` - ESP32摄像头驱动
- `U8g2lib` - OLED显示库
- `ESP32Servo` - 舵机控制库
- `ArduinoJson` - JSON处理
- `WiFi` / `WebServer` - ESP32网络库

## 📝 版本历史

- v1.0 (2026-04-19) - 自动控制引擎 + 蜂鸣器频率可调 + PSRAM优化
- v4.0 (2026-04-16) - 外设控制模块重构
- v3.0 (2026-04-18) - PSRAM智能内存管理 + 被动帧缓存

## 👨‍💻 作者

一个普通人

## 📄 许可证

MIT License
