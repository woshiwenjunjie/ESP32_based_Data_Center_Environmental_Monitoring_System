/**
 * ============================================================
 *  WiFi配置管理模块 - 安全的WiFi凭据存储 (STA模式)
 * ============================================================
 * 
 * 功能：
 *   - 使用SPIFFS存储WiFi配置（非硬编码，安全存储）
 *   - 纯STA模式，不使用AP模式
 *   - 配置持久化存储，断电不丢失
 *   - 自动重连机制
 *   - 通过串口提示首次配置
 * 
 * 使用方法：
 *   1. 首次启动时通过串口提示需要配置
 *   2. 用户可通过串口输入或编辑代码配置WiFi
 *   3. 配置保存后自动连接并持久化
 * 
 * 文件位置：/config/wifi_config.json
 * ============================================================
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

// WiFi配置结构体
struct WiFiConfig {
    char ssid[64];           // WiFi名称
    char password[64];       // WiFi密码
    bool is_configured;      // 是否已配置
    bool use_static_ip;      // 是否使用静态IP
    IPAddress static_ip;     // 静态IP地址
    IPAddress gateway;       // 网关地址
    IPAddress subnet;        // 子网掩码
};

class WiFiConfigManager {
public:
    /**
     * 初始化WiFi配置管理器
     */
    void begin();
    
    /**
     * 加载保存的WiFi配置
     * @return 是否成功加载配置
     */
    bool loadConfig();
    
    /**
     * 保存WiFi配置到SPIFFS
     * @param ssid WiFi名称
     * @param password WiFi密码
     * @return 是否保存成功
     */
    bool saveConfig(const String& ssid, const String& password);
    
    /**
     * 连接WiFi（阻塞式）
     * @param timeout_ms 超时时间（毫秒），默认30秒
     * @return 是否连接成功
     */
    bool connectWiFi(unsigned long timeout_ms = 30000);
    
    /**
     * 检查是否已配置WiFi
     * @return 是否已配置
     */
    bool isConfigured() const { return _config.is_configured; }
    
    /**
     * 获取当前WiFi配置
     * @return WiFi配置结构体
     */
    const WiFiConfig& getConfig() const { return _config; }
    
    /**
     * 获取当前连接状态
     * @return WiFi状态
     */
    wl_status_t getStatus() const { return WiFi.status(); }
    
    /**
     * 获取本地IP地址
     * @return IP地址字符串
     */
    String getIPAddress() const { return WiFi.localIP().toString(); }
    
    /**
     * 重置WiFi配置（清除保存的配置）
     */
    void resetConfig();

private:
    WiFiConfig _config;
    
    // 内部函数
    bool _initSPIFFS();
    bool _readConfigFromFile();
    bool _writeConfigToFile();
};

// 全局实例声明
extern WiFiConfigManager wifiManager;

#endif // WIFI_CONFIG_H
