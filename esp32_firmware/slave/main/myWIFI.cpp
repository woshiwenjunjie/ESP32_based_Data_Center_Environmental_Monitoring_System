/**
 * ============================================================
 *  myWIFI - WiFi主从通信实现（从机模式）
 * ============================================================
 */

#include "myWIFI.h"
#include "config.h"
#include "sensor_data.h"

// ==================== 全局变量定义 ====================
WiFiClient client;

unsigned long lastSendTime = 0;
bool wifiConnected = false;
bool networkReady = false;
unsigned long lastReconnectAttempt = 0;

/**
 * 初始化WiFi通信模块（从机模式）
 */
void mywifi_init(){
  Serial.println("[从机WiFi] 正在连接WiFi...");
  Serial.printf("[从机WiFi] SSID: %s\n", WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // 非阻塞方式等待连接（不使用WiFi.waitForConnectResult）
  Serial.println("[从机WiFi] 连接中（后台）...");
}

/**
 * 处理WiFi通信主逻辑（必须在loop中持续调用）
 */
void handleWiFiCommunication() {
  // 1. 检查 WiFi 状态
  bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
  
  if (currentlyConnected != wifiConnected) {
    wifiConnected = currentlyConnected;
    
    if (wifiConnected) {
    Serial.println("\n[从机WiFi] ✅ WiFi已连接!");
    Serial.printf("[从机WiFi] 本机IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[从机WiFi] 目标主机: %s:%d\n", HOST_IP, TCP_PORT);
      
      // 立即尝试连接主机
      networkReady = false;
      lastReconnectAttempt = millis();  // 立即开始第一次连接
      
    } else {
      Serial.println("[从机WiFi] ✗ WiFi已断开!");
      networkReady = false;
      if (client.connected()) {
        client.stop();
      }
    }
  }

  // 2. 如果 WiFi 已连接，处理 TCP 通信
  if (wifiConnected) {
    if (!networkReady) {
      // 尚未连接主机，尝试连接（非阻塞，定时重试）
      if (millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {
        lastReconnectAttempt = millis();
        
        Serial.print("[从机WiFi] 🔄 尝试连接主机...");
        if (client.connect(HOST_IP, TCP_PORT)) {
          Serial.println(" ✅成功!");
          Serial.printf("[从机WiFi] 已连接到主机 %s:%d\n", HOST_IP, TCP_PORT);
          networkReady = true;
          lastSendTime = millis();  // 立即发送第一份数据
        } else {
          Serial.println(" ❌失败，稍后重试");
        }
      }
    } else {
      // 已连接主机，正常工作
      
      // 发送本地数据（定时）
      if (millis() - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = millis();
        sendLocalData(client);
      }
      
      // 接收主机数据（控制指令，预留）
      while (client.available()) {
        String data = client.readStringUntil('\n');
        parseRemoteData(data);
      }
      
      // 检查连接是否断开
      if (!client.connected()) {
        Serial.println("[从机WiFi] ⚠️ 主机断开连接");
        client.stop();
        networkReady = false;
        lastReconnectAttempt = millis(); // 立即准备重连
      }
    }
  } else {
    // WiFi 未连接
    static unsigned long lastWifiStatus = 0;
    if (millis() - lastWifiStatus >= 10000) {  // 每10秒提示一次
      lastWifiStatus = millis();
      wl_status_t status = WiFi.status();
      Serial.printf("[从机WiFi] WiFi状态: %d (未连接)\n", status);
    }
  }
}

/**
 * 发送本地传感器数据到主机
 */
void sendLocalData(WiFiClient &cli) {
  if (!cli || !cli.connected()) {
    return;
  }
  
  // 构建CSV格式数据字符串
  String msg = String(temperature) + "," +
               String(humidity) + "," +
               String(smokeStatus) + "," +
               String(gasValue) + "," +
               String(waterValue) + "\n";
  
  // 发送数据
  size_t bytesSent = cli.print(msg);
  
  // 调试输出（降低频率）
  static unsigned long lastSendDebug = 0;
  if (millis() - lastSendDebug >= 5000) {  // 每5秒打印一次
    lastSendDebug = millis();
    
    if (bytesSent > 0) {
      Serial.printf("[从机发送] ✓ %d bytes | ", bytesSent);
      Serial.printf("T=%.1f H=%.1f S=%d G=%d W=%d\n",
                   temperature, humidity,
                   smokeStatus, gasValue, waterValue);
    } else {
      Serial.println("[从机发送] ⚠️ 发送失败！");
    }
  }
}

/**
 * 解析主机发送的数据（控制指令）
 * 预留功能：未来可扩展为主机下发控制指令
 */
void parseRemoteData(String data) {
  data.trim();
  
  if (data.length() == 0) return;
  
  // 当前仅打印接收到的数据（预留扩展）
  static unsigned long lastRecvDebug = 0;
  if (millis() - lastRecvDebug >= 10000) {  // 每10秒打印一次
    lastRecvDebug = millis();
    Serial.printf("[从机接收] 收到主机数据: '%s'\n", data.c_str());
  }
  
  // TODO: 未来可在此处添加控制指令解析
  // 例如：
  // - 修改传感器读取间隔
  // - 触发特定操作
  // - 固件升级等
}
