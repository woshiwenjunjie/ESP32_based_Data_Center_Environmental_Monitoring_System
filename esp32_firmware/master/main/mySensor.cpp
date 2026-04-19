/**
 * ============================================================
 *  mySensor - 传感器驱动实现
 * ============================================================
 */

#include "mySensor.h"
#include "public.h"

DHT dht(DHTPIN, DHTTYPE);

void Sensor_init() {
    DEBUG_PRINTLN("[传感器] 正在初始化...");
    
    dht.begin();
    DEBUG_PRINTLN("[传感器] DHT温湿度传感器已初始化");
    
    pinMode(MQ2_DO, INPUT);
    DEBUG_PRINTLN("[传感器] MQ2气体传感器已配置");
    
    analogSetPinAttenuation(MQ2_AO, ADC_11db);
    analogSetPinAttenuation(WATERPIN, ADC_11db);
    DEBUG_PRINTLN("[传感器] ADC引脚已配置(11dB衰减)");
    
    DEBUG_PRINTLN("[传感器] 所有传感器初始化完成！");
}

void readmyDHT(float &temp, float &humi) {
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    
    if (isnan(temp) || isnan(humi)) {
        DEBUG_PRINTLN("[传感器] DHT读取失败");
        temp = 0.0;
        humi = 0.0;
    }
}

void readMQ2(int &smoke, int &gas) {
    smoke = digitalRead(MQ2_DO);
    gas = analogRead(MQ2_AO);
}

void readwater(int &water) {
    water = analogRead(WATERPIN);
}
