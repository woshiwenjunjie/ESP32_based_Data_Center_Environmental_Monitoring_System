# Web前端项目 v1.0

> **前端技术文档** | HTML5 + CSS3 + JavaScript + 微信小程序

---

## 项目概述

ESP32环境监测系统的Web前端界面，提供实时视频流、传感器数据展示、AI识别、外设控制等功能。支持**浏览器访问**和**微信小程序**两种使用方式。

| 属性 | 说明 |
|------|------|
| 版本 | v1.0 (增强外设控制 + 视频模式切换) |
| 技术栈 | HTML5, CSS3, JavaScript ES6+, 微信小程序框架 |
| 兼容性 | Chrome 90+, Firefox 88+, Safari 14+, Edge 90+ |
| 更新日期 | 2026-04-18 |

### v1.0 更新亮点

- ✨ **视频流/快照模式切换**：解决微信小程序MJPEG限制问题
- ✨ **蜂鸣器频率自定义**：100-10000Hz范围可调
- ✨ **外设控制UI优化**：继电器和蜂鸣器界面重新设计
- ✨ **LLM模型切换**：支持多模型动态选择
- 🐛 **Bug修复**：修复频率同步、事件冒泡等问题
- 📝 **文档完善**：全面更新API和使用说明

---

## 功能特性

### 核心功能模块

#### 1️⃣ 视频监控模块（双模式架构）

**浏览器版本：**
- ✅ MJPEG实时视频流（原生支持）
- ✅ 暂停/继续视频功能
- ✅ 自动重连机制
- ✅ 高清拍照（支持多种分辨率）

**微信小程序版本：**
- ⚠️ **不支持原生MJPEG流**（微信小程序限制）
- ✅ **快照模式**：定时抓取JPEG帧模拟视频效果
- ✅ **模式切换按钮**：用户可选择流畅度或清晰度
- ✅ 智能刷新策略：
  - **视频流模式**：200ms间隔（流畅优先）
  - **快照模式**：2000ms间隔（省电优先）

**实现原理：**
```javascript
// 微信小程序 - 快照模式实现
async captureSnapshot() {
    try {
        const res = await wx.request({
            url: `${this.data.esp32Url}/capture?format=json`,
            method: 'GET',
            responseType: 'arraybuffer'
        })
        
        if (res.statusCode === 200) {
            const base64 = wx.arrayBufferToBase64(res.data)
            this.setData({ 
                videoSrc: 'data:image/jpeg;base64,' + base64 
            })
        }
    } catch (err) {
        console.error('快照失败:', err)
    }
}

// 模式切换逻辑
toggleVideoMode() {
    const newMode = this.data.videoMode === 'stream' ? 'snapshot' : 'stream'
    
    // 停止当前模式
    if (this.snapshotTimer) {
        clearInterval(this.snapshotTimer)
        this.snapshotTimer = null
    }
    
    // 启动新模式
    if (newMode === 'snapshot') {
        this.startSnapshotMode()  // 2秒间隔
    } else {
        this.startStreamMode()    // 200ms间隔
    }
}
```

#### 2️⃣ 传感器数据监控

- 实时数据展示（温度/湿度/烟雾/气体/水位）
- 5秒自动刷新（可配置）
- 数据格式化和单位转换
- 连接状态可视化指示
- 历史数据趋势图表（可选）

**数据格式示例：**
```json
{
    "temperature": 25.6,
    "humidity": 65.3,
    "smoke": 0,
    "gas": 320,
    "water": 512,
    "timestamp": "2026-04-18T10:30:00Z"
}
```

#### 3️⃣ AI视觉识别系统

**支持的AI服务商（OpenAI兼容API）：**

| 服务商 | 模型示例 | 特点 |
|--------|---------|------|
| OpenAI | GPT-4o, GPT-4o-Mini | 综合能力最强 |
| 阿里云 | 通义千问 Qwen-Turbo | 中文优化 |
| 月之暗面 | Kimi | 长文本处理 |
| DeepSeek | Chat, Reasoner | 推理能力强 |
| 火山引擎 | 豆包 Pro/Lite | 性价比高 |

**功能特性：**
- 自定义问题输入
- 图像Base64编码传输
- 多模型动态切换
- 错误处理和自动重试
- 响应时间显示

**请求流程：**
```
用户输入问题 → 选择照片/实时捕获 → Base64编码 → 
发送到AI服务 → 解析响应 → 显示结果
```

#### 4️⃣ 外设控制系统（v1.0增强）

**支持的设备：**

| 设备 | 控制方式 | 特殊参数 |
|------|---------|----------|
| 继电器 | 开关/切换 | - |
| 蜂鸣器 | 开关/试听 | **频率(100-10000Hz)** |
| 直流电机 | 速度调节(0-255) | PWM占空比 |
| SG90舵机 | 角度控制(0-180°) | 精确角度 |

**蜂鸣器频率控制（v1.0核心功能）：**

```javascript
// 微信小程序 - 蜂鸣器控制
onBuzzerToggle(e) {
    const turnOn = e.detail.value
    const freq = parseInt(this.data.freqOptions[this.data.selectedFreqIndex])
    
    if (turnOn) {
        // 使用当前选择的频率开启蜂鸣器
        this.controlActuator('buzzer', 'on', { freq })
    } else {
        this.controlActuator('buzzer', 'off')
    }
}

// 应用自定义频率
applyBuzzerFreqInput() {
    const value = parseInt(this.data.buzzerFreqInput)
    
    // 验证范围：100-10000Hz
    if (value < 100 || value > 10000) {
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
    
    // 如果蜂鸣器正在播放，立即应用新频率
    if (this.data.buzzerState && this.data.isOnline) {
        this.controlActuator('buzzer', 'on', { freq: value })
    }
    
    wx.showToast({ title: `已设置 ${value}Hz`, icon: 'success' })
}
```

**预置频率选项：**
```javascript
freqOptions: ['500', '1000', '1500', '2000', '2500', '3000', '4000', '5000']
// 用户可通过SET按钮添加自定义频率
```

#### 5️⃣ 操作日志记录

- 实时显示操作历史
- 时间戳精确到秒
- 设备名称和操作类型分类
- 成功/失败状态标识
- 自动滚动到最新记录

---

## 项目结构

```
web_frontend/
│
├── frontend/                    # 🌐 浏览器端Web界面
│   └── index.html              # 主页面（HTML+CSS+JS一体化）
│       ├── 视频流显示区域
│       ├── 传感器数据面板
│       ├── AI识别交互区
│       └── 外设控制面板
│
├── wechat_miniprogram/          # 📱 微信小程序
│   ├── app.js                  # 小程序入口
│   ├── app.json                # 全局配置
│   ├── app.wxss                # 全局样式
│   │
│   ├── pages/
│   │   ├── index/              # 🏠 主页面
│   │   │   ├── index.wxml      # 页面结构（含视频+AI）
│   │   │   ├── index.wxss      # 页面样式
│   │   │   └── index.js        # 页面逻辑（含模式切换）
│   │   │
│   │   ├── control/            # 🎮 控制页面
│   │   │   ├── control.wxml    # 外设控制UI（v1.0重设计）
│   │   │   ├── control.wxss    # 控制页面样式
│   │   │   └── control.js      # 控制逻辑
│   │   │
│   │   └── settings/           # ⚙️ 设置页面
│   │       ├── settings.wxml   # 配置界面
│   │       ├── settings.wxss   # 设置样式
│   │       └── settings.js     # 设置逻辑
│   │
│   ├── utils/                  # 🔧 工具库
│   │   ├── actuator-service.js # ⭐ 外设控制服务（含频率参数）
│   │   ├── ai-service.js       # AI服务封装
│   │   ├── sensor-service.js   # 传感器数据处理
│   │   └── video-service.js    # ⭐ 视频服务（双模式）
│   │
│   └── README.md               # 小程序详细文档
│
└── README.md                   # 本文档
```

---

## 配置说明

### 服务器地址配置

**浏览器版本（frontend/index.html）：**
```javascript
// ESP32服务器地址（需要根据实际情况修改）
const ESP32_URL = 'http://192.168.1.100';

// 大模型服务地址
const AI_SERVICE_URL = 'http://localhost:5000';
```

**微信小程序版本：**
```javascript
// pages/settings/settings.js 或 app.js
globalData: {
    esp32Url: 'http://192.168.1.100',  // ESP32主机IP
    aiServiceUrl: 'http://your-server.com',  // AI服务地址
    selectedModel: 'gpt-4o-mini',  // 默认AI模型
    videoMode: 'stream'  // 默认视频模式
}
```

### 视频模式配置（v1.0新增）

**微信小程序 - 刷新率配置：**
```javascript
// utils/video-service.js
const VIDEO_MODE_CONFIG = {
    stream: {
        interval: 200,          // 流畅模式：200ms
        description: '高帧率模式',
        batteryImpact: '较高'
    },
    snapshot: {
        interval: 2000,         // 省电模式：2000ms
        description: '省电模式',
        batteryImpact: '较低'
    }
}
```

### 蜂鸣器配置（v1.0新增）

**频率范围限制：**
```javascript
const BUZZER_CONFIG = {
    minFreq: 100,              // 最小频率 100Hz
    maxFreq: 10000,            // 最大频率 10000Hz
    defaultFreq: 2000,         // 默认频率 2000Hz
    presetFrequencies: [500, 1000, 1500, 2000, 2500, 3000, 4000, 5000],
    frequencyTips: {
        low: '500-1000Hz 低沉音效',
        normal: '1000-3000Hz 标准音效',
        high: '3000-5000Hz 清脆音效',
        ultra: '5000-10000Hz 高频音调'
    }
}
```

---

## API接口文档

### 1. 视频相关接口

| 方法 | 路径 | 说明 | 参数 |
|------|------|------|------|
| GET | `/stream` | MJPEG视频流 | - |
| GET | `/capture` | 单张拍照 | `resolution`, `quality`, `format` |

**拍照接口示例：**
```
GET /capture?resolution=VGA&quality=10&format=json
Response: { "image": "base64...", "size": 45678, "timestamp": "..." }
```

### 2. 传感器接口

| 方法 | 路径 | 说明 | 参数 |
|------|------|------|------|
| GET | `/sensors` | 获取所有传感器数据 | `field`(可选筛选) |

**响应示例：**
```json
{
    "temperature": 25.6,
    "humidity": 65.3,
    "smoke": 0,
    "gas": 320,
    "water": 512,
    "online": true
}
```

### 3. 外设控制接口（v1.0增强）

| 方法 | 路径 | 说明 | 参数 |
|------|------|------|------|
| GET | `/actuator` | 控制外设 | `device`, `action`, `freq/speed/angle` |

**蜂鸣器控制示例：**
```
# 开启蜂鸣器（默认频率2000Hz）
GET /actuator?device=buzzer&action=on

# 开启蜂鸣器（自定义频率3500Hz）
GET /actuator?device=buzzer&action=on&freq=3500

# 关闭蜂鸣器
GET /actuator?device=buzzer&action=off

# 试听
GET /actuator?device=buzzer&action=beep

# 查询状态
GET /actuator?device=buzzer&action=status
```

**其他外设控制示例：**
```
# 继电器控制
GET /actuator?device=relay&action=on
GET /actuator?device=relay&action=off
GET /actuator?device=relay&action=toggle

# 电机控制
GET /actuator?device=motor&action=run&speed=128
GET /actuator?device=motor&action=stop

# 舵机控制
GET /actuator?device=servo&action=write&angle=90
GET /actuator?device=servo&action=attach
GET /actuator?device=servo&action=detach
```

### 4. AI识别接口

| 方法 | 路径 | 说明 | 参数 |
|------|------|------|------|
| POST | `/ask` | AI视觉识别 | `question`, `image`(可选), `resolution` |

**请求示例：**
```json
{
    "question": "描述这张图片中的物体",
    "resolution": "VGA"
}
```

**响应示例：**
```json
{
    "success": true,
    "answer": "图片中显示了一个室内环境...",
    "model": "gpt-4o-mini",
    "process_time_ms": 2340
}
```

---

## 使用方法

### 浏览器版本

#### 本地运行
1. 确保ESP32已启动并连接网络
2. （可选）启动大模型服务
3. 修改 [frontend/index.html](frontend/index.html) 中的服务器地址
4. 直接在浏览器打开文件

#### 部署到服务器
1. 上传 [frontend/index.html](frontend/index.html) 到Web服务器
2. 配置HTTPS（推荐）
3. 配置CORS跨域访问
4. （可选）配置CDN加速

### 微信小程序版本

#### 开发环境准备
1. 安装[微信开发者工具](https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html)
2. 注册微信小程序账号（如需真机调试）
3. 导入项目目录：`web_frontend/wechat_miniprogram`

#### 配置步骤
1. 打开 [pages/settings/settings.js](wechat_miniprogram/pages/settings/settings.js)
2. 修改 `esp32Url` 为你的ESP32主机IP
3. （可选）修改 `aiServiceUrl` 和 `selectedModel`
4. 编译运行

#### 真机测试
1. 在微信开发者工具中点击"预览"
2. 手机扫码打开小程序
3. 确保手机与ESP32在同一WiFi环境
4. 测试各项功能

---

## UI设计规范（v1.0更新）

### 设计原则

1. **移动优先**：针对手机屏幕优化布局
2. **触摸友好**：按钮尺寸≥44px，间距合理
3. **状态反馈**：操作后立即给出视觉反馈
4. **信息层级**：重要信息突出显示
5. **色彩语义**：
   - 🟢 绿色 = 正常/成功/开启
   - 🔴 红色 = 异常/危险/关闭
   - 🔵 蓝色 = 信息/进行中
   - 🟡 黄色 = 警告/待定

### 外设控制卡片设计（v1.0）

**继电器卡片布局：**
```
┌─────────────────────────────┐
│ ⏻ 继电器          [开关]   │ ← 顶部栏（紧凑）
├─────────────────────────────┤
│                             │
│   [ ON ]  [ OFF ]  [ ↻ ]   │ ← 操作按钮组
│                             │
│   ● 电路接通                 │ ← 状态指示线
└─────────────────────────────┘
```

**蜂鸣器卡片布局：**
```
┌─────────────────────────────┐
│ ♫ 蜂鸣器          [开关]   │ ← 顶部栏
│                             │
│        ╭─────╮              │
│        │2000 │              │ ← 当前频率显示（开启时）
│        ╰─────╯              │
│          Hz 播放中           │
├─────────────────────────────┤
│ [▼2000Hz] [自定义__] [SET]  │ ← 频率控制行
│ 范围 100~10000 Hz           │ ← 提示文字
│                             │
│       [ ♪ 试听 ]             │ ← 试听按钮
└─────────────────────────────┘
```

---

## 性能优化策略

### 1. 视频性能优化

**浏览器版本：**
- 使用MJPEG原生流（低延迟）
- 图片缓存减少重复请求
- 自动暂停节省带宽

**微信小程序版本：**
- ⚡ 快照模式降低CPU/GPU占用
- 📱 可调节刷新率平衡流畅度和功耗
- 💾 缓存最近帧避免闪烁
- 🔄 智能重连机制

### 2. 网络请求优化

- 批量合并传感器数据请求
- 节流控制避免频繁调用
- 请求队列管理避免并发过多
- 失败指数退避重试策略

### 3. 内存管理

- 及时释放不用的图片对象
- 避免内存泄漏（定时器清理）
- 使用轻量级数据结构
- 监控内存占用情况

---

## 故障排除指南

### 常见问题及解决方案

#### ❌ 问题1：微信小程序视频无法显示

**原因分析：**
微信小程序不支持MJPEG视频流协议。

**解决方案：**
✅ 使用**快照模式**替代（v1.0已实现自动降级）
✅ 点击**模式切换按钮**选择合适模式
✅ 检查网络连接是否正常

**调试步骤：**
1. 打开微信开发者工具控制台
2. 查看是否有错误输出
3. 确认ESP32_IP地址正确
4. 尝试手动触发快照

---

#### ❌ 问题2：蜂鸣器频率设置不生效

**可能原因：**
1. HTTP请求未包含freq参数
2. 频率值超出有效范围
3. 后端代码未更新到v1.0

**解决方案：**
✅ 确认使用最新版前端代码
✅ 检查串口日志确认freq参数被解析
✅ 验证频率在100-10000Hz范围内
✅ 重启ESP32设备

**调试代码：**
```javascript
// 在control.js中添加临时日志
console.log('[Debug] 发送蜂鸣器命令:', {
    device: 'buzzer',
    action: 'on',
    freq: targetFreq
})
```

---

#### ❌ 问题3：操作按钮无反应

**原因分析：**
事件冒泡导致点击被父元素拦截。

**解决方案：**
✅ 使用 `catchtap` 替代 `bindtap`（v1.0已修复）
✅ 检查按钮是否被 `disabled` 属性禁用
✅ 确认设备在线状态

**代码对比：**
```xml
<!-- ❌ 错误写法 -->
<button bindtap="submitMotorInput">提交</button>

<!-- ✅ 正确写法（阻止冒泡）-->
<button catchtap="submitMotorInput">提交</button>
```

---

#### ❌ 问题4：电机运转时舵机导致断连

**根本原因：**
电源供应不足，电流需求超过USB供电能力。

**技术细节：**
- 电机全速运行：~500mA-1A
- 舵机动作瞬间：~200mA-500mA
- USB供电上限：通常500mA-1A
- 总需求：可能超过供电极限

**解决方案：**
🔋 使用独立外部电源（5V/2A以上）
⚡ 并联1000μF电解电容缓冲
⚙️ 软件限流：避免同时满负荷运行
⏱️ 分时操作：错开电机和舵机的峰值电流

---

#### ❌ 问题5：AI识别返回慢或失败

**排查步骤：**
1. 确认AI服务正常运行
2. 检查API Key是否有效
3. 查看网络延迟情况
4. 尝试切换到更快的模型

**优化建议：**
- 使用GPT-4o-Mini代替GPT-4o（速度快10倍）
- 降低图像分辨率（QVGA代替VGA）
- 启用缓存机制避免重复请求

---

## 安全建议

1. **网络安全**：
   - 内网使用为主，避免公网暴露
   - 必须外网访问时使用VPN
   - 配置防火墙规则限制访问

2. **数据安全**：
   - API Key不要硬编码在前端代码
   - 使用环境变量存储敏感信息
   - 启用HTTPS加密传输

3. **物理安全**：
   - ESP32设备放置在安全位置
   - 定期检查设备状态
   - 及时更新固件版本

---

## 浏览器兼容性

| 浏览器 | 最低版本 | MJPEG支持 | WebSocket | 完整功能 |
|--------|---------|-----------|-----------|---------|
| Chrome | 90+ | ✅ | ✅ | ✅ |
| Firefox | 88+ | ✅ | ✅ | ✅ |
| Safari | 14+ | ✅ | ✅ | ⚠️ (部分) |
| Edge | 90+ | ✅ | ✅ | ✅ |
| 微信内置浏览器 | 最新版 | ❌ | ⚠️ | ✅ (快照模式) |

> **注意**：微信小程序使用快照模式替代MJPEG流，功能完整但刷新率受限。

---

## 更新日志

### v1.0 (2026-04-18)

**新增功能：**
- ✨ 视频流/快照双模式架构（解决微信小程序限制）
- ✨ 蜂鸣器自定义频率控制（100-10000Hz）
- ✨ LLM多模型动态切换功能
- ✨ 外设控制UI全面重新设计
- ✨ 操作日志实时记录功能
- ✨ 频率预设选项和自定义输入

**Bug修复：**
- 🐛 修复事件冒泡导致按钮无响应问题
- 🐛 修复蜂鸣器频率设置不同步问题
- 🐛 修复视频模式切换失效问题
- 🐛 优化异步错误处理机制

**性能优化：**
- ⚡ 快照模式智能刷新策略
- ⚡ 网络请求节流和批量处理
- ⚡ 内存泄漏防护
- ⚡ 移动端触摸体验优化

**文档完善：**
- 📝 新增完整的API接口文档
- 📝 新增故障排除指南
- 📝 新增UI设计规范说明
- 📝 新增性能优化策略文档

### v4.1 (2026-04-17)

- ✨ 优化传感器数据显示格式
- 🐛 修复AI识别响应截断问题
- 📝 补充使用说明文档

### v4.0 (2026-04-16)

- 🎉 初始版本发布
- ✨ 实现基础视频流功能
- ✨ 传感器数据展示
- ✨ AI识别集成
- ✨ 基础外设控制

---

## 相关资源

- **主项目文档**: [../README.md](../README.md)
- **ESP32固件文档**: [../esp32_firmware/README.md](../esp32_firmware/README.md)
- **微信小程序文档**: [wechat_miniprogram/README.md](wechat_miniprogram/README.md)
- **AI服务文档**: [../ai_service/README.md](../ai_service/README.md)

---

## 开发团队

**现代交换技术实验项目**

- 嵌入式系统开发：ESP32主从通信架构
- Web前端开发：响应式UI设计
- 微信小程序开发：跨平台移动应用
- AI集成：多模型视觉识别系统

---

## 许可证

MIT License

Copyright (c) 2026 现代交换技术实验项目

---

**🎯 版本：v1.0 | 最后更新：2026-04-18 | 状态：活跃开发中**
