/**
 * ============================================================
 *  AI服务配置 - 统一管理所有AI模型的API配置
 * ============================================================
 * 
 * 【使用说明】
 * 
 * 1. 选择AI服务商
 *    - 本配置支持多个AI服务商（通义千问、OpenAI、Kimi、DeepSeek等）
 *    - 根据您的需求和可用性选择合适的服务商
 * 
 * 2. 获取API Key
 *    - 通义千问: https://dashscope.console.aliyun.com/apiKey
 *    - OpenAI: https://platform.openai.com/api-keys
 *    - Kimi: https://platform.moonshot.cn/console/api-keys
 *    - DeepSeek: https://platform.deepseek.com/api_keys
 *    - HuggingFace: https://huggingface.co/settings/tokens
 *    - Ultralytics: https://hub.ultralytics.com/settings?tab=api-keys
 * 
 * 3. 配置API Key
 *    - 将对应服务商的 YOUR_XXX_API_KEY 替换为实际获取的API Key
 *    - 至少配置一个服务商才能使用AI功能
 *    - 推荐优先配置通义千问（国内访问稳定）
 * 
 * 4. 选择模型（可选）
 *    - 每个服务商提供多个模型可选
 *    - 根据需要选择适合的模型（速度/精度平衡）
 *    - 默认使用推荐模型（recommended: true）
 * 
 * 5. 测试验证
 *    - 保存配置后重新编译小程序
 *    - 在小程序中使用"网络诊断"功能测试AI连接
 *    - 或使用AI识别功能验证配置是否正确
 * 
 * 【注意事项】
 * - 请勿将包含真实API Key的代码提交到公共仓库
 * - 建议将API Key保存在安全的环境变量或配置中心
 * - 国内网络建议使用通义千问、Kimi、DeepSeek等国内服务商
 * - HuggingFace和OpenAI在国内可能访问不稳定
 * 
 * 【故障排查】
 * - 如果AI功能无法使用，先检查API Key是否正确
 * - 查看小程序控制台输出的错误信息
 * - 使用网络诊断工具测试API连通性
 * 
 * @module ai-config
 * @author AI Assistant
 * @version 1.0.0
 */

const AI_CONFIG = {

  // ==================== HuggingFace 配置 ====================
  huggingface: {
    // ⚠️ 请替换为您的HuggingFace API Key
    api_key: 'YOUR_HUGGINGFACE_API_KEY',

    model_name: 'facebook/detr-resnet-50',

    api_url: '',

    timeout: 30000,

    confidence_threshold: 0.5,

    max_detections: 10,

    supported_models: [
      { name: 'DETR-ResNet-50 (通用检测)', value: 'facebook/detr-resnet-50', recommended: true },
      { name: 'YOLOS-Small (轻量级)', value: 'hustvl/yolos-small' },
      { name: 'DETR-ResNet-101 (高精度)', value: 'facebook/detr-resnet-101' }
    ]
  },

  // ==================== Ultralytics 云服务配置 ====================
  ultralytics: {
    // ⚠️ 请替换为您的Ultralytics API Key
    api_key: 'YOUR_ULTRALYTICS_API_KEY',

    model_name: 'yolo11n',

    api_url: 'https://predict.ultralytics.com',

    timeout: 30000,

    confidence_threshold: 0.5,

    max_detections: 10,

    supported_models: [
      { name: 'YOLO11n (超轻量)', value: 'yolo11n', recommended: true },
      { name: 'YOLO11s (轻量)', value: 'yolo11s' },
      { name: 'YOLO11m (均衡)', value: 'yolo11m' },
      { name: 'YOLO11l (高精度)', value: 'yolo11l' },
      { name: 'YOLO11x (最强)', value: 'yolo11x' },
      { name: 'YOLOv8n (经典)', value: 'yolov8n' },
      { name: 'YOLOv8s (经典)', value: 'yolov8s' },
      { name: 'YOLOv8m (经典)', value: 'yolov8m' }
    ]
  },

  // ==================== 通义千问配置 ====================
  qwen: {
    // ⚠️ 请替换为您的通义千问 API Key
    api_key: 'YOUR_QWEN_API_KEY',

    model_name: 'qwen-plus',

    api_url: 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',

    vl_api_url: 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',

    timeout: 60000,

    temperature: 0.7,

    max_tokens: 2000,

    supported_models: [
      { name: '通义千问-3 (最新)', value: 'qwen3', type: 'text' },
      { name: '通义千问-Turbo (快速)', value: 'qwen-turbo', recommended: true, type: 'text' },
      { name: '通义千问-Plus (均衡)', value: 'qwen-plus', type: 'text' },
      { name: '通义千问-Max (最强)', value: 'qwen-max', type: 'text' },
      { name: '通义千问-VL (视觉理解)', value: 'qwen-vl-max', type: 'vision' }

    ],

    system_prompt: '你是一个智能助手，请根据用户提供的信息进行准确、简洁的回答。'
  },

  // ==================== 全局设置 ====================
  global: {
    debug_mode: true,
    
    enable_cache: false,
    
    cache_expire_time: 5 * 60 * 1000,
    
    max_retries: 2,
    
    retry_delay: 1000,
    
    default_mode: 'llm'
  }
}

// ==================== 多服务商支持 (v1.0 新增) ====================
// 解决国内网络无法访问 HuggingFace 等国外服务的问题

const AI_PROVIDERS = {
  
  qwen: {
    name: '通义千问',
    nameEn: 'Qwen',
    priority: 1,
    available: true,
    region: 'domestic',
    endpoints: {
      text: 'https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation',
      vision: 'https://dashscope.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation'
    },
    models: [
      { name: '通义千问-3 (最新)', value: 'qwen3', type: 'text' },
      { name: '通义千问-Turbo (快速)', value: 'qwen-turbo', recommended: true, type: 'text' },
      { name: '通义千问-Plus (均衡)', value: 'qwen-plus', type: 'text' },
      { name: '通义千问-Max (最强)', value: 'qwen-max', type: 'text' },
      { name: '通义千问-VL (视觉理解)', value: 'qwen-vl-max', type: 'vision' }
    ],
    capabilities: ['text', 'vision', 'code'],
    latency: '~1-3s',
    cost: '免费额度充足'
  },

  openai: {
    name: 'OpenAI GPT',
    nameEn: 'OpenAI',
    priority: 2,
    available: false,
    region: 'overseas',
    // ⚠️ 请替换为您的 OpenAI API Key
    api_key: 'YOUR_OPENAI_API_KEY',
    endpoint: 'https://api.openai.com/v1/chat/completions',
    models: [
      { name: 'GPT-4o (最新)', value: 'gpt-4o', recommended: true, type: 'vision' },
      { name: 'GPT-4 Turbo', value: 'gpt-4-turbo', type: 'vision' },
      { name: 'GPT-4', value: 'gpt-4', type: 'text' },
      { name: 'GPT-3.5 Turbo (快速)', value: 'gpt-3.5-turbo', type: 'text' }
    ],
    capabilities: ['text', 'vision', 'code'],
    latency: '~2-5s',
    cost: '按量计费'
  },

  kimi: {
    name: 'Kimi',
    nameEn: 'Kimi',
    priority: 3,
    available: true,
    region: 'domestic',
    // ⚠️ 请替换为您的 Kimi API Key
    api_key: 'YOUR_KIMI_API_KEY',
    endpoint: 'https://api.moonshot.cn/v1/chat/completions',
    models: [
      { name: 'Kimi-VL (视觉)', value: 'moonshot-v1-8k-vision', recommended: true, type: 'vision' },
      { name: 'Kimi-Text (文本)', value: 'moonshot-v1-8k', type: 'text' }
    ],
    capabilities: ['text', 'vision'],
    latency: '~2-5s',
    cost: '免费额度充足'
  },

  deepseek: {
    name: 'DeepSeek',
    nameEn: 'DeepSeek',
    priority: 4,
    available: true,
    region: 'domestic',
    // ⚠️ 请替换为您的 DeepSeek API Key
    api_key: 'YOUR_DEEPSEEK_API_KEY',
    endpoint: 'https://api.deepseek.com/v1/chat/completions',
    models: [
      { name: 'DeepSeek Chat', value: 'deepseek-chat', recommended: true, type: 'text' },
      { name: 'DeepSeek Reasoner', value: 'deepseek-reasoner', type: 'reasoning' }
    ],
    capabilities: ['text', 'reasoning'],
    latency: '~2-5s',
    cost: '性价比极高'
  },

  huggingface: {
    name: 'HuggingFace',
    nameEn: 'HuggingFace',
    priority: 2,
    available: false,
    region: 'overseas',
    endpoint: 'https://api-inference.huggingface.co/models/',
    models: [
      { name: 'DETR-ResNet-50 (通用检测)', value: 'facebook/detr-resnet-50', recommended: true },
      { name: 'YOLOS-Small (轻量级)', value: 'hustvl/yolos-small' },
      { name: 'DETR-ResNet-101 (高精度)', value: 'facebook/detr-resnet-101' }
    ],
    capabilities: ['vision', 'detection'],
    latency: '~5-15s (可能超时)',
    cost: '免费但有限额'
  },

  ultralytics: {
    name: 'Ultralytics',
    nameEn: 'Ultralytics',
    priority: 1,
    available: true,
    region: 'global',
    endpoint: 'https://predict.ultralytics.com',
    models: [
      { name: 'YOLO11n (超轻量)', value: 'yolo11n', recommended: true, type: 'detect' },
      { name: 'YOLO11s (轻量)', value: 'yolo11s', type: 'detect' },
      { name: 'YOLO11m (均衡)', value: 'yolo11m', type: 'detect' },
      { name: 'YOLO11l (高精度)', value: 'yolo11l', type: 'detect' },
      { name: 'YOLO11x (最强)', value: 'yolo11x', type: 'detect' },
      { name: 'YOLOv8n (经典)', value: 'yolov8n', type: 'detect' },
      { name: 'YOLOv8s (经典)', value: 'yolov8s', type: 'detect' },
      { name: 'YOLOv8m (经典)', value: 'yolov8m', type: 'detect' }
    ],
    capabilities: ['detection'],
    latency: '~1-3s',
    cost: '$0.0001/次 按量计费'
  }
}

/**
 * 获取所有可用服务商列表
 * @returns {Array} 可用服务商数组，按优先级排序
 */
function getAvailableProviders() {
  return Object.values(AI_PROVIDERS)
    .filter(p => p.available)
    .sort((a, b) => a.priority - b.priority)
}

/**
 * 根据使用模式推荐最佳服务商
 * @param {string} mode - 使用模式 ('text'|'vision'|'yolo'|'reasoning')
 * @returns {object|null} 推荐的服务商配置，无匹配返回null
 */
function getBestProviderForMode(mode) {
  const providers = getAvailableProviders()
  
  for (const provider of providers) {
    if (provider.capabilities && provider.capabilities.includes(mode)) {
      return provider
    }
  }
  
  console.warn(`[AI Config] 无可用服务商支持模式: ${mode}`)
  return null
}

/**
 * 动态修改服务商可用性状态
 * @param {string} providerName - 服务商名称 (qwen/huggingface/deepseek)
 * @param {boolean} status - 是否可用
 */
function setProviderAvailability(providerName, status) {
  if (AI_PROVIDERS[providerName]) {
    const oldStatus = AI_PROVIDERS[providerName].available
    AI_PROVIDERS[providerName].available = status
    
    if (oldStatus !== status) {
      console.log(`[AI Config] 服务商 ${providerName} 可用性变更: ${oldStatus} → ${status}`)
      
      if (!status) {
        console.warn(`[AI Config] ⚠️ ${AI_PROVIDERS[providerName].name} 已被标记为不可用，将自动降级到其他服务商`)
      } else {
        console.log(`[AI Config] ✅ ${AI_PROVIDERS[providerName].name} 已恢复可用`)
      }
    }
    
    return true
  }
  
  console.error(`[AI Config] 未知服务商: ${providerName}`)
  return false
}

/**
 * 获取服务商完整配置（含向后兼容）
 * @param {string} providerName - 服务商名称
 * @returns {object|null} 服务商配置对象
 */
function getProviderConfig(providerName) {
  if (providerName === 'huggingface') {
    return {
      ...AI_CONFIG.huggingface,
      name: 'HuggingFace',
      endpoint: AI_PROVIDERS.huggingface.endpoint
    }
  }
  
  if (AI_PROVIDERS[providerName]) {
    const provider = AI_PROVIDERS[providerName]
    return {
      api_key: provider.api_key || '',
      model_name: provider.models && provider.models[0] ? provider.models[0].value : '',
      api_url: provider.endpoint || '',
      ...provider
    }
  }
  
  console.error(`[AI Config] 未知服务商: ${providerName}`)
  return null
}

// 导出配置（兼容不同模块化方案）
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { AI_CONFIG, AI_PROVIDERS, getAvailableProviders, getBestProviderForMode, setProviderAvailability, getProviderConfig }
}
