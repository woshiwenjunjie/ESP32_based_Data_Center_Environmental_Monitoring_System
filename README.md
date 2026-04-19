# ESP32-CAM 智能环境监测站 v1.0

> 基于 ESP32-S3 的**主从分布式**环境监测系统，支持多平台大模型 AI 视觉识别、外设远程控制

---

## 📋 项目概述

本项目是一个完整的 **IoT 智能环境监测站**，采用 **主从分布式架构**：以 **ESP32-S3-WROOM** 为主机节点（集成 OV3660 摄像头 + Web服务器），另一块 ESP32-S3 为从机节点（专责传感器采集），通过 **WiFi TCP协议** 实现数据传输。客户端支持 **Web浏览器** 和 **微信小程序** 双平台访问，具备实时视频流、拍照捕获、传感器数据可视化、**AI 大模型视觉识别** 和 **外设远程控制** 功能。

### 核心特性

| 特性 | 说明 |
|------|------|
| 📹 **MJPEG 视频流** | 支持最多 2 个客户端并发观看，被动帧缓存架构零冲突 |
| 📸 **高清拍照** | JPEG 直出，支持 VGA/SVGA/QVGA 动态分辨率切换 |
| 🤖 **AI 视觉识别** | 支持 10+ 国内外主流大模型（OpenAI / 通义千问 / Kimi / DeepSeek / 豆包）+ YOLO本地检测 + LLM多模态 |
| 📊 **数据可视化** | Web 端 Chart.js + 小程序原生 Canvas Catmull-Rom 样条插值双平台图表 |
| 🎛️ **外设控制** | 继电器/蜂鸣器/电机/舵机四路外设独立控制页面，支持精确数值输入和自定义频率设置 |
| 🔗 **主从分布式** | TCP 协议 8888 端点通信，主机汇聚从机传感器数据 |
| 💾 **PSRAM 智能内存管理** | 自动检测 PSRAM，大缓冲区优先使用外部内存（smart_malloc）|
| 📱 **视频流快照模式** | 微信小程序兼容方案，定时抓帧模拟实时画面，支持视频流/快照模式切换 |
| 🎵 **蜂鸣器频率控制** | 支持预设频率选择和自定义频率输入（100-10000Hz），实时同步到硬件 |
| ⚙️ **超限自动控制** | 传感器阈值自动触发外设（继电器/蜂鸣器/电机/舵机），支持5种触发条件 |

---

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           客户端层 (Clients)                                  │
├──────────────┬──────────────────────┬───────────────────────┬────────────────┤
│  🌐 Web浏览器 │   📱 微信小程序       │   📱 小程序控制页      │  🔧 开发调试工具  │
│  index.html  │   pages/index/       │   pages/control/      │  network_test   │
│  Chart.js    │   Canvas Catmull-Rom │   继电器/蜂鸣器        │  LLM验证+诊断   │
│  MJPEG原生流  │   快照模式模拟视频流   │   电机/舵机控制       │                 │
└──────┬───────┴──────────┬───────────┴───────────┬───────────┴────────┬───────┘
       │ HTTP/HTTPS       │ HTTP/HTTPS            │ HTTP/HTTPS         │ HTTP/HTTPS
       ▼                  ▼                       ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      主机层 (ESP32-S3 Master Edge)                          │
│                                                                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐    │
│  │ OV3660   │ │ DHT11/22 │ │ MQ-2     │ │ 水位传感  │ │ 外设控制模块     │    │
│  │ 摄像头    │ │ 温湿度(本 │ │ 气体检测  │ │ 器(本地)  │ │ 继电器/蜂鸣器    │    │
│  │          │ │ 地备选)   │ │ (本地备选)│ │ (本地备选)│ │ 电机PWM/舵机    │    │
│  └─────┬────┘ └─────┬────┘ └─────┬────┘ └─────┬────┘ └───────┬────────┘    │
│        │            │            │            │              │             │
│  ┌─────▼────────────▼────────────▼────────────▼──────────────▼────────┐    │
│  │              web_server.cpp (HTTP Server :80)                      │    │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌──────┐ │    │
│  │  │/stream │ │/capture│ │/sensors│ │ /ask   │ │/control│ │/status│ │    │
│  │  │MJPEG流 │ │JPEG照片│ │传感器  │ │AI识别  │ │外设控制 │ │状态   │ │    │
│  │  └────────┘ └────────┘ └────────┘ └────────┘ └────────┘ └──────┘ │    │
│  └───────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────┐     │
│  │           tcp_server (TCP Server :8888) - 主从通信                  │     │
│  │           接收从机传感器数据 → 解析 → 存入 remoteData               │     │
│  └───────────────────────────────────────────────────────────────────┘     │
│                                                                             │
│  ⚡ 内存: Internal SRAM (~300KB) + PSRAM (8MB)                             │
│  🔒 线程安全: Mutex + Atomic + Passive Frame Buffer                        │
│  🖥️ OLED: U8g2 驱动，显示远程传感器数据和连接状态                            │
└───────────────────────────────────────────────────┬───────────────────────┘
                                                    │ TCP :8888
                    ┌───────────────────────────────┘
                    │ 数据格式: "temperature,humidity,smoke,gas,water\n"
                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                      从机层 (ESP32-S3 Slave Sensor)                         │
│                                                                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐                      │
│  │ DHT11/22 │ │ MQ-2     │ │ 水位传感  │ │ 报警检测  │                      │
│  │ 温湿度   │ │ 气体/烟雾 │ │ 器ADC    │ │ 烟雾/气体 │                      │
│  └─────┬────┘ └─────┬────┘ └─────┬────┘ └─────┬────┘                      │
│        │            │            │            │                            │
│  ┌─────▼────────────▼────────────▼────────────▼────────────────────┐      │
│  │  sensor_data.cpp — 1秒周期采集 → TCP Client 发送至主机:8888       │      │
│  └─────────────────────────────────────────────────────────────────┘      │
│                                                                             │
│  🖥️ OLED: U8g2 驱动，本地数据显示（简略/详细双模式切换）                     │
└─────────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
                              📊 完整数据流向
═══════════════════════════════════════════════════════════════════════════════

  从机传感器 ──采集(1s周期)──▶ 从机TCP Client ──TCP:8888──▶ 主机TCP Server
       │                                                              │
       │  temperature, humidity, smoke, gas, water                     │
       │  格式: "25.3,60.5,0,120,15.2\n"                               │
       ▼                                                              ▼
  从机OLED显示(本地)                                           主机解析存储
                                                                    │
                                              ┌─────────────────────┤
                                              ▼                     ▼
                                        主机OLED显示          HTTP API:/sensors
                                                                   │
                                    ┌──────────────────────────────┤
                                    ▼                              ▼
                              Web前端Chart.js              小程序Canvas绘图
                                                                    │
                                    ┌──────────────────────────────┤
                                    ▼                              ▼
                               浏览器渲染曲线                手机屏幕平滑曲线
```

---

## 📁 项目结构

```
Project_v1/
│
├── esp32_firmware/                      # 🔧 ESP32 固件 (Arduino/PlatformIO)
│   ├── master/main/                     # ⭐ 主机固件 (摄像头/Web服务器/TCP服务端)
│   │   ├── main.ino                    # 主程序入口 (setup/loop/WiFi/PSRAM初始化)
│   │   ├── web_server.h/.cpp           # ⭐ HTTP服务器核心 (视频流/拍照/AI/传感器/外设API)
│   │   ├── myOV3660.h/.cpp             # OV3660摄像头驱动与引脚配置
│   │   ├── sensors.h/.cpp              # 本地传感器数据采集模块
│   │   ├── mySensor.h/.cpp             # DHT11/MQ2/水位传感器驱动
│   │   ├── sensor_data.h/.cpp          # 传感器数据全局变量定义（含remoteData）
│   │   ├── actuators.h/.cpp            # 外设控制模块（继电器/蜂鸣器/电机/舵机）
  │   │   ├── auto_control.h/.cpp         # ⭐ 超限自动控制模块（规则引擎/条件检查/动作执行）
  │   │   ├── myWIFI.h/.cpp               # WiFi连接管理（SPIFFS安全存储配置）
│   │   ├── wifi_config.h/.cpp          # WiFi配置持久化管理器
│   │   ├── u8g2_oled.h/.cpp            # OLED显示屏驱动（U8g2库）
│   │   └── public.h                    # 公共类型定义与常量
│   │
│   └── slave/main/                      # ⭐ 从机固件 (传感器采集/TCP客户端)
│       ├── main.ino                    # 从机程序入口 (传感器初始化/TCP连接)
│       ├── mySensor.h/.cpp             # DHT11/MQ2/水位传感器驱动
│       ├── sensor_data.h/.cpp          # 传感器数据定义
│       ├── myWIFI.h/.cpp               # WiFi连接管理
│       ├── u8g2_oled.h/.cpp            # OLED本地数据显示（简略/详细切换）
│       ├── config.h                    # 系统配置文件（时间间隔参数等）
│       └── public.h                    # 公共类型定义
│
├── web_frontend/                        # 🌐 前端应用
│   ├── frontend/
│   │   └── index.html                  # ⭐ Web控制面板 (单文件全功能)
│   │                                   #    - Chart.js 数据可视化
│   │                                   #    - MJPEG视频流 + 拍照
│   │                                   #    - AI视觉识别 + LLM验证测试
│   │
│   └── wechat_miniprogram/             # 📱 微信小程序
│       ├── app.js/json/wxss            # 小程序全局配置
│       ├── utils/                      # 🔧 工具服务层
│       │   ├── ai-api-service.js       # ⭐ AI API服务层（HTTP封装/错误处理/重试）
│       │   ├── actuator-service.js     # ⭐ 外设控制服务（继电器/蜂鸣器/电机/舵机）
│       │   ├── ai-config.js            # ⭐ AI配置管理（多模型Key/URL统一管理）
│       │   └── error-logger.js         # 错误日志系统
│       │
│       └── pages/
│           ├── index/                  # ⭐ 主页面
│           │   ├── index.js            # ⭐ 主逻辑（传感器展示/Canvas图表/视频快照/拍照/AI识别/外设面板/测试工具）
│           │   ├── index.wxml/wxss     # 主页面布局和样式
│           │   └── network_test.js/wxss/wxml  # 网络诊断 + LLM验证测试页面
│           │
│           ├── control/                # ⭐ 外设控制独立页面
│           │   ├── control.js          # 控制逻辑（继电器开关/蜂鸣器频率/电机PWM/舵机角度）
│           │   ├── control.wxml/wxss   # 控制页面布局和样式
│           │   └── control.json        # 页面配置
│           │
│           └── auto-control/           # ⭐ 超限自动控制配置页面
│               ├── auto-control.js     # 自动控制规则管理逻辑
│               ├── auto-control.wxml/wxss  # 规则配置界面
│               └── auto-control.json   # 页面配置
│
├── ai_service/                         # 🤖 Python AI 服务（可选本地推理）
│   ├── main.py                         # 服务入口
│   ├── config/                         # 配置模块
│   ├── src/models/                     # AI模型实现（OpenAI/通义/Ollama/HuggingFace）
│   ├── src/yolo_detector.py            # YOLO目标检测器
│   └── models/yolo_models/             # YOLO模型权重文件
│
└── README.md                           # 📖 本文档
```

---

## 📚 文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| **项目总览** | [README.md](./README.md) | 本文件 - 系统架构和功能概述 |
| **主机固件** | [esp32_firmware/master/README.md](./esp32_firmware/master/README.md) | ESP32-S3主控系统详细文档 |
| **从机固件** | [esp32_firmware/slave/README.md](./esp32_firmware/slave/README.md) | 从机传感器采集详细文档 |
| **硬件总览** | [esp32_firmware/README.md](./esp32_firmware/README.md) | 硬件整体说明 |
| **小程序** | [web_frontend/wechat_miniprogram/README.md](./web_frontend/wechat_miniprogram/README.md) | 微信小程序开发文档 |
| **Web前端** | [web_frontend/README.md](./web_frontend/README.md) | Web前端技术文档 |
| **AI服务** | [ai_service/README.md](./ai_service/README.md) | Python AI服务文档 |

---

## ⚡ 快速开始

### 1. 硬件准备

| 数量 | 设备 | 用途 |
|------|------|------|
| 1 | ESP32-S3-WROOM (8MB PSRAM) | 主机（摄像头+Web服务器） |
| 1 | ESP32 DevKit | 从机（传感器采集） |
| 1 | OV3660 摄像头模块 | 视频监控 |
| 1 | SSD1306 OLED (128x64) x2 | 本地显示（主从各一） |
| 1 | DHT11 温湿度传感器 x2 | 温湿度检测 |
| 1 | MQ-2 气体传感器 x2 | 烟雾/气体检测 |
| 1 | 水位传感器 x2 | 液位检测 |
| 4 | 外设模块 | 继电器/蜂鸣器/电机/舵机 |

### 2. 编译烧录

#### 主机
1. 使用 Arduino IDE 打开 `esp32_firmware/master/main/`
2. 选择开发板: **ESP32S3 Dev Module**
3. 配置PSRAM: **OPI PSRAM**
4. 上传代码

#### 从机
1. 使用 Arduino IDE 打开 `esp32_firmware/slave/main/`
2. 选择开发板: **ESP32 Dev Module V1**
3. 上传代码

### 3. 启动服务

```bash
# AI服务（可选）
cd ai_service
pip install -r requirements.txt
python main.py
```

### 4. 访问系统

- **Web端**: 浏览器访问 `http://<ESP32_IP>`
- **小程序**: 使用微信开发者工具打开 `wechat_miniprogram/`

---

## 📦 模块详细说明

### 一、微信小程序模块

#### `pages/index/index.js` — 主页面（核心）

> 文件路径: [index.js](web_frontend/wechat_miniprogram/pages/index/index.js)

**功能清单：**

| 模块 | 功能描述 | 关键技术 |
|------|----------|----------|
| 📊 **传感器数据展示** | 温度/湿度/气体/水位四路传感器卡片实时显示 | 3秒轮询 `/sensors` API，`SensorHistory` 类管理100个历史数据点 |
| 📈 **Canvas 图表** | 原生 Canvas 绘制平滑曲线图 | **Catmull-Rom 样条插值**算法 + 线性渐变填充 + Glow发光效果 |
| 📹 **视频流预览** | 实时画面显示，支持视频流/快照模式切换 | **快照模式**：定时 fetch(`/capture`) 模拟 MJPEG 流（微信兼容方案）|
| 📸 **拍照捕获** | JPEG 单张拍摄与 Base64 显示 | 调用 `/capture` API，支持手动触发 |
| 🤖 **AI 视觉识别** | 图片上传 + 问题输入 → 大模型分析 | 支持 **LLM / YOLO / 本地** 三种识别模式，调用 `ai-api-service` |
| 🎛️ **外设控制面板** | 内嵌式继电器/蜂鸣器/电机/舵机控制 | 调用 `actuator-service`，内嵌于主页底部 |
| 🔧 **测试工具** | 网络诊断 + LLM 配置验证 | 可跳转至 `network_test` 页面 |
| ⚙️ **配置管理** | ESP32 地址设置与本地存储同步 | `wx.setStorageSync` 持久化 |

#### `pages/control/control.js` — 外设控制独立页面

> 文件路径: [control.js](web_frontend/wechat_miniprogram/pages/control/control.js)

**功能清单：**

| 设备 | 操作类型 | 说明 |
|------|----------|------|
| 🔌 **继电器** | on / off / toggle / status | 开关控制与状态读取，优化UI布局 |
| 🔔 **蜂鸣器** | on / off / beep / status | 支持可调频率（100-10000Hz），预设频率+自定义输入，实时同步 |
| ⚡ **电机** | stop / run / status | PWM调速（0-255对应0-100%），支持精确数值输入 |
| 🎯 **舵机** | attach / detach / write / status | 角度控制（0-180°），支持精确数值输入 |

**特点：**
- 独立全屏操作界面，带操作历史记录
- 设备在线状态指示和错误提示机制
- 蜂鸣器频率实时同步：切换频率或设置自定义频率时立即生效
- 继电器控制界面优化：紧凑布局，ON/OFF/Toggle三按钮设计
- 电机和舵机支持精确数值输入：解决事件冒泡问题，使用catchtap阻止冒泡

#### `pages/index/network_test.js` — 网络诊断与 LLM 验证测试

> 文件路径: [network_test.js](web_frontend/wechat_miniprogram/pages/index/network_test.js)

**网络诊断：** WiFi状态检测 → ESP32连接测试(`GET /`) → 传感器API测试(`/sensors`) → 视频流API测试(`/stream`) → 拍照API测试(`/capture`)

**LLM 验证测试：**
- 支持 10+ 国内外主流大模型选择
- 选择模型后自动填充对应 API 地址
- 实时显示响应时间和 Token 用量
- 配置保存到本地存储
- 一键同步 AI 配置到 ESP32 设备

#### `utils/ai-api-service.js` — AI API 服务层

> 文件路径: [ai-api-service.js](web_frontend/wechat_miniprogram/utils/ai-api-service.js)

**职责：** 封装所有 AI 模型的 HTTP 请求逻辑，提供统一的调用接口。

**核心能力：**
- HuggingFace YOLO 目标检测 API 调用
- 通义千问 LLM/VLM 多模态 API 调用
- 统一的错误处理和重试机制（`requestWithRetry`，最多重试2次）
- 请求/响应日志记录（debug 模式）
- 错误类型分类（CONFIG_MISSING / AUTH_FAILED / NETWORK_ERROR / TIMEOUT / RATE_LIMIT 等）

#### `utils/actuator-service.js` — 外设控制服务

> 文件路径: [actuator-service.js](web_frontend/wechat_miniprogram/utils/actuator-service.js)

**职责：** 封装与 ESP32 HTTP 外设控制 API 的所有通信逻辑。

**核心能力：**
- 统一 HTTP 请求封装（带重试、超时、错误处理）
- 设备类型枚举：RELAY / BUZZER / MOTOR / SERVO
- 操作类型白名单校验（防止非法指令）
- 自动构建请求 URL 和参数
- 响应解析和数据提取

#### `utils/ai-config.js` — AI 配置管理

> 文件路径: [ai-config.js](web_frontend/wechat_miniprogram/utils/ai-config.js)

**职责：** 统一管理所有 AI 模型的 API Key、模型名称、API 地址等配置。

**支持的模型：**
- HuggingFace: DETR-ResNet-50 / YOLOS-Small / DETR-ResNet-101
- 通义千问: 文本对话 / 视觉理解（阿里云 DashScope）

---

### 二、固件模块（主从架构）

#### `master/main/` — 主机代码

> 目录路径: [master/main/](esp32_firmware/master/main/)

主机是系统的核心节点，承担以下职责：

| 模块 | 文件 | 功能 |
|------|------|------|
| **摄像头** | [myOV3660.h/.cpp](esp32_firmware/master/main/myOV3660.h) | OV3660 初始化与引脚配置，自动根据 PSRAM 选择分辨率 |
| **Web 服务器** | [web_server.h/.cpp](esp32_firmware/master/main/web_server.h) | HTTP 80端口，处理视频流/拍照/AI/传感器/外设/CORS 全部 API |
| **TCP 服务器** | [web_server.cpp](esp32_firmware/master/main/web_server.cpp) 中 `wifi_server_init()` | 监听 8888 端口，接收从机传感器数据 |
| **传感器采集** | [sensors.h/.cpp](esp32_firmware/master/main/sensors.h) | 本地传感器数据采集（DHT11温湿度、MQ-2气体、水位）|
| **外设控制** | [actuators.h/.cpp](esp32_firmware/master/main/actuators.h) | 继电器/蜂鸣器/电机/舵机四路外设控制，支持蜂鸣器频率参数 |
| **WiFi管理** | [myWIFI.h/.cpp](esp32_firmware/master/main/myWIFI.h) | WiFi连接管理，SPIFFS安全存储SSID/密码 |
| **OLED显示** | [u8g2_oled.h/.cpp](esp32_firmware/master/main/u8g2_oled.h) | U8g2驱动OLED显示屏，显示远程传感器数据和连接状态 |

**关键API端点：**

| 端点 | 方法 | 功能 | 参数示例 |
|------|------|------|----------|
| `/stream` | GET | MJPEG视频流 | - |
| `/capture` | GET | JPEG单张拍照 | - |
| `/sensors` | GET | 获取传感器数据 | - |
| `/ask` | POST | AI视觉识别 | `image`(Base64), `question`, `model` |
| `/actuator` | GET/POST | 外设控制 | `device`, `action`, `freq`(蜂鸣器频率), `speed`(电机速度), `angle`(舵机角度) |
| `/status` | GET | 系统状态 | - |
| `/config` | POST | AI配置同步 | `ai_api_url`, `ai_api_key`, `ai_model` |
| `/auto_control` | GET/POST | 自动控制规则管理 | `action`, `rule`, `id` |

**蜂鸣器频率控制实现：**

```cpp
// web_server.cpp 中的蜂鸣器控制逻辑
else if (strcmp(device, "buzzer") == 0) {
    if (strcmp(action, "on") == 0) {
        // 支持频率参数，例如: /actuator?device=buzzer&action=on&freq=2000
        char freq_str[16] = {0};
        int freq = BUZZER_FREQ_DEFAULT;  // 默认频率
        
        esp_err_t ret = httpd_query_key_value(query, "freq", freq_str, sizeof(freq_str));
        if (ret == ESP_OK) {
            freq = atoi(freq_str);
            freq = constrain(freq, 100, 10000);  // 限制频率范围
        }
        
        buzzer_on_with_freq(freq);  // 使用指定频率开启蜂鸣器
    } else if (strcmp(action, "off") == 0) {
        buzzer_off();
    }
    // ... 其他操作
}
```

**超限自动控制实现：**

```cpp
// auto_control.h - 规则结构体定义
typedef struct {
    uint8_t id;                          // 规则ID
    char name[RULE_NAME_MAX_LEN];        // 规则名称
    bool enabled;                        // 是否启用
    SensorType sensorType;               // 传感器类型
    TriggerCondition condition;          // 触发条件（>/<=/in_range/out_of_range）
    float threshold1;                    // 阈值1
    float threshold2;                    // 阈值2（范围条件使用）
    uint16_t debounceMs;                 // 去抖动时间
    ActionType actionType;               // 动作类型
    int actionParam;                     // 动作参数
    uint16_t actionDurationMs;           // 动作持续时间
    bool lastTriggered;                  // 上次触发状态（内部使用）
} AutoControlRule;

// auto_control.cpp - 条件检查
bool check_condition(float value, TriggerCondition condition, float t1, float t2) {
    switch (condition) {
        case CONDITION_GREATER_THAN: return value > t1;
        case CONDITION_LESS_THAN:    return value < t1;
        case CONDITION_EQUAL_TO:     return fabs(value - t1) < 0.01;
        case CONDITION_IN_RANGE:     return value >= t1 && value <= t2;
        case CONDITION_OUT_OF_RANGE: return value < t1 || value > t2;
        default: return false;
    }
}

// main.ino - 主循环中每秒检查一次
void loop() {
    static unsigned long lastAutoControlCheck = 0;
    if (millis() - lastAutoControlCheck >= 1000) {
        lastAutoControlCheck = millis();
        check_and_execute_auto_control();  // 检查并执行自动控制
    }
    // ... 其他代码
}
```

**自动控制API：**

| 操作 | 请求体 | 说明 |
|------|--------|------|
| 获取状态 | GET /auto_control | 返回所有规则和系统状态 |
| 添加规则 | POST `{"action":"add","rule":{...}}` | 添加新规则，返回规则ID |
| 更新规则 | POST `{"action":"update","id":1,"rule":{...}}` | 更新指定规则 |
| 删除规则 | POST `{"action":"delete","id":1}` | 删除指定规则 |
| 启用/禁用 | POST `{"action":"enable/disable","id":1}` | 切换规则状态 |
| 手动触发 | POST `{"action":"trigger","id":1}` | 手动执行规则动作 |
| 加载默认 | POST `{"action":"load_defaults"}` | 加载5条默认规则 |
| 清除所有 | POST `{"action":"clear"}` | 删除所有规则 |

**支持的触发条件：**
- `>` 大于阈值
- `<` 小于阈值
- `=` 等于阈值
- `in_range` 在范围内 [threshold1, threshold2]
- `out_of_range` 超出范围

**支持的动作类型：**
- `relay_on/off/toggle` - 继电器控制
- `buzzer_on/off/beep` - 蜂鸣器控制（支持频率参数）
- `motor_run/stop` - 电机控制（支持速度参数）
- `servo_write` - 舵机控制（支持角度参数）

#### `slave/main/` — 从机代码

> 目录路径: [slave/main/](esp32_firmware/slave/main/)

从机专责传感器数据采集和传输：

| 模块 | 文件 | 功能 |
|------|------|------|
| **传感器驱动** | [mySensor.h/.cpp](esp32_firmware/slave/main/mySensor.h) | DHT11温湿度、MQ-2气体、水位传感器ADC读取 |
| **数据采集** | [sensor_data.h/.cpp](esp32_firmware/slave/main/sensor_data.h) | 1秒周期采集，数据格式化 |
| **TCP客户端** | [main.ino](esp32_firmware/slave/main/main.ino) | 连接主机8888端口，发送传感器数据 |
| **WiFi管理** | [myWIFI.h/.cpp](esp32_firmware/slave/main/myWIFI.h) | WiFi连接管理 |
| **OLED显示** | [u8g2_oled.h/.cpp](esp32_firmware/slave/main/u8g2_oled.h) | 本地数据显示（简略/详细双模式切换）|
| **系统配置** | [config.h](esp32_firmware/slave/main/config.h) | 时间间隔参数等系统配置 |

---

### 三、Web前端模块

#### `frontend/index.html` — Web控制面板

> 文件路径: [index.html](web_frontend/frontend/index.html)

单文件全功能Web应用，包含：

- **Chart.js 数据可视化**：温度/湿度/气体/水位实时曲线图
- **MJPEG视频流**：原生video标签播放（支持浏览器）
- **拍照捕获**：JPEG图片捕获和显示
- **AI视觉识别**：图片上传和大模型分析
- **外设控制面板**：继电器/蜂鸣器/电机/舵机控制
- **LLM验证测试**：API连通性测试和配置同步

---

## 🚀 快速开始

### 环境要求

**硬件：**
- 2× ESP32-S3-WROOM 开发板（1个主机 + 1个从机）
- OV3660 摄像头模块
- DHT11/DHT22 温湿度传感器
- MQ-2 气体传感器
- 水位传感器（ADC接口）
- 0.96寸 OLED显示屏（I2C接口，SSD1306/SH1106）
- 继电器模块、蜂鸣器、直流电机、SG90舵机
- USB数据线（供电和编程）

**软件：**
- Arduino IDE 2.x + ESP32 Board Package (3.x)
- PlatformIO（可选，推荐用于高级开发）
- 微信开发者工具（小程序开发）
- Python 3.x + pip（AI服务，可选）

### 编译与烧录

#### 1. 主机固件（Master）

```bash
# 使用 Arduino IDE
# 1. 打开 esp32_firmware/master/main/main.ino
# 2. 选择开发板: ESP32S3 Dev Module
# 3. 配置分区方案: Huge App (3MB No OTA/1MB SPIFFS)
# 4. 设置 PSRAM: OPI PSRAM
# 5. 点击编译并上传
```

#### 2. 从机固件（Slave）

```bash
# 使用 Arduino IDE
# 1. 打开 esp32_firmware/slave/main/main.ino
# 2. 选择开发板: ESP32S3 Dev Module
# 3. 配置分区方案: Default 4MB with spiffs (1.2MB APP/190KB SPIFFS)
# 4. 点击编译并上传
```

#### 3. 微信小程序

```bash
# 1. 使用微信开发者工具打开 web_frontend/wechat_miniprogram
# 2. 填写合法的 AppID（或使用测试号）
# 3. 在 app.js 中配置 ESP32 的 IP 地址
# 4. 点击编译预览
```

### 网络配置

**首次启动：**
1. 主机和从机上电后会创建 WiFi 热点（AP模式）
2. 连接热点后访问 `192.168.4.1` 进行配置
3. 输入目标 WiFi 的 SSID 和密码
4. 设备重启后自动连接到指定网络

**查看IP地址：**
- 串口监视器（115200波特率）会打印分配的IP
- OLED屏幕会显示连接状态和IP地址
- 或者使用路由器管理页面查找设备IP

---

## 📖 使用指南

### 1. 视频监控

**Web浏览器：**
- 访问 `http://<ESP32_IP>` 
- 页面自动加载 MJPEG 视频流
- 支持多客户端同时观看（最多2个）

**微信小程序：**
- 默认使用**快照模式**（微信不支持MJPEG流）
- 可点击切换按钮在**视频流模式**和**快照模式**间切换
- 快照模式：定时抓帧刷新（默认500ms间隔）
- 视频流模式：尝试使用video组件（部分机型支持）

### 2. 传感器数据查看

- 数据每3秒自动刷新一次
- Web端：Chart.js绘制实时曲线图
- 小程序：Canvas Catmull-Rom样条插值绘制平滑曲线
- 支持温度/湿度/气体浓度/水位四路数据同时显示

### 3. AI视觉识别

**支持的模式：**
- **LLM模式**：调用云端大模型进行图像理解和问答
- **YOLO模式**：调用HuggingFace YOLO API进行目标检测
- **本地模式**：调用本地AI服务（需部署ai_service）

**使用步骤：**
1. 选择要使用的AI模型（OpenAI/通义千问/Kimi/DeepSeek/豆包等）
2. 配置对应的API Key（在小程序设置页或network_test页）
3. 拍摄或上传图片
4. 输入问题（如："图片中有什么？"）
5. 点击发送，等待AI回复

### 4. 外设控制

**继电器控制：**
- ON：闭合继电器
- OFF：断开继电器
- Toggle：切换状态
- 支持开关按钮和独立操作按钮

**蜂鸣器控制：**
- **长鸣模式**：开关控制持续发声
- **试听模式**：短促提示音
- **频率设置**：
  - 预设频率：500Hz / 1000Hz / 2000Hz / 3000Hz / 5000Hz
  - 自定义输入：支持100-10000Hz范围
  - 实时同步：频率变化立即应用到正在播放的蜂鸣器

**电机控制：**
- PWM调速：0-255对应0-100%占空比
- 精确输入：支持直接输入数值（解决事件冒泡问题）
- 状态反馈：实时显示运行状态

**舵机控制：**
- 角度控制：0-180度范围
- 精确输入：支持直接输入角度值
- attach/detach：控制舵机使能状态

**注意事项：**
- ⚠️ 电机和舵机同时工作可能导致电流过大，建议使用独立电源
- ⚠️ 操作前请确保设备在线（绿色指示灯）
- ⚠️ 蜂鸣器频率过高可能影响听力，建议使用2000Hz以下

---

## 🔧 高级配置

### 时间间隔参数优化

从机配置文件 [config.h](esp32_firmware/slave/main/config.h) 定义了各项时间间隔参数：

| 参数 | 当前值 | 说明 | 优化建议 |
|------|--------|------|----------|
| `SENSOR_READ_INTERVAL_MS` | 1000ms | 传感器采集周期 | ✅ 合适，平衡精度和性能 |
| `DATA_SEND_INTERVAL_MS` | 1000ms | TCP数据发送周期 | ✅ 与采集周期一致 |
| `OLED_UPDATE_INTERVAL_MS` | 500ms | OLED刷新周期 | ✅ 平衡流畅度和资源占用 |
| `WIFI_RECONNECT_INTERVAL_MS` | 5000ms | WiFi重连间隔 | ✅ 合理的退避策略 |
| `TCP_RECONNECT_INTERVAL_MS` | 3000ms | TCP重连间隔 | ✅ 快速恢复连接 |

### 蜂鸣器频率范围

| 频率范围 | 效果 | 适用场景 |
|----------|------|----------|
| 500-1000Hz | 低沉音效 | 提示音、警报 |
| 1000-2000Hz | 标准音效 | 常规提示、通知 |
| 2000-3000Hz | 清脆音效 | 操作确认、成功提示 |
| 3000-5000Hz | 尖锐音效 | 紧急警告（注意听力保护）|
| 5000-10000Hz | 高频音调 | 特殊效果（建议短时使用）|

---

## 🐛 常见问题

### Q1: 微信小程序无法显示视频流？

**A:** 微信小程序不支持原生MJPEG流播放。本项目使用**快照模式**作为兼容方案：
- 定时调用 `/capture` API获取JPEG图片
- 通过快速刷新模拟视频效果
- 可在设置中切换视频流/快照模式

### Q2: 电机运转时操作舵机会断开连接？

**A:** 这是电源供应不足导致的：
- 电机和舵机同时工作时电流需求较大
- USB供电能力有限（通常500mA-1A）
- **解决方案：**
  1. 使用独立外部电源（5V/2A以上）
  2. 在电源端并联1000μF电解电容缓冲
  3. 避免同时满负荷运行电机和舵机

### Q3: 蜂鸣器频率设置不生效？

**A:** 请检查以下步骤：
1. 确保输入的频率在100-10000Hz范围内
2. 点击SET按钮后检查是否有成功提示
3. 如果蜂鸣器正在播放，频率会立即生效
4. 如果蜂鸣器关闭，频率会在下次开启时生效
5. 查看控制台日志确认请求是否发送成功

### Q4: 如何切换AI模型？

**A:** 有两种方式：
1. **小程序设置页**：在AI配置中选择模型
2. **network_test页**：
   - 选择预设模型或手动输入API地址
   - 测试API连通性
   - 一键同步配置到ESP32设备

### Q5: 从机数据无法传输到主机？

**A:** 检查以下几点：
1. 确认主机和从机在同一局域网
2. 查看主机串口输出是否显示TCP连接信息
3. 检查从机config.h中的主机IP配置
4. 确认主机TCP服务器在8888端口正常运行
5. 重启两个设备重新建立连接

---

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| 视频流延迟 | <100ms | 局域网环境下 |
| 传感器采集精度 | ±0.1°C / ±2%RH | DHT11规格 |
| 数据传输延迟 | <50ms | TCP局域网 |
| 并发客户端数 | 2个 | MJPEG流限制 |
| 外设响应时间 | <200ms | HTTP请求往返 |
| 蜂鸣器频率精度 | ±10Hz | PWM精度限制 |

---

## � 安全注意事项

1. **WiFi密码安全**：避免使用简单密码，定期更换
2. **API Key保护**：不要将AI API Key提交到公开仓库
3. **物理安全**：确保设备放置在安全位置，避免未经授权的物理接触
4. **网络安全**：建议在内网中使用，如需外网访问请配置VPN或防火墙
5. **电源安全**：使用合格的电源适配器，避免过载

---

## 📝 更新日志

### v1.0 (2026-04-18)
- ✨ 新增蜂鸣器自定义频率输入功能（100-10000Hz）
- ✨ 优化蜂鸣器频率实时同步机制
- ✨ 重新设计继电器控制界面（紧凑布局）
- ✨ 修复电机和舵机精确输入按钮无响应问题（事件冒泡）
- ✨ 优化控制页面操作记录显示（时间和名称不再遮挡）
- ✨ 新增视频流/快照模式切换功能
- 🐛 修复蜂鸣器SET按钮设置不生效的问题
- 🐛 修复大模型返回文本右侧被遮挡的问题
- 🧹 清理项目无关文件和冗余代码
- 📝 更新全部README文档

### v0.2 (2026-04-17)
- ✨ 新增LLM验证测试面板
- ✨ 优化PSRAM内存管理
- 🐛 修复视频流并发冲突问题
- 📝 完善项目文档

### v0.1 (2026-04-16)
- 🎉 初始版本发布
- ✨ 实现主从分布式架构
- ✨ 支持多平台大模型AI识别
- ✨ 实现Web和小程序双端访问

---

## 👥 贡献者

- **一个普通人** - 项目发起者和主要开发者

---

## 📄 许可证

本项目仅供学习和研究使用。商业使用请联系作者获得授权。

---

## 🙏 致谢

- [Espressif Systems](https://www.espressif.com/) - ESP32芯片和开发工具
- [Arduino](https://www.arduino.cc/) - 开源电子原型平台
- [Chart.js](https://www.chartjs.org/) - 数据可视化库
- [U8g2](https://github.com/olikraus/U8g2) - OLED显示库
- 各大AI服务商提供的API支持

---

**🎯 项目状态：活跃开发中 | 最后更新：2026-04-18**
