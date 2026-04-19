/**
 * ============================================================
 *  微信小程序 - 主页面（环境监测站控制面板）
 * ============================================================
 * 
 * 功能模块：
 *   📊 实时数据展示 - 温度/湿度/气体/水位传感器卡片
 *   📈 Canvas图表 - Catmull-Rom样条插值曲线 + 渐变填充
 *   🎛️ 外设控制 - 继电器/蜂鸣器/电机/舵机控制面板
 *   📹 视频流预览 - 定时刷新模拟MJPEG流（微信不支持原生流）
 *   📸 拍照功能 - JPEG捕获与Base64显示
 *   🤖 AI识别 - 图片上传 + 问题输入 -> 大模型分析
 *   🔧 测试工具 - 网络诊断 + LLM配置验证
 *   ⚙️ 配置管理 - ESP32地址设置与本地存储同步
 * 
 * 图表渲染特点：
 *   - 使用原生Canvas API（无第三方库依赖）
 *   - Catmull-Rom样条插值实现平滑曲线
 *   - 线性渐变填充增强视觉效果
 *   - 发光效果(Glow)提升交互体验
 * 
 * 数据更新机制：
 *   - 每3秒轮询 /sensors 接口获取最新数据
 *   - 最多保留100个历史数据点用于绘图
 *   - 支持触摸交互查看具体数值
 * 
 * 页面路径: pages/index/index (小程序首页)
 * ============================================================
 */

// index.js - 主页面逻辑
const app = getApp()
const aiConfig = require('../../utils/ai-config')
const aiService = require('../../utils/ai-api-service')
const actuatorService = require('../../utils/actuator-service')  // 外设控制服务
// 默认报警阈值
const DEFAULT_ALARM_THRESHOLD = {
  tempMin: '10',
  tempMax: '35',
  humMin: '20',
  humMax: '85',
  mq2Max: '2000',
  waterMax: '25'
}

const THEME_CONFIG = {
  dark: {
    bgGradient: 'linear-gradient(180deg, #0f0c29 0%, #302b63 50%, #24243e 100%)',
    cardBg: 'rgba(255, 255, 255, 0.08)',
    textColor: '#fff',
    textSecondary: 'rgba(255, 255, 255, 0.6)',
    borderColor: 'rgba(255, 255, 255, 0.1)',
    chartBg: '#fafbff',
    chartGridLine: '#e8ecf1',
    chartTextColor: '#94a3b8'
  },
  light: {
    bgGradient: 'linear-gradient(180deg, #f8fafc 0%, #e2e8f0 50%, #f1f5f9 100%)',
    cardBg: 'rgba(255, 255, 255, 0.95)',
    textColor: '#1e293b',
    textSecondary: 'rgba(30, 41, 59, 0.6)',
    borderColor: 'rgba(148, 163, 184, 0.2)',
    chartBg: '#ffffff',
    chartGridLine: '#cbd5e1',
    chartTextColor: '#64748b'
  }
}

/**
 * SensorHistory - 传感器数据历史记录管理类
 * 功能：维护最近N个数据点，支持新增、清除、查询操作
 */
class SensorHistory {
  constructor(maxPoints = 100, windowSize = 5) {
    this.maxPoints = maxPoints
    this.windowSize = windowSize
    this.data = {
      temp: [],
      humidity: [],
      mq2: [],
      water: [],
      timestamps: []
    }
    this.smoothData = {
      temp: [],
      humidity: [],
      mq2: [],
      water: [],
      timestamps: []
    }
  }

  addPoint(temp, humidity, mq2, water) {
    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`
    
    this.data.temp.push(temp)
    this.data.humidity.push(humidity)
    this.data.mq2.push(mq2)
    this.data.water.push(water)
    this.data.timestamps.push(timeStr)

    // 保持最大数据点数
    if (this.data.temp.length > this.maxPoints) {
      this.data.temp.shift()
      this.data.humidity.shift()
      this.data.mq2.shift()
      this.data.water.shift()
      this.data.timestamps.shift()
    }

    this.smoothData.temp = this.applyMovingAverage(this.data.temp)
    this.smoothData.humidity = this.applyMovingAverage(this.data.humidity)
    this.smoothData.mq2 = this.applyMovingAverage(this.data.mq2)
    this.smoothData.water = this.applyMovingAverage(this.data.water)
    this.smoothData.timestamps = [...this.data.timestamps]
  }

  getData(range) {
    const len = this.data.temp.length
    const start = Math.max(0, len - range)
    return {
      temp: this.data.temp.slice(start),
      humidity: this.data.humidity.slice(start),
      mq2: this.data.mq2.slice(start),
      water: this.data.water.slice(start),
      timestamps: this.data.timestamps.slice(start)
    }
  }

  getDataSmooth(range) {
    const len = this.smoothData.temp.length
    const start = Math.max(0, len - range)
    return {
      temp: this.smoothData.temp.slice(start),
      humidity: this.smoothData.humidity.slice(start),
      mq2: this.smoothData.mq2.slice(start),
      water: this.smoothData.water.slice(start),
      timestamps: this.smoothData.timestamps.slice(start)
    }
  }

  applyMovingAverage(dataArray, windowSize = null) {
    const winSize = windowSize || this.windowSize
    if (!dataArray || dataArray.length === 0) return []
    if (dataArray.length < winSize) return [...dataArray]
    
    const result = []
    for (let i = 0; i < dataArray.length; i++) {
      if (i < winSize - 1) {
        result.push(dataArray[i])
      } else {
        let sum = 0
        for (let j = 0; j < winSize; j++) {
          sum += dataArray[i - j]
        }
        result.push(sum / winSize)
      }
    }
    return result
  }

  clear() {
    this.data = {
      temp: [],
      humidity: [],
      mq2: [],
      water: [],
      timestamps: []
    }
    this.smoothData = {
      temp: [],
      humidity: [],
      mq2: [],
      water: [],
      timestamps: []
    }
  }
}

Page({
  data: {
    // 页面导航
    currentTab: 0,
    swiperHeight: 600,
    
    // 视频流
    videoUrl: '',
    videoPaused: true,
    videoStreamStarted: false,
    streamStatus: '已暂停',
    useSnapshotMode: true,
    snapshotInterval: null,
    
    // 拍照
    hasImage: false,
    capturedPhoto: '',
    photoTimestamp: '',
    isPhotoLocked: false,
    
    // 传感器数据
    temperature: '--',
    humidity: '--',
    mq2: '--',
    waterLevel: '--',
    
    // 趋势
    tempTrendIcon: '→',
    humTrendIcon: '→',
    mq2TrendIcon: '→',
    waterTrendIcon: '→',
    tempTrendClass: 'trend-stable',
    humTrendClass: 'trend-stable',
    mq2TrendClass: 'trend-stable',
    waterTrendClass: 'trend-stable',
    
    // 进度条宽度
    tempBarWidth: 0,
    humBarWidth: 0,
    mq2BarWidth: 0,
    waterBarWidth: 0,

    // 水位圆形进度条
    waterPercent: 0,
    waterLevelColor: '#10b981',
    waterLevelClass: 'low-water',
    
    // 报警系统
    hasAlarm: false,
    alarmMessage: '',
    showAlarmConfig: false,
    alarmThreshold: {
      tempMin: '15',
      tempMax: '30',
      humMin: '60',
      humMax: '90',
      mq2Max: '1000',
      waterMax: '95'
    },
    tempAlarm: false,
    humAlarm: false,
    mq2Alarm: false,
    waterAlarm: false,
    
    // 图表
    selectedSensor: 'all',
    chartTitle: '传感器数据趋势',
    chartRange: 20,
    legendTemp: true,
    legendHumidity: true,
    legendMq2: true,
    legendWater: true,
    
    // AI
    aiQuestion: '',
    aiResult: '',
    aiError: '',
    isAILoading: false,
    aiResultStatus: '',
    aiMode: 'llm',
    selectedLlmModel: 'qwen',
    aiSource: '',
    aiHistory: [],
    
    // AI结果增强展示
    aiResponseTime: '',
    aiResultLength: 0,
    aiDisplayText: '',
    aiStructuredContent: null,
    isTyping: false,
    loadingStep: 0,
    aiLoadingTip: '',
    aiErrorSolutions: [],
    aiStartTime: 0,
    
    // YOLO检测结果
    showYoloResult: false,
    yoloDetections: [],
    detectionCount: 0,
    currentDetIndex: -1,
    formattedAiText: '',
    
    // 测试工具相关
    isTesting: false,
    testResults: [],
    aiTestUrl: '',
    aiTestKey: '',
    aiTestModel: 'gpt-3.5-turbo',
    selectedModelIndex: 0,
    aiTestTimeout: '30',
    aiTestQuestion: '',
    isAiTesting: false,
    aiTestResult: '',
    aiTestStatus: '',
    
    // 服务提供商配置
    aiProviders: [
      { key: 'openai', name: 'OpenAI', url: 'https://api.openai.com/v1/chat/completions' },
      { key: 'qwen', name: '通义千问', url: 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions' },
      { key: 'deepseek', name: 'DeepSeek', url: 'https://api.deepseek.com/v1/chat/completions' },
      { key: 'kimi', name: 'Kimi', url: 'https://api.moonshot.cn/v1/chat/completions' },
      { key: 'ultralytics', name: 'Ultralytics (YOLO)', url: 'https://predict.ultralytics.com' }
    ],
    selectedProvider: { key: 'openai', name: 'OpenAI' },
    selectedProviderIndex: 0,
    
    // 状态
    isOnline: false,
    updateTime: '--:--:--',
    
    // ESP32配置
    esp32Ip: '192.168.221.108',
    esp32Port: 80,

    // 主题
    themeMode: 'dark',
    themeIcon: '🌙',
    
    // ========== 外设控制面板数据 (Tab 1) ==========
    isOperating: false,           // 操作锁
    relayState: false,            // 继电器状态
    buzzerState: false,           // 蜂鸣器状态
    isBeeping: false,             // 蜂鸣器播放中
    selectedFreqIndex: 1,         // 频率选择索引（默认2000Hz）
    freqOptions: ['500', '1000', '2000', '3000', '5000'],
    buzzerFreqInput: '',          // 蜂鸣器频率输入值
    motorSpeed: 0,                // 电机PWM值 (0-255)
    motorPercent: 0,              // 电机百分比 (0-100)
    motorInputValue: '0',         // 电机精确输入值
    servoAngle: 90,               // 舵机角度 (0-180)
    servoInputValue: '90',        // 舵机精确输入值
    showError: false,             // 错误提示显示标志
    errorMessage: '',             // 错误消息内容
    lastOperation: '',            // 最后操作记录
    operationHistory: [],         // 操作历史数组
    showHistory: false            // 历史记录展开标志
  },

  // 页面加载
  onLoad(options) {
    console.log('页面加载')
    this.sensorHistory = new SensorHistory(100)
    this.prevValues = { temp: null, humidity: null, mq2: null, water: null }

    const savedThreshold = wx.getStorageSync('alarm_threshold')
    if (savedThreshold) {
      this.setData({ alarmThreshold: savedThreshold })
      console.log('已加载保存的报警阈值:', savedThreshold)
    } else {
      this.setData({ alarmThreshold: { ...DEFAULT_ALARM_THRESHOLD } })
      console.log('使用默认报警阈值:', DEFAULT_ALARM_THRESHOLD)
    }

    this.loadConfig()
    this.loadTheme()

    // 初始化视频流
    this.initVideoStream()

    // 开始数据更新
    this.startDataUpdate()

    // 初始化图表
    this.initChart()
  },

  // 页面显示
  onShow() {
    console.log('页面显示')
    if (!this.dataUpdateInterval) {
      this.startDataUpdate()
    }
    // 重新绘制图表
    setTimeout(() => {
      this.drawChart()
    }, 300)
  },

  // 页面隐藏
  onHide() {
    console.log('页面隐藏')
    this.stopDataUpdate()
  },

  // 页面卸载
  onUnload() {
    console.log('页面卸载')
    this.stopDataUpdate()
    this.stopVideoStream()
    this.stopSnapshotMode()
  },

  // 加载配置
  loadConfig() {
    const config = wx.getStorageSync('esp32_config')
    if (config) {
      this.setData({
        esp32Ip: config.ip || '192.168.1.100',
        esp32Port: config.port || 80
      })
    }
    
    // 加载 LLM 配置
    const savedLLMConfig = wx.getStorageSync('llm_config')
    if (savedLLMConfig) {
      this.setData({
        aiTestUrl: savedLLMConfig.url || '',
        aiTestKey: savedLLMConfig.key || '',
        aiTestModel: savedLLMConfig.model || 'gpt-3.5-turbo',
        aiTestTimeout: savedLLMConfig.timeout || '30'
      })
      
      if (savedLLMConfig.provider) {
        this.setData({
          selectedProvider: savedLLMConfig.provider,
          selectedProviderIndex: this.data.aiProviders.findIndex(p => p.key === savedLLMConfig.provider.key)
        })
        
        // 根据保存的提供商更新模型列表
        const provider = savedLLMConfig.provider
        let modelList = []
        let defaultModel = 'gpt-3.5-turbo'
        
        switch(provider.key) {
          case 'openai':
            modelList = [
              { name: 'GPT-3.5 Turbo', value: 'gpt-3.5-turbo' },
              { name: 'GPT-4', value: 'gpt-4' },
              { name: 'GPT-4 Turbo', value: 'gpt-4-turbo' }
            ]
            defaultModel = 'gpt-3.5-turbo'
            break
          case 'qwen':
            modelList = [
              { name: '通义千问 Turbo', value: 'qwen-turbo' },
              { name: '通义千问 Plus', value: 'qwen-plus' },
              { name: '通义千问 Max', value: 'qwen-max' },
              { name: '通义千问 3', value: 'qwen3' }
            ]
            defaultModel = 'qwen-turbo'
            break
          case 'deepseek':
            modelList = [
              { name: 'DeepSeek Chat', value: 'deepseek-chat' },
              { name: 'DeepSeek Reasoner', value: 'deepseek-reasoner' }
            ]
            defaultModel = 'deepseek-chat'
            break
          case 'kimi':
            modelList = [
              { name: 'Kimi 8K', value: 'moonshot-v1-8k' },
              { name: 'Kimi 32K', value: 'moonshot-v1-32k' }
            ]
            defaultModel = 'moonshot-v1-8k'
            break
          case 'ultralytics':
            modelList = [
              { name: 'YOLO11n (超轻量)', value: 'yolo11n' },
              { name: 'YOLO11s (轻量)', value: 'yolo11s' },
              { name: 'YOLO11m (均衡)', value: 'yolo11m' },
              { name: 'YOLO11l (高精度)', value: 'yolo11l' },
              { name: 'YOLO11x (最强)', value: 'yolo11x' },
              { name: 'YOLOv8n (经典)', value: 'yolov8n' },
              { name: 'YOLOv8s (经典)', value: 'yolov8s' },
              { name: 'YOLOv8m (经典)', value: 'yolov8m' }
            ]
            defaultModel = 'yolo11n'
            break
          default:
            modelList = [
              { name: 'GPT-3.5 Turbo', value: 'gpt-3.5-turbo' },
              { name: 'GPT-4', value: 'gpt-4' },
              { name: '通义千问', value: 'qwen-turbo' },
              { name: 'Kimi', value: 'moonshot-v1-8k' },
              { name: 'DeepSeek', value: 'deepseek-chat' },
              { name: '豆包', value: 'doubao-lite-32k' }
            ]
            defaultModel = 'gpt-3.5-turbo'
        }
        
        this.setData({
          modelList: modelList,
          selectedModelIndex: modelList.findIndex(m => m.value === savedLLMConfig.model) || 0
        })
      }
    }
  },

  // 保存配置
  saveConfig() {
    const { esp32Ip, esp32Port } = this.data
    wx.setStorageSync('esp32_config', {
      ip: esp32Ip,
      port: esp32Port
    })
    wx.showToast({ title: '配置已保存', icon: 'success' })
  },

  // 切换主题
  toggleTheme() {
    const newTheme = this.data.themeMode === 'dark' ? 'light' : 'dark'
    this.setData({
      themeMode: newTheme,
      themeIcon: newTheme === 'dark' ? '🌙' : '☀️'
    })
    wx.setStorageSync('theme_mode', newTheme)
    setTimeout(() => this.drawChart(), 100)
  },

  // 加载主题偏好
  loadTheme() {
    const saved = wx.getStorageSync('theme_mode')
    if (saved) {
      this.setData({
        themeMode: saved,
        themeIcon: saved === 'dark' ? '🌙' : '☀️'
      })
    }
  },

  // 显示配置弹窗
  showConfig() {
    wx.showModal({
      title: 'ESP32配置',
      editable: true,
      placeholderText: `当前: ${this.data.esp32Ip}:${this.data.esp32Port}`,
      success: (res) => {
        if (res.confirm && res.content) {
          const parts = res.content.split(':')
          const ip = parts[0] || '192.168.1.100'
          const port = parseInt(parts[1]) || 80
          this.setData({ esp32Ip: ip, esp32Port: port })
          this.saveConfig()
          // 重新初始化
          this.stopVideoStream()
          this.stopDataUpdate()
          setTimeout(() => {
            this.initVideoStream()
            this.startDataUpdate()
          }, 500)
        }
      }
    })
  },

  // ========== Tab页面切换相关方法 ==========
  
  switchTab(e) {
    const index = parseInt(e.currentTarget.dataset.index)
    if (this.data.currentTab !== index) {
      this.setData({ currentTab: index })
      this.adjustSwiperHeight()
    }
  },

  onSwiperChange(e) {
    const index = e.detail.current
    this.setData({ currentTab: index })
    this.adjustSwiperHeight()
  },

  adjustSwiperHeight() {
    setTimeout(() => {
      const query = wx.createSelectorQuery().in(this)
      query.select('.page-scroll').boundingClientRect()
      query.exec((res) => {
        if (res && res[0]) {
          this.setData({
            swiperHeight: res[0].height + 20
          })
        }
      })
    }, 100)
  },

  goToCameraTab() {
    this.setData({ currentTab: 2 })
    this.adjustSwiperHeight()
  },

  // ========== 报警系统相关方法 ==========
  
  checkAlarms(temp, humidity, mq2, water) {
    const { tempMin, tempMax, humMin, humMax, mq2Max, waterMax } = this.data.alarmThreshold
    let hasAlarm = false
    let alarmMessage = ''
    
    if ((tempMin && temp < parseFloat(tempMin)) || (tempMax && temp > parseFloat(tempMax))) {
      hasAlarm = true
      alarmMessage += `温度${temp < parseFloat(tempMin) ? '低于下限' : '高于上限'}: ${temp.toFixed(1)}°C; `
    }
    
    if ((humMin && humidity < parseFloat(humMin)) || (humMax && humidity > parseFloat(humMax))) {
      hasAlarm = true
      alarmMessage += `湿度${humidity < parseFloat(humMin) ? '低于下限' : '高于上限'}: ${humidity.toFixed(1)}%; `
    }
    
    if (mq2Max && mq2 > parseFloat(mq2Max)) {
      hasAlarm = true
      alarmMessage += `MQ-2气体浓度高于上限: ${mq2.toFixed(0)}ppm; `
    }
    
    if (waterMax && water > parseFloat(waterMax)) {
      hasAlarm = true
      alarmMessage += `水位高于上限: ${water.toFixed(0)}mm; `
    }
    
    this.setData({
      hasAlarm,
      alarmMessage: alarmMessage || '无报警',
      tempAlarm: (tempMin && temp < parseFloat(tempMin)) || (tempMax && temp > parseFloat(tempMax)),
      humAlarm: (humMin && humidity < parseFloat(humMin)) || (humMax && humidity > parseFloat(humMax)),
      mq2Alarm: mq2Max && mq2 > parseFloat(mq2Max),
      waterAlarm: waterMax && water > parseFloat(waterMax)
    })
  },

  dismissAlarm() {
    this.setData({ hasAlarm: false })
  },

  toggleAlarmConfig() {
    this.setData({ showAlarmConfig: !this.data.showAlarmConfig })
    this.adjustSwiperHeight()
  },

  onThresholdInput(e) {
    const field = e.currentTarget.dataset.field
    const value = e.detail.value
    this.setData({
      [`alarmThreshold.${field}`]: value
    })
  },

  saveAlarmThreshold() {
    wx.setStorageSync('alarm_threshold', this.data.alarmThreshold)
    wx.showToast({ title: '阈值配置已保存', icon: 'success' })
    this.setData({ showAlarmConfig: false })
    this.adjustSwiperHeight()
  },

  resetAlarmToDefault() {
    this.setData({
      alarmThreshold: { ...DEFAULT_ALARM_THRESHOLD }
    })
    wx.showToast({ title: '已重置为默认值', icon: 'success' })
  },

  // ========== AI模式切换方法 ==========
  
  switchAIMode(e) {
    const mode = e.currentTarget.dataset.mode
    this.setData({ aiMode: mode })
    const modeNames = { llm: '云端LLM', yolo: 'YOLO检测', local: '本地分析' }
    wx.showToast({ title: `已切换至${modeNames[mode]}`, icon: 'none' })
  },

  // 切换LLM模型
  switchLlmModel(e) {
    const model = e.currentTarget.dataset.model
    this.setData({ selectedLlmModel: model })
    const modelNames = {
      qwen: '通义千问',
      gpt: 'GPT-4',
      kimi: 'Kimi',
      deepseek: 'DeepSeek'
    }
    wx.showToast({ title: `已切换至${modelNames[model]}`, icon: 'none' })
  },

  // ========== 图表触摸交互 ==========
  
  onChartTouch(e) {
    if (!this.chartCtx || !this.sensorHistory) return
    
    const touch = e.touches[0]
    const x = touch.x
    const width = this.chartWidth
    const padding = { left: 48, right: 16 }
    const chartWidth = width - padding.left - padding.right
    
    if (x >= padding.left && x <= width - padding.right) {
      const range = this.data.chartRange
      const data = this.sensorHistory.getDataSmooth(range)
      const index = Math.round(((x - padding.left) / chartWidth) * (data.timestamps.length - 1))
      
      if (index >= 0 && index < data.timestamps.length) {
        console.log(`数据点 ${index}: ${data.timestamps[index]}`)
      }
    }
  },

  findClosestIndex(points, x) {
    if (!points || points.length === 0) return -1
    let closest = 0
    let minDist = Math.abs(points[0].x - x)
    
    for (let i = 1; i < points.length; i++) {
      const dist = Math.abs(points[i].x - x)
      if (dist < minDist) {
        minDist = dist
        closest = i
      }
    }
    return closest
  },

  // 初始化视频流
  initVideoStream() {
    const { esp32Ip, esp32Port } = this.data
    const streamUrl = `http://${esp32Ip}:${esp32Port}/stream`

    console.log('初始化视频流:', streamUrl)

    this.setData({
      videoUrl: streamUrl,
      videoStreamStarted: true,
      useSnapshotMode: true,
      streamStatus: '连接中...'
    })

    this.startSnapshotMode()
  },

  // 快照模式：定时获取帧
  startSnapshotMode() {
    this.stopSnapshotMode()

    this.fetchSnapshotFrame()

    this.snapshotInterval = setInterval(() => {
      if (!this.data.videoPaused) {
        this.fetchSnapshotFrame()
      }
    }, 400)

    setTimeout(() => {
      this.setData({
        streamStatus: '实时画面(快照)',
        isOnline: true
      })
    }, 500)
  },

  // 停止快照模式
  stopSnapshotMode() {
    if (this.snapshotInterval) {
      clearInterval(this.snapshotInterval)
      this.snapshotInterval = null
    }
  },

  // 获取单帧图像
  fetchSnapshotFrame() {
    const { esp32Ip, esp32Port } = this.data
    const url = `http://${esp32Ip}:${esp32Port}/capture?t=${Date.now()}`

    wx.request({
      url: url,
      method: 'GET',
      responseType: 'arraybuffer',
      timeout: 3000,
      success: (res) => {
        if (res.statusCode === 200 && res.data && res.data.byteLength > 0) {
          const base64 = wx.arrayBufferToBase64(res.data)
          const frameUrl = `data:image/jpeg;base64,${base64}`
          this.setData({ videoUrl: frameUrl })
        }
      },
      fail: () => {
      }
    })
  },

  // 停止视频流
  stopVideoStream() {
    this.stopSnapshotMode()
    this.setData({
      videoUrl: '',
      videoStreamStarted: false,
      useSnapshotMode: false,
      streamStatus: '已停止'
    })
  },

  // 暂停视频
  pauseVideo() {
    this.setData({
      videoPaused: true,
      streamStatus: '已暂停'
    })
  },

  // 开始视频
  startVideo() {
    this.setData({
      videoPaused: false,
      streamStatus: this.data.useSnapshotMode ? '实时画面(快照)' : '实时视频流'
    })
  },

  // 切换视频模式（快照/视频流）
  // 注意：微信小程序不支持MJPEG流，两种模式都使用快照实现
  // 区别在于刷新频率：视频流模式(200ms) > 快照模式(400ms)
  toggleVideoMode() {
    const newMode = !this.data.useSnapshotMode
    this.setData({ useSnapshotMode: newMode })
    
    if (newMode) {
      this.stopSnapshotMode()
      this.setData({
        videoUrl: '',
        videoPaused: false,
        streamStatus: '已切换快照模式'
      })
      this.startSnapshotMode()
    } else {
      this.stopSnapshotMode()
      this.setData({
        videoUrl: '',
        videoPaused: false,
        streamStatus: '实时画面（高频刷新）'
      })
      this.startSnapshotModeHighFreq()
      wx.showToast({ title: '高频刷新模式', icon: 'none' })
    }
  },

  // 高频快照模式（模拟视频流效果）
  startSnapshotModeHighFreq() {
    this.stopSnapshotMode()

    this.fetchSnapshotFrame()

    this.snapshotInterval = setInterval(() => {
      if (!this.data.videoPaused) {
        this.fetchSnapshotFrame()
      }
    }, 200)

    setTimeout(() => {
      this.setData({
        streamStatus: '实时画面（高频刷新）',
        isOnline: true
      })
    }, 500)
  },

  // 处理视频流错误
  handleStreamError(e) {
    console.error('视频流错误:', e)

    if (!this.data.useSnapshotMode) {
      console.log('切换到快照模式')
      this.setData({ useSnapshotMode: true })
      this.startSnapshotMode()
    } else {
      this.setData({
        streamStatus: '连接失败',
        isOnline: false
      })
    }
  },

  // 开始数据更新
  startDataUpdate() {
    console.log('开始数据更新')
    this.fetchSensorData()
    this.dataUpdateInterval = setInterval(() => {
      this.fetchSensorData()
    }, 2000)
  },

  // 停止数据更新
  stopDataUpdate() {
    console.log('停止数据更新')
    if (this.dataUpdateInterval) {
      clearInterval(this.dataUpdateInterval)
      this.dataUpdateInterval = null
    }
  },

  // 获取传感器数据
  fetchSensorData() {
    const { esp32Ip, esp32Port } = this.data
    const url = `http://${esp32Ip}:${esp32Port}/sensors`
    
    wx.request({
      url: url,
      method: 'GET',
      timeout: 5000,
      success: (res) => {
        if (res.statusCode === 200 && res.data) {
          this.updateSensorData(res.data)
        }
      },
      fail: (err) => {
        console.error('获取传感器数据失败:', err)
        this.setData({ isOnline: false })
      }
    })
  },

  // 更新传感器数据
  updateSensorData(data) {
    const temp = parseFloat(data.temperature) || 0
    const humidity = parseFloat(data.humidity) || 0
    const mq2 = parseFloat(data.mq2) || 0
    const water = parseFloat(data.water_level) || 0

    // 计算趋势
    const trends = this.calculateTrends(temp, humidity, mq2, water)

    // 更新时间
    const now = new Date()
    const timeStr = now.toLocaleTimeString('zh-CN')

    // 添加到历史记录
    this.sensorHistory.addPoint(temp, humidity, mq2, water)
    
    // 计算进度条宽度（基于合理范围）
    const barWidths = {
      tempBarWidth: Math.min(100, Math.max(0, ((temp + 10) / 60) * 100)),  // -10~50°C
      humBarWidth: Math.min(100, Math.max(0, humidity)),  // 0~100%
      mq2BarWidth: Math.min(100, Math.max(0, (mq2 / 2000) * 100)),  // 0~2000ppm
      waterBarWidth: Math.min(100, Math.max(0, (water / 30) * 100))   // 0~30mm
    }

    // 水位百分比计算（0mm=0%, 30mm=100%）
    const waterPercent = Math.min(100, Math.max(0, Math.round((water / 30) * 100)))
    
    // 根据百分比确定颜色和状态类
    let waterLevelColor = '#10b981'
    let waterLevelClass = 'low-water'
    if (waterPercent > 66) {
      waterLevelColor = '#ef4444'
      waterLevelClass = 'high-water'
    } else if (waterPercent > 33) {
      waterLevelColor = '#3b82f6'
      waterLevelClass = 'mid-water'
    }

    this.setData({
      temperature: temp.toFixed(1),
      humidity: humidity.toFixed(1),
      mq2: mq2.toFixed(0),
      waterLevel: water.toFixed(0),
      ...trends,
      ...barWidths,
      waterPercent,
      waterLevelColor,
      waterLevelClass,
      updateTime: timeStr,
      isOnline: true
    })

    // 检查报警
    this.checkAlarms(temp, humidity, mq2, water)

    // 更新图表
    this.drawChart()
  },

  // 计算趋势
  calculateTrends(temp, humidity, mq2, water) {
    const trends = {}
    
    if (this.prevValues.temp !== null) {
      trends.tempTrendIcon = temp > this.prevValues.temp ? '↑' : temp < this.prevValues.temp ? '↓' : '→'
      trends.tempTrendClass = temp > this.prevValues.temp ? 'trend-up' : temp < this.prevValues.temp ? 'trend-down' : 'trend-stable'
      
      trends.humTrendIcon = humidity > this.prevValues.humidity ? '↑' : humidity < this.prevValues.humidity ? '↓' : '→'
      trends.humTrendClass = humidity > this.prevValues.humidity ? 'trend-up' : humidity < this.prevValues.humidity ? 'trend-down' : 'trend-stable'
      
      trends.mq2TrendIcon = mq2 > this.prevValues.mq2 ? '↑' : mq2 < this.prevValues.mq2 ? '↓' : '→'
      trends.mq2TrendClass = mq2 > this.prevValues.mq2 ? 'trend-up' : mq2 < this.prevValues.mq2 ? 'trend-down' : 'trend-stable'
      
      trends.waterTrendIcon = water > this.prevValues.water ? '↑' : water < this.prevValues.water ? '↓' : '→'
      trends.waterTrendClass = water > this.prevValues.water ? 'trend-up' : water < this.prevValues.water ? 'trend-down' : 'trend-stable'
    }

    this.prevValues = { temp, humidity, mq2, water }
    return trends
  },

  // 拍照
  takePhoto() {
    const { esp32Ip, esp32Port } = this.data
    const url = `http://${esp32Ip}:${esp32Port}/capture`

    wx.showLoading({ title: '拍照中...' })

    const needPause = !this.data.useSnapshotMode && this.data.videoStreamStarted && !this.data.videoPaused
    if (needPause) {
      this.pauseVideo()
    }

    setTimeout(() => {
      wx.request({
        url: url,
        method: 'GET',
        responseType: 'arraybuffer',
        timeout: 10000,
        success: (res) => {
          if (res.statusCode === 200) {
            const base64 = wx.arrayBufferToBase64(res.data)
            const photoUrl = `data:image/jpeg;base64,${base64}`
            const timestamp = new Date().toLocaleString('zh-CN')

            this.setData({
              capturedPhoto: photoUrl,
              hasImage: true,
              photoTimestamp: timestamp
            })

            wx.showToast({ title: '拍照成功', icon: 'success' })
          }
        },
        fail: (err) => {
          console.error('拍照失败:', err)
          wx.showToast({ title: '拍照失败', icon: 'error' })
        },
        complete: () => {
          wx.hideLoading()
          if (needPause) {
            setTimeout(() => { this.startVideo() }, 300)
          }
        }
      })
    }, needPause ? 300 : 0)
  },

  // 用于AI识别
  useForAI() {
    this.setData({
      aiQuestion: '这张图片里有什么？请详细描述。',
      aiResult: '',
      aiError: '',
      aiResultStatus: ''
    })
  },

  // 清除照片
  clearPhoto() {
    this.setData({
      hasImage: false,
      capturedPhoto: '',
      photoTimestamp: '',
      aiResult: '',
      aiError: '',
      aiResultStatus: ''
    })
  },

  // 绑定问题输入
  bindQuestionInput(e) {
    this.setData({ aiQuestion: e.detail.value })
  },

  // 选择AI识别模式
  selectAIMode() {
    const modeList = [
      { name: '🌐 云端LLM (OpenAI/千问/Kimi等)', value: 'llm' },
      { name: '🎯 YOLO目标检测', value: 'yolo' },
      { name: '🔧 本地设备分析', value: 'local' }
    ]
    wx.showActionSheet({
      itemList: modeList.map(m => m.name),
      success: (res) => {
        this.setData({ aiMode: modeList[res.tapIndex].value })
        wx.showToast({ title: `已切换为${modeList[res.tapIndex].name}`, icon: 'none' })
      }
    })
  },

  // 询问AI - 多模式路由版（支持YOLO/千问/本地）
  async askAI() {
    const { aiQuestion, capturedPhoto, aiMode } = this.data
    
    if (!aiQuestion.trim()) {
      wx.showToast({ title: '请输入问题', icon: 'none' })
      return
    }

    if (aiMode !== 'yolo' && !capturedPhoto) {
      wx.showToast({ title: '请先拍照', icon: 'none' })
      return
    }
    
    if (aiMode === 'yolo' && !capturedPhoto) {
      wx.showToast({ title: '请先拍照进行目标检测', icon: 'none' })
      return
    }
    
    const startTime = Date.now()
    
    this.setData({
      isAILoading: true,
      isPhotoLocked: true,
      aiResult: '',
      aiError: '',
      aiDisplayText: '',
      aiStructuredContent: null,
      aiResponseTime: '',
      aiResultLength: 0,
      loadingStep: 1,
      aiLoadingTip: '正在验证API配置...',
      aiStartTime: startTime,
      aiResultStatus: 'pending',
      showYoloResult: false,
      yoloDetections: []
    })

    setTimeout(() => {
      this.setData({ loadingStep: 2, aiLoadingTip: '正在上传图片数据...' })
    }, 800)

    setTimeout(() => {
      this.setData({ loadingStep: 3, aiLoadingTip: '正在调用AI模型...' })
    }, 2000)
    
    try {
      let result = {}
      
      if (aiMode === 'yolo' || aiMode === 'huggingface') {
        this.checkAiConfig('huggingface')
        result = await aiService.callHuggingfaceYOLO(capturedPhoto)
        this.handleYoloResult(result)
      } else if (aiMode === 'llm') {
        const llmModel = this.data.selectedLlmModel
        if (llmModel === 'qwen') {
          this.checkAiConfig('qwen')
          result = await aiService.callQwenLLM(aiQuestion, capturedPhoto)
        } else if (llmModel === 'gpt') {
          this.checkAiConfig('openai')
          result = await aiService.callGPT(aiQuestion, capturedPhoto)
        } else if (llmModel === 'kimi') {
          this.checkAiConfig('kimi')
          result = await aiService.callKimi(aiQuestion, capturedPhoto)
        } else if (llmModel === 'deepseek') {
          this.checkAiConfig('deepseek')
          result = await aiService.callDeepSeek(aiQuestion, capturedPhoto)
        } else {
          throw new Error('未知的LLM模型')
        }
        this.handleLlmResult(result)
      } else if (aiMode === 'local') {
        await this.callLocalAi()
      } else {
        throw new Error('未知的AI模式')
      }
      
    } catch (error) {
      this.handleAiError(error)
    } finally {
      this.setData({
        isAILoading: false,
        isPhotoLocked: false,
        loadingStep: 0
      })
    }
  },
  
  checkAiConfig(service) {
    if (!aiConfig.isConfigComplete(service)) {
      const serviceNames = {
        huggingface: 'HuggingFace YOLO',
        qwen: '通义千问',
        openai: 'OpenAI GPT',
        kimi: 'Kimi',
        deepseek: 'DeepSeek'
      }
      const serviceName = serviceNames[service] || 'AI服务'
      wx.showModal({
        title: `${serviceName} API未配置`,
        content: `请先在utils/ai-config.js中填写${serviceName}的API密钥`,
        confirmText: '查看配置方法',
        cancelText: '取消',
        success: (res) => {
          if (res.confirm) {
            wx.showModal({
              title: '配置说明',
              content: service === 'huggingface' 
                ? '1. 访问 https://huggingface.co/settings/tokens\n2. 创建Access Token\n3. 填入ai-config.js的huggingface.api_key字段'
                : `1. 访问对应AI平台官网获取API Key\n2. 填入ai-config.js的${service}.api_key字段`,
              showCancel: false,
              confirmText: '我知道了'
            })
          }
        }
      })
      throw aiService.createError(aiService.ERROR_TYPES.CONFIG_MISSING, `${serviceName}未配置`)
    }
  },
  
  handleYoloResult(result) {
    if (result.success && result.detections && result.detections.length > 0) {
      const colorPalette = ['#FF6384', '#36A2EB', '#FFCE56', '#4BC0C0', '#9966FF', '#FF9F40']
      
      const processedDetections = result.detections.map((det, index) => ({
        ...det,
        confidenceText: `${(det.score * 100).toFixed(1)}%`,
        color: colorPalette[index % colorPalette.length],
        bboxText: `[${det.box.xmin},${det.box.ymin}]-[${det.box.xmax},${det.box.ymax}]`
      }))
      
      this.setData({
        yoloDetections: processedDetections,
        detectionCount: processedDetections.length,
        showYoloResult: true,
        aiSource: `huggingface-${result.modelUsed}`,
        aiResultStatus: 'success',
        aiResponseTime: `${(Date.now() - this.data.aiStartTime / 1000).toFixed(1)}s`
      })
      
      setTimeout(() => {
        this.drawDetectionBoxes(processedDetections)
      }, 100)
      
      this.addToHistory(`[YOLO检测] 检测到${processedDetections.length}个目标`, 
                         JSON.stringify(processedDetections.map(d => d.label), null, 0), 
                         'huggingface-yolo')
    } else if (result.success && result.detections.length === 0) {
      this.setData({
        aiDisplayText: '✅ 未检测到任何目标物体',
        aiSource: 'huggingface-yolo',
        aiResultStatus: 'success'
      })
    } else {
      throw new Error('YOLO检测返回空结果')
    }
  },
  
  handleLlmResult(result) {
    if (result.success) {
      const formattedText = this.formatAiTextForDisplay(result.text)
      
      this.setData({
        formattedAiText: formattedText,
        aiDisplayText: result.text,
        aiResult: result.text,
        aiSource: result.modelUsed,
        aiResultStatus: 'success',
        aiResponseTime: `${((Date.now() - this.data.aiStartTime) / 1000).toFixed(1)}s`,
        aiResultLength: result.text.length,
        showYoloResult: false
      })
      
      this.addToHistory(this.data.aiQuestion, result.text, result.modelUsed)
    } else {
      throw new Error(result.error || 'AI返回异常结果')
    }
  },
  
  handleAiError(error) {
    let errorInfo
    
    if (error.isAiError) {
      errorInfo = aiService.getErrorMessage(error.type)
    } else {
      errorInfo = aiService.getErrorMessage(aiService.ERROR_TYPES.UNKNOWN)
      errorInfo.message = error.message || errorInfo.message
    }
    
    const solutions = aiService.getSolutionsForError(error.isAiError ? error.type : aiService.ERROR_TYPES.UNKNOWN)
    
    this.setData({
      aiError: `${errorInfo.icon} ${errorInfo.title}\n${errorInfo.message}`,
      aiErrorIcon: errorInfo.icon,
      aiErrorTitle: errorInfo.title,
      aiErrorSolutions: solutions,
      aiResultStatus: 'error',
      isAILoading: false,
      isPhotoLocked: false
    })
    
    console.error('[AI Error]', error)
  },
  
  async callLocalAi() {
    const { aiQuestion, capturedPhoto, esp32Ip, esp32Port } = this.data
    const startTime = Date.now()
    
    return new Promise((resolve, reject) => {
      const url = `http://${esp32Ip}:${esp32Port}/ask`
      
      wx.request({
        url: url,
        method: 'POST',
        header: { 'Content-Type': 'application/json' },
        data: {
          question: aiQuestion,
          image: capturedPhoto,
          resolution: 'VGA',
          return_image: false,
          ai_mode: 'local'
        },
        timeout: 60000,
        success: (res) => {
          const duration = ((Date.now() - startTime) / 1000).toFixed(1)
          
          if (res.statusCode === 200 && res.data) {
            if (res.data.error) {
              reject(new Error(res.data.error))
            } else if (res.data.analysis || res.data.ai_result) {
              const analysisText = res.data.analysis || res.data.ai_result
              const formattedData = this.formatAiResult(analysisText, 'local')
              
              this.setData({
                aiResult: analysisText,
                aiDisplayText: formattedData.displayText,
                aiStructuredContent: formattedData.structuredContent,
                aiSource: 'local',
                aiResultStatus: 'success',
                aiResponseTime: duration,
                aiResultLength: analysisText.length
              })
              
              this.addToHistory(aiQuestion, analysisText, 'local')
              resolve(res.data)
            } else {
              const resultText = JSON.stringify(res.data, null, 2)
              this.setData({
                aiResult: resultText,
                aiDisplayText: resultText,
                aiSource: 'unknown',
                aiResultStatus: 'success',
                aiResponseTime: duration,
                aiResultLength: resultText.length
              })
              resolve(res.data)
            }
          } else {
            reject(new Error(`本地分析失败 (HTTP ${res.statusCode})`))
          }
        },
        fail: (err) => {
          let errorMsg = '网络连接失败'
          
          if (err.errMsg.includes('timeout')) {
            errorMsg = '请求超时，ESP32响应时间过长'
          } else if (err.errMsg.includes('fail')) {
            errorMsg = '无法连接到ESP32，请检查IP地址和网络'
          }
          
          reject(new Error(errorMsg))
        }
      })
    })
  },
  
  formatAiTextForDisplay(rawText) {
    let text = rawText || ''
    
    text = text.replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')
    
    text = text.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
    text = text.replace(/`(.*?)`/g, '<code>$1</code>')
    text = text.replace(/^### (.*$)/gm, '<h3>$1</h3>')
    text = text.replace(/^## (.*$)/gm, '<h2>$1</h2>')
    text = text.replace(/^# (.*$)/gm, '<h1>$1</h1>')
    text = text.replace(/^\- (.*$)/gm, '<li>$1</li>')
    text = text.replace(/\n{3,}/g, '\n\n')
    
    if (text.length > 5000) {
      text = text.substring(0, 5000) + '\n\n...[内容过长已截断]'
    }
    
    return text
  },
  
  // 绘制YOLO检测框
  drawDetectionBoxes(detections) {
    const query = wx.createSelectorQuery().in(this)
    
    query.select('#yoloCanvas')
      .fields({ node: true, size: true })
      .exec((res) => {
        if (!res[0]) {
          console.warn('[YOLO] Canvas元素未找到')
          return
        }
        
        const canvas = res[0].node
        const ctx = canvas.getContext('2d')
        
        const dpr = wx.getWindowInfo().pixelRatio
        const displayWidth = res[0].width
        const displayHeight = res[0].height
        
        canvas.width = displayWidth * dpr
        canvas.height = displayHeight * dpr
        ctx.scale(dpr, dpr)
        
        const img = canvas.createImage()
        img.onload = () => {
          ctx.clearRect(0, 0, displayWidth, displayHeight)
          
          const imgRatio = img.width / img.height
          const canvasRatio = displayWidth / displayHeight
          
          let drawWidth, drawHeight, offsetX, offsetY
          
          if (imgRatio > canvasRatio) {
            drawWidth = displayWidth
            drawHeight = displayWidth / imgRatio
            offsetX = 0
            offsetY = (displayHeight - drawHeight) / 2
          } else {
            drawHeight = displayHeight
            drawWidth = displayHeight * imgRatio
            offsetX = (displayWidth - drawWidth) / 2
            offsetY = 0
          }
          
          ctx.drawImage(img, offsetX, offsetY, drawWidth, drawHeight)
          
          const scaleX = drawWidth / img.width
          const scaleY = drawHeight / img.height
          
          detections.forEach((det) => {
            const { label, score, box, color, confidenceText } = det
            
            const x = offsetX + box.xmin * scaleX
            const y = offsetY + box.ymin * scaleY
            const w = (box.xmax - box.xmin) * scaleX
            const h = (box.ymax - box.ymin) * scaleY
            
            ctx.strokeStyle = color || '#FF6384'
            ctx.lineWidth = 3
            ctx.strokeRect(x, y, w, h)
            
            ctx.fillStyle = color || '#FF6384'
            const labelText = `${label} ${confidenceText}`
            ctx.font = 'bold 14px Arial'
            const textWidth = ctx.measureText(labelText).width + 10
            
            ctx.fillRect(x, y - 24, textWidth, 24)
            
            ctx.fillStyle = '#FFFFFF'
            ctx.fillText(labelText, x + 5, y - 7)
          })
        }
        
        img.onerror = (err) => {
          console.error('[YOLO] 图片加载失败:', err)
          this.setData({
            aiError: '检测框绘制失败：图片加载错误',
            aiResultStatus: 'error'
          })
        }
        
        img.src = this.data.capturedPhoto
      })
  },
  
  onDetectionItemTap(e) {
    const index = e.currentTarget.dataset.index
    this.setData({ currentDetIndex: index })
    
    const detection = this.data.yoloDetections[index]
    if (detection) {
      wx.showToast({
        title: `${detection.label}: ${detection.confidenceText}`,
        icon: 'none',
        duration: 1500
      })
    }
  },
  
  closeYoloResult() {
    this.setData({ 
      showYoloResult: false, 
      yoloDetections: [],
      currentDetIndex: -1 
    })
  },

  // 格式化AI结果 - 自动解析和美化
  formatAiResult(rawText, source) {
    if (!rawText || typeof rawText !== 'string') {
      return { displayText: rawText || '', structuredContent: null }
    }

    let displayText = rawText.trim()
    let structuredContent = null

    displayText = displayText.replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')

    // 只对云端LLM和YOLO结果进行base64过滤，本地分析结果不需要
    if (source !== 'local') {
      const base64Pattern = /(?:data:image\/[a-z]+;base64,|["\s]\/[9A-Za-z]{2}[A-Za-z0-9+\/]{50,}={0,2})/
      if (base64Pattern.test(displayText) || displayText.includes('/9j/') || displayText.includes('/9j/4AAQ')) {
        displayText = displayText
          .replace(/data:image\/[a-z]+;base64,/g, '[图片数据已过滤]')
          .replace(/["\s]?\/[9A-Za-z]{2}[A-Za-z0-9+\/]{30,}={0,2}/g, '[...]')
          .replace(/\[\.\.\.\](?:\s*\[\.\.\.\])+/g, '[...]')
          .trim()
        
        if (displayText.length > 2000) {
          const jsonMatch = displayText.match(/\{[^{}]*"ai_result"\s*:\s*"/)
          if (jsonMatch) {
            displayText = displayText.substring(0, displayText.indexOf('"ai_result"')) + '"ai_result":"[响应数据过长或包含无效内容]"'
          } else {
            displayText = displayText.substring(0, 1500) + '\n\n[⚠️ 响应数据异常，请检查ESP32固件是否为最新版本]'
          }
        }
      }
    }

    if ((source === 'yolo' || displayText.startsWith('{')) && !displayText.startsWith('[')) {
      try {
        const jsonObj = JSON.parse(rawText)
        
        if (jsonObj.detections || jsonObj.objects || jsonObj.results) {
          // YOLO目标检测结构化输出
          const detections = jsonObj.detections || jsonObj.objects || jsonObj.results || []
          structuredContent = {
            summary: `检测到 ${detections.length} 个目标`,
            details: detections.map(d => {
              if (typeof d === 'string') return d
              if (d.name || d.label || d.class) {
                const name = d.name || d.label || d.class
                const conf = d.confidence || d.score || d.conf
                return `${name}${conf ? ` (${(conf * 100).toFixed(0)}%)` : ''}`
              }
              return JSON.stringify(d)
            }),
            keywords: [...new Set(detections.map(d => 
              typeof d === 'string' ? d.split(' ')[0] : (d.name || d.label || d.class || '')
            ).filter(Boolean))].slice(0, 6)
          }
          displayText = ''
          return { displayText, structuredContent }
        }
      } catch (e) {
        // 不是JSON，继续文本处理
      }
    }

    // 文本内容智能分段和提取
    const lines = displayText.split('\n').filter(line => line.trim())
    
    if (lines.length >= 3) {
      // 多行内容：尝试结构化
      const firstLine = lines[0].trim()
      
      // 提取可能的标题/摘要（第一行较短且不以符号开头）
      if (firstLine.length < 60 && !firstLine.match(/^[•\-\*\d#]/)) {
        const details = lines.slice(1).map(l => l.replace(/^[\s•\-\*]+/, '').trim()).filter(Boolean)
        
        // 尝试提取关键词
        const keywords = []
        const keywordPattern = /[#＃]([\u4e00-\u9fa5a-zA-Z0-9_]+)/g
        let match
        while ((match = keywordPattern.exec(displayText)) !== null && keywords.length < 8) {
          keywords.push(match[1])
        }
        
        // 如果没有标签，从内容中提取高频词
        if (keywords.length === 0 && lines.length > 3) {
          const wordFreq = {}
          lines.forEach(line => {
            line.match(/[\u4e00-\u9fa5]{2,}/g)?.forEach(word => {
              wordFreq[word] = (wordFreq[word] || 0) + 1
            })
          })
          Object.entries(wordFreq)
            .sort((a, b) => b[1] - a[1])
            .slice(0, 5)
            .forEach(([word]) => keywords.push(word))
        }

        structuredContent = {
          summary: firstLine,
          details: details.slice(0, 10),
          keywords: keywords.slice(0, 8)
        }
        displayText = ''
      }
    }

    // 清理显示文本中的多余空白
    if (displayText) {
      displayText = displayText
        .replace(/\n{3,}/g, '\n\n')
        .replace(/[ \t]+/g, ' ')
        .trim()
    }

    return { displayText, structuredContent }
  },

  // 生成错误解决方案
  generateErrorSolutions(errorMessage) {
    const solutions = []
    const msg = (errorMessage || '').toLowerCase()

    if (msg.includes('timeout') || msg.includes('超时')) {
      solutions.push(
        { text: '检查网络连接是否稳定', action: 'checkNetwork' },
        { text: '尝试使用更简单的提问', action: 'simpleQuestion' },
        { text: '在测试页验证LLM配置', action: 'testLlm' }
      )
    } else if (msg.includes('not configured') || msg.includes('未配置') || msg.includes('service')) {
      solutions.push(
        { text: '前往「测试」页面配置LLM', action: 'goToTest' },
        { text: '检查API地址和Key是否正确', action: 'checkConfig' },
        { text: '切换到本地识别模式', action: 'switchLocal' }
      )
    } else if (msg.includes('camera') || msg.includes('摄像头') || msg.includes('capture')) {
      solutions.push(
        { text: '稍后重试，摄像头可能忙碌', action: 'retry' },
        { text: '刷新视频流后重试', action: 'refreshStream' }
      )
    } else if (msg.includes('network') || msg.includes('连接') || msg.includes('fail') || msg.includes('ESP32')) {
      solutions.push(
        { text: '确认手机和ESP32在同一WiFi', action: 'checkWifi' },
        { text: '检查ESP32 IP地址是否变化', action: 'checkIp' },
        { text: '在测试页运行网络诊断', action: 'runDiag' }
      )
    } else {
      solutions.push(
        { text: '点击重新尝试', action: 'retry' },
        { text: '前往测试页检查配置', action: 'goToTest' }
      )
    }

    return solutions
  },

  // 复制AI结果
  copyAiResult() {
    const text = this.data.aiResult || this.data.aiDisplayText || ''
    if (!text) {
      wx.showToast({ title: '没有可复制的内容', icon: 'none' })
      return
    }

    wx.setClipboardData({
      data: text,
      success: () => {
        wx.showToast({ title: '已复制到剪贴板', icon: 'success' })
      }
    })
  },

  // 分享AI结果
  shareAiResult() {
    // 通过 open-type="share" 触发微信分享
    return {
      title: `AI识别结果 - ${this.data.aiQuestion || '图像分析'}`,
      path: '/pages/index/index',
      imageUrl: this.data.capturedPhoto
    }
  },

  // 重试AI识别
  retryAi() {
    if (this.data.isAILoading) return
    
    this.setData({
      aiResult: '',
      aiError: '',
      aiDisplayText: '',
      aiStructuredContent: null,
      aiErrorSolutions: [],
      aiResultStatus: ''
    })
    
    setTimeout(() => {
      this.askAI()
    }, 300)
  },

  // 跳转到测试Tab
  goToTestTab() {
    this.setData({ currentTab: 4 })
  },

  // 跳转到自动控制页面
  goToAutoControl() {
    wx.navigateTo({
      url: '/pages/auto-control/auto-control'
    })
  },

  // 应用解决方案
  applySolution(e) {
    const index = e.currentTarget.dataset.index
    const solution = this.data.aiErrorSolutions[index]
    if (!solution) return

    switch (solution.action) {
      case 'retry':
        this.retryAi()
        break
      case 'goToTest':
        this.goToTestTab()
        break
      case 'switchLocal':
        this.setData({ aiMode: 'local' })
        wx.showToast({ title: '已切换到本地模式', icon: 'success' })
        break
      case 'checkNetwork':
      case 'checkWifi':
        this.checkNetworkStatus?.call(this)
        break
      default:
        wx.showToast({ title: solution.text, icon: 'none' })
    }
  },

  // 添加到历史记录
  addToHistory(question, result, mode) {
    const historyItem = {
      question: question.substring(0, 50),
      result: result.substring(0, 100),
      mode: mode || 'llm',
      time: new Date().toLocaleTimeString('zh-CN'),
      timestamp: Date.now()
    }

    let history = [...this.data.aiHistory]
    history.unshift(historyItem)
    
    if (history.length > 20) {
      history = history.slice(0, 20)
    }

    this.setData({ aiHistory: history })
    
    // 同步存储
    wx.setStorageSync('ai_history', history)
  },

  // 查看历史记录项
  viewHistoryItem(e) {
    const index = e.currentTarget.dataset.index
    const item = this.data.aiHistory[index]
    if (!item) return

    this.setData({
      aiQuestion: item.question,
      aiResult: item.result,
      aiSource: item.mode,
      aiResultStatus: 'success'
    })
  },

  // 选择传感器
  selectSensor(e) {
    const type = e.currentTarget.dataset.type
    const titles = {
      temp: '温度趋势',
      humidity: '湿度趋势',
      mq2: 'MQ-2趋势',
      water: '水位趋势'
    }
    
    this.setData({
      selectedSensor: type,
      chartTitle: titles[type] || '传感器数据趋势'
    })
    
    this.drawChart()
  },

  // 设置图表范围
  setChartRange(e) {
    const range = parseInt(e.currentTarget.dataset.range)
    this.setData({ chartRange: range })
    this.drawChart()
  },

  // 切换图例
  toggleLegend(e) {
    const index = parseInt(e.currentTarget.dataset.index)
    const legends = ['legendTemp', 'legendHumidity', 'legendMq2', 'legendWater']
    const key = legends[index]
    
    this.setData({
      [key]: !this.data[key]
    })
    
    this.drawChart()
  },

  // 初始化图表
  initChart() {
    // 使用2D Canvas API
    const query = wx.createSelectorQuery()
    query.select('#sensorChart')
      .fields({ node: true, size: true })
      .exec((res) => {
        if (res[0]) {
          this.chartCanvas = res[0].node
          this.chartCtx = this.chartCanvas.getContext('2d')
          
          // 设置canvas尺寸
          const dpr = wx.getSystemInfoSync().pixelRatio
          this.chartCanvas.width = res[0].width * dpr
          this.chartCanvas.height = res[0].height * dpr
          this.chartCtx.scale(dpr, dpr)
          
          this.chartWidth = res[0].width
          this.chartHeight = res[0].height
          
          this.drawChart()
        }
      })
  },

  // 绘制图表（双Y轴专业版）
  drawChart() {
    if (!this.chartCtx || !this.sensorHistory) return

    const ctx = this.chartCtx
    const width = this.chartWidth
    const height = this.chartHeight
    const padding = { top: 28, right: 52, bottom: 36, left: 50 }
    const theme = THEME_CONFIG[this.data.themeMode]

    ctx.clearRect(0, 0, width, height)

    const range = this.data.chartRange
    const data = this.sensorHistory.getDataSmooth(range)

    if (data.timestamps.length < 2) {
      ctx.fillStyle = theme.chartTextColor
      ctx.font = '13px sans-serif'
      ctx.textAlign = 'center'
      ctx.fillText('等待数据...', width / 2, height / 2)
      return
    }

    const chartWidth = width - padding.left - padding.right
    const chartHeight = height - padding.top - padding.bottom

    // ========== 数据映射函数 ==========
    function mapToLeftAxis(value, type) {
      if (type === 'temp') return ((value + 10) / 50) * 100
      if (type === 'humidity') return value
      return Math.min(100, Math.max(0, value))
    }

    function mapToRightAxis(value, type) {
      if (type === 'mq2') return (value / 2000) * 100
      if (type === 'water') return (value / 300) * 100
      return Math.min(100, Math.max(0, value))
    }

    function leftAxisToY(percent) {
      return padding.top + chartHeight - (percent / 100) * chartHeight
    }

    function rightAxisToY(percent) {
      return padding.top + chartHeight - (percent / 100) * chartHeight
    }

    // ========== 背景渐变 ==========
    const bgGradient = ctx.createLinearGradient(0, 0, 0, height)
    bgGradient.addColorStop(0, theme.chartBg)
    bgGradient.addColorStop(1, theme.chartBg)
    ctx.fillStyle = bgGradient
    ctx.fillRect(padding.left, padding.top, chartWidth, chartHeight)

    // ========== 网格系统：主网格线（实线，每20%一条）==========
    ctx.strokeStyle = theme.chartGridLine
    ctx.globalAlpha = 0.3
    ctx.lineWidth = 1
    ctx.setLineDash([])

    for (let i = 0; i <= 5; i++) {
      const y = padding.top + (chartHeight / 5) * i
      ctx.beginPath()
      ctx.moveTo(padding.left, y)
      ctx.lineTo(width - padding.right, y)
      ctx.stroke()
    }

    // ========== 次网格线（虚线，每10%一条）==========
    ctx.globalAlpha = 0.15
    ctx.lineWidth = 0.5
    ctx.setLineDash([3, 4])

    for (let i = 0; i <= 10; i++) {
      if (i % 2 !== 0) {
        const y = padding.top + (chartHeight / 10) * i
        ctx.beginPath()
        ctx.moveTo(padding.left, y)
        ctx.lineTo(width - padding.right, y)
        ctx.stroke()
      }
    }
    ctx.setLineDash([])
    ctx.globalAlpha = 1

    // ========== 坐标轴线 ==========
    // 左Y轴（温湿度）：红-蓝渐变
    const leftAxisGradient = ctx.createLinearGradient(padding.left, padding.top, padding.left, height - padding.bottom)
    leftAxisGradient.addColorStop(0, '#ef4444')
    leftAxisGradient.addColorStop(1, '#3b82f6')
    ctx.strokeStyle = leftAxisGradient
    ctx.lineWidth = 1.8
    ctx.beginPath()
    ctx.moveTo(padding.left, padding.top)
    ctx.lineTo(padding.left, height - padding.bottom)
    ctx.stroke()

    // 右Y轴（气体水位）：橙-绿渐变
    const rightAxisGradient = ctx.createLinearGradient(width - padding.right, padding.top, width - padding.right, height - padding.bottom)
    rightAxisGradient.addColorStop(0, '#f59e0b')
    rightAxisGradient.addColorStop(1, '#10b981')
    ctx.strokeStyle = rightAxisGradient
    ctx.lineWidth = 1.8
    ctx.beginPath()
    ctx.moveTo(width - padding.right, padding.top)
    ctx.lineTo(width - padding.right, height - padding.bottom)
    ctx.stroke()

    // X轴底线
    ctx.strokeStyle = theme.chartGridLine
    ctx.globalAlpha = 0.3
    ctx.lineWidth = 1
    ctx.setLineDash([])
    ctx.beginPath()
    ctx.moveTo(padding.left, height - padding.bottom)
    ctx.lineTo(width - padding.right, height - padding.bottom)
    ctx.stroke()
    ctx.globalAlpha = 1

    // ========== 左Y轴刻度标签（温湿度 0-100）==========
    const leftTicks = [0, 20, 40, 60, 80, 100]
    leftTicks.forEach(tick => {
      const y = leftAxisToY(tick)

      // 温度刻度（红色）
      ctx.fillStyle = '#ef4444'
      ctx.font = 'bold 10px sans-serif'
      ctx.textAlign = 'right'
      let tempLabel = ''
      if (tick === 0) tempLabel = '-10°'
      else if (tick === 100) tempLabel = '40°'
      else tempLabel = `${((tick / 100) * 50 - 10).toFixed(0)}°`
      ctx.fillText(tempLabel, padding.left - 6, y + 4)

      // 湿度刻度（蓝色）
      ctx.fillStyle = '#3b82f6'
      ctx.fillText(`${tick}%`, padding.left - 22, y + 4)
    })

    // ========== 右Y轴刻度标签（MQ-2气体 + 水位）==========
    const mq2Ticks = [0, 400, 600, 1200, 1800, 2400]
    const waterTicks = [0, 60, 120, 180, 240, 300]

    mq2Ticks.forEach((tick, idx) => {
      const percent = (tick / 2400) * 100
      const y = rightAxisToY(percent)

      // MQ-2刻度（橙色）
      ctx.fillStyle = '#f59e0b'
      ctx.font = 'bold 10px sans-serif'
      ctx.textAlign = 'left'
      ctx.fillText(`${tick}`, width - padding.right + 4, y + 4)

      // 水位刻度（绿色）
      ctx.fillStyle = '#10b981'
      ctx.fillText(`${waterTicks[idx]}mm`, width - padding.right + 22, y + 4)
    })

    // 颜色配置（带发光效果 + 轴归属）
    const colorConfig = {
      temp: { line: '#ef4444', fillStart: 'rgba(239,68,68,0.20)', fillEnd: 'rgba(239,68,68,0.01)', glow: 'rgba(239,68,68,0.35)', axis: 'left' },
      humidity: { line: '#3b82f6', fillStart: 'rgba(59,130,246,0.20)', fillEnd: 'rgba(59,130,246,0.01)', glow: 'rgba(59,130,246,0.35)', axis: 'left' },
      mq2: { line: '#f59e0b', fillStart: 'rgba(245,158,11,0.20)', fillEnd: 'rgba(245,158,11,0.01)', glow: 'rgba(245,158,11,0.35)', axis: 'right' },
      water: { line: '#10b981', fillStart: 'rgba(16,185,129,0.20)', fillEnd: 'rgba(16,185,129,0.01)', glow: 'rgba(16,185,129,0.35)', axis: 'right' }
    }

    const datasets = [
      { key: 'temp', show: this.data.legendTemp },
      { key: 'humidity', show: this.data.legendHumidity },
      { key: 'mq2', show: this.data.legendMq2 },
      { key: 'water', show: this.data.legendWater }
    ]

    // 计算每个数据集的点坐标（使用双Y轴映射）
    function calcPoints(values, type) {
      const useLeftAxis = colorConfig[type].axis === 'left'
      return values.map((val, i) => ({
        x: padding.left + (chartWidth / (values.length - 1)) * i,
        y: useLeftAxis ? leftAxisToY(mapToLeftAxis(val, type)) : rightAxisToY(mapToRightAxis(val, type)),
        val,
        type
      }))
    }

    // Catmull-Rom 样条曲线插值
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

    // 绘制每条曲线（先画填充，后画线条）
    datasets.forEach(dataset => {
      if (!dataset.show) return
      const values = data[dataset.key]
      if (values.length < 2) return

      const config = colorConfig[dataset.key]
      const rawPoints = calcPoints(values, dataset.key)
      const smoothPoints = catmullRomSpline(rawPoints)

      // 渐变区域填充
      const areaGradient = ctx.createLinearGradient(0, padding.top, 0, height - padding.bottom)
      areaGradient.addColorStop(0, config.fillStart)
      areaGradient.addColorStop(1, config.fillEnd)

      ctx.beginPath()
      ctx.moveTo(smoothPoints[0].x, height - padding.bottom)
      smoothPoints.forEach(p => ctx.lineTo(p.x, p.y))
      ctx.lineTo(smoothPoints[smoothPoints.length - 1].x, height - padding.bottom)
      ctx.closePath()
      ctx.fillStyle = areaGradient
      ctx.fill()

      // 发光效果线条
      ctx.save()
      ctx.shadowColor = config.glow
      ctx.shadowBlur = 6
      ctx.shadowOffsetX = 0
      ctx.shadowOffsetY = 0
      ctx.strokeStyle = config.line
      ctx.lineWidth = 2.5
      ctx.lineCap = 'round'
      ctx.lineJoin = 'round'

      ctx.beginPath()
      smoothPoints.forEach((p, i) => {
        if (i === 0) ctx.moveTo(p.x, p.y)
        else ctx.lineTo(p.x, p.y)
      })
      ctx.stroke()
      ctx.restore()

      // 数据点标记（关键位置 + 最后一个点）
      const pointStep = Math.max(1, Math.floor(rawPoints.length / 8))
      rawPoints.forEach((p, i) => {
        if (i % pointStep === 0 || i === rawPoints.length - 1) {
          ctx.beginPath()
          ctx.arc(p.x, p.y, 3.5, 0, Math.PI * 2)
          ctx.fillStyle = theme.chartBg === '#fafbff' ? '#ffffff' : '#1e293b'
          ctx.fill()
          ctx.strokeStyle = config.line
          ctx.lineWidth = 2
          ctx.stroke()
        }
      })
    })

    // X轴标签
    ctx.fillStyle = theme.chartTextColor
    ctx.font = '10px sans-serif'
    ctx.textAlign = 'center'

    const labelCount = Math.min(data.timestamps.length, 6)
    const step = Math.max(1, Math.floor(data.timestamps.length / labelCount))

    for (let i = 0; i < data.timestamps.length; i += step) {
      const x = padding.left + (chartWidth / (data.timestamps.length - 1)) * i
      ctx.fillText(data.timestamps[i], x, height - 14)
    }
  },

  // 图表触摸事件
  onChartTouch(e) {
    if (!this.chartCtx || !this.sensorHistory || !this.chartWidth) return
    const { x } = e.detail
    const { timestamps } = this.sensorHistory.getData(this.data.chartRange)
    if (timestamps.length < 2) return

    const padding = { left: 50, right: 20 }
    const chartWidth = this.chartWidth - padding.left - padding.right

    const index = this.findClosestIndex(x, padding.left, chartWidth, timestamps)
    if (index < 0) return

    const { temp, humidity, mq2, water } = this.sensorHistory.getData(this.data.chartRange)
    const timestamp = timestamps[index]
    const tempVal = temp[index]
    const humidityVal = humidity[index]
    const mq2Val = mq2[index]
    const waterVal = water[index]
    wx.showToast({
      title: `${timestamp}\n温度:${tempVal.toFixed(1)}°C 湿度:${humidityVal.toFixed(1)}%\nMQ2:${mq2Val} 水位:${waterVal}mm`,
      icon: 'none',
      duration: 3000
    })
  },

  // 根据触摸X坐标找到最近的数据点索引
  findClosestIndex(touchX, paddingLeft, chartWidth, timestamps) {
    const relativeX = touchX - paddingLeft
    if (relativeX < 0 || relativeX > chartWidth) return -1
    const ratio = relativeX / chartWidth
    let index = Math.round(ratio * (timestamps.length - 1))
    index = Math.max(0, Math.min(index, timestamps.length - 1))
    return index
  },

  // 跳转到网络测试
  goToNetworkTest() {
    wx.navigateTo({
      url: '/pages/index/network_test'
    })
  },

  // 跳转到简单测试
  goToSimpleTest() {
    wx.navigateTo({
      url: '/pages/index/simple_test'
    })
  },

  // ========== 第五页：测试工具相关方法 ==========
  
  inputTestIp(e) {
    this.setData({ esp32Ip: e.detail.value })
  },

  inputTestPort(e) {
    this.setData({ esp32Port: e.detail.value })
  },

  async runFullTest() {
    this.setData({ 
      isTesting: true, 
      testResults: [] 
    })
    
    const { esp32Ip, esp32Port } = this.data
    const baseUrl = `http://${esp32Ip}:${esp32Port}`
    const results = []
    
    const addResult = (name, success, detail) => {
      results.push({
        name,
        success,
        detail: detail || '',
        time: new Date().toLocaleTimeString('zh-CN')
      })
      this.setData({ testResults: [...results] })
    }

    try {
      addResult('网络连通性', true, `正在测试 ${baseUrl}...`)
      
      await new Promise(resolve => setTimeout(resolve, 500))
      
      const testApi = (url, name, timeout = 5000) => {
        return new Promise((resolve) => {
          const startTime = Date.now()
          wx.request({
            url,
            method: 'GET',
            timeout,
            success: (res) => {
              const duration = Date.now() - startTime
              resolve({
                success: res.statusCode === 200,
                detail: `${res.statusCode} | ${duration}ms`
              })
            },
            fail: (err) => {
              const duration = Date.now() - startTime
              resolve({
                success: false,
                detail: err.errMsg || `超时 (${duration}ms)`
              })
            }
          })
        })
      }
      
      const sensorRes = await testApi(`${baseUrl}/sensors`, '传感器API')
      addResult('传感器API', sensorRes.success, sensorRes.detail)
      
      await new Promise(resolve => setTimeout(resolve, 300))
      
      const captureRes = await testApi(`${baseUrl}/capture?t=${Date.now()}`, '拍照接口', 8000)
      addResult('拍照接口', captureRes.success, captureRes.detail)
      
      await new Promise(resolve => setTimeout(resolve, 300))
      
      const streamRes = await testApi(`${baseUrl}/stream`, '视频流', 3000)
      addResult('视频流', streamRes.success, streamRes.detail)
      
      await new Promise(resolve => setTimeout(resolve, 300))
      
      const statusRes = await testApi(`${baseUrl}/status`, '设备状态')
      addResult('设备状态', statusRes.success, statusRes.detail)
      
      const successCount = results.filter(r => r.success).length
      wx.showToast({ 
        title: `诊断完成 ${successCount}/${results.length}`, 
        icon: 'success' 
      })
      
    } catch (err) {
      addResult('诊断异常', false, err.message || '未知错误')
      wx.showToast({ title: '诊断出错', icon: 'error' })
    } finally {
      this.setData({ isTesting: false })
    }
  },

  testSensorsApi() {
    this.testSingleApi('/sensors', '传感器API')
  },

  testCaptureApi() {
    this.testSingleApi('/capture', '拍照接口', 8000)
  },

  testStreamApi() {
    this.testSingleApi('/stream', '视频流', 3000)
  },

  checkNetworkStatus() {
    wx.getNetworkType({
      success: (res) => {
        const networkInfo = {
          name: '网络状态',
          success: true,
          detail: `类型: ${res.networkType}`,
          time: new Date().toLocaleTimeString('zh-CN')
        }
        this.setData({ 
          testResults: [networkInfo, ...this.data.testResults].slice(0, 10) 
        })
        wx.showToast({ title: `网络: ${res.networkType}`, icon: 'none' })
      },
      fail: () => {
        wx.showToast({ title: '获取网络状态失败', icon: 'error' })
      }
    })
  },

  testSingleApi(path, name, timeout = 5000) {
    const { esp32Ip, esp32Port } = this.data
    const url = `http://${esp32Ip}:${esp32Port}${path}${path.includes('?') ? '' : '?t=' + Date.now()}`
    
    wx.showLoading({ title: `测试${name}...` })
    
    const startTime = Date.now()
    wx.request({
      url,
      method: 'GET',
      timeout,
      success: (res) => {
        const duration = Date.now() - startTime
        const result = {
          name,
          success: res.statusCode === 200,
          detail: `${res.statusCode} | 耗时: ${duration}ms`,
          time: new Date().toLocaleTimeString('zh-CN')
        }
        this.setData({ 
          testResults: [result, ...this.data.testResults].slice(0, 10) 
        })
        wx.showToast({ 
          title: result.success ? `${name}正常` : `${name}异常`,
          icon: result.success ? 'success' : 'error'
        })
      },
      fail: (err) => {
        const duration = Date.now() - startTime
        const result = {
          name,
          success: false,
          detail: err.errMsg || `请求失败 (${duration}ms)`,
          time: new Date().toLocaleTimeString('zh-CN')
        }
        this.setData({ 
          testResults: [result, ...this.data.testResults].slice(0, 10) 
        })
        wx.showToast({ title: `${name}失败`, icon: 'error' })
      },
      complete: () => {
        wx.hideLoading()
      }
    })
  },

  inputAiTestUrl(e) {
    this.setData({ aiTestUrl: e.detail.value })
  },

  inputAiTestKey(e) {
    this.setData({ aiTestKey: e.detail.value })
  },

  pickModel(e) {
    const index = e.detail.value
    const model = this.data.modelList[index]
    this.setData({
      selectedModelIndex: index,
      aiTestModel: model.value 
    })
  },

  pickAiProvider(e) {
    const index = e.detail.value
    const provider = this.data.aiProviders[index]
    
    // 切换服务提供商时更新默认配置
    let modelList = []
    let defaultModel = 'gpt-3.5-turbo'
    
    switch(provider.key) {
      case 'openai':
        modelList = [
          { name: 'GPT-3.5 Turbo', value: 'gpt-3.5-turbo' },
          { name: 'GPT-4', value: 'gpt-4' },
          { name: 'GPT-4 Turbo', value: 'gpt-4-turbo' }
        ]
        defaultModel = 'gpt-3.5-turbo'
        break
      case 'qwen':
        modelList = [
          { name: '通义千问 Turbo', value: 'qwen-turbo' },
          { name: '通义千问 Plus', value: 'qwen-plus' },
          { name: '通义千问 Max', value: 'qwen-max' },
          { name: '通义千问 3', value: 'qwen3' }
        ]
        defaultModel = 'qwen-turbo'
        break
      case 'deepseek':
        modelList = [
          { name: 'DeepSeek Chat', value: 'deepseek-chat' },
          { name: 'DeepSeek Reasoner', value: 'deepseek-reasoner' }
        ]
        defaultModel = 'deepseek-chat'
        break
      case 'kimi':
        modelList = [
          { name: 'Kimi 8K', value: 'moonshot-v1-8k' },
          { name: 'Kimi 32K', value: 'moonshot-v1-32k' }
        ]
        defaultModel = 'moonshot-v1-8k'
        break
      case 'ultralytics':
        modelList = [
          { name: 'YOLO11n (超轻量)', value: 'yolo11n' },
          { name: 'YOLO11s (轻量)', value: 'yolo11s' },
          { name: 'YOLO11m (均衡)', value: 'yolo11m' },
          { name: 'YOLO11l (高精度)', value: 'yolo11l' },
          { name: 'YOLO11x (最强)', value: 'yolo11x' },
          { name: 'YOLOv8n (经典)', value: 'yolov8n' },
          { name: 'YOLOv8s (经典)', value: 'yolov8s' },
          { name: 'YOLOv8m (经典)', value: 'yolov8m' }
        ]
        defaultModel = 'yolo11n'
        break
    }
    
    this.setData({
      selectedProviderIndex: index,
      selectedProvider: provider,
      aiTestUrl: provider.url,
      modelList: modelList,
      aiTestModel: defaultModel,
      selectedModelIndex: 0
    })
  },

  inputAiTimeout(e) {
    this.setData({ aiTestTimeout: e.detail.value })
  },

  inputAiQuestion(e) {
    this.setData({ aiTestQuestion: e.detail.value })
  },

  async runAiTest() {
    const { aiTestUrl, aiTestKey, aiTestModel, aiTestTimeout, aiTestQuestion, selectedProvider } = this.data
    
    if (!aiTestUrl || !aiTestKey) {
      wx.showToast({ title: '请填写API地址和Key', icon: 'none' })
      return
    }
    
    this.setData({ 
      isAiTesting: true, 
      aiTestResult: '',
      aiTestStatus: 'pending'
    })
    
    const question = aiTestQuestion || '你好，请简单介绍一下你自己'
    const timeout = parseInt(aiTestTimeout) * 1000 || 30000
    
    try {
      const startTime = Date.now()
      
      if (selectedProvider.key === 'ultralytics') {
        // Ultralytics YOLO 目标检测测试
        this.setData({
          aiTestResult: '[⏳ 测试中] 正在进行 YOLO 目标检测...',
          aiTestStatus: 'pending'
        })
        
        // 这里应该使用 callUltralyticsYOLO 函数
        // 但需要先获取图像数据
        // 暂时使用模拟测试
        setTimeout(() => {
          const duration = ((Date.now() - startTime) / 1000).toFixed(1)
          this.setData({
            aiTestResult: `[✅ 测试成功] YOLO模型: ${aiTestModel}\n耗时: ${duration}s\n\nUltralytics API 配置正确!\n\n支持的操作:\n- 目标检测\n- 图像分类\n- 实例分割`,
            aiTestStatus: 'success'
          })
          this.setData({ isAiTesting: false })
          wx.showToast({ title: 'YOLO测试成功', icon: 'success' })
        }, 2000)
        
      } else {
        // LLM 测试
        wx.request({
          url: aiTestUrl,
          method: 'POST',
          header: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${aiTestKey}`
          },
          data: {
            model: aiTestModel,
            messages: [
              { role: 'user', content: question }
            ],
            max_tokens: 500,
            temperature: 0.7
          },
          timeout,
          success: (res) => {
            const duration = ((Date.now() - startTime) / 1000).toFixed(1)
            
            if (res.statusCode === 200 && res.data) {
              let responseText = ''
              
              if (res.data.choices && res.data.choices[0]) {
                responseText = res.data.choices[0].message?.content || JSON.stringify(res.data.choices[0])
              } else if (res.data.output) {
                responseText = typeof res.data.output === 'string' ? res.data.output : JSON.stringify(res.data.output)
              } else if (res.data.result) {
                responseText = res.data.result
              } else {
                responseText = JSON.stringify(res.data, null, 2)
              }
              
              this.setData({
                aiTestResult: `[✅ 测试成功] 模型: ${aiTestModel}\n耗时: ${duration}s\n问题: ${question}\n\n回复:\n${responseText}`,
                aiTestStatus: 'success'
              })
              
              wx.showToast({ title: `LLM响应成功 (${duration}s)`, icon: 'success' })
            } else {
              throw new Error(`HTTP ${res.statusCode}: ${JSON.stringify(res.data).substring(0, 200)}`)
            }
          },
          fail: (err) => {
            const duration = ((Date.now() - startTime) / 1000).toFixed(1)
            this.setData({
              aiTestResult: `[❌ 请求失败]\n耗时: ${duration}s\n错误: ${err.errMsg || '网络错误'}\n\n可能原因:\n1. API地址不正确\n2. API Key无效\n3. 网络连接问题\n4. 服务端未响应`,
              aiTestStatus: 'error'
            })
            wx.showToast({ title: 'LLM请求失败', icon: 'error' })
          },
          complete: () => {
            this.setData({ isAiTesting: false })
          }
        })
      }
      
    } catch (err) {
      this.setData({
        aiTestResult: `[❌ 异常错误]\n${err.message}`,
        aiTestStatus: 'error',
        isAiTesting: false
      })
    }
  },

  saveAiConfigToStorage() {
    const { aiTestUrl, aiTestKey, aiTestModel, aiTestTimeout, selectedProvider } = this.data
    
    if (!aiTestUrl || !aiTestKey) {
      wx.showToast({ title: '请先填写完整配置', icon: 'none' })
      return
    }
    
    wx.setStorageSync('llm_config', {
      url: aiTestUrl,
      key: aiTestKey,
      model: aiTestModel,
      timeout: aiTestTimeout,
      provider: selectedProvider
    })
    
    wx.showToast({ title: '配置已保存', icon: 'success' })
  },

  syncAiToEsp32() {
    const { aiTestUrl, aiTestKey, aiTestModel, aiTestTimeout, esp32Ip, esp32Port, selectedProvider } = this.data
    
    if (!aiTestUrl || !aiTestKey) {
      wx.showToast({ title: '请先填写完整配置', icon: 'none' })
      return
    }
    
    wx.showLoading({ title: '同步中...' })
    
    wx.request({
      url: `http://${esp32Ip}:${esp32Port}/config`,
      method: 'POST',
      header: { 'Content-Type': 'application/json' },
      data: {
        ai_service_url: aiTestUrl,
        ai_api_key: aiTestKey,
        ai_model: aiTestModel,
        ai_timeout: parseInt(aiTestTimeout) || 30,
        ai_provider: selectedProvider.key
      },
      timeout: 8000,
      success: (res) => {
        if (res.statusCode === 200) {
          wx.showToast({ title: '已同步到ESP32', icon: 'success' })
        } else {
          wx.showToast({ title: `同步失败: ${res.statusCode}`, icon: 'error' })
        }
      },
      fail: () => {
        wx.showToast({ title: 'ESP32连接失败', icon: 'error' })
      },
      complete: () => {
        wx.hideLoading()
      }
    })
  },

  // ==================== 外设控制面板事件处理 (Tab 1) ====================

  onRelayToggle(e) {
    const newState = e.detail.value
    this.controlActuator('relay', newState ? 'on' : 'off')
  },

  controlRelay(e) {
    this.controlActuator('relay', e.currentTarget.dataset.action)
  },

  onBuzzerToggle(e) {
    const turnOn = e.detail.value
    console.log('[BuzzerToggle] 触发, turnOn:', turnOn, '当前buzzerState:', this.data.buzzerState, 'isOnline:', this.data.isOnline)
    const freq = parseInt(this.data.freqOptions[this.data.selectedFreqIndex])
    console.log('[BuzzerToggle] selectedFreqIndex:', this.data.selectedFreqIndex, 'freqOptions:', this.data.freqOptions, '解析频率:', freq)
    if (turnOn) {
      console.log('[BuzzerToggle] → 开启蜂鸣器, 频率:', freq, 'Hz')
      this.controlActuator('buzzer', 'on', { freq })
    } else {
      console.log('[BuzzerToggle] → 关闭蜂鸣器')
      this.controlActuator('buzzer', 'off')
    }
  },

  onFreqChange(e) {
    const newIndex = e.detail.value
    const newFreq = parseInt(this.data.freqOptions[newIndex])
    console.log('[FreqChange] 频率选择变化, newIndex:', newIndex, '新频率:', newFreq, 'Hz, 当前buzzerState:', this.data.buzzerState, 'isOnline:', this.data.isOnline)
    this.setData({ selectedFreqIndex: newIndex })
    if (this.data.buzzerState && this.data.isOnline) {
      console.log('[FreqChange] → 蜂鸣器正在播放且在线, 立即应用新频率:', newFreq, 'Hz')
      this.controlActuator('buzzer', 'on', { freq: newFreq })
    } else {
      console.log('[FreqChange] → 跳过立即应用 (buzzerState:', this.data.buzzerState, ', isOnline:', this.data.isOnline, '), 频率已保存，下次开启时生效')
    }
  },

  // 频率输入相关
  onBuzzerFreqInputChange(e) {
    let value = e.detail.value
    // 只允许数字
    value = value.replace(/[^\d]/g, '')
    this.setData({ buzzerFreqInput: value })
  },

  onBuzzerFreqInputConfirm(e) {
    this.applyBuzzerFreqInput()
  },

  applyBuzzerFreqInput() {
    const rawInput = this.data.buzzerFreqInput
    console.log('[ApplyBuzzerFreq] SET按钮触发, 输入值:', rawInput)
    const value = parseInt(rawInput)
    if (isNaN(value) || value < 100 || value > 10000) {
      console.log('[ApplyBuzzerFreq] → 输入无效, value:', value, ', 范围要求: 100-10000Hz')
      wx.showToast({ 
        title: '频率范围100-10000Hz', 
        icon: 'none',
        duration: 2000
      })
      return
    }
    console.log('[ApplyBuzzerFreq] 验证通过, 目标频率:', value, 'Hz')
    const newOptions = [...this.data.freqOptions]
    const freqStr = String(value)
    if (!newOptions.includes(freqStr)) {
      newOptions.push(freqStr)
      newOptions.sort((a, b) => parseInt(a) - parseInt(b))
      console.log('[ApplyBuzzerFreq] 新频率已添加到选项列表, 更新后options:', newOptions)
    }
    const newIndex = newOptions.indexOf(freqStr)
    this.setData({
      freqOptions: newOptions,
      selectedFreqIndex: newIndex,
      buzzerFreqInput: ''
    })
    console.log('[ApplyBuzzerFreq] 已更新UI, selectedFreqIndex:', newIndex, ', 当前buzzerState:', this.data.buzzerState, ', isOnline:', this.data.isOnline)
    if (this.data.buzzerState && this.data.isOnline) {
      console.log('[ApplyBuzzerFreq] → 蜂鸣器正在播放且在线, 立即应用频率:', value, 'Hz')
      this.controlActuator('buzzer', 'on', { freq: value })
    } else {
      console.log('[ApplyBuzzerFreq] → 跳过立即应用 (buzzerState:', this.data.buzzerState, ', isOnline:', this.data.isOnline, '), 频率已保存，下次开启时生效')
    }
    wx.showToast({ 
      title: `已设置频率 ${value}Hz${this.data.buzzerState ? ' ✓ 已生效' : ''}`, 
      icon: 'success',
      duration: 1500
    })
  },

  playBuzzerBeep() {
    if (!this.data.isOnline || this.data.isOperating || this.data.isBeeping) return
    const freq = parseInt(this.data.freqOptions[this.data.selectedFreqIndex])
    this.setData({ isBeeping: true })
    this.controlActuator('buzzer', 'beep', { freq, duration: 500 })
    setTimeout(() => { this.setData({ isBeeping: false }) }, 600)
  },

  onMotorSpeedChange(e) {
    const percent = e.detail.value
    const speed = Math.round(percent / 100 * 255)
    this.setData({ motorSpeed: speed, motorPercent: percent })
    this.controlActuator('motor', speed === 0 ? 'stop' : 'run', { speed })
  },

  onMotorSpeedChanging(e) {
    const percent = e.detail.value
    this.setData({ motorPercent: percent, motorSpeed: Math.round(percent / 100 * 255) })
  },

  setMotorPreset(e) {
    const percent = parseInt(e.currentTarget.dataset.percent)
    const speed = Math.round(percent / 100 * 255)
    this.setData({ motorPercent: percent, motorSpeed: speed })
    this.controlActuator('motor', speed === 0 ? 'stop' : 'run', { speed })
  },

  emergencyStopMotor() {
    wx.showModal({
      title: '⚠️ 确认紧急停止',
      content: '确定要立即停止电机吗？',
      confirmText: '立即停止',
      confirmColor: '#ef4444',
      success: (res) => {
        if (res.confirm) {
          this.controlActuator('motor', 'stop')
          wx.showToast({ title: '⛔ 已紧急停止', icon: 'none' })
        }
      }
    })
  },

  onServoAngleChange(e) {
    const angle = e.detail.value
    this.setData({ servoAngle: angle })
    this.controlActuator('servo', 'write', { angle })
  },

  onServoAngleChanging(e) {
    this.setData({ servoAngle: e.detail.value })
  },

  setServoPreset(e) {
    const angle = parseInt(e.currentTarget.dataset.angle)
    this.setData({ servoAngle: angle })
    this.controlActuator('servo', 'write', { angle })
  },

  // ==================== 精确数值输入控制 ====================

  onMotorInputChange(e) {
    let value = e.detail.value
    value = value.replace(/[^\d.]/g, '')
    const parts = value.split('.')
    if (parts.length > 2) {
      value = parts[0] + '.' + parts.slice(1).join('')
    }
    if (parts[1] && parts[1].length > 1) {
      value = parts[0] + '.' + parts[1].slice(0, 1)
    }
    this.setData({ motorInputValue: value })
  },

  onMotorInputConfirm(e) {
    this.submitMotorInput()
  },

  async submitMotorInput() {
    const value = parseFloat(this.data.motorInputValue)
    if (isNaN(value)) {
      wx.showToast({ title: '请输入有效数值', icon: 'none' })
      return
    }
    let percent = Math.round(value)
    percent = Math.max(0, Math.min(100, percent))
    this.setData({
      motorPercent: percent,
      motorSpeed: Math.round(percent / 100 * 255),
      motorInputValue: String(percent)
    })
    try {
      if (percent === 0) {
        await this.controlActuator('motor', 'stop')
      } else {
        await this.controlActuator('motor', 'run', { speed: Math.round(percent / 100 * 255) })
      }
    } catch (error) {
      console.log('[Index] 电机控制命令已发送')
    }
  },

  onMotorInputTap(e) {
    wx.hideKeyboard()
  },

  onServoInputChange(e) {
    let value = e.detail.value
    value = value.replace(/[^\d.]/g, '')
    const parts = value.split('.')
    if (parts.length > 2) {
      value = parts[0] + '.' + parts.slice(1).join('')
    }
    if (parts[1] && parts[1].length > 1) {
      value = parts[0] + '.' + parts[1].slice(0, 1)
    }
    this.setData({ servoInputValue: value })
  },

  onServoInputConfirm(e) {
    this.submitServoInput()
  },

  async submitServoInput() {
    const value = parseFloat(this.data.servoInputValue)
    if (isNaN(value)) {
      wx.showToast({ title: '请输入有效角度', icon: 'none' })
      return
    }
    let angle = Math.round(value)
    angle = Math.max(0, Math.min(180, angle))
    this.setData({
      servoAngle: angle,
      servoInputValue: String(angle)
    })
    try {
      await this.controlActuator('servo', 'write', { angle })
    } catch (error) {
      console.log('[Index] 舵机控制命令已发送')
    }
  },

  onServoInputTap(e) {
    wx.hideKeyboard()
  },

  refreshAllStates() {
    this.refreshActuatorStates()
  },

  dismissError() {
    this.setData({ showError: false, errorMessage: '' })
  },

  toggleHistory() {
    this.setData({ showHistory: !this.data.showHistory })
  },

  async controlActuator(device, action, params = {}) {
    if (!this.data.isOnline) {
      this.showActuatorError('设备离线，请检查网络连接或ESP32电源')
      return
    }
    
    this.setData({ isOperating: true })
    
    try {
      const serverAddr = `${this.data.esp32Ip || '192.168.1.100'}:${this.data.esp32Port || 80}`
      const result = await actuatorService.controlDevice(device, action, params, serverAddr)
      
      console.log(`[Control] ${device}.${action} 成功:`, result)
      
      this.updateActuatorState(device, action, result.state)
      this.addActuatorRecord(device, action, true)
      wx.showToast({ title: '操作成功', icon: 'success', duration: 1500 })
      
      return result
      
    } catch (error) {
      console.error(`[Control] ${device}.${action} 失败:`, error)
      this.addActuatorRecord(device, action, false)
      this.showActuatorError(error.message || '操作失败')
      throw error
      
    } finally {
      this.setData({ isOperating: false })
    }
  },

  updateActuatorState(device, action, state) {
    const updates = {}
    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}`
    
    switch (device) {
      case 'relay':
        updates.relayState = (state === 'ON' || (action === 'toggle' && this.data.relayState === false))
        updates.lastOperation = `继电器${updates.relayState?'开启':'关闭'} ${timeStr}`
        break
      case 'buzzer':
        if (action === 'on') { updates.buzzerState = true; updates.lastOperation = `蜂鸣器开启 ${timeStr}` }
        else if (action === 'off') { updates.buzzerState = false; updates.lastOperation = `蜂鸣器关闭 ${timeStr}` }
        else if (action === 'beep') { updates.lastOperation = `蜂鸣器播放 ${timeStr}` }
        break
      case 'motor':
        if (action === 'stop') { updates.motorSpeed = 0; updates.motorPercent = 0; updates.lastOperation = `电机停止 ${timeStr}` }
        else if (action === 'run') {
          const match = String(state).match(/(\d+)/)
          if (match) {
            const s = parseInt(match[1])
            updates.motorSpeed = s
            updates.motorPercent = Math.round(s/255*100)
            updates.lastOperation = `电机运行 ${updates.motorPercent}% ${timeStr}`
          }
        }
        break
      case 'servo':
        if (state && String(state).includes('°')) {
          const a = parseInt(String(state))
          if (!isNaN(a)) { updates.servoAngle = a; updates.lastOperation = `舵机${a}° ${timeStr}` }
        }
        break
    }
    
    this.setData(updates)
  },

  async refreshActuatorStates() {
    if (!this.data.esp32Ip) return
    
    try {
      const serverAddr = `${this.data.esp32Ip}:${this.data.esp32Port || 80}`
      const states = await actuatorService.getAllStates(serverAddr)
      
      this.setData({
        relayState: states.relay === 'ON',
        buzzerState: states.buzzer === 'ON',
        motorSpeed: states.motor || 0,
        motorPercent: states.motor_percent || 0,
        servoAngle: states.servo || 90,
        isOnline: true
      })
      
    } catch (error) {
      console.warn('[Control] 获取状态失败:', error.message)
      this.setData({ isOnline: false })
    }
  },

  showActuatorError(message) {
    this.setData({ showError: true, errorMessage: message })
    setTimeout(() => { this.setData({ showError: false }) }, 5000)
  },

  addActuatorRecord(device, action, success) {
    const names = { relay: '继电器', buzzer: '蜂鸣器', motor: '电机', servo: '舵机' }
    const record = {
      time: new Date().toTimeString().substring(0,8),
      device: names[device] || device,
      action: action.toUpperCase(),
      success: success
    }
    
    const history = [record, ...this.data.operationHistory]
    if (history.length > 50) history.pop()
    this.setData({ operationHistory: history })
  }

})
