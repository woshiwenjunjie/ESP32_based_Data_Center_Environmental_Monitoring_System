# ESP32 环境监测从机系统

## 📋 项目简介

本系统为ESP32从机设备，负责采集环境传感器数据并通过WiFi发送到主机，同时支持MQTT上传到云平台。

## 🎯 功能特性

- 📡 **多传感器采集**：温度、湿度、烟雾、气体浓度、水位
- 🖥️ **OLED实时显示**：本地显示传感器数据
- 📶 **双通信模式**：TCP直连主机 + MQTT上云
- 🔄 **自动重连**：网络断开自动恢复连接
- ⚠️ **报警功能**：烟雾检测自动报警

## 🔌 硬件接口图

```
                    ESP32 从机引脚分配
    ┌─────────────────────────────────────────────┐
    │                                             │
    │    ┌─────────────┐                         │
    │    │    DHT11    │                         │
    │    │  温湿度传感器 │                         │
    │    └──────┬──────┘                         │
    │           │ DATA ──────────────── GPIO 21   │
    │           │ VCC  ──────────────── 3.3V      │
    │           │ GND  ──────────────── GND       │
    │           │                                 │
    │    ┌──────┴──────┐                            │
    │    │   MQ-2      │                            │
    │    │  气体传感器  │                            │
    │    └──────┬──────┘                            │
    │           │ AO   ──────────────── GPIO 36(VN) │
    │           │ DO   ──────────────── GPIO 16   │
    │           │ VCC  ──────────────── 5V        │
    │           │ GND  ──────────────── GND       │
    │           │                                 │
    │    ┌──────┴──────┐                         │
    │    │  水位传感器  │                         │
    │    └──────┬──────┘                         │
    │           │ SIGNAL ────────────── GPIO 32   │
    │           │ VCC    ────────────── 3.3V/5V   │
    │           │ GND    ────────────── GND       │
    │           │                                 │
    │    ┌──────┴──────┐                         │
    │    │ OLED 128x64 │                         │
    │    │  I2C显示屏  │                         │
    │    └──────┬──────┘                         │
    │           │ SCL  ──────────────── GPIO 22   │
    │           │ SDA  ──────────────── GPIO 23   │
    │           │ VCC  ──────────────── 3.3V      │
    │           │ GND  ──────────────── GND       │
    │                                             │
    └─────────────────────────────────────────────┘
```

## 📍 引脚定义表

| 设备 | 信号 | ESP32引脚 | 说明 |
|------|------|-----------|------|
| **DHT11** | DATA | GPIO 21 | 温湿度传感器数据 |
| | VCC | 3.3V | 电源 |
| | GND | GND | 地线 |
| **MQ-2** | AO | GPIO 36 | 模拟输出（气体浓度） |
| | DO | GPIO 16 | 数字输出（烟雾报警） |
| | VCC | 5V | 电源 |
| | GND | GND | 地线 |
| **水位传感器** | SIGNAL | GPIO 32 | 模拟信号 |
| | VCC | 3.3V/5V | 电源 |
| | GND | GND | 地线 |
| **OLED** | SCL | GPIO 22 | I2C时钟 |
| | SDA | GPIO 23 | I2C数据 |
| | VCC | 3.3V | 电源 |
| | GND | GND | 地线 |

## 🔧 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                        从机系统架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │   传感器层    │───▶│   数据处理层  │───▶│   通信层     │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
│         │                   │                   │          │
│    ┌────┴────┐         ┌────┴────┐         ┌────┴────┐    │
│    │ DHT11   │         │ 数据解析 │         │ TCP客户端 │   │
│    │ MQ-2    │         │ 报警检测 │         │ MQTT客户端│   │
│    │ 水位    │         │ OLED显示 │         │          │   │
│    └─────────┘         └─────────┘         └─────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 📁 文件结构

```
slave/
├── main/
│   ├── main.ino          # 主程序入口
│   ├── config.h          # 配置文件（WiFi/MQTT参数）
│   ├── public.h          # 公共头文件
│   ├── sensor_data.h/cpp # 传感器数据定义
│   ├── mySensor.h/cpp    # 传感器驱动
│   ├── u8g2_oled.h/cpp   # OLED显示驱动
│   ├── myWIFI.h/cpp      # WiFi通信模块
│   └── mqtt_test.h/cpp   # MQTT客户端
└── README.md             # 本文件
```

## ⚙️ 配置说明

编辑 `config.h` 文件修改以下参数：

```cpp
// WiFi配置
#define WIFI_SSID      "你的WiFi名称"
#define WIFI_PASSWORD  "你的WiFi密码"

// 主机通信配置
#define HOST_IP        "192.168.x.x"  // 主机IP地址
#define TCP_PORT       8888            // 主机端口

// MQTT配置（OneNet平台）
#define MQTT_SERVER    "mqtts.heclouds.com"
#define PRODUCT_ID     "你的产品ID"
#define DEVICE_NAME    "你的设备名称"
#define DEVICE_SECRET  "你的设备密钥"
```

## 📡 TCP通信详解

### TCP/IP协议简介

**TCP**（Transmission Control Protocol，传输控制协议）是一种面向连接的、可靠的、基于字节流的传输层通信协议：

- **面向连接**：通信前需要建立连接（三次握手）
- **可靠传输**：通过确认应答、重传机制保证数据不丢失
- **流量控制**：通过滑动窗口机制控制发送速率
- **拥塞控制**：根据网络状况调整发送速率
- **全双工通信**：双方可同时发送和接收数据

### TCP客户端/服务器模型

```
┌─────────────────────────────────────────────────────────────┐
│                    TCP客户端-服务器通信模型                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   服务器端 (主机)              客户端 (从机)                  │
│   ┌──────────────┐            ┌──────────────┐              │
│   │ 创建socket   │            │ 创建socket   │              │
│   │ bind()       │            │              │              │
│   │ listen()     │            │              │              │
│   │ accept() ◄───┼────────────┼── connect()  │              │
│   │              │  三次握手   │              │              │
│   │ read() ◄─────┼────────────┼── write()    │              │
│   │ write() ─────┼────────────►── read()     │              │
│   │              │  数据传输   │              │              │
│   │ close() ─────┼────────────►── close()    │              │
│   │              │  四次挥手   │              │              │
│   └──────────────┘            └──────────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 本项目的TCP通信实现

从机作为**TCP客户端**，主动连接到主机的TCP服务器：

#### 连接管理流程

```cpp
// 状态机管理连接
enum ConnectionState {
  STATE_WIFI_DISCONNECTED,    // WiFi未连接
  STATE_WIFI_CONNECTED,       // WiFi已连接
  STATE_TCP_CONNECTING,       // 正在连接TCP
  STATE_TCP_CONNECTED,        // TCP已连接
  STATE_TCP_DISCONNECTED      // TCP断开
};

// 自动重连机制
if (!networkReady && millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {
    lastReconnectAttempt = millis();
    if (client.connect(HOST_IP, TCP_PORT)) {
        networkReady = true;
    }
}
```

#### 数据发送实现

```cpp
void sendLocalData(WiFiClient &cli) {
  // 构建CSV格式数据：温度,湿度,烟雾,气体,水位
  String msg = String(temperature) + "," +
               String(humidity) + "," +
               String(smokeStatus) + "," +
               String(gasValue) + "," +
               String(waterValue) + "\n";
  
  // 发送数据
  cli.print(msg);
}
```

#### 数据接收处理

```cpp
while (client.available()) {
  String data = client.readStringUntil('\n');
  parseRemoteData(data);  // 解析主机下发的指令
}
```

### TCP通信参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 协议类型 | TCP | 面向连接的可靠传输 |
| 从机角色 | Client | 主动连接服务器 |
| 目标地址 | HOST_IP | 主机IP地址 |
| 目标端口 | TCP_PORT | 主机监听端口 |
| 重连间隔 | 8000ms | 连接失败后的重试间隔 |
| 发送间隔 | 5000ms | 数据上报周期 |

## 📡 MQTT与OneNet云平台详解

### 什么是MQTT？

**MQTT**（Message Queuing Telemetry Transport，消息队列遥测传输）是一种轻量级的发布/订阅模式的消息传输协议，特别适用于物联网（IoT）场景：

- **轻量级**：协议头部仅2字节，适合带宽受限的网络
- **发布/订阅模式**：设备发布消息到主题，其他设备订阅主题接收消息
- **QoS机制**：支持3种服务质量等级（0-至多一次，1-至少一次，2-恰好一次）
- **遗嘱消息**：设备异常断开时自动发送通知
- **保留消息**：新订阅者立即收到最新状态

### MQTT发布/订阅模型

```
┌─────────────────────────────────────────────────────────────┐
│                     MQTT发布/订阅模型                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│     发布者                    代理(Broker)         订阅者     │
│   ┌─────────┐                ┌─────────┐         ┌─────────┐│
│   │ 传感器A │──publish──────▶│         │         │ 手机APP ││
│   │ temp:25 │   /home/temp   │  OneNet │         │         ││
│   └─────────┘                │  MQTT   │────────▶│ 显示25°C││
│                              │  Broker │ subscribe│         ││
│   ┌─────────┐                │         │/home/temp└─────────┘│
│   │ 传感器B │──publish──────▶│         │         ┌─────────┐│
│   │ humi:60 │   /home/humi   │         │────────▶│ Web页面 ││
│   └─────────┘                └─────────┘         └─────────┘│
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### OneNet平台简介

**OneNet**是中国移动推出的物联网开放平台，提供：

- **设备接入**：支持MQTT、HTTP、CoAP等多种协议
- **数据存储**：自动存储设备上报的历史数据
- **规则引擎**：数据触发告警、转发等操作
- **可视化**：提供数据图表、仪表盘展示
- **API接口**：支持应用开发调用设备数据

### OneJson协议

OneNet使用**OneJson**协议进行设备与平台的通信，这是一种基于JSON的数据格式规范：

#### 属性上报格式
```json
{
  "id": "123456789",
  "version": "1.0",
  "params": {
    "temperature": {
      "value": 25.5
    },
    "humidity": {
      "value": 60.0
    }
  }
}
```

#### 字段说明
| 字段 | 类型 | 说明 |
|------|------|------|
| id | string | 消息唯一标识，通常使用时间戳 |
| version | string | 协议版本，固定为"1.0" |
| params | object | 属性集合，包含多个属性对象 |
| propertyName | object | 属性对象，包含value字段 |
| value | any | 属性值，可以是数字、字符串、布尔值等 |

### MQTT连接参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 服务器地址 | mqtts.heclouds.com | OneNet MQTT服务器 |
| 端口 | 1883 | 非加密端口（8883为TLS加密端口） |
| Client ID | 设备名称 | 如：esp32_slave |
| Username | 产品ID | 如：YSfR5EtwDx |
| Password | 设备密钥 | 平台生成的鉴权令牌 |

### 主题（Topic）规范

#### 属性上报主题
```
$sys/{PRODUCT_ID}/{DEVICE_NAME}/thing/property/post
```
示例：
```
$sys/YSfR5EtwDx/esp32_slave/thing/property/post
```

#### 属性设置响应主题
```
$sys/{PRODUCT_ID}/{DEVICE_NAME}/thing/property/post_reply
```

## 🔌 I2C通信详解

### I2C协议简介

**I2C**（Inter-Integrated Circuit，集成电路总线）是一种串行通信总线，使用两根线进行通信：

- **SDA**（Serial Data）：串行数据线，双向传输数据
- **SCL**（Serial Clock）：串行时钟线，由主设备提供时钟信号

### I2C特点

- **两线制**：仅需SDA和SCL两根线
- **多主从架构**：支持多个主设备和多个从设备
- **地址寻址**：每个从设备有唯一的7位地址
- **应答机制**：每传输8位数据，接收方发送ACK/NACK
- **标准速率**：100Kbps（标准模式）、400Kbps（快速模式）

### I2C通信时序

```
┌─────────────────────────────────────────────────────────────┐
│                     I2C通信时序图                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ SCL  ─┐   ┌─┐   ┌─┐   ┌─┐   ┌─┐   ┌─┐   ┌─┐   ┌─┐         │
│       └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘           │
│            │   │   │   │   │   │   │   │                   │
│ SDA  ──────┼───┼───┼───┼───┼───┼───┼───┼───┼───           │
│       ┌────┘   │   │   │   │   │   │   │   └────┐        │
│       │Start   D7  D6  D5  D4  D3  D2  D1  D0  ACK│        │
│       │         │   │   │   │   │   │   │   │    │        │
│       │         └────────── 数据位 ──────────┘    │        │
│       │                                           │        │
│       └────────────── 起始条件 ───────────────────┘        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### OLED的I2C实现

本项目使用SSD1306 OLED显示屏，I2C地址为0x3C：

```cpp
// I2C引脚定义
#define SCL_slave1   22
#define SDA_slave1   23

// OLED对象初始化
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_slave1(
    U8G2_R0,              // 旋转角度
    U8X8_PIN_NONE,        // 无复位引脚
    SCL_slave1,           // I2C时钟线
    SDA_slave1            // I2C数据线
);

void MyU8g2Oled_init() {
  // 设置I2C地址（SSD1306默认0x3C）
  oled_slave1.setI2CAddress(0x3C << 1);
  oled_slave1.begin();
}
```

## 📊 传感器技术详解

### DHT11温湿度传感器

#### 工作原理
DHT11使用单总线协议，通过一根数据线传输40位数据：

```
数据格式：8bit湿度整数 + 8bit湿度小数 + 8bit温度整数 + 8bit温度小数 + 8bit校验和
```

#### 通信时序
```
┌─────────────────────────────────────────────────────────────┐
│                    DHT11通信时序                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ 主机启动信号：                                               │
│     ┌────┐                                                │
│     │ 低 │ 18ms                                            │
│ ────┘    └───────────────────────────────                 │
│                                                             │
│ DHT响应：                                                   │
│              ┌──┐        ┌────────────────                 │
│ ────────────┘  └─80us───┘ 80us                             │
│              低电平      高电平                              │
│                                                             │
│ 数据位'0'：                                                 │
│     ┌──┐                                                  │
│ ────┘  └────────────────────────────────                   │
│     50us低  26-28us高                                      │
│                                                             │
│ 数据位'1'：                                                 │
│     ┌────────┐                                            │
│ ────┘        └────────────────────────────                 │
│     50us低    70us高                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 代码实现
```cpp
void readmyDHT(float &temp, float &humi) {
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    
    if (isnan(temp) || isnan(humi)) {
        Serial.println("[从机传感器] ⚠️ DHT读取失败！");
        temp = 0.0;
        humi = 0.0;
    }
}
```

### MQ-2气体传感器

#### 工作原理
MQ-2是一种半导体气体传感器，使用二氧化锡（SnO2）气敏材料：

- **清洁空气中**：电导率低
- **存在可燃气体时**：电导率随气体浓度增加而增加
- **检测气体**：烟雾、液化气、丙烷、氢气、一氧化碳等

#### 引脚说明
| 引脚 | 功能 | 说明 |
|------|------|------|
| VCC | 电源 | 5V供电 |
| GND | 地线 | 接地 |
| AO | 模拟输出 | 0-5V电压，对应气体浓度 |
| DO | 数字输出 | 超过阈值时输出低电平 |

#### 代码实现
```cpp
void readMQ2(int &smoke, int &gas) {
    smoke = digitalRead(MQ2_DO);    // 读取烟雾报警状态
    gas = analogRead(MQ2_AO);       // 读取气体浓度（0-4095）
}
```

#### 注意事项
- 需要预热时间（建议30秒以上）
- 灵敏度可通过模块上的电位器调节
- 受温度和湿度影响，需要定期校准

### 水位传感器

#### 工作原理
使用电阻式水位检测原理：

- **无水时**：电阻最大，输出电压最低
- **有水时**：电阻随水位上升而减小，输出电压升高
- **输出范围**：0-3.3V（通过ADC转换为0-4095）

#### 代码实现
```cpp
void readwater(int &water) {
    water = analogRead(WATERPIN);  // 读取水位（0-4095）
}
```

### ADC（模数转换器）

ESP32内置12位ADC，分辨率4096（0-4095）：

```cpp
// 配置ADC衰减（输入电压范围）
analogSetPinAttenuation(MQ2_AO, ADC_11db);    // 0-3.3V
analogSetPinAttenuation(WATERPIN, ADC_11db);  // 0-3.3V

// 读取ADC值
int adcValue = analogRead(pin);  // 返回0-4095

// 转换为电压
float voltage = adcValue * 3.3 / 4095.0;
```

#### ADC衰减模式
| 模式 | 输入范围 | 适用场景 |
|------|----------|----------|
| ADC_0db | 0-1.1V | 小信号精确测量 |
| ADC_2_5db | 0-1.5V | 一般信号 |
| ADC_6db | 0-2.2V | 较大信号 |
| ADC_11db | 0-3.3V | 标准3.3V信号 |

## 💻 程序实现详解

### TCP通信模块

#### 架构图
```
┌─────────────────────────────────────────────┐
│           TCP通信模块 (myWIFI.cpp)            │
├─────────────────────────────────────────────┤
│                                             │
│  ┌──────────────────────────────────────┐  │
│  │         WiFi连接管理                  │  │
│  │  WiFi.begin() → 检查WiFi.status()   │  │
│  └──────────────────┬───────────────────┘  │
│                     │                       │
│  ┌──────────────────▼───────────────────┐  │
│  │         TCP连接管理                   │  │
│  │  client.connect() → 定时重连机制     │  │
│  └──────────────────┬───────────────────┘  │
│                     │                       │
│  ┌──────────────────▼───────────────────┐  │
│  │         数据收发处理                  │  │
│  │  sendLocalData() / parseRemoteData() │  │
│  └──────────────────────────────────────┘  │
│                                             │
└─────────────────────────────────────────────┘
```

#### 核心代码
```cpp
void handleWiFiCommunication() {
  // 1. 检查WiFi状态
  bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
  
  // 2. 处理TCP连接
  if (wifiConnected) {
    if (!networkReady) {
      // 尝试连接主机
      if (client.connect(HOST_IP, TCP_PORT)) {
        networkReady = true;
      }
    } else {
      // 发送数据
      if (millis() - lastSendTime >= SEND_INTERVAL) {
        sendLocalData(client);
      }
      
      // 接收数据
      while (client.available()) {
        String data = client.readStringUntil('\n');
        parseRemoteData(data);
      }
    }
  }
}
```

### MQTT模块架构

```
┌─────────────────────────────────────────────┐
│              MQTT模块 (mqtt_test.cpp)         │
├─────────────────────────────────────────────┤
│                                             │
│  ┌─────────────┐      ┌─────────────────┐  │
│  │  WiFi连接   │─────▶│  MQTT连接管理    │  │
│  │ connectWiFi │      │ connectMQTT     │  │
│  └─────────────┘      └────────┬────────┘  │
│                                │           │
│  ┌─────────────────────────────┴────────┐ │
│  │         数据上报流程                  │ │
│  │  buildJsonDoc() → serializeJson()   │ │
│  │       ↓                             │ │
│  │  mqttClient.publish()               │ │
│  └──────────────────────────────────────┘ │
│                                             │
│  ┌──────────────────────────────────────┐ │
│  │         主循环处理                    │ │
│  │  loopMQTT(): 10秒检查连接            │ │
│  │  5秒上报周期 → publishAllSensorData() │ │
│  └──────────────────────────────────────┘ │
│                                             │
└─────────────────────────────────────────────┘
```

### 传感器模块

```cpp
void Sensor_init() {
    dht.begin();  // 初始化DHT传感器
    pinMode(MQ2_DO, INPUT);  // 配置MQ2数字引脚
    
    // 配置ADC衰减
    analogSetPinAttenuation(MQ2_AO, ADC_11db);
    analogSetPinAttenuation(WATERPIN, ADC_11db);
}

void readSensors() {
    readmyDHT(temperature, humidity);
    readMQ2(smokeStatus, gasValue);
    readwater(waterValue);
}
```

### 核心代码实现

#### 1. MQTT客户端初始化

```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setupMQTT() {
  // 设置MQTT服务器地址和端口
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  
  // 连接WiFi
  connectWiFi();
  
  // 连接MQTT服务器
  connectMQTT();
}
```

#### 2. WiFi连接

```cpp
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi已连接, IP:%s\n", 
                  WiFi.localIP().toString().c_str());
  }
}
```

#### 3. MQTT连接

```cpp
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("连接OneNET...");
    
    // 使用设备信息进行鉴权
    String clientId = DEVICE_NAME;
    String username = PRODUCT_ID;
    String password = DEVICE_SECRET;
    
    if (mqttClient.connect(clientId.c_str(), 
                           username.c_str(), 
                           password.c_str())) {
      Serial.println("成功");
    } else {
      Serial.printf("失败, 状态码:%d\n", mqttClient.state());
      delay(5000);
    }
  }
}
```

#### 4. 构建OneJson数据包

```cpp
void publishAllSensorData() {
  // 创建JSON文档，512字节容量
  StaticJsonDocument<512> doc;
  
  // 消息头和版本
  doc["id"] = String(millis());
  doc["version"] = "1.0";
  
  // 创建params对象
  JsonObject params = doc.createNestedObject("params");
  
  // 添加温度属性
  JsonObject tempObj = params.createNestedObject("temperature");
  tempObj["value"] = round(temperature * 10) / 10.0;
  
  // 添加湿度属性
  JsonObject humObj = params.createNestedObject("humidity");
  humObj["value"] = round(humidity * 10) / 10.0;
  
  // 添加烟雾状态
  JsonObject smokeObj = params.createNestedObject("smoke_status");
  smokeObj["value"] = (smokeStatus == HIGH) ? 1 : 0;
  
  // 添加气体浓度
  JsonObject gasObj = params.createNestedObject("gas_value");
  gasObj["value"] = gasValue;
  
  // 添加水位
  JsonObject waterObj = params.createNestedObject("water_level");
  waterObj["value"] = waterValue;

  // 序列化为JSON字符串
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  // 构建主题
  String topic = "$sys/" + String(PRODUCT_ID) + "/" + 
                 String(DEVICE_NAME) + "/thing/property/post";

  // 发布消息
  if (mqttClient.publish(topic.c_str(), jsonBuffer)) {
    Serial.println("[MQTT] 上报成功");
  }
}
```

#### 5. 主循环处理

```cpp
void loopMQTT() {
  static unsigned long lastPublishTime = 0;
  static unsigned long lastReconnectTime = 0;
  
  // 每10秒检查一次连接状态
  if (millis() - lastReconnectTime >= 10000) {
    lastReconnectTime = millis();
    if (!mqttClient.connected()) {
      connectMQTT();  // 断线重连
    }
  }
  
  // 每5秒上报一次数据
  if (millis() - lastPublishTime >= SEND_INTERVAL && 
      mqttClient.connected()) {
    lastPublishTime = millis();
    publishAllSensorData();
  }
  
  // 处理MQTT消息（必须定期调用）
  mqttClient.loop();
}
```

### 上报数据示例

实际发送到OneNet的JSON数据：

```json
{
  "id": "12345678",
  "version": "1.0",
  "params": {
    "temperature": {
      "value": 25.5
    },
    "humidity": {
      "value": 60.0
    },
    "smoke_status": {
      "value": 1
    },
    "gas_value": {
      "value": 1500
    },
    "water_level": {
      "value": 2000
    }
  }
}
```

### 数据流向图

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  ESP32   │────▶│   WiFi   │────▶│  OneNet  │────▶│  云平台  │
│  从机    │     │  路由器   │     │  MQTT    │     │  数据库  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
     │                                 │
     │ 1. 采集传感器数据                │ 3. 存储历史数据
     │ 2. 打包为OneJson格式            │ 4. 触发规则引擎
     │ 3. 通过MQTT发布                 │ 5. 可视化展示
     │                                 │
     ▼                                 ▼
温度: 25.5°C                      Web仪表盘
湿度: 60.0%                       手机APP
烟雾: 正常                         API接口
气体: 1500
水位: 2000
```

## � 数据格式

### 发送到主机的数据格式（TCP）
```
temperature,humidity,smoke,gas,water\n
```
示例：
```
25.5,60.0,1,1500,2000\n
```

### 字段说明
| 字段 | 类型 | 说明 |
|------|------|------|
| temperature | float | 温度（°C） |
| humidity | float | 湿度（%） |
| smoke | int | 烟雾状态（1=正常，0=报警） |
| gas | int | 气体浓度（ADC值 0-4095） |
| water | int | 水位（ADC值 0-4095） |

## 🖥️ OLED显示说明

从机OLED显示两种界面，每5秒自动切换：

### 主界面 - 传感器数据
```
┌─────────────────┐
│   Sensor Data   │
├─────────────────┤
│ Temp: 25.5 C    │
│ Humi: 60.0 %    │
│ Smoke: OK       │
│ Gas: [████░░░░] │
└─────────────────┘
```

### 详情界面 - 气体和水位
```
┌─────────────────┐
│   Detail View   │
├─────────────────┤
│ Gas: [████░░]50%│
│ Water:[██░░░░]30%│
│ ADC: G=2000     │
│      W=1500     │
└─────────────────┘
```

## 🚀 启动流程

```
上电启动
    │
    ▼
初始化OLED ───────────▶ 显示"System Ready"
    │
    ▼
初始化传感器 ─────────▶ DHT11, MQ-2, 水位
    │
    ▼
初始化WiFi ───────────▶ 连接WiFi网络
    │
    ▼
初始化MQTT ───────────▶ 连接OneNet平台
    │
    ▼
连接主机 ─────────────▶ TCP连接到主机
    │
    ▼
进入主循环 ───────────▶ 采集数据 → 显示 → 发送
```

## ⚠️ 报警功能

- **烟雾报警**：MQ-2数字输出检测到烟雾时，串口输出报警信息
- **气体报警**：气体浓度超过2500时，串口输出警告

## 🔧 故障排查

### OLED不显示
1. 检查I2C接线（SCL-GPIO22, SDA-GPIO23）
2. 检查OLED地址是否为0x3C
3. 检查电源电压是否为3.3V

### 无法连接主机
1. 检查WiFi配置是否正确
2. 检查主机IP地址和端口
3. 确认主机已启动并监听端口

### MQTT连接失败
1. 检查OneNet产品ID、设备名称、设备密钥是否正确
2. 检查WiFi是否已连接
3. 查看串口输出的错误状态码：
   - `-4` = MQTT_CONNECTION_TIMEOUT
   - `-2` = MQTT_CONNECT_FAILED
   - `-1` = MQTT_DISCONNECTED

### 传感器数据异常
1. 检查传感器电源连接
2. 检查信号线是否接错引脚
3. 检查DHT11/MQ-2是否需要预热

## 📦 依赖库

- `WiFi` - ESP32 WiFi库
- `DHT sensor library` - DHT传感器驱动
- `Adafruit Unified Sensor` - 统一传感器接口
- `U8g2` - OLED显示库
- `PubSubClient` - MQTT客户端库
- `ArduinoJson` - JSON处理库

## 📝 版本历史

- v1.0 (2026-04-18) - 初始版本，基础传感器采集和显示功能
- v1.1 (2026-04-19) - 添加MQTT上云功能，支持OneNet平台

## 👨‍💻 作者

一个普通人

## 📄 许可证

MIT License
