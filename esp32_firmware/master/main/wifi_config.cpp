/**
 * ============================================================
 *  WiFi配置管理模块实现 (STA模式 - 安全版)
 * ============================================================
 * 
 * 【使用说明】
 * 
 * 本模块提供三种WiFi配置方式：
 * 
 * 方法1: 直接修改代码（最简单，适合开发调试）
 *   - 编辑本文件第35-36行和第52-53行
 *   - 将 "YOUR_WIFI_SSID" 替换为您的WiFi名称
 *   - 将 "YOUR_WIFI_PASSWORD" 替换为您的WiFi密码
 *   - 重新编译上传即可
 * 
 * 方法2: 通过串口命令配置（无需重新编译）
 *   - 连接ESP32串口监视器（波特率115200）
 *   - 发送命令: wifi_ssid=您的WiFi名称
 *   - 发送命令: wifi_password=您的WiFi密码
 *   - 发送命令: wifi_save 保存配置
 *   - 配置会自动保存到SPIFFS文件系统，断电不丢失
 * 
 * 方法3: 通过SPIFFS上传配置文件（批量配置）
 *   - 创建 wifi_config.json 文件，内容格式：
 *     {
 *       "ssid": "您的WiFi名称",
 *       "password": "您的WiFi密码",
 *       "is_configured": true
 *     }
 *   - 使用ESP32 Sketch Data Upload工具上传到 /config/wifi_config.json
 *   - 重启ESP32即可自动加载
 * 
 * 【配置优先级】
 * 1. 首先尝试从SPIFFS读取已保存的配置
 * 2. 如果读取失败，使用代码中的默认配置
 * 3. 首次启动时会显示配置提示信息
 * 
 * 【故障排查】
 * - 如果WiFi连接失败，检查SSID和密码是否正确
 * - 确保WiFi是2.4GHz频段（ESP32不支持5GHz）
 * - 检查WiFi信号强度，建议靠近路由器测试
 * - 查看串口输出获取详细的连接日志
 * 
 * 【安全提示】
 * - 请勿将包含真实WiFi密码的代码提交到公共仓库
 * - 生产环境建议使用串口或SPIFFS方式配置，避免硬编码
 */

#include "wifi_config.h"
#include "public.h"
#include <ArduinoJson.h>

// 全局实例
WiFiConfigManager wifiManager;

void WiFiConfigManager::begin() {
    DEBUG_PRINTLN("[WiFi配置] 初始化配置管理器...");
    
    if (!_initSPIFFS()) {
        DEBUG_PRINTLN("[WiFi配置] SPIFFS初始化失败，使用默认配置");
        
        // ⚠️ 请替换为您的WiFi名称和密码
        strlcpy(_config.ssid, "YOUR_WIFI_SSID", sizeof(_config.ssid));
        strlcpy(_config.password, "YOUR_WIFI_PASSWORD", sizeof(_config.password));
        _config.is_configured = true;
        
        DEBUG_PRINTLN("[WiFi配置] 使用默认配置（请修改为您的WiFi信息）");
        return;
    }
    
    if (loadConfig()) {
        DEBUG_PRINTF("[WiFi配置] 已加载保存的配置: %s\n", _config.ssid);
    } else {
        DEBUG_PRINTLN("[WiFi配置] 未找到保存的配置，使用默认配置");
        
        // ⚠️ 请替换为您的WiFi名称和密码
        strlcpy(_config.ssid, "YOUR_WIFI_SSID", sizeof(_config.ssid));
        strlcpy(_config.password, "YOUR_WIFI_PASSWORD", sizeof(_config.password));
        _config.is_configured = true;
        
        DEBUG_PRINTLN("[WiFi配置] 使用默认配置（请修改为您的WiFi信息）");
        
        DEBUG_PRINTLN("\n配置方法:");
        DEBUG_PRINTLN("  方法1: 通过串口监视器输入命令");
        DEBUG_PRINTLN("  方法2: 编辑此文件添加默认配置");
        DEBUG_PRINTLN("  方法3: 使用SPIFFS上传工具创建 /config/wifi_config.json\n");
    }
}

bool WiFiConfigManager::_initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("[WiFi配置] SPIFFS挂载失败");
        return false;
    }
    DEBUG_PRINTLN("[WiFi配置] SPIFFS挂载成功");
    return true;
}

bool WiFiConfigManager::loadConfig() {
    return _readConfigFromFile();
}

bool WiFiConfigManager::_readConfigFromFile() {
    if (!SPIFFS.exists("/config/wifi_config.json")) {
        DEBUG_PRINTLN("[WiFi配置] 配置文件不存在");
        _config.is_configured = false;
        return false;
    }
    
    File configFile = SPIFFS.open("/config/wifi_config.json", "r");
    if (!configFile) {
        DEBUG_PRINTLN("[WiFi配置] 无法打开配置文件");
        _config.is_configured = false;
        return false;
    }
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    
    if (error) {
        DEBUG_PRINTF("[WiFi配置] JSON解析失败: %s\n", error.c_str());
        _config.is_configured = false;
        return false;
    }
    
    strlcpy(_config.ssid, doc["ssid"] | "", sizeof(_config.ssid));
    strlcpy(_config.password, doc["password"] | "", sizeof(_config.password));
    _config.is_configured = doc["configured"] | false;
    _config.use_static_ip = doc["use_static_ip"] | false;
    
    if (_config.use_static_ip && doc.containsKey("static_ip")) {
        _config.static_ip.fromString(doc["static_ip"].as<String>());
        _config.gateway.fromString(doc["gateway"].as<String>());
        _config.subnet.fromString(doc["subnet"].as<String>());
    }
    
    if (strlen(_config.ssid) > 0) {
        DEBUG_PRINTF("[WiFi配置] 配置加载成功: SSID=%s\n", _config.ssid);
        return true;
    } else {
        DEBUG_PRINTLN("[WiFi配置] 配置为空");
        _config.is_configured = false;
        return false;
    }
}

bool WiFiConfigManager::saveConfig(const String& ssid, const String& password) {
    strlcpy(_config.ssid, ssid.c_str(), sizeof(_config.ssid));
    strlcpy(_config.password, password.c_str(), sizeof(_config.password));
    _config.is_configured = true;
    
    return _writeConfigToFile();
}

bool WiFiConfigManager::_writeConfigToFile() {
    DynamicJsonDocument doc(256);
    doc["ssid"] = _config.ssid;
    doc["password"] = _config.password;
    doc["configured"] = _config.is_configured;
    doc["use_static_ip"] = _config.use_static_ip;
    
    if (_config.use_static_ip) {
        doc["static_ip"] = _config.static_ip.toString();
        doc["gateway"] = _config.gateway.toString();
        doc["subnet"] = _config.subnet.toString();
    }
    
    File configFile = SPIFFS.open("/config/wifi_config.json", "w");
    if (!configFile) {
        DEBUG_PRINTLN("[WiFi配置] 无法创建配置文件");
        return false;
    }
    
    serializeJson(doc, configFile);
    configFile.close();
    
    DEBUG_PRINTLN("[WiFi配置] 配置保存成功到SPIFFS");
    return true;
}

bool WiFiConfigManager::connectWiFi(unsigned long timeout_ms) {
    if (!_config.is_configured || strlen(_config.ssid) == 0) {
        DEBUG_PRINTLN("[WiFi配置] 未配置WiFi，无法连接");
        return false;
    }
    
    DEBUG_PRINTF("\n[WiFi] 正在连接: %s\n", _config.ssid);
    DEBUG_PRINT("[WiFi] 连接进度: ");
    
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    
    if (_config.use_static_ip) {
        WiFi.config(_config.static_ip, _config.gateway, _config.subnet);
    }
    
    WiFi.begin(_config.ssid, _config.password);
    
    unsigned long start_time = millis();
    int retry_count = 0;
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        retry_count++;
        
        if (retry_count % 2 == 0) {
            DEBUG_PRINT(".");
        }
        
        if (retry_count % 20 == 0) {
            DEBUG_PRINTF(" (%lds)\n[WiFi] 继续连接: ", (millis() - start_time) / 1000);
        }
        
        if (millis() - start_time > timeout_ms) {
            DEBUG_PRINTLN("");
            DEBUG_PRINTLN("[WiFi] 连接超时");
            
            wl_status_t status = WiFi.status();
            DEBUG_PRINTF("[WiFi] 状态码: %d\n", status);
            switch(status) {
                case WL_NO_SSID_AVAIL:
                    DEBUG_PRINTLN("[WiFi] 原因: 找不到该WiFi网络");
                    break;
                case WL_CONNECT_FAILED:
                    DEBUG_PRINTLN("[WiFi] 原因: 密码错误或认证失败");
                    break;
                case WL_DISCONNECTED:
                    DEBUG_PRINTLN("[WiFi] 原因: 连接被断开");
                    break;
                default:
                    DEBUG_PRINTLN("[WiFi] 原因: 未知错误");
            }
            
            return false;
        }
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[WiFi] 连接成功");
    DEBUG_PRINTF("  IP地址: %s\n", WiFi.localIP().toString().c_str());
    DEBUG_PRINTF("  信号强度: %d dBm\n", WiFi.RSSI());
    DEBUG_PRINTF("  MAC地址: %s\n", WiFi.macAddress().c_str());
    
    return true;
}

void WiFiConfigManager::resetConfig() {
    if (SPIFFS.exists("/config/wifi_config.json")) {
        SPIFFS.remove("/config/wifi_config.json");
        DEBUG_PRINTLN("[WiFi配置] 配置已重置");
    }
    _config.is_configured = false;
    memset(_config.ssid, 0, sizeof(_config.ssid));
    memset(_config.password, 0, sizeof(_config.password));
}
