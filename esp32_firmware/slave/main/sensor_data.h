/**
 * ============================================================
 *  传感器数据定义 - 全局变量声明（从机版）
 * ============================================================
 * 
 * 【硬件说明】
 * 
 * 本文件定义了从机采集的传感器数据变量，对应以下硬件模块：
 * 
 * 1. DHT11/DHT22 温湿度传感器
 *    - 测量范围: 温度 0~50°C, 湿度 20~90%RH
 *    - 精度: 温度 ±2°C, 湿度 ±5%RH
 *    - 连接: 数据引脚 → GPIO21, VCC → 3.3V/5V, GND → GND
 * 
 * 2. MQ-2 烟雾/气体传感器
 *    - 检测: 烟雾、液化气、丁烷、丙烷、甲烷、酒精、氢气等
 *    - 输出: 模拟值(0-4095)和数字值(0/1)
 *    - 连接: AO → GPIO36, DO → GPIO16, VCC → 5V, GND → GND
 *    - 注意: 首次使用需预热1-2分钟才能稳定
 * 
 * 3. 水位传感器（模拟式）
 *    - 测量: 水位高度或浸水状态
 *    - 输出: 模拟值(0-4095)，值越大表示水位越高
 *    - 连接: AO → GPIO32, VCC → 3.3V/5V, GND → GND
 * 
 * 【数据流向】
 * 传感器硬件 → mySensor驱动 → sensor_data变量 → TCP发送 → 主机
 * 
 * 【使用说明】
 * - 这些变量由sensor_data.cpp中的采集函数更新
 * - 通过TCP协议定时发送给主机（默认5秒间隔）
 * - 数据格式: "temperature,humidity,smoke,gas,water\n"
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

// ==================== 本地传感器数据 ====================
extern float temperature;     // 温度 (°C) - 来自DHT传感器
extern float humidity;        // 湿度 (%RH) - 来自DHT传感器
extern int smokeStatus;       // 烟雾状态 (1=正常, 0=报警) - 来自MQ-2数字输出
extern int gasValue;          // 气体浓度 (ADC值, 0-4095) - 来自MQ-2模拟输出
extern int waterValue;        // 水位值 (ADC值, 0-4095) - 来自水位传感器

// ==================== 远程数据（接收自主机，预留）====================
extern float remoteTemp;
extern float remoteHumi;
extern int remoteSmoke;
extern int remoteGas;
extern int remoteWater;

#endif // SENSOR_DATA_H
