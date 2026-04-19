/**
 * ============================================================
 *  mySensor - 传感器驱动模块（适配版）
 * ============================================================
 */

#ifndef _MYSENSOR_H
#define _MYSENSOR_H

#include <Arduino.h>

// 避免 sensor_t 结构体冲突
#define sensor_t adafruit_sensor_t
#include <DHT.h>
#include <Adafruit_Sensor.h>
#undef sensor_t

// ==================== 引脚定义 ====================
#define WATERPIN 32           // 水位传感器引脚
#define DHTPIN 21             // DHT传感器数据引脚
#define DHTTYPE DHT11        // 传感器类型 (DHT11/DHT22)
#define MQ2_AO 36            // MQ2模拟输出引脚
#define MQ2_DO 16            // MQ2数字输出引脚

extern DHT dht;

void Sensor_init();
void readmyDHT(float &temp, float &humi);
void readMQ2(int &smoke, int &gas);
void readwater(int &water);

#endif // _MYSENSOR_H
