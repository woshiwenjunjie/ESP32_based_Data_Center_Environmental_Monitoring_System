/**
 * ============================================================
 *  传感器数据定义 - 变量初始化（从机版）
 * ============================================================
 */

#include "sensor_data.h"

// ==================== 本地传感器数据初始化 ====================
float temperature = 0.0;
float humidity = 0.0;
int smokeStatus = 1;       // 默认正常状态
int gasValue = 0;
int waterValue = 0;

// ==================== 远程数据初始化（预留）====================
float remoteTemp = 0.0;
float remoteHumi = 0.0;
int remoteSmoke = 1;       // 默认正常状态
int remoteGas = 0;
int remoteWater = 0;
