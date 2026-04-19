/**
 * 外设控制面板 - 主页面逻辑
 * 功能：继电器/蜂鸣器/电机/舵机的远程控制与状态管理
 */

const actuatorService = require('../../utils/actuator-service')

Page({
  
  /**
   * 页面初始数据
   */
  data: {
    // 连接状态
    isOnline: false,
    
    // 操作锁（防止重复提交）
    isOperating: false,
    
    // 继电器状态
    relayState: false,
    
    // 蜂鸣器状态
    buzzerState: false,
    isBeeping: false,
    selectedFreqIndex: 1,  // 默认2000Hz
    freqOptions: ['500', '1000', '2000', '3000', '5000'],
    
    // 电机状态
    motorSpeed: 0,        // PWM值 (0-255)
    motorPercent: 0,      // 百分比 (0-100)
    lastMotorPercent: 50, // 上次使用的速度值（用于恢复）
    
    // 舵机状态
    servoAngle: 90,       // 角度 (0-180)
    
    // 数值输入状态
    motorInputValue: '0',  // 电机输入值
    servoInputValue: '90',  // 舵机输入值
    
    // 设备在线状态
    onlineCount: 4,
    
    // 错误提示
    showError: false,
    errorMessage: '',
    
    // 最后操作记录
    lastOperation: '',
    
    // 操作历史
    operationHistory: [],
    showHistory: false,
    
    // 主题模式
    themeMode: 'dark'
  },

  /**
   * 生命周期 - 页面加载
   */
  onLoad(options) {
    console.log('[Control] 页面加载完成')
    
    // 从全局数据获取ESP32地址配置
    const app = getApp()
    if (app.globalData && app.globalData.esp32Ip) {
      this.esp32Ip = app.globalData.esp32Ip
      this.esp32Port = app.globalData.esp32Port || 80
      console.log(`[Control] ESP32地址: ${this.esp32Ip}:${this.esp32Port}`)
    } else {
      // 尝试从本地存储读取
      try {
        const config = wx.getStorageSync('esp32_config') || {}
        this.esp32Ip = config.ip || '192.168.1.100'
        this.esp32Port = config.port || 80
        console.log(`[Control] 从本地存储加载ESP32地址: ${this.esp32Ip}:${this.esp32Port}`)
      } catch (e) {
        console.error('[Control] 无法读取ESP32配置:', e)
        this.showErrorMsg('请先在"测试"页面配置ESP32地址')
      }
    }
    
    // 加载保存的主题设置
    try {
      const theme = wx.getStorageSync('theme_mode') || 'dark'
      this.setData({ themeMode: theme })
    } catch (e) {
      console.warn('[Control] 读取主题设置失败:', e)
    }
    
    // 默认设置为在线状态，让用户可以尝试操作
    // 实际连接状态会在 refreshAllStates 中验证
    if (this.esp32Ip) {
      this.setData({ isOnline: true })
    }
    
    // 初始化时获取所有外设状态
    this.refreshAllStates()
  },

  /**
   * 生命周期 - 页面显示
   */
  onShow() {
    console.log('[Control] 页面显示')
    // 每次显示页面时刷新状态
    this.refreshAllStates()
  },

  /**
   * 生命周期 - 页面隐藏
   */
  onHide() {
    console.log('[Control] 页面隐藏')
  },

  // ==================== 网络请求封装 ====================

  /**
   * 控制外设的通用方法
   * @param {string} device - 设备类型 (relay/buzzer/motor/servo)
   * @param {string} action - 操作 (on/off/toggle/beep/run/stop/write...)
   * @param {object} params - 额外参数 {freq, duration, speed, angle}
   */
  async controlDevice(device, action, params = {}) {
    if (!this.checkOnline()) {
      throw new Error('设备离线')
    }
    
    this.setData({ isOperating: true })
    
    const startTime = Date.now()
    
    try {
      const result = await actuatorService.controlDevice(
        device, 
        action, 
        params,
        `${this.esp32Ip}:${this.esp32Port}`
      )
      
      const duration = Date.now() - startTime
      console.log(`[Control] ${device}.${action} 成功 (${duration}ms):`, result)
      
      // 更新本地状态
      this.updateLocalState(device, action, result.state)
      
      // 记录操作历史
      this.addOperationRecord(device, action, true, duration)
      
      // 显示成功提示
      wx.showToast({
        title: '操作成功',
        icon: 'success',
        duration: 1500
      })
      
      return result
      
    } catch (error) {
      console.error(`[Control] ${device}.${action} 失败:`, error)
      
      // 记录失败历史
      this.addOperationRecord(device, action, false, Date.now() - startTime)
      
      // 显示错误信息
      this.showErrorMsg(error.message || '操作失败，请重试')
      
      throw error
      
    } finally {
      this.setData({ isOperating: false })
    }
  },

  /**
   * 检查设备是否在线
   */
  checkOnline() {
    if (!this.data.isOnline) {
      this.showErrorMsg('设备离线，请检查网络连接或ESP32电源')
      return false
    }
    return true
  },

  /**
   * 显示错误提示
   */
  showErrorMsg(message) {
    this.setData({
      showError: true,
      errorMessage: message
    })
    
    // 5秒后自动消失
    setTimeout(() => {
      this.setData({ showError: false })
    }, 5000)
  },

  /**
   * 关闭错误提示
   */
  dismissError() {
    this.setData({ 
      showError: false,
      errorMessage: '' 
    })
  },

  // ==================== 本地状态更新 ====================

  /**
   * 根据API响应更新本地状态
   */
  updateLocalState(device, action, state) {
    const updates = {}
    
    switch (device) {
      case 'relay':
        if (action === 'on' || (action === 'toggle' && state === 'ON')) {
          updates.relayState = true
          updates.lastOperation = `继电器开启 ${this.getCurrentTime()}`
        } else if (action === 'off' || (action === 'toggle' && state === 'OFF')) {
          updates.relayState = false
          updates.lastOperation = `继电器关闭 ${this.getCurrentTime()}`
        }
        break
        
      case 'buzzer':
        if (action === 'on') {
          updates.buzzerState = true
          updates.lastOperation = `蜂鸣器开启 ${this.getCurrentTime()}`
        } else if (action === 'off') {
          updates.buzzerState = false
          updates.isBeeping = false
          updates.lastOperation = `蜂鸣器关闭 ${this.getCurrentTime()}`
        } else if (action === 'beep') {
          updates.isBeeping = true
          updates.lastOperation = `蜂鸣器播放 ${this.getCurrentTime()}`
          
          // 模拟播放结束（实际时间由ESP32控制，这里仅UI反馈）
          setTimeout(() => {
            this.setData({ isBeeping: false })
          }, 600)
        }
        break
        
      case 'motor':
        if (action === 'stop') {
          updates.motorSpeed = 0
          updates.motorPercent = 0
          updates.lastOperation = `电机停止 ${this.getCurrentTime()}`
        } else if (action === 'run') {
          // state格式: "RUN@128"
          const speedMatch = state.match(/RUN@(\d+)/)
          if (speedMatch) {
            const speed = parseInt(speedMatch[1])
            updates.motorSpeed = speed
            updates.motorPercent = Math.round(speed / 255 * 100)
            updates.lastMotorPercent = updates.motorPercent
            updates.lastOperation = `电机运行 ${updates.motorPercent}% ${this.getCurrentTime()}`
          }
        }
        break
        
      case 'servo':
        if (state && state.includes('°')) {
          const angle = parseInt(state.replace('°', ''))
          if (!isNaN(angle)) {
            updates.servoAngle = angle
            updates.lastOperation = `舵机${angle}° ${this.getCurrentTime()}`
          }
        }
        break
    }
    
    this.setData(updates)
  },

  /**
   * 获取当前时间字符串
   */
  getCurrentTime() {
    const now = new Date()
    return `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`
  },

  // ==================== 继电器控制 ====================

  /**
   * Switch组件切换事件
   */
  onRelayToggle(e) {
    const newState = e.detail.value
    this.controlDevice('relay', newState ? 'on' : 'off')
  },

  /**
   * 快捷按钮控制
   */
  controlRelay(e) {
    const action = e.currentTarget.dataset.action
    this.controlDevice('relay', action)
  },

  // ==================== 蜂鸣器控制 ====================

  /**
   * Switch切换蜂鸣器
   */
  onBuzzerToggle(e) {
    const newState = e.detail.value
    this.controlDevice('buzzer', newState ? 'on' : 'off')
  },

  /**
   * 频率选择变化
   */
  onFreqChange(e) {
    const index = e.detail.value
    this.setData({ selectedFreqIndex: index })
  },

  /**
   * 播放单次蜂鸣
   */
  playBuzzerBeep() {
    const freq = parseInt(this.data.freqOptions[this.data.selectedFreqIndex])
    this.controlDevice('buzzer', 'beep', { freq, duration: 500 })
  },

  // ==================== 电机控制 ====================

/**
 * 速度滑块变化结束（松手后触发）
 */
onMotorSpeedChange(e) {
  const percent = e.detail.value
  const speed = Math.round(percent / 100 * 255)
  
  this.setData({
    motorPercent: percent,
    motorSpeed: speed,
    lastMotorPercent: percent
  })
  
  if (speed === 0) {
    this.controlDevice('motor', 'stop')
  } else {
    this.controlDevice('motor', 'run', { speed })
  }
},

/**
 * 速度滑块拖动中（实时显示，不发送请求）
 */
onMotorSpeedChanging(e) {
  const percent = e.detail.value
  const speed = Math.round(percent / 100 * 255)
  
  this.setData({
    motorPercent: percent,
    motorSpeed: speed
  })
},

/**
 * 设置预设速度
 */
setMotorPreset(e) {
  if (!this.checkOnline()) return
  
  const percent = parseInt(e.currentTarget.dataset.percent)
  
  this.setData({ 
    motorPercent: percent,
    lastMotorPercent: percent
  })
  
  const speed = Math.round(percent / 100 * 255)
  this.setData({ motorSpeed: speed })
  
  if (speed === 0) {
    this.controlDevice('motor', 'stop')
  } else {
    this.controlDevice('motor', 'run', { speed })
  }
},

/**
 * 电机数值输入变化
 */
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

  const numValue = parseFloat(value)
  if (!isNaN(numValue)) {
    const percent = Math.round(numValue)
    this.setData({ 
      motorInputValue: value,
      motorPercent: percent
    })
  } else {
    this.setData({ motorInputValue: value })
  }
},

/**
 * 电机输入区域点击（收起键盘）
 * 注意：提交按钮有独立的事件处理，这里只处理键盘收起
 */
onMotorInputTap(e) {
  // 只收起键盘，不提交，避免与提交按钮冲突
  wx.hideKeyboard()
},

/**
 * 电机数值输入确认（按回车键）
 */
onMotorInputConfirm(e) {
  this.submitMotorInput()
},

/**
 * 提交电机数值输入
 */
async submitMotorInput() {
  if (!this.checkOnline()) return
  
  const value = parseFloat(this.data.motorInputValue)
  
  if (isNaN(value)) {
    wx.showToast({
      title: '请输入有效数值',
      icon: 'none',
      duration: 1500
    })
    return
  }
  
  // 限制范围 0-100
  let percent = Math.round(value)
  percent = Math.max(0, Math.min(100, percent))
  
  // 更新状态
  this.setData({
    motorPercent: percent,
    motorSpeed: Math.round(percent / 100 * 255),
    lastMotorPercent: percent,
    motorInputValue: String(percent)
  })
  
  // 发送控制命令
  try {
    if (percent === 0) {
      await this.controlDevice('motor', 'stop')
    } else {
      await this.controlDevice('motor', 'run', { speed: Math.round(percent / 100 * 255) })
    }
  } catch (error) {
    // 错误已在controlDevice中处理，这里只需要捕获避免未处理的Promise拒绝
    console.log('[Control] 电机控制命令已发送，但可能失败')
  }
},

/**
 * 点击电机滑块轨道 - 直接跳转到点击位置
 */
onMotorSliderTrackTap(e) {
  if (!this.data.isOnline) return
  
  const query = wx.createSelectorQuery()
  query.select('.motor-slider').boundingClientRect()
  query.exec((res) => {
    if (!res || !res[0]) return
    
    const sliderRect = res[0]
    const touchX = e.touches[0].clientX
    
    // 计算点击位置在滑块上的比例
    const sliderWidth = sliderRect.width - 22 // 减去滑块块的大小
    const offsetX = touchX - sliderRect.left - 11 // 减去半个滑块块
    const ratio = Math.max(0, Math.min(1, offsetX / sliderWidth))
    
    // 计算对应的百分比值
    const percent = Math.round(ratio * 100)
    const speed = Math.round(percent / 100 * 255)
    
    // 更新状态
    this.setData({
      motorPercent: percent,
      motorSpeed: speed
    })
    
    // 发送控制命令
    if (speed === 0) {
      this.controlDevice('motor', 'stop')
    } else {
      this.controlDevice('motor', 'run', { speed })
    }
  })
},

/**
 * 点击舵机滑块轨道 - 直接跳转到点击位置
 */
onServoSliderTrackTap(e) {
  if (!this.data.isOnline) return
  
  const query = wx.createSelectorQuery()
  query.select('.servo-slider').boundingClientQuery()
  query.exec((res) => {
    if (!res || !res[0]) return
    
    const sliderRect = res[0]
    const touchX = e.touches[0].clientX
    
    // 计算点击位置在滑块上的比例
    const sliderWidth = sliderRect.width - 18 // 减去滑块块的大小
    const offsetX = touchX - sliderRect.left - 9 // 减去半个滑块块
    const ratio = Math.max(0, Math.min(1, offsetX / sliderWidth))
    
    // 计算对应的角度值 (0-180)
    const angle = Math.round(ratio * 180)
    
    // 更新状态
    this.setData({ servoAngle: angle })
    
    // 发送控制命令
    this.controlDevice('servo', 'write', { angle })
  })
},

/**
 * 滑块触摸开始 - 阻止事件冒泡防止页面切换
 */
onSliderTouchStart(e) {
  // 阻止事件冒泡，防止触发页面滑动切换
  // 小程序中 catch 事件会自动阻止冒泡
  console.log('[Control] 滑块触摸开始，阻止页面切换')
},

/**
 * 滑块触摸结束
 */
onSliderTouchEnd(e) {
  console.log('[Control] 滑块触摸结束')
},

  // ==================== 舵机控制 ====================

  /**
   * 角度滑块变化结束
   */
  onServoAngleChange(e) {
    const angle = e.detail.value
    this.controlDevice('servo', 'write', { angle })
  },

  /**
   * 角度滑块拖动中
   */
  onServoAngleChanging(e) {
    const angle = e.detail.value
    this.setData({ servoAngle: angle })
  },

  /**
 * 设置预设角度
 */
setServoPreset(e) {
  const angle = parseInt(e.currentTarget.dataset.angle)
  this.setData({ 
    servoAngle: angle,
    servoInputValue: String(angle)
  })
  this.controlDevice('servo', 'write', { angle })
},

/**
 * 舵机数值输入变化
 */
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

  const numValue = parseFloat(value)
  if (!isNaN(numValue)) {
    const angle = Math.round(numValue)
    this.setData({ 
      servoInputValue: value,
      servoAngle: angle
    })
  } else {
    this.setData({ servoInputValue: value })
  }
},

/**
 * 舵机输入区域点击（收起键盘）
 * 注意：提交按钮有独立的事件处理，这里只处理键盘收起
 */
onServoInputTap(e) {
  // 只收起键盘，不提交，避免与提交按钮冲突
  wx.hideKeyboard()
},

/**
 * 舵机数值输入确认（按回车键）
 */
onServoInputConfirm(e) {
  this.submitServoInput()
},

/**
 * 提交舵机数值输入
 */
async submitServoInput() {
  if (!this.checkOnline()) return
  
  const value = parseFloat(this.data.servoInputValue)
  
  if (isNaN(value)) {
    wx.showToast({
      title: '请输入有效角度',
      icon: 'none',
      duration: 1500
    })
    return
  }
  
  // 限制范围 0-180
  let angle = Math.round(value)
  angle = Math.max(0, Math.min(180, angle))
  
  // 更新状态
  this.setData({
    servoAngle: angle,
    servoInputValue: String(angle)
  })
  
  // 发送控制命令
  try {
    await this.controlDevice('servo', 'write', { angle })
  } catch (error) {
    // 错误已在controlDevice中处理，这里只需要捕获避免未处理的Promise拒绝
    console.log('[Control] 舵机控制命令已发送，但可能失败')
  }
},

  // ==================== 状态查询与刷新 ====================

  /**
   * 刷新所有外设状态
   */
  async refreshAllStates() {
    if (!this.esp32Ip) {
      console.warn('[Control] 未配置ESP32地址，跳过状态刷新')
      return
    }
    
    console.log('[Control] 正在刷新所有外设状态...')
    
    try {
      const result = await actuatorService.getAllStates(`${this.esp32Ip}:${this.esp32Port}`)
      
      console.log('[Control] 获取状态成功:', result)
      
      // 计算电机状态
      const motorSpeed = result.motor || 0
      const motorPercent = result.motor_percent || 0
      const motorEnabled = motorSpeed > 0
      
      // 更新所有设备状态
      const servoAngle = result.servo || 90
      this.setData({
        relayState: result.relay === 'ON',
        buzzerState: result.buzzer === 'ON',
        motorSpeed: motorSpeed,
        motorPercent: motorPercent,
        lastMotorPercent: motorPercent > 0 ? motorPercent : (this.data.lastMotorPercent || 50),
        servoAngle: servoAngle,
        servoInputValue: String(servoAngle),
        isOnline: true,
        onlineCount: 4  // 假设全部在线（后续可扩展为真实检测）
      })
      
    } catch (error) {
      console.error('[Control] 获取状态失败:', error)
      this.setData({ isOnline: false })
      
      // 仅在首次加载时显示错误
      if (!this.data.hasLoadedOnce) {
        this.showErrorMsg('无法连接ESP32，请检查网络和设备电源')
        this.setData({ hasLoadedOnce: true })
      }
    }
  },

  /**
   * 手动触发刷新按钮
   */
  manualRefreshStates() {
    this.refreshAllStates()
    wx.showToast({
      title: '刷新中...',
      icon: 'loading',
      duration: 1000
    })
  },

  // ==================== 操作历史管理 ====================

  /**
   * 添加操作记录到历史列表
   */
  addOperationRecord(device, action, success, duration) {
    const deviceNames = {
      relay: '继电器',
      buzzer: '蜂鸣器',
      motor: '电机',
      servo: '舵机'
    }
    
    const record = {
      time: this.getCurrentTime(),
      device: deviceNames[device] || device,
      action: action.toUpperCase(),
      success: success,
      duration: duration
    }
    
    // 添加到历史数组头部
    const history = [record, ...this.data.operationHistory]
    
    // 最多保留50条记录
    if (history.length > 50) {
      history.pop()
    }
    
    this.setData({ operationHistory: history })
  },

  /**
   * 切换历史记录展开/折叠
   */
  toggleHistory() {
    this.setData({ showHistory: !this.data.showHistory })
  },

  /**
   * 跳转到自动控制配置页面
   */
  goToAutoControl() {
    wx.navigateTo({
      url: '/pages/auto-control/auto-control'
    })
  }
})
