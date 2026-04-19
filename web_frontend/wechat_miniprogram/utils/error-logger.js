/**
 * ============================================================
 *  ErrorLogger - AI 服务错误日志管理系统 (v4.0)
 * ============================================================
 * 
 * 功能：
 *   - 统一记录所有AI服务请求的错误信息
 *   - 提供用户友好的中文错误提示
 *   - 支持本地持久化存储（最近20条）
 *   - 错误分类与统计
 *   - 一键复制完整日志（方便反馈）
 * 
 * 使用示例：
 *   const errorLogger = require('./error-logger')
 *   
 *   try {
 *     await aiService.callQwenLLM(question)
 *   } catch (error) {
 *     const logEntry = errorLogger.log(error)
 *     wx.showToast({ title: logEntry.userVisibleMsg })
 *   }
 */

// ==================== 配置常量 ====================

const MAX_MEMORY_LOGS = 50           // 内存中最大保存条数
const MAX_STORAGE_LOGS = 20          // 本地存储最大条数
const STORAGE_KEY = 'ai_error_logs' // 本地存储键名

// ==================== 错误类型定义 ====================

const ERROR_TYPES = {
  NETWORK_ERROR: 'network_error',
  TIMEOUT: 'timeout',
  AUTH_FAILED: 'auth_failed',
  RATE_LIMIT: 'rate_limit',
  SERVER_ERROR: 'server_error',
  INVALID_PARAMS: 'invalid_params',
  PROVIDER_UNAVAILABLE: 'provider_unavailable',
  UNKNOWN: 'unknown'
}

// ==================== 用户友好提示映射 ====================

const USER_FRIENDLY_MESSAGES = {
  [ERROR_TYPES.NETWORK_ERROR]: '网络连接失败，请检查WiFi设置或尝试切换网络（如4G/5G）',
  [ERROR_TYPES.TIMEOUT]: '请求超时（服务器响应过慢），建议简化问题或稍后重试',
  [ERROR_TYPES.AUTH_FAILED]: 'API密钥无效或过期，请在"测试"页面更新配置',
  [ERROR_TYPES.RATE_LIMIT]: '请求过于频繁，请等待30秒后再试',
  [ERROR_TYPES.SERVER_ERROR]: 'AI服务暂时不可用，已自动尝试其他服务商',
  [ERROR_TYPES.INVALID_PARAMS]: '请求参数有误，请检查输入内容',
  [ERROR_TYPES.PROVIDER_UNAVAILABLE]: '当前服务商不可用，正在自动切换备用方案...',
  [ERROR_TYPES.UNKNOWN]: '发生未知错误，请联系技术支持'
}

// ==================== ErrorLogger 类实现 ====================

class ErrorLogger {
  
  constructor() {
    this.logs = []                    // 内存中的日志数组
    this.maxLogs = MAX_MEMORY_LOGS    // 最大容量
    
    // 初始化时从本地存储加载历史日志
    this._loadFromStorage()
    
    console.log('[ErrorLogger] 初始化完成')
    console.log(`[ErrorLogger] 已加载 ${this.logs.length} 条历史记录`)
  }

  /**
   * 记录一条错误日志
   * @param {object|Error} error - 错误对象（可包含 type, message, statusCode, url 等）
   * @returns {object} 完整的日志条目对象
   */
  log(error) {
    const entry = this._createLogEntry(error)
    
    // 添加到内存数组头部（最新的在前）
    this.logs.unshift(entry)
    
    // 超出最大容量时移除最旧的
    if (this.logs.length > this.maxLogs) {
      this.logs.pop()
    }
    
    // 持久化到本地存储（仅保留最近的 N 条）
    this._saveToStorage()
    
    // 控制台输出详细日志（开发者调试用）
    this._consoleOutput(entry)
    
    return entry
  }

  /**
   * 获取最近的错误日志
   * @param {number} count - 返回的条数，默认10
   * @returns {Array} 日志数组
   */
  getRecentErrors(count = 10) {
    return this.logs.slice(0, count)
  }

  /**
   * 按类型筛选日志
   * @param {string} type - 错误类型
   * @returns {Array} 匹配的日志数组
   */
  getErrorsByType(type) {
    return this.logs.filter(log => log.type === type)
  }

  /**
   * 获取错误统计摘要
   * @returns {object} 统计数据
   */
  getStatistics() {
    const stats = {
      total: this.logs.length,
      byType: {},
      last24h: 0,
      lastHour: 0
    }
    
    const now = Date.now()
    const oneDayAgo = now - 24 * 60 * 60 * 1000
    const oneHourAgo = now - 60 * 60 * 1000
    
    for (const log of this.logs) {
      // 按类型统计
      stats.byType[log.type] = (stats.byType[log.type] || 0) + 1
      
      // 时间范围统计
      if (log.timestamp > oneHourAgo) {
        stats.lastHour++
      }
      if (log.timestamp > oneDayAgo) {
        stats.last24h++
      }
    }
    
    return stats
  }

  /**
   * 清空所有日志
   */
  clear() {
    this.logs = []
    try {
      wx.removeStorageSync(STORAGE_KEY)
      console.log('[ErrorLogger] 所有日志已清空')
    } catch (e) {
      console.error('[ErrorLogger] 清空本地存储失败:', e)
    }
  }

  /**
   * 导出为文本格式（用于复制/分享）
   * @param {number} count - 导出的条数
   * @returns {string} 格式化的文本
   */
  exportAsText(count = 20) {
    const logs = this.getRecentErrors(count)
    
    let text = `===== ESP32 环境监测站 - AI 错误日志 =====\n`
    text += `导出时间: ${new Date().toLocaleString('zh-CN')}\n`
    text += `总条数: ${logs.length}\n`
    text += `========================================\n\n`
    
    for (let i = 0; i < logs.length; i++) {
      const log = logs[i]
      text += `[${i + 1}] ${this._formatTimestamp(log.timestamp)}\n`
      text += `    类型: ${log.type}\n`
      text += `    描述: ${log.message}\n`
      text += `    HTTP状态码: ${log.httpCode || 'N/A'}\n`
      text += `    用户提示: ${log.userVisibleMsg}\n`
      
      if (log.url) {
        text += `    URL: ${log.url}\n`
      }
      
      if (log.requestId) {
        text += `    请求ID: ${log.requestId}\n`
      }
      
      text += '\n'
    }
    
    return text
  }

  /**
   * 复制最近日志到剪贴板
   * @param {number} count - 复制的条数
   * @returns {Promise<boolean>} 是否成功
   */
  async copyToClipboard(count = 10) {
    const text = this.exportAsText(count)
    
    try {
      await wx.setClipboardData({
        data: text,
        success: () => {
          console.log(`[ErrorLogger] 已复制 ${count} 条日志到剪贴板`)
          wx.showToast({
            title: '✅ 日志已复制',
            icon: 'success',
            duration: 1500
          })
        },
        fail: () => {
          throw new Error('剪贴板写入失败')
        }
      })
      return true
    } catch (error) {
      console.error('[ErrorLogger] 复制失败:', error)
      wx.showToast({
        title: '❌ 复制失败',
        icon: 'none',
        duration: 2000
      })
      return false
    }
  }

  // ==================== 私有方法 ====================

  _createLogEntry(error) {
    const now = Date.now()
    
    return {
      id: now.toString(36),                    // 唯一ID（基于时间戳）
      timestamp: now,                          // 时间戳（毫秒）
      timeFormatted: new Date().toLocaleString('zh-CN'), // 格式化时间
      
      // 错误基本信息
      type: error.type || ERROR_TYPES.UNKNOWN,
      message: error.message || String(error),
      
      // HTTP 相关信息
      httpCode: error.statusCode || error.code || null,
      url: error.url ? this._truncateUrl(error.url, 80) : null,
      
      // 追踪信息
      requestId: error.requestId || this._generateRequestId(),
      stack: error.stack ? this._truncateText(error.stack, 200) : null,
      
      // 用户可见的友好提示
      userVisibleMsg: this._getUserMessage(error),
      
      // 元数据
      retryable: this._isRetryable(error.type),
      providerUsed: error.providerUsed || null
    }
  }

  _getUserMessage(error) {
    const type = error.type || ERROR_TYPES.UNKNOWN
    
    let msg = USER_FRIENDLY_MESSAGES[type] || USER_FRIENDLY_MESSAGES[ERROR_TYPES.UNKNOWN]
    
    // 如果是认证失败，添加额外提示
    if (type === ERROR_TYPES.AUTH_FAILED) {
      msg += '\n💡 提示：请访问对应平台获取新的 API Key'
    }
    
    // 如果是限流，添加等待建议
    if (type === ERROR_TYPES.RATE_LIMIT) {
      msg += '\n⏰ 建议：等待30秒后再试，或切换到其他服务商'
    }
    
    return msg
  }

  _isRetryable(type) {
    const retryableTypes = [
      ERROR_TYPES.NETWORK_ERROR,
      ERROR_TYPES.TIMEOUT,
      ERROR_TYPES.SERVER_ERROR,
      ERROR_TYPES.PROVIDER_UNAVAILABLE
    ]
    return retryableTypes.includes(type)
  }

  _generateRequestId() {
    return 'REQ_' + Date.now().toString(36).toUpperCase() + '_' + Math.random().toString(36).substr(2, 6).toUpperCase()
  }

  _truncateUrl(url, maxLength) {
    if (!url) return ''
    if (url.length <= maxLength) return url
    return url.substring(0, maxLength) + '...'
  }

  _truncateText(text, maxLength) {
    if (!text) return ''
    if (text.length <= maxLength) return text
    return text.substring(0, maxLength) + '...'
  }

  _formatTimestamp(timestamp) {
    const date = new Date(timestamp)
    return `${date.getFullYear()}-${String(date.getMonth()+1).padStart(2,'0')}-${String(date.getDate()).padStart(2,'0')} ` +
           `${String(date.getHours()).padStart(2,'0')}:${String(date.getMinutes()).padStart(2,'0')}:${String(date.getSeconds()).padStart(2,'0')}`
  }

  _consoleOutput(entry) {
    const style = entry.retryable ? 'color: #f59e0b' : 'color: #ef4444'
    
    console.log(
      `%c[AI Error] ${entry.timeFormatted}`,
      style,
      `\n  类型: ${entry.type}`,
      `\n  信息: ${entry.message}`,
      `\n  HTTP: ${entry.httpCode || 'N/A'}`,
      `\n  ID: ${entry.requestId}`
    )
  }

  _saveToStorage() {
    try {
      const recentLogs = this.logs.slice(0, MAX_STORAGE_LOGS)
      wx.setStorageSync(STORAGE_KEY, recentLogs)
    } catch (e) {
      console.warn('[ErrorLogger] 本地存储失败:', e)
    }
  }

  _loadFromStorage() {
    try {
      const stored = wx.getStorageSync(STORAGE_KEY)
      if (Array.isArray(stored)) {
        this.logs = stored
      }
    } catch (e) {
      console.warn('[ErrorLogger] 加载本地存储失败:', e)
      this.logs = []
    }
  }
}

// ==================== 创建单例实例并导出 ====================

const instance = new ErrorLogger()

module.exports = instance

// 同时导出常量供其他模块使用
module.exports.ERROR_TYPES = ERROR_TYPES
module.exports.USER_FRIENDLY_MESSAGES = USER_FRIENDLY_MESSAGES
