#include "public.h"
#include "config.h"
#include "u8g2_oled.h"
#include "mySensor.h"
#include "myWIFI.h"
#include "sensor_data.h"
#include "mqtt_test.h"

bool showDetail = false;
unsigned long lastSwitchTime = 0;

void setup(){
  Serial.begin(115200);
  delay(100);

  // 1. 先初始化OLED（让用户立即看到反馈）
  MyU8g2Oled_init();
  Serial.println("[从机] OLED初始化完成");
  
  // 2. 显示启动画面
  displayData(-999.0, -999.0, 1, 0, 0);
  
  // 3. 初始化传感器
  Sensor_init();
  Serial.println("[从机] 传感器初始化完成");
  
  // 4. 初始化WiFi（可能耗时较长）
  mywifi_init();
  Serial.println("[从机] WiFi初始化完成");
  
  // 5. 初始化MQTT
  setupMQTT();
  Serial.println("[从机] MQTT初始化完成");
  
  Serial.println("\n[从机] ✅ 系统启动完成！");
  Serial.println("[从机] 正在连接主机...");
  Serial.println("======================================\n");
}

void loop(){
  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime >= SENSOR_READ_INTERVAL) {
    lastReadTime = millis();
    readSensors();
    printToSerial();
  }
  
  static unsigned long lastDisplayTime = 0;
  if (millis() - lastDisplayTime >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayTime = millis();
    updateDisplay();
  }
  
  // 检查报警状态
  checkAlarm();
  handleWiFiCommunication();
  loopMQTT(); // 处理MQTT通信
  delay(10);
}

void readSensors() {
  readmyDHT(temperature, humidity);
  readMQ2(smokeStatus, gasValue);
  readwater(waterValue);
}

void printToSerial() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 3000) {
    lastPrint = millis();
    
    Serial.printf("🌡️ 温度:%.1f°C 💧湿度:%.1f%% ", temperature, humidity);
    Serial.printf("💨烟雾:%s 🔥气体:%d 💧水位:%d | ", 
                 smokeStatus == LOW ? "⚠️" : "✓", 
                 gasValue, 
                 waterValue);
    
    if (networkReady) {
      Serial.printf("📡主机:✅已连接\n");
    } else {
      Serial.printf("📡主机:⏳连接中...\n");
    }
  }
}

void updateDisplay() {
  static float lastTemp = -999;
  static float lastHumi = -999;
  static int lastSmoke = -1;
  static int lastGas = -1;
  static int lastWater = -1;
  static unsigned long lastDisplay = 0;
  
  // 检查数据是否变化
  bool dataChanged = (abs(temperature - lastTemp) > 0.1 ||
                     abs(humidity - lastHumi) > 0.1 ||
                     smokeStatus != lastSmoke ||
                     abs(gasValue - lastGas) > 10 ||
                     abs(waterValue - lastWater) > 10);
  
  // 检查是否需要切换显示模式
  if (millis() - lastSwitchTime > 5000) {
    lastSwitchTime = millis();
    showDetail = !showDetail;
    dataChanged = true; // 切换模式时强制更新
  }
  
  // 数据变化时刷新，或每3秒强制刷新一次（保持OLED稳定）
  if (dataChanged || (millis() - lastDisplay >= 3000)) {
    lastDisplay = millis();
    
    // 切换显示模式
    if (!showDetail) {
      displayData(temperature, humidity, smokeStatus, gasValue, waterValue);
    } else {
      displayDetail(gasValue, waterValue);
    }
    
    // 更新上次数据
    lastTemp = temperature;
    lastHumi = humidity;
    lastSmoke = smokeStatus;
    lastGas = gasValue;
    lastWater = waterValue;
  }
}

void checkAlarm() {
  static bool alarmTriggered = false;
  
  if (smokeStatus == LOW) {
    if (!alarmTriggered) {
      Serial.println("\n!!! ⚠️⚠️⚠️ 烟雾报警 !!!\n");
      alarmTriggered = true;
    }
  } else {
    alarmTriggered = false;
  }
  
  if (gasValue > 2500) {
    Serial.println("!!! ⚠️ 气体浓度过高 !!!");
  }
}
