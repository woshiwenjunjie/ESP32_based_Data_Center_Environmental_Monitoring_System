/**
 * ============================================================
 *  myWIFI - WiFi主从通信模块（从机模式）
 * ============================================================
 * 
 * 功能：
 *   - TCP客户端，连接到主机服务器
 *   - 定时发送传感器数据到主机
 *   - 接收主机的控制指令（预留）
 *   - 自动重连机制
 * 
 * 数据协议：
 *   发送: "temperature,humidity,smoke,gas,water\n"
 *   示例: "25.5,60.0,1,1500,2000\n"
 */

#ifndef _MYWIFI_H
#define _MYWIFI_H

#include <WiFi.h>
#include "sensor_data.h"
#include "config.h"

// ==================== 全局变量声明 ====================
extern WiFiClient client;

// 状态标志
extern bool wifiConnected;
extern bool networkReady;

// 时间控制
extern unsigned long lastReconnectAttempt;
extern unsigned long lastSendTime;

// 远程数据（接收自主机，预留）
extern float remoteTemp;
extern float remoteHumi;
extern int remoteSmoke;
extern int remoteGas;
extern int remoteWater;

// ==================== 函数接口 ====================

void mywifi_init();
void handleWiFiCommunication();
void sendLocalData(WiFiClient &cli);
void parseRemoteData(String data);

#endif // _MYWIFI_H
