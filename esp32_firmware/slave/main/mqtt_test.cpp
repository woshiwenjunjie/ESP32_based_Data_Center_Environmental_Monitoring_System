#include "mqtt_test.h"
#include "sensor_data.h"
#include "config.h"
#include "myWIFI.h"

// ==================== MQTT 客户端 ====================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============== 连接WiFi ==============
void connectWiFi() {
  if (wifiConnected) {
    Serial.printf("[MQTT] WiFi已连接, IP:%s\n", WiFi.localIP().toString().c_str());
    return;
  }
  
  Serial.print("[MQTT] 连接WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[MQTT] WiFi已连接, IP:%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[MQTT] WiFi连接失败");
  }
}

// ============== 连接MQTT ==============
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("连接OneNET...");
    String clientId = DEVICE_NAME;
    String username = PRODUCT_ID;
    String password = DEVICE_SECRET;
    
    if (mqttClient.connect(clientId.c_str(), username.c_str(), password.c_str())) {
      Serial.println("成功");
    } else {
      Serial.printf("失败, 状态码:%d\n", mqttClient.state());
      delay(5000);
    }
  }
}

// ============== 上报所有传感器数据 ==============
void publishAllSensorData() {
  if (!wifiConnected) {
    Serial.println("[MQTT] WiFi未连接，等待连接...");
    return;
  }
  
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  // 构建JSON文档
  doc["id"] = String(millis());
  doc["version"] = "1.0";
  // 构建参数对象
  JsonObject params = doc.createNestedObject("params");
  // 构建温度对象
  JsonObject tempObj = params.createNestedObject("temperature");
  tempObj["value"] = round(temperature * 10) / 10.0;
  // 构建湿度对象
  JsonObject humObj = params.createNestedObject("humidity");
  humObj["value"] = round(humidity * 10) / 10.0;
  // 构建烟雾状态对象
  JsonObject smokeObj = params.createNestedObject("smoke_status");
  smokeObj["value"] = (smokeStatus == HIGH) ? 1 : 0;
  // 构建气体浓度对象
  JsonObject gasObj = params.createNestedObject("gas_value");
  gasObj["value"] = gasValue;
  // 构建水位对象
  JsonObject waterObj = params.createNestedObject("water_level");
  waterObj["value"] = waterValue;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  String topic = "$sys/" + String(PRODUCT_ID) + "/" + String(DEVICE_NAME) + "/thing/property/post";

  if (mqttClient.publish(topic.c_str(), jsonBuffer)) {
    Serial.printf("[MQTT] 上报成功: T:%.1f°C H:%.1f%% S:%d G:%d W:%d\n",
                 temperature, humidity, 
                 (smokeStatus == HIGH) ? 1 : 0, 
                 gasValue, waterValue);
  } else {
    Serial.println("[MQTT] 上报失败");
  }
}

// ============== 初始化MQTT ==============
void setupMQTT() {
  Serial.println("[MQTT] 初始化MQTT客户端...");
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  connectWiFi();
  connectMQTT();
  Serial.println("[MQTT] ✅ MQTT初始化完成");
}

// ============== MQTT循环处理 ==============
void loopMQTT() {
  static unsigned long lastPublishTime = 0;
  static unsigned long lastReconnectTime = 0;
  
  if (!wifiConnected) {
    return;
  }
  // 检查MQTT连接状态
  if (millis() - lastReconnectTime >= 10000) {
    lastReconnectTime = millis();
    if (!mqttClient.connected()) {
      connectMQTT();
    }
  }
  // 检查是否需要上报数据
  if (millis() - lastPublishTime >= SEND_INTERVAL && mqttClient.connected()) {
    lastPublishTime = millis();
    publishAllSensorData();
  }
  // 处理MQTT消息
  mqttClient.loop();
}