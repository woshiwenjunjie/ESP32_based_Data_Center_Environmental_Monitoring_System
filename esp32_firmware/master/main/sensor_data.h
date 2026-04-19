/**
 * ============================================================
 *  传感器数据定义 - 全局变量声明
 * ============================================================
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

extern float temperature;
extern float humidity;
extern int smokeStatus;
extern int gasValue;
extern int waterValue;

extern float remoteTemp;
extern float remoteHumi;
extern int remoteSmoke;
extern int remoteGas;
extern int remoteWater;

extern bool localSensorValid;
extern bool remoteSensorValid;
extern unsigned long lastRemoteUpdate;

#endif // SENSOR_DATA_H
