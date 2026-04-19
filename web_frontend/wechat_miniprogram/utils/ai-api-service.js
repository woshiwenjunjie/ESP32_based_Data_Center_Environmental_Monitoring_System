/**
 * ============================================================
 *  AI API服务层 - 封装所有AI模型的HTTP请求逻辑
 * ============================================================
 * 
 * 功能：
 * - HuggingFace YOLO目标检测API调用
 * - 通义千问LLM/VLM API调用
 * - 统一的错误处理和重试机制
 * - 请求/响应日志记录（debug模式）
 * 
 * @module ai-api-service
 * @version 1.0.0
 */

const aiConfig = require('./ai-config')
const errorLogger = require('./error-logger')  // 错误日志系统 (v4.0 新增)

const ERROR_TYPES = {
  CONFIG_MISSING: 'config_missing',
  AUTH_FAILED: 'auth_failed',
  NETWORK_ERROR: 'network_error',
  TIMEOUT: 'timeout',
  RATE_LIMIT: 'rate_limit',
  SERVER_ERROR: 'server_error',
  MODEL_LOADING: 'model_loading',
  IMAGE_TOO_LARGE: 'image_too_large',
  PARSE_ERROR: 'parse_error',
  UNKNOWN: 'unknown'
}

function requestWithRetry(options, maxRetries = 2) {
  return new Promise((resolve, reject) => {
    let retryCount = 0
    
    const attemptRequest = () => {
      const startTime = Date.now()
      
      wx.request({
        url: options.url,
        method: options.method || 'GET',
        header: options.header || {},
        data: options.data || null,
        timeout: options.timeout || 30000,
        success: (res) => {
          const duration = Date.now() - startTime
          
          if (aiConfig.AI_CONFIG.global.debug_mode) {
            console.log(`[AI API] ${options.url.split('?')[0]}: ${duration}ms, Status: ${res.statusCode}`)
          }
          
          if (res.statusCode === 200 || res.statusCode === 201) {
            resolve({
              data: res.data,
              statusCode: res.statusCode,
              headers: res.header,
              duration
            })
          } else if (res.statusCode === 401 || res.statusCode === 403) {
            reject(createError(ERROR_TYPES.AUTH_FAILED, `认证失败 (${res.statusCode})`, res.data))
          } else if (res.statusCode === 429) {
            reject(createError(ERROR_TYPES.RATE_LIMIT, '请求频率超限', res.data))
          } else if (res.statusCode >= 500 && res.statusCode < 600) {
            reject(createError(ERROR_TYPES.SERVER_ERROR, `服务器错误 (${res.statusCode})`, res.data))
          } else {
            reject(createError(ERROR_TYPES.UNKNOWN, `请求失败 (${res.statusCode})`, res.data))
          }
        },
        fail: (err) => {
          const duration = Date.now() - startTime
          
          if (err.errMsg.includes('timeout')) {
            if (retryCount < maxRetries) {
              retryCount++
              const delay = aiConfig.AI_CONFIG.global.retry_delay * retryCount
              setTimeout(attemptRequest, delay)
              return
            }
            reject(createError(ERROR_TYPES.TIMEOUT, `请求超时 (${(duration/1000).toFixed(1)}s)`))
          } else if (err.errMsg.includes('fail') || err.errMsg.includes('net::')) {
            reject(createError(ERROR_TYPES.NETWORK_ERROR, '网络连接失败'))
          } else {
            reject(createError(ERROR_TYPES.NETWORK_ERROR, err.errMsg || '未知网络错误'))
          }
        }
      })
    }
    
    attemptRequest()
  })
}

function createError(type, message, data = null) {
  return {
    type,
    message,
    data,
    timestamp: new Date().toISOString(),
    isAiError: true
  }
}

function preprocessImage(base64Image, maxSizeKB = 500) {
  if (!base64Image) {
    throw createError(ERROR_TYPES.PARSE_ERROR, '图片数据为空')
  }
  
  let cleanBase64 = base64Image
  
  if (cleanBase64.includes(',')) {
    cleanBase64 = cleanBase64.split(',')[1]
  }
  
  const sizeInBytes = Math.ceil((cleanBase64.length * 3) / 4)
  const sizeInKB = sizeInBytes / 1024
  
  if (sizeInKB > maxSizeKB * 2) {
    throw createError(ERROR_TYPES.IMAGE_TOO_LARGE, 
      `图片过大 (${sizeInKB.toFixed(0)}KB)，建议压缩至${maxSizeKB}KB以内`)
  }
  
  if (sizeInKB > maxSizeKB) {
    console.warn(`[AI API] 图片较大 (${sizeInKB.toFixed(0)}KB)，可能影响响应速度`)
  }
  
  return cleanBase64
}

async function callHuggingfaceYOLO(imageBase64) {
  const config = aiConfig.getHuggingfaceConfig()
  
  if (!aiConfig.isConfigComplete('huggingface')) {
    throw createError(ERROR_TYPES.CONFIG_MISSING, '请先配置HuggingFace API Key')
  }
  
  try {
    const processedImage = preprocessImage(imageBase64)
    
    const apiUrl = config.getEffectiveApiUrl()
    
    const response = await requestWithRetry({
      url: apiUrl,
      method: 'POST',
      header: {
        'Authorization': `Bearer ${config.api_key}`,
        'Content-Type': 'application/json'
      },
      data: {
        image: processedImage
      },
      timeout: config.timeout
    }, aiConfig.AI_CONFIG.global.max_retries)
    
    let detections = []
    
    if (Array.isArray(response.data)) {
      detections = response.data.map(det => ({
        label: det.label || det.class || 'unknown',
        score: det.score || det.confidence || 0,
        box: det.box || det.bbox || {
          xmin: det.xmin || 0,
          ymin: det.ymin || 0,
          xmax: det.xmax || 0,
          ymax: det.ymax || 0
        }
      }))
    } else if (response.data && typeof response.data === 'object') {
      if (Array.isArray(response.data.detections)) {
        detections = response.data.detections
      } else if (Array.isArray(response.data.objects)) {
        detections = response.data.objects
      } else if (response.data.error) {
        if (typeof response.data.error === 'string' && response.data.error.includes('loading')) {
          throw createError(ERROR_TYPES.MODEL_LOADING, 
            `模型正在加载中，预计等待${response.data.estimated_time || 20}秒`,
            response.data)
        }
        throw createError(ERROR_TYPES.SERVER_ERROR, response.data.error, response.data)
      }
    }
    
    const filteredDetections = detections
      .filter(det => det.score >= config.confidence_threshold)
      .sort((a, b) => b.score - a.score)
      .slice(0, config.max_detections)
    
    return {
      success: true,
      detections: filteredDetections,
      rawResponse: response.data,
      modelUsed: config.model_name,
      totalDetections: detections.length,
      filteredDetections: filteredDetections.length,
      confidenceThreshold: config.confidence_threshold,
      duration: response.duration
    }
    
  } catch (error) {
    // v4.0: 记录错误日志
    errorLogger.log({
      ...error,
      type: error.type || ERROR_TYPES.UNKNOWN
    })
    
    if (error.isAiError) {
      // 检查是否需要降级处理（网络错误/超时/服务器错误）
      if (this._shouldFallback(error.type)) {
        console.warn('[AI Service] HuggingFace 请求失败，尝试降级到通义千问VL...')
        
        try {
          const fallbackResult = await this._fallbackToQwenVL(imageBase64)
          
          return {
            ...fallbackResult,
            fallbackUsed: true,
            originalProvider: 'huggingface',
            fallbackProvider: 'qwen-vl'
          }
        } catch (fallbackError) {
          console.error('[AI Service] 降级也失败:', fallbackError.message)
          errorLogger.log(fallbackError)
        }
      }
      
      throw error
    }
    
    throw createError(ERROR_TYPES.UNKNOWN, error.message || 'YOLO检测失败', error)
  }
}

async function callQwenLLM(question, imageBase64 = null) {
  const config = aiConfig.getQwenConfig()
  
  if (!aiConfig.isConfigComplete('qwen')) {
    throw createError(ERROR_TYPES.CONFIG_MISSING, '请先配置通义千问API Key')
  }
  
  if (!question || !question.trim()) {
    throw createError(ERROR_TYPES.PARSE_ERROR, '问题内容不能为空')
  }
  
  try {
    let modelName = config.model_name
    let apiUrl = config.api_url
    let requestBody = {}
    
    const hasImage = !!imageBase64
    
    if (hasImage) {
      modelName = 'qwen-vl-max'
      apiUrl = config.vl_api_url

      const processedImage = preprocessImage(imageBase64, 1000)

      requestBody = {
        model: modelName,
        messages: [
          {
            role: 'user',
            content: [
              { type: 'image_url', image_url: { url: `data:image/jpeg;base64,${processedImage}` } },
              { type: 'text', text: question }
            ]
          }
        ],
        temperature: config.temperature
      }
    } else {
      requestBody = {
        model: modelName,
        messages: [
          {
            role: 'system',
            content: config.system_prompt
          },
          {
            role: 'user',
            content: question
          }
        ],
        temperature: config.temperature,
        max_tokens: config.max_tokens
      }
    }
    
    const response = await requestWithRetry({
      url: apiUrl,
      method: 'POST',
      header: {
        'Authorization': `Bearer ${config.api_key}`,
        'Content-Type': 'application/json'
      },
      data: requestBody,
      timeout: Math.max(config.timeout, 60000)  // v4.0: 增加至60s以适应国内网络延迟
    }, aiConfig.AI_CONFIG.global.max_retries)
    
    let textResult = ''

    if (response.data && response.data.choices && response.data.choices[0] && response.data.choices[0].message) {
      textResult = response.data.choices[0].message.content || ''
    } else if (response.data && typeof response.data === 'object') {
      if (response.data.error) {
        const errorMsg = response.data.error.message || response.data.error || '未知错误'
        if (response.data.error.code === 'InvalidApiKey' || response.status === 401) {
          throw createError(ERROR_TYPES.AUTH_FAILED, '通义千问API Key无效', response.data)
        } else if (response.data.error.code === 'QuotaExceeded' || response.status === 429) {
          throw createError(ERROR_TYPES.RATE_LIMIT, 'API配额已用尽，请充值或下月再试', response.data)
        } else {
          throw createError(ERROR_TYPES.SERVER_ERROR, errorMsg, response.data)
        }
      } else if (response.data.code && response.data.message) {
        if (response.data.code === 'InvalidApiKey') {
          throw createError(ERROR_TYPES.AUTH_FAILED, '通义千问API Key无效', response.data)
        } else if (response.data.code === 'QuotaExceeded') {
          throw createError(ERROR_TYPES.RATE_LIMIT, 'API配额已用尽，请充值或下月再试', response.data)
        } else {
          throw createError(ERROR_TYPES.SERVER_ERROR, response.data.message, response.data)
        }
      } else {
        throw createError(ERROR_TYPES.PARSE_ERROR, '无法解析API响应格式', response.data)
      }
    }
    
    if (!textResult) {
      throw createError(ERROR_TYPES.PARSE_ERROR, 'AI返回空内容')
    }
    
    textResult = textResult
      .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')
      .replace(/\n{3,}/g, '\n\n')
      .trim()
    
    const usage = response.data.usage || {}
    
    return {
      success: true,
      text: textResult,
      modelUsed: modelName,
      mode: hasImage ? 'vision' : 'text',
      usage: {
        promptTokens: usage.prompt_tokens || 0,
        completionTokens: usage.completion_tokens || 0,
        totalTokens: usage.total_tokens || 0
      },
      duration: response.duration
    }
    
  } catch (error) {
    // v4.0: 记录错误日志
    errorLogger.log({
      ...error,
      url: apiUrl,
      type: error.type || ERROR_TYPES.UNKNOWN
    })
    
    if (error.isAiError) {
      throw error
    }
    throw createError(ERROR_TYPES.UNKNOWN, error.message || '千问API调用失败', error)
  }
}

async function callGPT(question, imageBase64 = null) {
  const config = aiConfig.AI_CONFIG.openai || {}
  
  if (!config.api_key) {
    throw createError(ERROR_TYPES.CONFIG_MISSING, '请先配置 OpenAI API Key')
  }
  
  if (!question || !question.trim()) {
    throw createError(ERROR_TYPES.PARSE_ERROR, '问题内容不能为空')
  }
  
  try {
    const modelName = config.model_name || 'gpt-4o'
    const hasImage = !!imageBase64
    
    const messages = [
      {
        role: 'system',
        content: '你是一个智能助手，请根据用户提供的信息进行准确、简洁的回答。'
      },
      {
        role: 'user',
        content: hasImage
          ? [
              { type: 'image_url', image_url: { url: `data:image/jpeg;base64,${imageBase64}` } },
              { type: 'text', text: question }
            ]
          : question
      }
    ]
    
    const requestBody = {
      model: modelName,
      messages: messages
    }
    
    const response = await requestWithRetry({
      url: 'https://api.openai.com/v1/chat/completions',
      method: 'POST',
      header: {
        'Authorization': `Bearer ${config.api_key}`,
        'Content-Type': 'application/json'
      },
      data: requestBody,
      timeout: config.timeout || 30000
    }, aiConfig.AI_CONFIG.global.max_retries)
    
    let textResult = ''
    if (response.data.choices && response.data.choices[0] && response.data.choices[0].message) {
      textResult = response.data.choices[0].message.content || ''
    }
    
    if (!textResult) {
      throw createError(ERROR_TYPES.PARSE_ERROR, 'AI返回空内容')
    }
    
    textResult = textResult
      .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')
      .replace(/\n{3,}/g, '\n\n')
      .trim()
    
    return {
      success: true,
      text: textResult,
      modelUsed: modelName,
      mode: hasImage ? 'vision' : 'text',
      duration: response.duration
    }
  } catch (error) {
    errorLogger.log({ ...error, url: 'https://api.openai.com/v1/chat/completions', type: error.type || ERROR_TYPES.UNKNOWN })
    if (error.isAiError) throw error
    throw createError(ERROR_TYPES.UNKNOWN, error.message || 'GPT API调用失败', error)
  }
}

async function callKimi(question, imageBase64 = null) {
  const config = aiConfig.AI_CONFIG.kimi || {}
  
  if (!config.api_key) {
    throw createError(ERROR_TYPES.CONFIG_MISSING, '请先配置 Kimi API Key')
  }
  
  if (!question || !question.trim()) {
    throw createError(ERROR_TYPES.PARSE_ERROR, '问题内容不能为空')
  }
  
  try {
    const modelName = config.model_name || 'moonshot-v1-8k-vision'
    const hasImage = !!imageBase64
    
    const messages = [
      {
        role: 'system',
        content: '你是一个智能助手，请根据用户提供的信息进行准确、简洁的回答。'
      },
      {
        role: 'user',
        content: hasImage
          ? [
              { type: 'image_url', image_url: { url: `data:image/jpeg;base64,${imageBase64}` } },
              { type: 'text', text: question }
            ]
          : question
      }
    ]
    
    const response = await requestWithRetry({
      url: 'https://api.moonshot.cn/v1/chat/completions',
      method: 'POST',
      header: {
        'Authorization': `Bearer ${config.api_key}`,
        'Content-Type': 'application/json'
      },
      data: {
        model: modelName,
        messages: messages
      },
      timeout: config.timeout || 30000
    }, aiConfig.AI_CONFIG.global.max_retries)
    
    let textResult = ''
    if (response.data.choices && response.data.choices[0] && response.data.choices[0].message) {
      textResult = response.data.choices[0].message.content || ''
    }
    
    if (!textResult) {
      throw createError(ERROR_TYPES.PARSE_ERROR, 'AI返回空内容')
    }
    
    textResult = textResult
      .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')
      .replace(/\n{3,}/g, '\n\n')
      .trim()
    
    return {
      success: true,
      text: textResult,
      modelUsed: modelName,
      mode: hasImage ? 'vision' : 'text',
      duration: response.duration
    }
  } catch (error) {
    errorLogger.log({ ...error, url: 'https://api.moonshot.cn/v1/chat/completions', type: error.type || ERROR_TYPES.UNKNOWN })
    if (error.isAiError) throw error
    throw createError(ERROR_TYPES.UNKNOWN, error.message || 'Kimi API调用失败', error)
  }
}

async function callDeepSeek(question, imageBase64 = null) {
  const config = aiConfig.AI_CONFIG.deepseek || {}
  
  if (!config.api_key) {
    throw createError(ERROR_TYPES.CONFIG_MISSING, '请先配置 DeepSeek API Key')
  }
  
  if (!question || !question.trim()) {
    throw createError(ERROR_TYPES.PARSE_ERROR, '问题内容不能为空')
  }
  
  try {
    const modelName = config.model_name || 'deepseek-chat'
    
    if (imageBase64) {
      throw createError(ERROR_TYPES.PARSE_ERROR, 'DeepSeek暂不支持图片输入，请使用纯文本问题')
    }
    
    const response = await requestWithRetry({
      url: 'https://api.deepseek.com/v1/chat/completions',
      method: 'POST',
      header: {
        'Authorization': `Bearer ${config.api_key}`,
        'Content-Type': 'application/json'
      },
      data: {
        model: modelName,
        messages: [
          {
            role: 'system',
            content: '你是一个智能助手，请根据用户提供的信息进行准确、简洁的回答。'
          },
          {
            role: 'user',
            content: question
          }
        ]
      },
      timeout: config.timeout || 30000
    }, aiConfig.AI_CONFIG.global.max_retries)
    
    let textResult = ''
    if (response.data.choices && response.data.choices[0] && response.data.choices[0].message) {
      textResult = response.data.choices[0].message.content || ''
    }
    
    if (!textResult) {
      throw createError(ERROR_TYPES.PARSE_ERROR, 'AI返回空内容')
    }
    
    textResult = textResult
      .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '')
      .replace(/\n{3,}/g, '\n\n')
      .trim()
    
    return {
      success: true,
      text: textResult,
      modelUsed: modelName,
      mode: 'text',
      duration: response.duration
    }
  } catch (error) {
    errorLogger.log({ ...error, url: 'https://api.deepseek.com/v1/chat/completions', type: error.type || ERROR_TYPES.UNKNOWN })
    if (error.isAiError) throw error
    throw createError(ERROR_TYPES.UNKNOWN, error.message || 'DeepSeek API调用失败', error)
  }
}

function parseHuggingfaceError(responseData) {
  if (!responseData) return { type: ERROR_TYPES.UNKNOWN, message: '未知错误' }
  
  if (responseData.error) {
    const errorMsg = typeof responseData.error === 'string' 
      ? responseData.error 
      : JSON.stringify(responseData.error)
    
    if (errorMsg.includes('loading') || errorMsg.includes('Loading')) {
      return {
        type: ERROR_TYPES.MODEL_LOADING,
        message: `模型正在加载中，预计等待${responseData.estimated_time || 20}秒`,
        estimatedTime: responseData.estimated_time || 20
      }
    }
    
    if (errorMsg.includes('Authorization') || errorMsg.includes('Unauthorized') || errorMsg.includes('401')) {
      return { type: ERROR_TYPES.AUTH_FAILED, message: 'HuggingFace API Key无效或已过期' }
    }
    
    if (errorMsg.includes('rate limit') || errorMsg.includes('429')) {
      return { type: ERROR_TYPES.RATE_LIMIT, message: '请求过于频繁，请稍后重试' }
    }
    
    return { type: ERROR_TYPES.SERVER_ERROR, message: errorMsg }
  }
  
  return { type: ERROR_TYPES.UNKNOWN, message: JSON.stringify(responseData).substring(0, 200) }
}

function getErrorMessage(errorType) {
  const messages = {
    [ERROR_TYPES.CONFIG_MISSING]: {
      title: 'API未配置',
      message: '请在ai-config.js中填写对应的API密钥',
      icon: '⚠️'
    },
    [ERROR_TYPES.AUTH_FAILED]: {
      title: '认证失败',
      message: 'API密钥无效或已过期，请检查配置',
      icon: '🔴'
    },
    [ERROR_TYPES.NETWORK_ERROR]: {
      title: '网络连接失败',
      message: '无法连接到服务器，请检查网络设置',
      icon: '🌐'
    },
    [ERROR_TYPES.TIMEOUT]: {
      title: '请求超时',
      message: '服务器响应时间过长，请稍后重试或简化问题',
      icon: '⏰'
    },
    [ERROR_TYPES.RATE_LIMIT]: {
      title: '请求过于频繁',
      message: '已达到API调用限制，请稍后再试',
      icon: '🔵'
    },
    [ERROR_TYPES.SERVER_ERROR]: {
      title: '服务器错误',
      message: 'AI服务暂时不可用，请稍后重试',
      icon: '❌'
    },
    [ERROR_TYPES.MODEL_LOADING]: {
      title: '模型加载中',
      message: '首次使用该模型需要初始化，请耐心等待',
      icon: '🔄'
    },
    [ERROR_TYPES.IMAGE_TOO_LARGE]: {
      title: '图片过大',
      message: '图片文件超出限制，请使用更小的图片',
      icon: '📦'
    },
    [ERROR_TYPES.PARSE_ERROR]: {
      title: '数据异常',
      message: '返回数据格式不正确，可能服务端有更新',
      icon: '⚡'
    },
    [ERROR_TYPES.UNKNOWN]: {
      title: '发生错误',
      message: '未知错误，请联系技术支持',
      icon: '❓'
    }
  }
  
  return messages[errorType] || messages[ERROR_TYPES.UNKNOWN]
}

function getSolutionsForError(errorType) {
  const solutions = {
    [ERROR_TYPES.CONFIG_MISSING]: [
      { text: '查看配置教程', action: 'showTutorial' },
      { text: '跳过此功能', action: 'skip' }
    ],
    [ERROR_TYPES.AUTH_FAILED]: [
      { text: '检查API Key', action: 'checkKey' },
      { text: '获取新Key', action: 'getKey' }
    ],
    [ERROR_TYPES.NETWORK_ERROR]: [
      { text: '检查WiFi连接', action: 'checkWifi' },
      { text: '重试请求', action: 'retry' }
    ],
    [ERROR_TYPES.TIMEOUT]: [
      { text: '重新尝试', action: 'retry' },
      { text: '换用更简单的提问', action: 'simplifyQuestion' }
    ],
    [ERROR_TYPES.RATE_LIMIT]: [
      { text: '等待30秒后重试', action: 'waitRetry' },
      { text: '切换到其他模型', action: 'switchModel' }
    ],
    [ERROR_TYPES.MODEL_LOADING]: [
      { text: '继续等待', action: 'wait' },
      { text: '换用其他模型', action: 'switchModel' }
    ],
    [ERROR_TYPES.IMAGE_TOO_LARGE]: [
      { text: '重新拍摄（更远距离）', action: 'retakePhoto' },
      { text: '使用本地分析模式', action: 'switchLocal' }
    ]
  }
  
  return solutions[errorType] || [
    { text: '点击重试', action: 'retry' },
    { text: '查看帮助文档', action: 'showHelp' }
  ]
}

// ==================== 降级策略辅助函数 (v4.0 新增) ====================

/**
 * 判断错误类型是否应该触发降级处理
 * @param {string} errorType - 错误类型
 * @returns {boolean} 是否需要降级
 */
function _shouldFallback(errorType) {
  const fallbackTypes = [
    ERROR_TYPES.NETWORK_ERROR,
    ERROR_TYPES.TIMEOUT,
    ERROR_TYPES.SERVER_ERROR
  ]
  return fallbackTypes.includes(errorType)
}

/**
 * 降级到通义千问VL进行视觉识别
 * 当 HuggingFace YOLO 不可用时使用此方案
 * @param {string} imageBase64 - Base64编码的图像数据
 * @returns {Promise<object>} 降级后的结果对象
 */
async function _fallbackToQwenVL(imageBase64) {
  console.log('[AI Service] 开始执行降级策略: Qwen-VL 视觉识别')
  
  const qwenPrompt = `请分析这张图片，列出你看到的所有物体及其位置（使用相对坐标描述，如"左上角"、"中央"、"右下角"等）。格式要求：
1. 每个物体一行
2. 格式：[物体名称] - [置信度描述] - [位置描述]
3. 只列出明确的物体，不要猜测

图片内容分析：`
  
  try {
    const result = await callQwenLLM(qwenPrompt, imageBase64)
    
    if (result.success && result.text) {
      // 解析 VL 返回的文本为类检测格式
      const detections = parseVLResponseToDetections(result.text)
      
      return {
        success: true,
        detections: detections,
        modelUsed: 'qwen-vl-max (fallback)',
        totalDetections: detections.length,
        filteredDetections: detections.length,
        fallbackMessage: '⚠️ HuggingFace不可用，已自动切换到通义千问VL进行视觉识别',
        rawText: result.text,
        duration: result.duration
      }
    } else {
      throw createError(ERROR_TYPES.PARSE_ERROR, '降级服务返回空内容')
    }
    
  } catch (error) {
    console.error('[AI Service] 降级到Qwen-VL失败:', error)
    throw createError(
      ERROR_TYPES.PROVIDER_UNAVAILABLE, 
      '所有AI服务商均不可用（HuggingFace + 通义千问），请检查网络连接',
      error
    )
  }
}

/**
 * 解析通义千问VL的文本响应为检测格式
 * @param {string} text - VL返回的文本
 * @returns {Array} 检测结果数组
 */
function parseVLResponseToDetections(text) {
  const detections = []
  
  try {
    // 尝试按行解析
    const lines = text.split('\n').filter(line => line.trim())
    
    for (const line of lines) {
      // 匹配格式: [物体名称] - [置信度/位置]
      const match = line.match(/\[(.+?)\]\s*[-–—]\s*(.+)/)
      
      if (match) {
        const label = match[1].trim()
        const details = match[2].trim()
        
        detections.push({
          label: label,
          score: 0.85,  // VL模型无法给出精确分数，给一个默认高值
          position: details,
          source: 'qwen-vl-fallback'
        })
      }
    }
    
  } catch (e) {
    console.warn('[AI Service] VL响应解析失败:', e)
  }
  
  // 如果解析失败或无结果，返回原始文本作为单个检测结果
  if (detections.length === 0) {
    detections.push({
      label: '视觉识别结果',
      score: 0.8,
      position: text.substring(0, 200),
      source: 'qwen-vl-raw'
    })
  }
  
  return detections
}

async function callUltralyticsYOLO(imageBase64) {
  const config = aiConfig.getUltralyticsConfig()

  if (!aiConfig.isConfigComplete('ultralytics')) {
    throw createError(ERROR_TYPES.CONFIG_MISSING, '请先配置Ultralytics API Key')
  }

  try {
    const processedImage = preprocessImage(imageBase64, 500)

    const response = await requestWithRetry({
      url: config.api_url,
      method: 'POST',
      header: {
        'x-api-key': config.api_key,
        'Content-Type': 'application/json'
      },
      data: {
        model: config.model_name,
        imgsz: 640,
        conf: config.confidence_threshold,
        iou: 0.45,
        image: processedImage
      },
      timeout: config.timeout
    }, aiConfig.AI_CONFIG.global.max_retries)

    let detections = []

    if (response.data && response.data.boxes) {
      const boxes = response.data.boxes
      const names = response.data.names || {}

      const xyxy = boxes.xyxy || []
      const conf = boxes.conf || []
      const cls = boxes.cls || []

      for (let i = 0; i < xyxy.length; i++) {
        const box = xyxy[i]
        const classId = Math.round(cls[i])
        const label = names[classId] || names[String(classId)] || `class_${classId}`

        detections.push({
          label: label,
          score: conf[i],
          box: {
            xmin: Math.round(box[0]),
            ymin: Math.round(box[1]),
            xmax: Math.round(box[2]),
            ymax: Math.round(box[3])
          }
        })
      }
    } else if (Array.isArray(response.data)) {
      detections = response.data.map(det => ({
        label: det.label || det.class || 'unknown',
        score: det.score || det.confidence || 0,
        box: det.box || det.bbox || {
          xmin: det.xmin || 0,
          ymin: det.ymin || 0,
          xmax: det.xmax || 0,
          ymax: det.ymax || 0
        }
      }))
    } else if (response.data && response.data.error) {
      throw createError(ERROR_TYPES.SERVER_ERROR, response.data.error, response.data)
    }

    const filteredDetections = detections
      .filter(det => det.score >= config.confidence_threshold)
      .sort((a, b) => b.score - a.score)
      .slice(0, config.max_detections)

    return {
      success: true,
      detections: filteredDetections,
      rawResponse: response.data,
      modelUsed: config.model_name,
      provider: 'ultralytics',
      totalDetections: detections.length,
      filteredDetections: filteredDetections.length,
      confidenceThreshold: config.confidence_threshold,
      duration: response.duration
    }

  } catch (error) {
    errorLogger.log({
      ...error,
      type: error.type || ERROR_TYPES.UNKNOWN
    })

    if (error.isAiError) {
      throw error
    }
    throw createError(ERROR_TYPES.UNKNOWN, error.message || 'Ultralytics YOLO检测失败', error)
  }
}

module.exports = {
  ERROR_TYPES,
  requestWithRetry,
  callHuggingfaceYOLO,
  callUltralyticsYOLO,
  callQwenLLM,
  callGPT,
  callKimi,
  callDeepSeek,
  preprocessImage,
  parseHuggingfaceError,
  getErrorMessage,
  getSolutionsForError,
  createError
}
