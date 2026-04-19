/**
 * ============================================================
 *  ESP32-CAM 智能环境监测站 - 主程序入口 (安全版)
 * ============================================================
 * 
 * 功能：
 *   1. 初始化串口调试输出（115200波特率）
 *   2. 检测并初始化PSRAM（ESP32-S3的8MB外部内存）
 *   3. 安全连接WiFi网络（STA模式）
 *   4. 初始化OV3660摄像头（自动检测PSRAM选择配置）
 *   5. 启动HTTP Web服务器（视频流/拍照/AI识别/传感器API）
 * 
 * 主从通信：
 *   - 作为TCP服务器(8888端口)接收从机传感器数据
 *   - 支持远程从机数据接收和显示
 * 
 * WiFi连接方式：
 *   - 使用SPIFFS存储WiFi配置（安全存储，非硬编码）
 *   - 首次运行或配置丢失时，通过串口提示配置
 *   - 配置持久化保存，断电不丢失
 * 
 * 硬件平台：ESP32-S3-WROOM + OV3660摄像头模块
 * 编译环境：Arduino IDE 2.x + ESP32 Arduino Core 3.0+
 * 
 * 版本: v5.1 (安全STA版)
 * 更新日期: 2026-04-18
 * ============================================================
 */

#include <esp_http_server.h> 
#include "public.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "myOV3660.h"
#include "sensors.h"
#include "mySensor.h"
#include "sensor_data.h"
#include "myWIFI.h"
#include "web_server.h"
#include "wifi_config.h"
#include "u8g2_oled.h"
#include "actuators.h"
#ifdef ENABLE_AUTO_CONTROL
#include "auto_control.h"
#endif

// ==================== 配置常量 ====================
#define WIFI_CONNECT_TIMEOUT_MS 15000

/**
 * setup() - 系统初始化函数（上电或复位后执行一次）
 */
void setup() 
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  DEBUG_PRINTLN();
  
  DEBUG_PRINTLN("========================================");
  DEBUG_PRINTLN("  ESP32 环境监测站 [主机模式] v4.1");
  DEBUG_PRINTLN("  (安全STA模式)");
  DEBUG_PRINTF("  芯片型号: %s\n", ESP.getChipModel());
  DEBUG_PRINTF("  芯片版本: %d\n", ESP.getChipRevision());
  
  if (psramInit()) {
    DEBUG_PRINTF("  PSRAM: %u bytes (%.1f MB)\n", 
                  ESP.getPsramSize(), ESP.getPsramSize() / (1024.0 * 1024));
  } else {
    DEBUG_PRINTLN("  PSRAM: 初始化失败");
  }
  
  DEBUG_PRINTF("  堆空闲: %u bytes\n", ESP.getFreeHeap());
  DEBUG_PRINTLN("========================================");
  
  // 先初始化OLED（无论WiFi是否成功都显示）
  MyU8g2Oled_init();
  
  // 显示初始状态（等待从机连接）
  displayRemote(-999.0, -999.0, -1, -1, -1, false);
  
  // 初始化WiFi配置管理器
  wifiManager.begin();
  
  // 检查是否有保存的WiFi配置
  if (wifiManager.isConfigured()) {
    DEBUG_PRINTLN("\n[WiFi] 发现已保存的配置，正在连接...");
    
    // 尝试连接WiFi
    if (wifiManager.connectWiFi(WIFI_CONNECT_TIMEOUT_MS)) {
      DEBUG_PRINTLN("[WiFi] 连接成功！");
      
      // WiFi连接成功，初始化其他模块
      initSensors();           // 初始化传感器
      actuators_init();        // 初始化外设控制模块（继电器/蜂鸣器/电机/舵机）
#ifdef ENABLE_AUTO_CONTROL
      auto_control_init();     // 初始化自动控制系统
      load_default_auto_rules(); // 加载默认自动控制规则
#endif
      myov3660_init();        // 初始化OV3660摄像头
      wifi_server_init();      // 初始化TCP服务器
      startCameraServer();    // 启动Web服务器
      
      DEBUG_PRINT("\n[就绪] Web地址: http://");
      DEBUG_PRINT(WiFi.localIP());
      DEBUG_PRINTLN("/");
      DEBUG_PRINTF("[就绪] TCP端口: %d (等待从机连接)\n", TCP_PORT);
      DEBUG_PRINTF("[内存] 堆空闲: %u bytes\n", ESP.getFreeHeap());
      
      DEBUG_PRINTLN("\n系统启动完成！");
      DEBUG_PRINTLN("========================================\n");
    } else {
      // WiFi连接失败
      DEBUG_PRINTLN("\nWiFi连接失败！请检查：");
      DEBUG_PRINTLN("   1. WiFi名称和密码是否正确");
      DEBUG_PRINTLN("   2. 路由器是否正常工作");
      DEBUG_PRINTLN("   3. 设备是否在信号范围内");
      DEBUG_PRINTLN("\n系统将尝试重新连接...");
      DEBUG_PRINTLN("   （可通过串口监视器查看状态）");
      DEBUG_PRINTLN("========================================\n");
    }
  } else {
    // 首次运行或配置丢失
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("  首次运行 - 需要配置WiFi");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("\n配置步骤：");
    DEBUG_PRINTLN("1. 打开Arduino IDE的串口监视器");
    DEBUG_PRINTLN("2. 设置波特率为 115200");
    DEBUG_PRINTLN("3. 按照提示输入WiFi信息");
    DEBUG_PRINTLN("\n或者使用以下方法快速配置：");
    DEBUG_PRINTLN("   方法1: 通过串口输入命令");
    DEBUG_PRINTLN("   方法2: 编辑 wifi_config.cpp 中的默认值");
    DEBUG_PRINTLN("========================================\n");
    
    // 尝试使用默认配置连接（如果有的话）
    DEBUG_PRINTLN("[WiFi] 正在尝试自动检测网络...");
  }
}

/**
 * loop() - 主循环
 */
void loop()
{
  // WiFi连接状态检查和自动重连
  static unsigned long lastWiFiCheck = 0;
  const unsigned long wifiCheckInterval = 5000;
  
  if (millis() - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED && wifiManager.isConfigured()) {
      DEBUG_PRINTLN("[WiFi] 连接断开，尝试重新连接...");
      if (wifiManager.connectWiFi(10000)) {
        DEBUG_PRINTLN("[WiFi] 重连成功");
      } else {
        DEBUG_PRINTLN("[WiFi] 重连失败，将在下次尝试");
      }
    }
  }
  
  // 只有在WiFi连接成功后才运行主要功能
  if (WiFi.status() == WL_CONNECTED) {
    buzzer_update();         // 更新异步蜂鸣器状态
    
#ifdef ENABLE_AUTO_CONTROL
    // 自动控制检查（每1秒执行一次）
    static unsigned long lastAutoControlCheck = 0;
    if (millis() - lastAutoControlCheck >= 1000) {
        lastAutoControlCheck = millis();
        check_and_execute_auto_control();
    }
#endif
    
    handleWiFiCommunication();
    
    static unsigned long last_sensor_read = 0;
    
    if (millis() - last_sensor_read >= 2000) {
      last_sensor_read = millis();
      
      SensorData data = readSensors();
      
      static unsigned long last_status = 0;
      if (millis() - last_status >= 10000) {
        last_status = millis();
        
        DEBUG_PRINTLN("\n========== [主机] 系统状态 ==========");
        DEBUG_PRINTF("数据源: %s\n", isRemoteDataValid() ? "远程从机" : "等待从机");
        DEBUG_PRINTF("温度: %.1f C | 湿度: %.1f %%\n", data.temperature, data.humidity);
        DEBUG_PRINTF("气体: %d | 水位: %.1f cm\n", data.mq2, data.water_level);
        DEBUG_PRINTF("从机: %s\n", isSlaveConnected() ? "已连接" : "等待连接..");
        DEBUG_PRINTF("内存: %u bytes\n", ESP.getFreeHeap());
        DEBUG_PRINTLN("=====================================\n");
      }
    }
    
    static unsigned long lastDisplay = 0;
    static float lastTemp = -999;
    static float lastHumi = -999;
    static int lastSmoke = -1;
    static int lastGas = -1;
    static int lastWater = -1;
    static bool lastOnline = false;
    const unsigned long displayInterval = 200;
    
    // 检查数据是否变化
    bool isOnline = isRemoteDataValid() && isSlaveConnected();
    bool dataChanged = (abs(remoteTemp - lastTemp) > 0.1 ||
                     abs(remoteHumi - lastHumi) > 0.1 ||
                     remoteSmoke != lastSmoke ||
                     abs(remoteGas - lastGas) > 10 ||
                     abs(remoteWater - lastWater) > 10 ||
                     isOnline != lastOnline);
    
    // 数据变化时刷新，或每3秒强制刷新一次（保持OLED稳定）
    if ((dataChanged && millis() - lastDisplay >= displayInterval) || 
        (millis() - lastDisplay >= 3000)) {
      lastDisplay = millis();
      displayRemote(remoteTemp, remoteHumi, remoteSmoke, remoteGas, remoteWater, isOnline);
      
      // 更新上次数据
      lastTemp = remoteTemp;
      lastHumi = remoteHumi;
      lastSmoke = remoteSmoke;
      lastGas = remoteGas;
      lastWater = remoteWater;
      lastOnline = isOnline;
    }
  } else {
    // WiFi未连接时，显示连接状态
    static unsigned long lastOfflineDisplay = 0;
    const unsigned long offlineDisplayInterval = 500;
    if (millis() - lastOfflineDisplay >= offlineDisplayInterval) {
      lastOfflineDisplay = millis();
      displayRemote(0.0, 0.0, 1, 0, 0, false);  // 显示离线状态
    }
  }
  
  delay(10);
}
