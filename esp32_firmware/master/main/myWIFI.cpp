/**
 * ============================================================
 *  myWIFI - WiFi主从通信实现（主机模式）
 * ============================================================
 */

#include "myWIFI.h"
#include "public.h"
#include "wifi_config.h"

WiFiServer tcpServer(TCP_PORT);
WiFiClient client;

bool wifiConnected = false;
bool networkReady = false;
bool slaveConnected = false;
unsigned long lastReconnectAttempt = 0;

void wifi_server_init() {
    DEBUG_PRINTLN("[WiFi通信] 初始化主机模式...");
    DEBUG_PRINTF("[WiFi通信] TCP监听端口: %d\n", TCP_PORT);
    tcpServer.begin();
    DEBUG_PRINTLN("[WiFi通信] TCP服务器已启动");
}

void handleWiFiCommunication() {
    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
    
    if (currentlyConnected != wifiConnected) {
        wifiConnected = currentlyConnected;
        
        if (wifiConnected) {
            DEBUG_PRINTLN("[WiFi通信] WiFi已连接 IP: " + WiFi.localIP().toString());
            DEBUG_PRINTLN("[WiFi通信] TCP服务器已就绪 (端口: " + String(TCP_PORT) + ")");
            networkReady = true;
        } else {
            DEBUG_PRINTLN("[WiFi通信] WiFi已断开!");
            networkReady = false;
            slaveConnected = false;
        }
    }
    
    if (wifiConnected && networkReady) {
        if (!client.connected()) {
            if (slaveConnected) {
                DEBUG_PRINTLN("[WiFi通信] 从机已断开");
                client.stop();
                slaveConnected = false;
                remoteSensorValid = false;
            }
            
            client = tcpServer.available();
            if (client) {
                DEBUG_PRINTLN("[WiFi通信] 从机已连接");
                DEBUG_PRINTF("[WiFi通信]   从机IP: %s\n", client.remoteIP().toString().c_str());
                slaveConnected = true;
            }
        } else {
            while (client.available()) {
                String data = client.readStringUntil('\n');
                parseRemoteData(data);
            }
            
            if (!client.connected() && slaveConnected) {
                DEBUG_PRINTLN("[WiFi通信] ⚠️ 从机连接丢失");
                client.stop();
                slaveConnected = false;
                remoteSensorValid = false;
            }
        }
    }
}

void sendLocalData(WiFiClient &cli) {
    String msg = String(temperature) + "," +
                 String(humidity) + "," +
                 String(smokeStatus) + "," +
                 String(gasValue) + "," +
                 String(waterValue) + "\n";
    cli.print(msg);
}

void parseRemoteData(String data) {
    data.trim();
    
    int idx1 = data.indexOf(',');
    int idx2 = data.indexOf(',', idx1 + 1);
    int idx3 = data.indexOf(',', idx2 + 1);
    int idx4 = data.indexOf(',', idx3 + 1);
    
    if (idx1 != -1 && idx2 != -1 && idx3 != -1 && idx4 != -1) {
        remoteTemp = data.substring(0, idx1).toFloat();
        remoteHumi = data.substring(idx1 + 1, idx2).toFloat();
        remoteSmoke = data.substring(idx2 + 1, idx3).toInt();
        remoteGas = data.substring(idx3 + 1, idx4).toInt();
        remoteWater = data.substring(idx4 + 1).toInt();
        
        lastRemoteUpdate = millis();
        remoteSensorValid = true;
        
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint >= 2000) {
            lastPrint = millis();
            DEBUG_PRINTF("[远程数据] Temp=%.1f°C Humi=%.1f%% Smoke=%s Gas=%d Water=%d\n",
                         remoteTemp, remoteHumi,
                         remoteSmoke == LOW ? "⚠️ALERT" : "✓OK",
                         remoteGas, remoteWater);
        }
    } else {
        static unsigned long lastError = 0;
        if (millis() - lastError >= 5000) {
            lastError = millis();
            DEBUG_PRINTF("[WiFi通信] ⚠️ 数据格式错误: '%s'\n", data.c_str());
        }
    }
}

bool isRemoteDataValid() {
    if (!remoteSensorValid) return false;
    
    if (millis() - lastRemoteUpdate > REMOTE_DATA_TIMEOUT) {
        remoteSensorValid = false;
        DEBUG_PRINTLN("[远程数据] ⚠️ 数据超时");
        return false;
    }
    
    return true;
}

bool isSlaveConnected() {
    return slaveConnected && client.connected();
}
