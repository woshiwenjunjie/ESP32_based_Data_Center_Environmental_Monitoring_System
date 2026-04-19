# ESP32 环境监测站 - 微信小程序 v1.0

> **完整功能**：实时监控 · 视频流/快照切换 · AI识别 · 外设控制 · 数据可视化

---

## 目录

- [项目概述](#项目概述)
- [技术栈](#技术栈)
- [系统架构](#系统架构)
- [项目结构](#项目结构)
- [核心功能模块](#核心功能模块)
  - [1. 主页面 (pages/index/index)](#1-主页面-pagesindexindex)
  - [2. Canvas图表系统](#2-canvas图表系统)
  - [3. 视频流模块（支持模式切换）](#3-视频流模块支持模式切换)
  - [4. 拍照功能](#4-拍照功能)
  - [5. AI识别系统（三模式路由）](#5-ai识别系统三模式路由)
  - [6. 外设控制面板（tab-1）](#6-外设控制面板tab-1)
  - [7. 独立控制页面 (pages/control/control)](#7-独立控制页面-pagescontrolcontrol)
  - [8. 网络诊断和LLM测试 (pages/index/network_test)](#8-网络诊断和llm测试-pagesindexnetwork_test)
- [工具类模块架构](#工具类模块架构)
  - [ai-api-service.js](#ai-api-servicejs)
  - [actuator-service.js](#actuator-servicejs)
  - [ai-config.js](#ai-configjs)
  - [error-logger.js](#error-loggerjs)
- [数据流和状态管理](#数据流和状态管理)
- [性能优化策略](#性能优化策略)
- [配置说明](#配置说明)
- [开发指南](#开发指南)
- [API接口文档](#api接口文档)
- [常见问题](#常见问题)
- [更新日志](#更新日志)

---

## 项目概述

**ESP32环境监测站微信小程序 v1.0** 是一套完整的物联网移动端解决方案，基于微信小程序原生框架构建，提供与网页版同等丰富的功能，并针对移动端交互进行了深度优化。

### 核心能力矩阵

| 功能域 | 具体能力 | 技术实现 |
|--------|----------|----------|
| 📊 实时监控 | 温度/湿度/MQ-2气体/水位传感器数据 | 3秒轮询 + 趋势箭头 + 进度条 |
| 📈 数据可视化 | 双Y轴曲线图 + Catmull-Rom样条插值 | 原生Canvas 2D API（零依赖） |
| 📹 视频流 | 实时画面预览，支持视频流/快照模式切换 | 快照模式（500ms间隔）+ 模式切换按钮 |
| 📸 图像捕获 | JPEG拍照 + Base64编码显示 | /capture接口 + 时间戳记录 |
| 🤖 AI识别 | LLM对话 / YOLO检测 / 本地分析 | 三模式智能路由，支持10+主流模型 |
| 🎛️ 外设控制 | 继电器/蜂鸣器(自定义频率)/电机/舵机 | HTTP API统一接口，精确数值输入 |
| 🔧 诊断工具 | WiFi检测 + API连通性 + LLM验证 | 一键测试 + 配置同步 |

### v1.0 新增特性

- ✨ **视频流/快照模式切换**：新增切换按钮，可在两种模式间自由切换
- ✨ **蜂鸣器自定义频率**：支持100-10000Hz范围自定义频率输入
- ✨ **蜂鸣器频率实时同步**：频率变化立即应用到正在播放的蜂鸣器
- ✨ **继电器界面优化**：紧凑布局，ON/OFF/Toggle三按钮设计
- ✨ **精确输入修复**：修复电机和舵机数值输入按钮无响应问题（事件冒泡）
- ✨ **UI优化**：解决操作记录时间和名称遮挡问题、大模型文本遮挡问题

---

## 技术栈

```
┌─────────────────────────────────────────────────┐
│              微信小程序原生框架                    │
├──────────┬──────────┬──────────┬────────────────┤
│   WXML   │   WXSS   │    JS    │   微信API      │
│ 页面结构  │ 页面样式  │ 业务逻辑  │ 网络/存储/设备  │
└──────────┴──────────┴──────────┴────────────────┘
         │              │              │
         ▼              ▼              ▼
┌────────────────┐ ┌─────────────┐ ┌──────────────┐
│ Canvas 2D API  │ │ wx.request  │ │wx.setStorage │
│ (无第三方库)    │ │  网络请求    │ │ Sync 本地存储  │
└────────────────┘ └─────────────┘ └──────────────┘
```

### 技术选型决策

| 技术 | 选型理由 |
|------|----------|
| **原生框架** | 避免Taro/uni-app等跨平台框架的额外抽象层，最大化性能 |
| **Canvas 2D API** | 新版Canvas接口，支持离屏渲染和高DPI适配，无需引入ECharts等重型库 |
| **wx.request** | 微信原生网络请求，支持超时控制和详细错误回调 |
| **wx.setStorageSync** | 同步本地存储，用于配置持久化和报警阈值保存 |

---

## 系统架构

```
                          ┌──────────────────────────────┐
                          │        微信小程序客户端        │
                          │                              │
  ┌───────────────────────┼──────────────────────────────┤
  │                       │                              │
  │   ┌─────────┐  ┌──────▼────┐  ┌──────────────────┐  │
  │   │ pages/  │  │   utils/  │  │     app.js       │  │
  │   ├─────────┤  ├───────────┤  │  全局配置/状态    │  │
  │   │ index/  │  │ ai-api-   │  │  esp32Url        │  │
  │   │ control │  │ service.js│  │  aiServiceUrl    │  │
  │   │ network │  │ actuator- │  └────────┬─────────┘  │
  │   │ _test   │  │ service.js│           │            │
  │   └─────────┘  │ ai-config │           │            │
  │                │ .js       │           │            │
  │                │ error-    │           │            │
  │                │ logger.js │           │            │
  │                └─────┬─────┘           │            │
  │                      │                 │            │
  └──────────────────────┼─────────────────┼────────────┘
                         │                 │
                         ▼                 ▼
              ┌──────────────────┐  ┌──────────────┐
              │    ESP32 设备     │  │ AI 云服务     │
              │  (HTTP REST API) │  │              │
              ├──────────────────┤  ├──────────────┤
              │ /sensors         │  │ 通义千问(VL)  │
              │ /capture         │  │ HuggingFace   │
              │ /stream          │  │ DeepSeek      │
              │ /actuator        │  │ OpenAI兼容    │
              │ /ask             │  │ Kimi/豆包     │
              │ /config          │  │              │
              │ /status          │  └──────────────┘
              └──────────────────┘
```

### 数据流向

```
用户操作 → WXML事件绑定 → Page方法处理 → Service层封装 → wx.request发送HTTP请求
                                                                    │
响应返回 ← JSON解析 ← 数据校验 ← 回调处理 ← wx.request success/fail回调
    │
    ▼
setData()更新视图 → WXML数据绑定自动渲染 → 用户看到最新状态
```

---

## 项目结构

```
wechat_miniprogram/
├── app.js                          # 小程序入口：全局配置、生命周期
├── app.json                        # 页面注册、窗口样式配置
├── project.config.json             # 项目配置（appid、编译设置）
├── project.private.config.json     # 私有配置（不提交版本控制）
│
├── pages/
│   ├── index/                      # ★ 主页面（多功能聚合）
│   │   ├── index.js               # 核心逻辑：含所有功能模块
│   │   ├── index.wxml             # 页面结构：Tab切换 + Swiper容器
│   │   ├── index.wxss             # 页面样式：暗色主题优化
│   │   ├── network_test.js        # 网络诊断 + LLM验证逻辑
│   │   ├── network_test.wxml      # 测试工具UI
│   │   └── network_test.wxss      # 测试工具样式
│   │
│   └── control/                   # 独立外设控制页面
│       ├── control.js             # 外设控制逻辑（复用actuatorService）
│       ├── control.wxml           # 控制面板UI布局（优化后）
│       ├── control.wxss           # 控制面板样式（优化后）
│       └── control.json           # 页面配置
│
├── utils/                          # ★ 工具类模块层
│   ├── ai-api-service.js          # AI服务：HTTP封装/重试/错误分类
│   ├── actuator-service.js        # 外设控制：统一API接口/参数校验
│   ├── ai-config.js               # 多模型配置管理/服务商优先级
│   └── error-logger.js            # 错误日志系统：记录/统计/导出
│
└── sitemap.json                   # 小程序站点地图配置
```

---

## 核心功能模块

### 1. 主页面 (pages/index/index)

主页面是整个小程序的核心，采用 **Tab + Swiper** 架构将多个功能整合到单一页面中。包含6个Tab页签：

| Tab索引 | 功能名称 | 核心内容 |
|---------|----------|----------|
| Tab 0 | 监控面板 | 传感器卡片 + 报警系统 + 趋势指示 |
| Tab 1 | 外设控制 | 继电器/蜂鸣器/电机/舵机控制面板 |
| Tab 2 | 视频预览 | 视频流显示（支持模式切换）+ 拍照功能 |
| Tab 3 | AI识别 | 三模式AI分析（LLM/YOLO/本地） |
| Tab 4 | 数据图表 | Canvas双Y轴趋势图 + 图表交互 |
| Tab 5 | 测试工具 | 网络诊断 + LLM配置验证 |

#### 1.1 传感器数据实时展示

每 **3秒** 轮询ESP32的 `/sensors` 接口获取最新数据：

```javascript
// index.js - 数据更新定时器
startDataUpdate() {
  this.fetchSensorData()
  this.dataUpdateInterval = setInterval(() => {
    this.fetchSensorData()
  }, 3000)  // 3秒刷新间隔（优化后）
}
```

支持的传感器类型及展示方式：

| 传感器 | 数据字段 | 显示格式 | 进度条范围 | 默认阈值 |
|--------|----------|----------|------------|----------|
| 温度 | `temperature` | `xx.x°C` | -10°C ~ 50°C | 10°C ~ 40°C |
| 湿度 | `humidity` | `xx.x%` | 0% ~ 100% | 20% ~ 85% |
| MQ-2气体 | `mq2` | `xxx ppm` | 0 ~ 2000ppm | < 800ppm |
| 水位 | `water_level` | `xxx mm` | 0 ~ 30mm | < 250mm |

#### 1.2 数据趋势箭头和进度条可视化

通过对比前后两次采样值计算趋势方向：

```javascript
// index.js - 趋势计算算法
calculateTrends(temp, humidity, mq2, water) {
  const trends = {}
  if (this.prevValues.temp !== null) {
    trends.tempTrendIcon = temp > this.prevValues.temp ? '↑'
                       : temp < this.prevValues.temp ? '↓' : '→'
    // ... 其他传感器同理
  }
  this.prevValues = { temp, humidity, mq2, water }
  return trends
}
```

趋势图标映射：
- `↑` （红色）— 数值上升
- `↓` （蓝色）— 数值下降  
- `→` （灰色）— 数值稳定

#### 1.3 报警系统

支持为每个传感器独立配置上下限阈值，超出范围时触发报警提示：

```javascript
// index.js - 默认报警阈值
const DEFAULT_ALARM_THRESHOLD = {
  tempMin: '10',    tempMax: '40',     // 温度范围(°C)
  humMin: '20',     humMax: '85',      // 湿度范围(%)
  mq2Max: '800',                     // 气体上限(ppm)
  waterMax: '250'                    // 水位上限(mm)
}
```

---

### 2. Canvas图表系统

这是本项目最具技术亮点的模块——**完全使用原生Canvas 2D API实现专业级双Y轴图表**，无任何第三方依赖。

#### 2.1 双Y轴设计

```
  温度(°C)  湿度(%)
    40 │                                    MQ-2(ppm)  水位(mm)
       │ ╲                                  2400 │
    30 │   ╲  ╱──── temperature                │
       │    ╲╱                                 1800 │
    20 │  ╭──╮                                │
       │ ╱    ╲ humidity                      1200 │
    10 │╱      ╲                               │
       │        ╲                               600  │
     0 │────────────────────────                  0  │
       └────────────────────────────────────────────┘
         14:00   14:05   14:10   14:15   14:20
```

- **左Y轴**（红-蓝渐变）：温度（-10°C ~ 40°C）+ 湿度（0% ~ 100%）
- **右Y轴**（橙-绿渐变）：MQ-2气体（0 ~ 2400ppm）+ 水位（0 ~ 300mm）

#### 2.2 Catmull-Rom样条插值算法

使用Catmull-Rom样条曲线将离散数据点插值为平滑曲线：

```javascript
// index.js - Catmull-Rom样条插值核心算法
function catmullRomSpline(points) {
  if (points.length < 2) return points
  const result = []
  result.push(points[0])

  for (let i = 0; i < points.length - 1; i++) {
    const p0 = points[Math.max(i - 1, 0)]
    const p1 = points[i]
    const p2 = points[Math.min(i + 1, points.length - 1)]
    const p3 = points[Math.min(i + 2, points.length - 1)]

    for (let t = 0.05; t < 1; t += 0.15) {
      const t2 = t * t
      const t3 = t2 * t
      // Catmull-Rom基函数
      const x = 0.5 * (
        (2 * p1.x) +
        (-p0.x + p2.x) * t +
        (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
        (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3
      )
      const y = 0.5 * (
        (2 * p1.y) +
        (-p0.y + p2.y) * t +
        (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
        (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3
      )
      result.push({ x, y })
    }
    result.push(p2)
  }
  return result
}
```

**算法特点**：
- 参数 `t` 步长为 `0.15`，在每对相邻点之间生成约6个插值点
- 使用4个相邻点（p0-p3）计算当前段，保证C²连续性
- 曲线经过所有原始数据点（非近似拟合）

#### 2.3 渐变填充 + 发光效果(Glow)

每条曲线采用两层绘制策略：

```javascript
// index.js - 曲线绘制（渐变填充 + 发光线条）
datasets.forEach(dataset => {
  if (!dataset.show) return
  
  // 第一层：渐变区域填充
  const areaGradient = ctx.createLinearGradient(0, padding.top, 0, height - padding.bottom)
  areaGradient.addColorStop(0, config.fillStart)   // 顶部：半透明色
  areaGradient.addColorStop(1, config.fillEnd)     // 底部：近乎透明
  ctx.fillStyle = areaGradient
  // ... 填充闭合路径

  // 第二层：发光效果线条
  ctx.save()
  ctx.shadowColor = config.glow       // 发光颜色
  ctx.shadowBlur = 6                  // 发光模糊半径
  ctx.strokeStyle = config.line       // 线条颜色
  ctx.lineWidth = 2.5
  ctx.lineCap = 'round'
  ctx.lineJoin = 'round'
  // ... 描边路径
  ctx.restore()
})
```

四条曲线的颜色方案：

| 传感器 | 线条色 | 渐变起点(α=0.20) | 渐变终点(α=0.01) | 发光色(α=0.35) |
|--------|--------|-------------------|-------------------|----------------|
| 温度 | `#ef4444` 红 | rgba(239,68,68,0.20) | rgba(239,68,68,0.01) | rgba(239,68,68,0.35) |
| 湿度 | `#3b82f6` 蓝 | rgba(59,130,246,0.20) | rgba(59,130,246,0.01) | rgba(59,130,246,0.35) |
| MQ-2 | `#f59e0b` 橙 | rgba(245,158,11,0.20) | rgba(245,158,11,0.01) | rgba(245,158,11,0.35) |
| 水位 | `#10b981` 绿 | rgba(16,185,129,0.20) | rgba(16,185,129,0.01) | rgba(16,185,129,0.35) |

#### 2.4 移动平均平滑处理

通过 `SensorHistory` 类实现窗口大小为5的滑动平均滤波：

```javascript
// index.js - SensorHistory类
class SensorHistory {
  constructor(maxPoints = 100, windowSize = 5) {  // 最多100个点，窗口大小5
    this.maxPoints = maxPoints
    this.windowSize = windowSize
    // ...
  }

  applyMovingAverage(dataArray, windowSize = null) {
    const winSize = windowSize || this.windowSize
    if (dataArray.length < winSize) return [...dataArray]
    
    const result = []
    for (let i = 0; i < dataArray.length; i++) {
      if (i < winSize - 1) {
        result.push(dataArray[i])  // 边界处直接取原值
      } else {
        let sum = 0
        for (let j = 0; j < winSize; j++) {
          sum += dataArray[i - j]  // 窗口内求均值
        }
        result.push(sum / winSize)
      }
    }
    return result
  }
}
```

**数据处理流程**：
```
原始数据 → addPoint() → 存入data数组 → applyMovingAverage() → 存入smoothData数组 → drawChart()读取smoothData绘图
```

---

### 3. 视频流模块（支持模式切换）

#### 3.1 双模式架构

v1.0版本新增视频流/快照模式切换功能：

```
┌─────────────────────────────────────────────────────────────┐
│                     视频流模式选择                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │   📹 视频流模式   │    │   📷 快照模式     │                │
│  │                 │    │                 │                │
│  │ • 使用video组件  │    │ • 定时抓取帧     │                │
│  │ • 部分机型支持   │    │ • 全机型兼容     │                │
│  │ • 流式传输      │    │ • 500ms间隔      │                │
│  └─────────────────┘    └─────────────────┘                │
│                                                             │
│              ┌──────────────┐                               │
│              │  切换按钮     │                               │
│              └──────────────┘                               │
└─────────────────────────────────────────────────────────────┘
```

#### 3.2 快照模式（Snapshot Mode）- 默认模式

由于**微信小程序不支持原生MJPEG流播放**，本项目采用快照模拟视频流的方案作为默认模式：

```
┌──────────┐    500ms间隔     ┌──────────────┐   Base64编码   ┌──────────┐
│ 定时器    │ ──────────────→ │ GET /capture  │ ─────────────→ │ image标签 │
│setInterval│                 │ ?t=timestamp  │   data:image   │ 显示帧    │
└──────────┘                  └──────────────┘   /jpeg;base64  └──────────┘
```

```javascript
// index.js - 快照模式启动
startSnapshotMode() {
  this.stopSnapshotMode()
  this.fetchSnapshotFrame()  // 立即获取第一帧
  
  this.snapshotInterval = setInterval(() => {
    if (!this.data.videoPaused && this.data.videoStreamStarted) {
      this.fetchSnapshotFrame()  // 每500ms获取一帧（优化后）
    }
  }, 500)
}
```

关键参数：
- **刷新间隔**：500ms（约20fps，优化后的流畅度）
- **防缓存**：URL附加 `?t=Timestamp` 参数
- **响应类型**：`arraybuffer` → `wx.arrayBufferToBase64()` → `data:image/jpeg;base64,...`

#### 3.3 模式切换实现

```javascript
// index.js - 视频模式切换
toggleVideoMode() {
  const newMode = !this.data.useSnapshotMode
  
  if (newMode) {
    // 切换到快照模式
    this.setData({ useSnapshotMode: true })
    this.startSnapshotMode()
  } else {
    // 切换到视频流模式
    this.setData({ useSnapshotMode: false })
    this.stopSnapshotMode()
    // 尝试使用video组件播放
  }
  
  wx.showToast({
    title: newMode ? '已切换到快照模式' : '已切换到视频流模式',
    icon: 'none'
  })
}
```

#### 3.4 错误处理和自动降级

当视频流模式失败时，系统会自动降级到快照模式：

```javascript
// index.js - 流错误处理
handleStreamError(e) {
  if (!this.data.useSnapshotMode) {
    console.log('视频流失败，自动切换到快照模式')
    this.setData({ useSnapshotMode: true })
    this.startSnapshotMode()
  } else {
    this.setData({ streamStatus: '连接失败', isOnline: false })
  }
}
```

---

### 4. 拍照功能

拍照功能从ESP32的 `/capture` 接口获取JPEG图像：

```javascript
// index.js - 拍照流程
takePhoto() {
  const url = `http://${esp32Ip}:${esp32Port}/capture`
  
  wx.request({
    url: url,
    method: 'GET',
    responseType: 'arraybuffer',   // 以二进制接收
    timeout: 10000,                 // 10秒超时
    success: (res) => {
      const base64 = wx.arrayBufferToBase64(res.data)
      const photoUrl = `data:image/jpeg;base64,${base64}`
      const timestamp = new Date().toLocaleString('zh-CN')
      
      this.setData({
        capturedImage: photoUrl,
        captureTime: timestamp,
        showCapturedImage: true
      })
    }
  })
}
```

---

### 5. AI识别系统（三模式路由）

支持三种AI识别模式，通过统一的路由逻辑智能分发请求：

#### 5.1 支持的AI模型

| 模式 | 支持的模型 | 适用场景 |
|------|-----------|----------|
| **LLM模式** | OpenAI GPT-4o/mini、通义千问、Kimi、DeepSeek、豆包等10+ | 图像理解、问答对话 |
| **YOLO模式** | HuggingFace DETR/YOLOS | 目标检测、物体识别 |
| **本地模式** | 自部署AI服务 | 离线推理、隐私保护 |

#### 5.2 模式路由逻辑

```javascript
// index.js - AI模式路由
async sendToAI(imageBase64, question) {
  const mode = this.data.aiMode  // 'llm' | 'yolo' | 'local'
  
  switch(mode) {
    case 'llm':
      return await aiApiService.callLLM(imageBase64, question, this.data.selectedModel)
    case 'yolo':
      return await aiApiService.callYOLO(imageBase64)
    case 'local':
      return await aiApiService.callLocal(imageBase64, question)
  }
}
```

---

### 6. 外设控制面板（tab-1）

外设控制面板集成在主页面的Tab 1中，提供四路外设的快捷控制。

#### 6.1 支持的外设类型

| 外设 | 操作 | 特殊功能 |
|------|------|----------|
| 🔌 **继电器** | ON/OFF/Toggle | 状态指示灯 |
| 🔔 **蜂鸣器** | ON/OFF/Beep | **自定义频率（100-10000Hz）** |
| ⚡ **电机** | Stop/Run | PWM调速（0-255），**精确数值输入** |
| 🎯 **舵机** | Attach/Detach/Write | 角度控制（0-180°），**精确数值输入** |

#### 6.2 蜂鸣器频率控制（v1.0新增）

**预设频率选项：**
- 500Hz / 1000Hz / 2000Hz / 3000Hz / 5000Hz

**自定义频率输入：**
- 输入范围：100-10000Hz
- 点击SET按钮应用
- 实时同步到硬件

**频率同步机制：**
```javascript
// index.js - 频率变化实时同步
onFreqChange(e) {
  const newIndex = e.detail.value
  const newFreq = parseInt(this.data.freqOptions[newIndex])
  
  this.setData({ selectedFreqIndex: newIndex })
  
  // 如果蜂鸣器正在播放，立即应用新频率
  if (this.data.buzzerState && this.data.isOnline) {
    this.controlActuator('buzzer', 'on', { freq: newFreq })
  }
}

applyBuzzerFreqInput() {
  const value = parseInt(this.data.buzzerFreqInput)
  
  // 验证频率范围
  if (isNaN(value) || value < 100 || value > 10000) {
    wx.showToast({ title: '频率范围100-10000Hz', icon: 'none' })
    return
  }
  
  // 更新选项列表并选中
  const newOptions = [...this.data.freqOptions]
  if (!newOptions.includes(String(value))) {
    newOptions.push(String(value))
    newOptions.sort((a, b) => parseInt(a) - parseInt(b))
  }
  
  this.setData({
    freqOptions: newOptions,
    selectedFreqIndex: newOptions.indexOf(String(value)),
    buzzerFreqInput: ''
  })
  
  // 立即同步到硬件（如果蜂鸣器正在播放）
  if (this.data.buzzerState && this.data.isOnline) {
    this.controlActuator('buzzer', 'on', { freq: value })
  }
  
  wx.showToast({ 
    title: `已设置频率 ${value}Hz${this.data.buzzerState ? ' ✓ 已生效' : ''}`, 
    icon: 'success' 
  })
}
```

#### 6.3 精确数值输入（v1.0修复）

电机和舵机的精确数值输入使用 `catchtap` 替代 `bindtap` 解决事件冒泡问题：

```xml
<!-- control.wxml - 使用catchtap阻止事件冒泡 -->
<view class="input-btn" catchtap="submitMotorInput">▶</view>
<view class="input-btn" catchtap="submitServoInput">▶</view>
```

---

### 7. 独立控制页面 (pages/control/control)

独立的全屏外设控制页面，提供更详细的操作界面和操作历史记录。

#### 7.1 页面布局（v1.0优化）

**继电器控制卡片（重新设计）：**
- 紧凑顶部栏：设备图标 + 名称 + 开关
- 三按钮操作区：ON（绿色）/ OFF（红色）/ Toggle（黄色）
- 状态指示条：实时显示电路接通/断开状态

**蜂鸣器控制卡片（重新设计）：**
- 顶部开关控制长鸣/停止
- 开启时显示当前播放频率（大字体 + 动态光环）
- 频率选择器：预设频率下拉 + 自定义输入框 + SET按钮
- 试听按钮：短促提示音测试
- 频率范围提示：100~10000 Hz

#### 7.2 操作历史记录

每次操作都会记录到历史列表中：

```javascript
// control.js - 记录操作
logOperation(device, action, value) {
  const entry = {
    time: new Date().toLocaleTimeString('zh-CN'),
    device: device,
    action: action,
    value: value || ''
  }
  
  const history = [...this.data.operationHistory, entry]
  // 保持最近50条记录
  if (history.length > 50) {
    history.shift()
  }
  
  this.setData({ operationHistory: history })
}
```

**UI优化（v1.0）：**
- 时间和设备名称不再遮挡（添加text-overflow: ellipsis）
- 固定宽度布局确保对齐
- 操作类型彩色标识

---

### 8. 网络诊断和LLM测试 (pages/index/network_test)

独立的开发和调试工具页面。

#### 8.1 网络诊断功能

按顺序执行以下测试：

1. **WiFi状态检查** - 检测当前网络类型
2. **ESP32连接测试** - GET请求根路径
3. **传感器API测试** - GET /sensors
4. **视频流API测试** - GET /stream
5. **拍照API测试** - GET /capture

#### 8.2 LLM验证测试

- 支持10+主流大模型选择
- 选择模型自动填充API地址
- 实时显示响应时间和Token用量
- 配置保存到本地存储
- 一键同步AI配置到ESP32设备

---

## 工具类模块架构

### ai-api-service.js

> 文件路径: [utils/ai-api-service.js](utils/ai-api-service.js)

**职责：** 封装所有AI模型的HTTP请求逻辑。

**核心能力：**
- HuggingFace YOLO目标检测API调用
- 通义千问LLM/VLM多模态API调用
- 统一错误处理和重试机制（`requestWithRetry`，最多重试2次）
- 请求/响应日志记录（debug模式）
- 错误类型分类（CONFIG_MISSING / AUTH_FAILED / NETWORK_ERROR / TIMEOUT / RATE_LIMIT等）

### actuator-service.js

> 文件路径: [utils/actuator-service.js](utils/actuator-service.js)

**职责：** 封装与ESP32 HTTP外设控制API的所有通信逻辑。

**核心能力：**
- 统一HTTP请求封装（带重试、超时、错误处理）
- 设备类型枚举：RELAY / BUZZER / MOTOR / SERVO
- 操作类型白名单校验（防止非法指令）
- 自动构建请求URL和参数
- 响应解析和数据提取

**支持的设备操作：**

| 设备 | 支持的操作 | 特殊参数 |
|------|-----------|----------|
| relay | on, off, toggle, status | - |
| buzzer | on, off, beep, status | freq（频率，100-10000Hz）|
| motor | stop, run, status | speed（速度，0-255）|
| servo | attach, detach, write, status | angle（角度，0-180）|

### ai-config.js

> 文件路径: [utils/ai-config.js](utils/ai-config.js)

**职责：** 统一管理所有AI模型的API Key、模型名称、API地址等配置。

**支持的模型：**
- HuggingFace: DETR-ResNet-50 / YOLOS-Small / DETR-ResNet-101
- 通义千问: 文本对话 / 视觉理解（阿里云DashScope）

### error-logger.js

> 文件路径: [utils/error-logger.js](utils/error-logger.js)

**职责：** 错误日志系统，提供错误记录、统计和导出功能。

---

## 数据流和状态管理

### 状态管理策略

采用**页面级状态管理** + **本地持久化存储**的组合方案：

```
┌─────────────────────────────────────────────────────────────┐
│                      状态层次                                │
├─────────────────────┬───────────────────────────────────────┤
│   app.js 全局状态    │          Page 页面状态                 │
│   • esp32Url        │   • sensorData（传感器数据）           │
│   • esp32Ip         │   • isOnline（连接状态）               │
│   • esp32Port       │   • videoStreamStarted（视频流状态）    │
│   • aiServiceUrl    │   • buzzerState（蜂鸣器状态）          │
│                     │   • relayState（继电器状态）            │
│                     │   • selectedFreqIndex（选中频率索引）    │
└─────────────────────┴───────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              wx.setStorageSync 持久化存储                     │
│   • esp32_config（IP/端口配置）                               │
│   • alarm_threshold（报警阈值）                               │
│   • ai_test_config（AI测试配置）                              │
└─────────────────────────────────────────────────────────────┘
```

### 数据更新流程

```
定时器触发（3s间隔）
    ↓
fetchSensorData() → wx.request(GET /sensors)
    ↓
响应成功 → JSON解析 → 数据校验
    ↓
setData({ sensorData: newData }) → WXML数据绑定更新
    ↓
趋势计算 → 进度条更新 → 报警检查
```

---

## 性能优化策略

### 1. 请求优化

| 策略 | 实现方式 | 效果 |
|------|----------|------|
| **轮询间隔调整** | 传感器数据3s，视频快照500ms | 平衡实时性和性能 |
| **请求合并** | 单次/sensors请求获取全部数据 | 减少HTTP请求数 |
| **防抖处理** | 用户输入防抖300ms | 避免频繁请求 |
| **缓存策略** | URL时间戳参数防止缓存 | 确保数据新鲜度 |

### 2. 渲染优化

| 策略 | 实现方式 | 效果 |
|------|----------|------|
| **Canvas离屏渲染** | 使用新版Canvas 2D API | 提升绘制性能 |
| **数据量限制** | SensorHistory最多100个点 | 控制内存占用 |
| **移动平均滤波** | 窗口大小5的滑动平均 | 平滑曲线减少绘制点 |
| **按需绘制** | 仅在数据变化时重绘 | 减少不必要的渲染 |

### 3. 资源管理

| 策略 | 实现方式 | 效果 |
|------|----------|------|
| **生命周期管理** | onHide停止定时器，onShow重启 | 节省后台资源 |
| **内存清理** | onUnload清除所有定时器和监听器 | 防止内存泄漏 |
| **图片懒加载** | 仅在查看时加载拍照结果 | 减少内存占用 |

---

## 配置说明

### ESP32连接配置

在小程序首次使用时需要配置ESP32设备的IP地址：

```javascript
// app.js 或设置页
App({
  globalData: {
    esp32Url: 'http://192.168.1.100:80',
    esp32Ip: '192.168.1.100',
    esp32Port: 80
  }
})
```

**配置方式：**
1. 在主页面点击设置按钮手动输入
2. 通过network_test页配置并保存
3. 配置会保存到本地存储，下次自动加载

### AI模型配置

在使用AI识别功能前需要配置API密钥：

**支持的AI服务商：**

| 服务商 | 模型列表 | API地址格式 |
|--------|----------|-------------|
| OpenAI | GPT-4o, GPT-4o-mini | https://api.openai.com/v1/chat/completions |
| 通义千问 | Qwen-Turbo, Qwen-Plus, Qwen-Max | https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions |
| Kimi | moonshot-v1 | https://api.moonshot.cn/v1/chat/completions |
| DeepSeek | deepseek-chat, deepseek-reasoner | https://api.deepseek.com/v1/chat/completions |
| 豆包 | doubao-pro, doubao-lite | https://ark.cn-beijing.volces.com/api/v3/chat/completions |

---

## 开发指南

### 环境搭建

1. **安装微信开发者工具**
   - 下载地址：https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html

2. **导入项目**
   - 打开微信开发者工具
   - 选择"导入项目"
   - 选择 `web_frontend/wechat_miniprogram` 目录
   - 填写或申请AppID（测试号也可）

3. **编译运行**
   - 点击"编译"按钮
   - 在模拟器中预览
   - 或扫码在真机上调试

### 开发规范

1. **代码风格**
   - 使用2空格缩进
   - 变量命名采用camelCase
   - 常量命名采用UPPER_SNAKE_CASE

2. **文件组织**
   - 页面逻辑放在对应页面的.js文件中
   - 公共工具函数放在utils/目录
   - 样式文件与页面同名

3. **错误处理**
   - 所有异步操作使用try-catch包裹
   - 网络请求统一通过Service层封装
   - 错误信息通过error-logger记录

### 调试技巧

1. **查看日志**
   - 微信开发者工具Console面板
   - 真机调试：vconsole开启调试模式

2. **网络调试**
   - Network面板查看HTTP请求
   - 使用network_test页进行连通性测试

3. **性能分析**
   - Performance面板分析渲染性能
   - Audits面板获取优化建议

---

## API接口文档

### ESP32 HTTP API

所有API的基础URL：`http://{esp32_ip}:{port}`

#### 传感器相关

| 端点 | 方法 | 说明 | 返回示例 |
|------|------|------|----------|
| `/sensors` | GET | 获取所有传感器数据 | `{temperature:25.3,humidity:60.5,mq2:120,water_level:15.2}` |
| `/status` | GET | 获取系统状态 | `{uptime:3600,free_heap:200000,...}` |

#### 视频相关

| 端点 | 方法 | 说明 | 返回类型 |
|------|------|------|----------|
| `/stream` | GET | MJPEG视频流 | multipart/x-mixed-replace |
| `/capture` | GET | 拍摄单张JPEG照片 | image/jpeg (binary) |

#### AI相关

| 端点 | 方法 | 说明 | 请求体 |
|------|------|------|--------|
| `/ask` | POST | AI视觉识别 | `{image:"base64...",question:"...",model:"..."}` |
| `/config` | POST | 同步AI配置 | `{ai_api_url:"",ai_api_key:"",ai_model:"..."}` |

#### 外设控制

| 端点 | 方法 | 说明 | 参数 |
|------|------|------|------|
| `/actuator` | GET/POST | 外设控制 | `device`,`action`,可选:`freq`,`speed`,`angle` |

**设备操作示例：**

```bash
# 继电器ON
curl "http://192.168.1.100/actuator?device=relay&action=on"

# 蜂鸣器开启（频率2000Hz）
curl "http://192.168.1.100/actuator?device=buzzer&action=on&freq=2000"

# 电机运行（速度128）
curl "http://192.168.1.100/actuator?device=motor&action=run&speed=128"

# 舵机转动到90度
curl "http://192.168.1.100/actuator?device=servo&action=write&angle=90"
```

---

## 常见问题

### Q1: 微信小程序无法显示视频流？

**A:** 微信小程序不支持原生MJPEG流播放。本项目默认使用**快照模式**：
- 定时抓取JPEG帧（500ms间隔）
- v1.0新增切换按钮，可尝试视频流模式（部分机型支持）
- 视频流模式失败时会自动降级到快照模式

### Q2: 蜂鸣器频率设置不生效？

**A:** 请按以下步骤排查：
1. 确认输入频率在100-10000Hz范围内
2. 点击SET按钮后查看是否有成功提示
3. **关键**：如果蜂鸣器正在播放，频率会立即生效；如果关闭，会在下次开启时生效
4. 查看控制台日志确认HTTP请求是否发送成功
5. 确认ESP32端的web_server.cpp正确解析了freq参数

### Q3: 电机和舵机数值输入按钮无响应？

**A:** 这是v1.0已修复的问题。原因是**事件冒泡**导致点击事件被父元素拦截。
- **解决方案**：使用`catchtap`替代`bindtap`阻止事件冒泡
- 如果仍有问题，请确认使用的是最新版本的control.wxml

### Q4: 切换到快照模式后无法切回？

**A:** v1.0已修复此问题。确保：
1. 使用的是v1.0及以上版本
2. toggleVideoMode函数正确实现了状态切换
3. 切换按钮的bindtap事件正确绑定

### Q5: 操作记录的时间和名称被遮挡？

**A:** v1.0已优化CSS样式解决此问题：
- 为.entry-time和.entry-device设置了固定宽度和text-overflow: ellipsis
- 使用flex布局确保元素不重叠
- 如果仍有问题，请检查control.wxss中的相关样式

### Q6: 大模型返回文本右侧被遮挡？

**A:** v1.0已修复此问题：
- 调整了AI回复区域的max-width和padding
- 确保文本可以正常换行显示
- 添加了word-break: break-word防止长单词溢出

### Q7: 如何配置AI模型？

**A:** 有两种方式：
1. **主页面设置**：在AI识别Tab中配置API Key和选择模型
2. **network_test页**：
   - 选择预设模型（自动填充API地址）
   - 或手动输入完整的API地址和Key
   - 点击"测试"验证连通性
   - 点击"同步到ESP32"保存配置

### Q8: 数据更新不及时？

**A:** 检查以下几点：
1. 确认ESP32设备在线且IP地址正确
2. 查看网络请求是否正常（Console面板）
3. 当前轮询间隔为3秒（已优化），如需更快可以调整但会增加负载
4. 检查是否在后台（onHide会停止定时器）

---

## 更新日志

### v1.0 (2026-04-18)

**新增功能：**
- ✨ 视频流/快照模式切换按钮，可在两种模式间自由切换
- ✨ 蜂鸣器自定义频率输入功能（100-10000Hz范围）
- ✨ 蜂鸣器频率实时同步机制（变化立即生效）
- ✨ 继电器控制界面重新设计（紧凑布局，三按钮操作）
- ✨ 蜂鸣器控制界面重新设计（频率显示环 + 实时状态）

**Bug修复：**
- 🐛 修复电机和舵机精确数值输入按钮无响应问题（事件冒泡）
- 🐛 修复蜂鸣器SET按钮设置后频率不生效的问题
- 🐛 修复视频流模式无法切回快照模式的问题
- 🐛 修复控制页面操作记录时间和名称遮挡问题
- 🐛 修复大模型返回文本右侧被遮挡问题

**优化改进：**
- ⚡ 快照模式刷新间隔从800ms优化到500ms（更流畅）
- ⚡ 传感器数据轮询间隔从2s优化到3s（平衡性能）
- ⚡ UI交互体验全面优化
- 🧹 清理冗余代码和无用文件

### v4.0 (2026-04-17)

- 🎉 初始版本发布
- ✨ 实现完整功能：监控、视频、AI、外设控制、图表
- ✨ Canvas双Y轴图表系统（Catmull-Rom样条插值）
- ✨ 三模式AI识别路由（LLM/YOLO/本地）
- ✨ 网络诊断和LLM验证测试工具

---

## 相关链接

- **主项目README**: [../README.md](../README.md)
- **ESP32固件文档**: [../../esp32_firmware/README.md](../../esp32_firmware/README.md)
- **Web前端文档**: [../frontend/README.md](../frontend/README.md)

---

**🎯 版本：v1.0 | 最后更新：2026-04-18 | 状态：活跃开发中**
