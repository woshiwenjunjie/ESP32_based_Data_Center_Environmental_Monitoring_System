#ifndef _mqtt_test_H
#define _mqtt_test_H

#include "public.h"
#include "sensor_data.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ============== 全局变量声明 ==============
extern WiFiClient espClient;
extern PubSubClient mqttClient;

// ============== 功能函数 ==============
void connectWiFi();
void connectMQTT();
void publishAllSensorData();
void setupMQTT();
void loopMQTT();

#endif