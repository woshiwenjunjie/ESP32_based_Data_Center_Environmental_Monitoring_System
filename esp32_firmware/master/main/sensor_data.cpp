/**
 * ============================================================
 *  传感器数据定义 - 变量初始化
 * ============================================================
 */

#include "sensor_data.h"

float temperature = 0.0;
float humidity = 0.0;
int smokeStatus = 1;
int gasValue = 0;
int waterValue = 0;

float remoteTemp = -999.0;
float remoteHumi = -999.0;
int remoteSmoke = -1;
int remoteGas = -1;
int remoteWater = -1;

bool localSensorValid = false;
bool remoteSensorValid = false;
unsigned long lastRemoteUpdate = 0;
