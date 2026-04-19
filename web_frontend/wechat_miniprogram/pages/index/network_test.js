/**
 * ============================================================
 *  微信小程序 - 网络诊断 + 大模型验证测试页面
 * ============================================================
 * 
 * 功能模块：
 *   🔍 网络诊断工具：
 *     - WiFi状态检测
 *     - ESP32连接测试（HTTP GET /）
 *     - 传感器API测试（/sensors）
 *     - 视频流API测试（/stream）
 *     - 拍照API测试（/capture）
 *   
 *   🧪 LLM验证测试（新增）：
 *     - 支持国内外10+主流大模型
 *     - 选择模型自动填充API地址
 *     - 实时显示响应时间和Token用量
 *     - 配置保存到本地存储
 *     - 一键同步AI配置到ESP32设备
 * 
 * 支持的AI服务商：
 *   OpenAI (GPT-4o系列) | 通义千问(阿里云) | Kimi(月之暗面)
 *   DeepSeek | 豆包(火山引擎/字节跳动)
 * 
 * 页面路径: pages/index/network_test
 * 依赖: wx.request API, wx.setStorageSync API
 * ============================================================
 */

// network_test.js - 网络诊断工具 + 大模型验证测试
const app = getApp()

Page({
  data: {
    esp32Ip: '192.168.1.100',
    esp32Port: 80,
    testResults: [],
    isTesting: false,
    // AI测试配置
    aiUrl: '',
    aiKey: '',
    aiModel: 'gpt-4o-mini',
    aiTimeout: 30,
    aiQuestion: '你好，请用一句话介绍你自己。',
    aiTestResult: '',
    aiTestStatus: '',
    // 模型预设列表
    modelList: [
      { name: 'GPT-4o Mini (OpenAI)', value: 'gpt-4o-mini', group: 'OpenAI' },
      { name: 'GPT-4o (OpenAI)', value: 'gpt-4o', group: 'OpenAI' },
      { name: '通义千问 Qwen-Turbo (阿里云)', value: 'qwen-turbo', group: '国内主流模型' },
      { name: '通义千问 Qwen-Plus (阿里云)', value: 'qwen-plus', group: '国内主流模型' },
      { name: '通义千问 Qwen-Max (阿里云)', value: 'qwen-max', group: '国内主流模型' },
      { name: 'Kimi (月之暗面)', value: 'kimi', group: '国内主流模型' },
      { name: 'DeepSeek Chat', value: 'deepseek-chat', group: '国内主流模型' },
      { name: 'DeepSeek Reasoner', value: 'deepseek-reasoner', group: '国内主流模型' },
      { name: '豆包 Pro (火山引擎)', value: 'doubao-pro', group: '国内主流模型' },
      { name: '豆包 Lite (火山引擎)', value: 'doubao-lite', group: '国内主流模型' }
    ]
  },

  onLoad() {
    const config = wx.getStorageSync('esp32_config')
    if (config) {
      this.setData({
        esp32Ip: config.ip || '192.168.1.100',
        esp32Port: config.port || 80
      })
    }
    const aiConfig = wx.getStorageSync('ai_test_config')
    if (aiConfig) {
      this.setData({
        aiUrl: aiConfig.url || '',
        aiKey: aiConfig.key || '',
        aiModel: aiConfig.model || 'gpt-4o-mini',
        aiTimeout: aiConfig.timeout || 30
      })
    }
  },

  getEsp32Url() {
    return `http://${this.data.esp32Ip}:${this.data.esp32Port}`
  },

  // 输入IP
  inputIp(e) {
    this.setData({ esp32Ip: e.detail.value })
  },

  // 输入端口
  inputPort(e) {
    this.setData({ esp32Port: parseInt(e.detail.value) || 80 })
  },

  // 运行完整测试
  runFullTest() {
    this.setData({
      testResults: [],
      isTesting: true
    })

    // 依次运行测试
    this.testNetworkStatus()
      .then(() => this.testEsp32Connection())
      .then(() => this.testSensorsApi())
      .then(() => this.testStreamApi())
      .then(() => this.testCaptureApi())
      .finally(() => {
        this.setData({ isTesting: false })
      })
  },

  // 测试1: 网络状态
  testNetworkStatus() {
    return new Promise((resolve) => {
      this.addResult('网络状态检查', '正在检查...')
      
      wx.getNetworkType({
        success: (res) => {
          const isWifi = res.networkType === 'wifi'
          this.addResult('网络状态检查', 
            isWifi ? '通过' : '警告', 
            `当前网络: ${res.networkType}${isWifi ? '' : ' (建议使用WiFi)'}`,
            isWifi ? 'success' : 'warning'
          )
          resolve()
        },
        fail: () => {
          this.addResult('网络状态检查', '失败', '无法获取网络状态', 'error')
          resolve()
        }
      })
    })
  },

  // 测试2: ESP32连接
  testEsp32Connection() {
    return new Promise((resolve) => {
      this.addResult('ESP32连接测试', '正在连接...')
      
      const url = `${this.getEsp32Url()}/`

      wx.request({
        url: url,
        method: 'GET',
        timeout: 5000,
        success: (res) => {
          this.addResult('ESP32连接测试', '成功',
            `状态码: ${res.statusCode}, 响应长度: ${res.data?.length || 0}`,
            'success'
          )
          resolve()
        },
        fail: (err) => {
          this.addResult('ESP32连接测试', '失败',
            `错误: ${err.errMsg}`,
            'error'
          )
          resolve()
        }
      })
    })
  },

  // 测试3: 传感器API
  testSensorsApi() {
    return new Promise((resolve) => {
      this.addResult('传感器API测试', '正在请求...')
      
      const url = `${this.getEsp32Url()}/sensors`

      wx.request({
        url: url,
        timeout: 5000,
        success: (res) => {
          if (res.statusCode === 200) {
            const data = res.data
            const hasData = data &&
              (data.temperature !== undefined ||
               data.humidity !== undefined)

            this.addResult('传感器API测试', '成功',
              hasData ? `温度: ${data.temperature}, 湿度: ${data.humidity}` : '返回数据为空',
              hasData ? 'success' : 'warning'
            )
          } else {
            this.addResult('传感器API测试', '失败',
              `状态码: ${res.statusCode}`,
              'error'
            )
          }
          resolve()
        },
        fail: (err) => {
          this.addResult('传感器API测试', '失败',
            `错误: ${err.errMsg}`,
            'error'
          )
          resolve()
        }
      })
    })
  },

  // 测试4: 视频流API
  testStreamApi() {
    return new Promise((resolve) => {
      this.addResult('视频流API测试', '正在检查...')
      
      const url = `${this.getEsp32Url()}/stream`

      wx.request({
        url: url,
        timeout: 3000,
        success: (res) => {
          this.addResult('视频流API测试', '成功',
            '视频流接口可访问',
            'success'
          )
          resolve()
        },
        fail: (err) => {
          if (err.errMsg && err.errMsg.indexOf('timeout') !== -1) {
            this.addResult('视频流API测试', '通过',
              '连接建立成功（视频流为长连接）',
              'success'
            )
          } else {
            this.addResult('视频流API测试', '失败',
              '错误: ' + err.errMsg,
              'error'
            )
          }
          resolve()
        }
      })
    })
  },

  // 测试5: 拍照API
  testCaptureApi() {
    return new Promise((resolve) => {
      this.addResult('拍照API测试', '正在请求...')
      
      const url = `${this.getEsp32Url()}/capture`

      wx.request({
        url: url,
        responseType: 'arraybuffer',
        timeout: 10000,
        success: (res) => {
          if (res.statusCode === 200 && res.data) {
            const size = res.data.byteLength || res.data.length
            this.addResult('拍照API测试', '成功',
              `获取到图片数据，大小: ${(size/1024).toFixed(1)} KB`,
              'success'
            )
          } else {
            this.addResult('拍照API测试', '失败',
              `状态码: ${res.statusCode}`,
              'error'
            )
          }
          resolve()
        },
        fail: (err) => {
          this.addResult('拍照API测试', '失败',
            `错误: ${err.errMsg}`,
            'error'
          )
          resolve()
        }
      })
    })
  },

  // 格式化时间 - 兼容写法
  formatTime(date) {
    try {
      const hours = date.getHours() < 10 ? '0' + date.getHours() : String(date.getHours())
      const minutes = date.getMinutes() < 10 ? '0' + date.getMinutes() : String(date.getMinutes())
      const seconds = date.getSeconds() < 10 ? '0' + date.getSeconds() : String(date.getSeconds())
      return hours + ':' + minutes + ':' + seconds
    } catch (e) {
      return '--:--:--'
    }
  },

  // 添加测试结果
  addResult(name, status, detail = '', type = 'info') {
    const results = this.data.testResults
    results.push({
      name,
      status,
      detail,
      type,
      time: this.formatTime(new Date())
    })
    this.setData({ testResults: results })
  },

  // 保存配置
  saveConfig() {
    const { esp32Ip, esp32Port } = this.data
    wx.setStorageSync('esp32_config', { ip: esp32Ip, port: esp32Port })
    app.globalData.esp32Url = `http://${esp32Ip}:${esp32Port}`
    app.globalData.esp32Ip = esp32Ip
    app.globalData.esp32Port = esp32Port
    wx.showToast({
      title: '配置已保存',
      icon: 'success'
    })
  },

  // ==================== 大模型验证测试 ====================
  inputAiUrl(e) {
    this.setData({ aiUrl: e.detail.value })
  },
  inputAiKey(e) {
    this.setData({ aiKey: e.detail.value })
  },
  pickAiModel(e) {
    const names = this.data.modelList.map(m => m.name)
    wx.showActionSheet({
      itemList: names,
      success: (res) => {
        const selected = this.data.modelList[res.tapIndex]
        this.setData({ aiModel: selected.value })
        const presetUrl = this.getAiPresetUrl(selected.value)
        if (presetUrl) {
          this.setData({ aiUrl: presetUrl })
          wx.showToast({ title: 'API地址已自动填充', icon: 'none' })
        }
      }
    })
  },

  getAiPresetUrl(model) {
    const presets = {
      'gpt-4o-mini': 'https://api.openai.com/v1/chat/completions',
      'gpt-4o': 'https://api.openai.com/v1/chat/completions',
      'gpt-3.5-turbo': 'https://api.openai.com/v1/chat/completions',
      'qwen-turbo': 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
      'qwen-plus': 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
      'qwen-max': 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
      'kimi': 'https://api.moonshot.cn/v1/chat/completions',
      'deepseek-chat': 'https://api.deepseek.com/v1/chat/completions',
      'deepseek-reasoner': 'https://api.deepseek.com/v1/chat/completions',
      'doubao-pro': 'https://ark.cn-beijing.volces.com/api/v3/chat/completions',
      'doubao-lite': 'https://ark.cn-beijing.volces.com/api/v3/chat/completions'
    }
    return presets[model] || ''
  },
  inputAiQuestion(e) {
    this.setData({ aiQuestion: e.detail.value })
  },
  inputAiTimeout(e) {
    this.setData({ aiTimeout: parseInt(e.detail.value) || 30 })
  },

  runAITest() {
    const { aiUrl, aiKey, aiModel, aiTimeout, aiQuestion } = this.data

    if (!aiUrl || !aiKey) {
      wx.showToast({ title: '请填写API地址和Key', icon: 'none' })
      return
    }

    this.setData({
      aiTestStatus: 'testing',
      aiTestResult: '正在连接 ' + aiModel + '...'
    })

    const startTime = Date.now()

    wx.request({
      url: aiUrl,
      method: 'POST',
      header: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + aiKey
      },
      data: {
        model: aiModel,
        messages: [{ role: 'user', content: aiQuestion }],
        max_tokens: 500,
        temperature: 0.7
      },
      timeout: (aiTimeout || 30) * 1000,
      success: (res) => {
        const latency = Date.now() - startTime
        if (res.statusCode === 200 && res.data) {
          const reply = res.data.choices?.[0]?.message?.content || JSON.stringify(res.data)
          const usage = res.data.usage ? 
            `\nToken: 输入${res.data.usage.prompt_tokens}, 输出${res.data.usage.completion_tokens}` : ''
          this.setData({
            aiTestStatus: 'success',
            aiTestResult: `[✓] 测试成功 (${latency}ms)\n${usage}\n\n回复:\n${reply}`
          })
          wx.showToast({ title: 'AI连接正常', icon: 'success' })
        } else {
          this.setData({
            aiTestStatus: 'error',
            aiTestResult: `[✗] HTTP ${res.statusCode}\n响应: ${JSON.stringify(res.data).substring(0, 300)}`
          })
        }
      },
      fail: (err) => {
        const latency = Date.now() - startTime
        let msg = err.errMsg || '未知错误'
        if (msg.indexOf('timeout') !== -1) {
          msg = `请求超时 (${aiTimeout}秒)，耗时 ${latency}ms`
        }
        this.setData({
          aiTestStatus: 'error',
          aiTestResult: `[✗] 测试失败\n耗时: ${latency}ms\n错误: ${msg}`
        })
      }
    })
  },

  saveAiConfig() {
    const { aiUrl, aiKey, aiModel, aiTimeout } = this.data
    wx.setStorageSync('ai_test_config', {
      url: aiUrl, key: aiKey, model: aiModel, timeout: aiTimeout
    })
    this.setData({
      aiTestStatus: 'success',
      aiTestResult: '[✓] AI配置已保存'
    })
    wx.showToast({ title: '配置已保存', icon: 'success' })
  },

  syncAiToEsp32() {
    const { aiUrl, aiKey, aiModel } = this.data
    if (!aiUrl || !aiKey) {
      wx.showToast({ title: '请先填写配置', icon: 'none' })
      return
    }

    this.setData({
      aiTestStatus: 'testing',
      aiTestResult: '正在同步到ESP32...'
    })

    wx.request({
      url: `${this.getEsp32Url()}/config`,
      method: 'POST',
      header: { 'Content-Type': 'application/json' },
      data: {
        ai_api_url: aiUrl,
        ai_api_key: aiKey,
        ai_model: aiModel
      },
      timeout: 5000,
      success: (res) => {
        if (res.statusCode === 200) {
          this.setData({
            aiTestStatus: 'success',
            aiTestResult: '[✓] AI配置已同步到ESP32'
          })
          wx.showToast({ title: '同步成功', icon: 'success' })
        } else {
          this.setData({
            aiTestStatus: 'error',
            aiTestResult: `[✗] 同步失败 HTTP ${res.statusCode}`
          })
        }
      },
      fail: () => {
        this.setData({
          aiTestStatus: 'error',
          aiTestResult: '[✗] 同步失败，请检查ESP32连接'
        })
      }
    })
  },

  // 返回首页
  goBack() {
    wx.navigateBack()
  }
})
