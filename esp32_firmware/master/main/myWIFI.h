/**
 * ============================================================
 *  myWIFI - WiFi主从通信模块（主机模式）
 * ============================================================
 */

#ifndef _MYWIFI_H
#define _MYWIFI_H

#include <WiFi.h>
#include "sensor_data.h"

const uint16_t TCP_PORT = 8888;
const unsigned long SEND_INTERVAL = 1500;
const long RECONNECT_INTERVAL = 5000;
const unsigned long REMOTE_DATA_TIMEOUT = 5000;

extern WiFiServer tcpServer;
extern WiFiClient client;

extern bool wifiConnected;
extern bool networkReady;
extern bool slaveConnected;

extern unsigned long lastReconnectAttempt;

void wifi_server_init();
void handleWiFiCommunication();
void sendLocalData(WiFiClient &cli);
void parseRemoteData(String data);
bool isRemoteDataValid();
bool isSlaveConnected();

#endif // _MYWIFI_H
