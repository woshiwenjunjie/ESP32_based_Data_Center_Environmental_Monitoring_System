/**
 * 外设控制 API 服务层
 * 封装与 ESP32 HTTP API 的所有通信逻辑
 * 
 * 功能：
 *   - 统一的HTTP请求封装（带重试、超时、错误处理）
 *   - 设备类型和操作类型的枚举定义
 *   - 自动构建请求URL和参数
 *   - 响应解析和数据提取
 * 
 * 使用示例：
 *   const actuatorService = require('./actuator-service')
 *   
 *   // 控制继电器
 *   const result = await actuatorService.controlDevice('relay', 'on', {}, '192.168.1.100:80')
 *   console.log(result) // { success: true, device: 'relay', action: 'on', state: 'ON' }
 *   
 *   // 获取所有状态
 *   const states = await actuatorService.getAllStates('192.168.1.100:80')
 */

// ==================== 配置常量 ====================

const DEFAULT_TIMEOUT = 8000           // 默认超时时间：8秒（外设控制要求快速响应）
const MAX_RETRIES = 1                  // 最大重试次数
const RETRY_DELAY_BASE = 1000         // 重试基础延迟（毫秒）

const BASE_URL_PREFIX = 'http://'

// ==================== 设备类型枚举 ====================

const DEVICE_TYPES = {
  RELAY: 'relay',
  BUZZER: 'buzzer',
  MOTOR: 'motor',
  SERVO: 'servo'
}

// ==================== 操作类型白名单 ====================

const ACTION_MAP = {
  [DEVICE_TYPES.RELAY]: ['on', 'off', 'toggle', 'status'],
  [DEVICE_TYPES.BUZZER]: ['on', 'off', 'beep', 'status'],
  [DEVICE_TYPES.MOTOR]: ['stop', 'run', 'status'],
  [DEVICE_TYPES.SERVO]: ['attach', 'detach', 'write', 'status']
}

// ==================== 错误类型定义 ====================

const ERROR_TYPES = {
  NETWORK_ERROR: 'network_error',
  TIMEOUT: 'timeout',
  INVALID_PARAMS: 'invalid_params',
  DEVICE_ERROR: 'device_error',
  SERVER_ERROR: 'server_error',
  UNKNOWN: 'unknown'
}

// ==================== 错误消息映射 ====================

const ERROR_MESSAGES = {
  [ERROR_TYPES.NETWORK_ERROR]: '网络连接失败，请检查WiFi或ESP32是否在线',
  [ERROR_TYPES.TIMEOUT]: '请求超时，ESP32响应时间过长',
  [ERROR_TYPES.INVALID_PARAMS]: '无效的设备或操作参数',
  [ERROR_TYPES.DEVICE_ERROR]: '外设操作失败，请检查硬件连接',
  [ERROR_TYPES.SERVER_ERROR]: '服务器内部错误',
  [ERROR_TYPES.UNKNOWN]: '未知错误，请稍后重试'
}

// ==================== 核心API函数 ====================

/**
 * 控制指定外设
 * @param {string} device - 设备类型 (relay/buzzer/motor/servo)
 * @param {string} action - 操作类型 (on/off/toggle/beep/run/stop/write...)
 * @param {object} params - 可选参数对象
 * @param {string} serverAddress - ESP32地址 "IP:PORT" 格式
 * @returns {Promise<object>} API响应数据
 */
async function controlDevice(device, action, params = {}, serverAddress) {
  // 参数校验
  if (!validateDeviceAction(device, action)) {
    throw createError(
      ERROR_TYPES.INVALID_PARAMS,
      `不支持的设备或操作: ${device}.${action}\n支持的操作: ${(ACTION_MAP[device] || []).join(', ') || '无'}`
    )
  }
  
  if (!serverAddress) {
    throw createError(ERROR_TYPES.INVALID_PARAMS, '未提供ESP32服务器地址')
  }
  
  // 构建请求URL
  const url = buildRequestUrl(serverAddress, device, action, params)
  
  console.log(`[ActuatorService] 发送请求: ${url}`)
  
  try {
    // 发送HTTP请求（带重试机制）
    const response = await requestWithRetry(url, MAX_RETRIES)
    
    console.log(`[ActuatorService] 收到响应:`, response.data)
    
    // 解析响应
    return parseResponse(response.data)
    
  } catch (error) {
    // 错误已经过处理，直接抛出
    console.error(`[ActuatorService] 请求失败:`, error.message)
    throw error
  }
}

/**
 * 获取所有外设的当前状态
 * @param {string} serverAddress - ESP32地址 "IP:PORT"
 * @returns {Promise<object>} 所有外设状态对象
 */
async function getAllStates(serverAddress) {
  if (!serverAddress) {
    throw createError(ERROR_TYPES.INVALID_PARAMS, '未提供ESP32服务器地址')
  }
  
  const url = `${BASE_URL_PREFIX}${serverAddress}/actuator`
  
  console.log('[ActuatorService] 获取所有外设状态...')
  
  try {
    const response = await requestWithRetry(url, MAX_RETRIES)
    
    if (response.data && response.data.success) {
      // 返回状态数据（去除元数据字段）
      const { success, action, timestamp, ...states } = response.data
      return states
    } else {
      throw createError(ERROR_TYPES.SERVER_ERROR, '获取状态失败: ' + JSON.stringify(response.data))
    }
    
  } catch (error) {
    console.error('[ActuatorService] 获取状态失败:', error.message)
    throw error
  }
}

// ==================== 内部工具函数 ====================

/**
 * 验证设备和操作的合法性
 */
function validateDeviceAction(device, action) {
  // 检查设备类型是否有效
  if (!ACTION_MAP[device]) {
    return false
  }
  
  // 检查操作是否在该设备的白名单中
  const allowedActions = ACTION_MAP[device]
  if (!allowedActions.includes(action)) {
    return false
  }
  
  return true
}

/**
 * 构建完整的请求URL
 */
function buildRequestUrl(serverAddress, device, action, params = {}) {
  let url = `${BASE_URL_PREFIX}${serverAddress}/actuator?device=${encodeURIComponent(device)}&action=${encodeURIComponent(action)}`
  
  // 添加额外参数
  Object.keys(params).forEach(key => {
    const value = params[key]
    if (value !== undefined && value !== null) {
      url += `&${encodeURIComponent(key)}=${encodeURIComponent(value)}`
    }
  })
  
  return url
}

/**
 * 带重试机制的HTTP请求
 */
function requestWithRetry(url, maxRetries) {
  return new Promise((resolve, reject) => {
    let retryCount = 0
    
    const attemptRequest = () => {
      const startTime = Date.now()
      
      wx.request({
        url: url,
        method: 'GET',
        timeout: DEFAULT_TIMEOUT,
        header: {
          'Content-Type': 'application/json'
        },
        success: (res) => {
          const duration = Date.now() - startTime
          
          console.log(`[ActuatorService] HTTP ${res.statusCode} (${duration}ms): ${url.substring(0, 60)}...`)
          
          if (res.statusCode === 200) {
            resolve({
              data: res.data,
              statusCode: res.statusCode,
              duration: duration
            })
          } else if (res.statusCode >= 400 && res.statusCode < 500) {
            // 客户端错误（参数错误等），不重试
            const errorMsg = res.data?.error || `客户端错误 (${res.statusCode})`
            reject(createError(ERROR_TYPES.INVALID_PARAMS, errorMsg))
          } else if (res.statusCode >= 500) {
            // 服务器错误，可重试
            if (retryCount < maxRetries) {
              retryCount++
              const delay = RETRY_DELAY_BASE * retryCount
              console.warn(`[ActuatorService] 服务器错误，第${retryCount}次重试 (${delay}ms后)...`)
              setTimeout(attemptRequest, delay)
            } else {
              reject(createError(ERROR_TYPES.SERVER_ERROR, `服务器错误 (${res.statusCode}): ${res.data?.error || 'Unknown'}`))
            }
          } else {
            reject(createError(ERROR_TYPES.UNKNOWN, `意外的HTTP状态码: ${res.statusCode}`))
          }
        },
        fail: (err) => {
          const duration = Date.now() - startTime
          console.error(`[ActuatorService] 请求失败 (${duration}ms):`, err.errMsg)
          
          // 判断错误类型
          if (err.errMsg.includes('timeout')) {
            if (retryCount < maxRetries) {
              retryCount++
              const delay = RETRY_DELAY_BASE * retryCount
              console.warn(`[ActuatorService] 超时，第${retryCount}次重试 (${delay}ms后)...`)
              setTimeout(attemptRequest, delay)
            } else {
              reject(createError(ERROR_TYPES.TIMEOUT, `请求超时 (${(duration/1000).toFixed(1)}s)，ESP32可能未响应`))
            }
          } else if (err.errMsg.includes('fail') || err.errMsg.includes('net::')) {
            // 网络连接问题
            reject(createError(ERROR_TYPES.NETWORK_ERROR, '无法连接到ESP32，请检查网络设置'))
          } else {
            reject(createError(ERROR_TYPES.UNKNOWN, err.errMsg || '网络请求失败'))
          }
        }
      })
    }
    
    // 发起首次请求
    attemptRequest()
  })
}

/**
 * 解析API响应并返回标准化数据
 */
function parseResponse(data) {
  if (!data) {
    throw createError(ERROR_TYPES.SERVER_ERROR, '空响应数据')
  }
  
  if (data.success) {
    // 成功响应
    return {
      success: true,
      device: data.device,
      action: data.action,
      state: data.state,
      timestamp: data.timestamp,
      raw: data
    }
  } else {
    // 失败响应（服务端返回的错误）
    throw createError(
      ERROR_TYPES.DEVICE_ERROR,
      data.error || '外设操作失败'
    )
  }
}

/**
 * 创建标准化的错误对象
 */
function createError(type, message, data = null) {
  const error = new Error(message)
  error.type = type
  error.isActuatorError = true
  error.timestamp = new Date().toISOString()
  error.data = data
  
  return error
}

// ==================== 导出模块 ====================

module.exports = {
  // 核心API
  controlDevice,
  getAllStates,
  
  // 常量导出（供其他模块使用）
  DEVICE_TYPES,
  ACTION_MAP,
  ERROR_TYPES,
  ERROR_MESSAGES,
  
  // 工具函数（可选暴露）
  validateDeviceAction,
  createError
}
