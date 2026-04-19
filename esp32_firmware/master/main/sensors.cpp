/**
 * ============================================================
 *  传感器数据采集模块 - 实现版（主机模式）
 * ============================================================
 * 
 * 职责：
 *   - 初始化本地传感器硬件（DHT11/MQ2/水位）
 *   - 读取并过滤本地传感器数据
 *   - 融合本地+远程从机传感器数据
 *   - 提供统一的数据接口
 * 
 * 注意：
 *   - WiFi通信初始化由 main.ino 的 setup() 负责
 *   - 本模块只调用 handleWiFiCommunication() 处理通信
 */

#include "sensors.h"
#include "public.h"
#include "mySensor.h"
#include "sensor_data.h"
#include "myWIFI.h"

const unsigned long SENSOR_READ_INTERVAL_MS = 2000;
const int FILTER_WINDOW_SIZE = 5;

static float temperature_buffer[FILTER_WINDOW_SIZE] = {0};
static float humidity_buffer[FILTER_WINDOW_SIZE] = {0};
static int mq2_buffer[FILTER_WINDOW_SIZE] = {0};
static int water_buffer[FILTER_WINDOW_SIZE] = {0};
static int buffer_index = 0;

static unsigned long last_read_time = 0;
static bool sensors_initialized = false;

enum DataSource {
    SOURCE_LOCAL,
    SOURCE_REMOTE,
    SOURCE_FUSED
};

static DataSource preferred_source = SOURCE_FUSED;

static void readLocalSensors();
static SensorData getLocalSensorData();
static SensorData getRemoteSensorData();
static SensorData getFusedSensorData();
static float applyMovingAverage(float new_val, float* buffer, int size);
static int applyMovingAverageInt(int new_val, int* buffer, int size);
static bool isAbnormalValue(float value, float expected, float tolerance_percent);

void initSensors() {
    if (sensors_initialized) return;
    
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("  [主机] 传感器模块初始化");
    DEBUG_PRINTLN("========================================");
    
    DEBUG_PRINTLN("[传感器] 初始化本地传感器硬件...");
    
    DEBUG_PRINTLN("[传感器] 跳过本地传感器初始化（主机模式）");
    
    DEBUG_PRINTLN("[传感器] 初始化数据滤波缓冲区...");
    
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
        temperature_buffer[i] = 25.0;
        humidity_buffer[i] = 50.0;
        mq2_buffer[i] = 100;
        water_buffer[i] = 1500;
    }
    
    last_read_time = millis() - SENSOR_READ_INTERVAL_MS;
    sensors_initialized = true;
    localSensorValid = false;
    
    DEBUG_PRINTLN("[主机] 初始化完成");
    DEBUG_PRINTLN("[主机] 数据源模式: 融合模式（优先远程）");
    DEBUG_PRINTLN("========================================\n");
}

SensorData readSensors() {
    SensorData data;
    
    if (!sensors_initialized) initSensors();
    
    handleWiFiCommunication();
    
    localSensorValid = false;
    
    switch (preferred_source) {
        case SOURCE_LOCAL:
            data = getLocalSensorData();
            break;
        case SOURCE_REMOTE:
        case SOURCE_FUSED:
        default:
            if (isRemoteDataValid()) {
                data = getRemoteSensorData();
            } else {
                data.temperature = 25.0;
                data.humidity = 50.0;
                data.mq2 = 0;
                data.water_level = 0.0;
            }
            break;
    }
    
    return data;
}

static SensorData getLocalSensorData() {
    SensorData data;
    
    if (!localSensorValid) {
        data.temperature = temperature_buffer[(buffer_index - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE];
        data.humidity = humidity_buffer[(buffer_index - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE];
        data.mq2 = mq2_buffer[(buffer_index - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE];
        data.water_level = analogToMm(water_buffer[(buffer_index - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE]);
        return data;
    }
    
    data.temperature = temperature;
    data.humidity = humidity;
    data.mq2 = gasValue;
    data.water_level = analogToMm(waterValue);
    
    return data;
}

static SensorData getRemoteSensorData() {
    SensorData data;
    data.temperature = remoteTemp;
    data.humidity = remoteHumi;
    data.mq2 = remoteGas;
    data.water_level = analogToMm(remoteWater);
    return data;
}

static SensorData getFusedSensorData() {
    SensorData data;
    
    bool remote_ok = isRemoteDataValid();
    bool local_ok = localSensorValid;
    
    if (remote_ok && local_ok) {
        data = getRemoteSensorData();
        data.mq2 = max(data.mq2, gasValue);
    } else if (remote_ok) {
        data = getRemoteSensorData();
    } else if (local_ok) {
        data = getLocalSensorData();
    } else {
        data.temperature = temperature_buffer[0];
        data.humidity = humidity_buffer[0];
        data.mq2 = mq2_buffer[0];
        data.water_level = analogToMm(water_buffer[0]);
    }
    
    return data;
}

static void readLocalSensors() {
    float raw_temp, raw_humi;
    int raw_smoke, raw_gas, raw_water;
    
    readmyDHT(raw_temp, raw_humi);
    readMQ2(raw_smoke, raw_gas);
    readwater(raw_water);
    
    temperature = raw_temp;
    humidity = raw_humi;
    smokeStatus = raw_smoke;
    gasValue = raw_gas;
    waterValue = raw_water;
    
    applyMovingAverage(raw_temp, temperature_buffer, FILTER_WINDOW_SIZE);
    applyMovingAverage(raw_humi, humidity_buffer, FILTER_WINDOW_SIZE);
    applyMovingAverageInt(raw_gas, mq2_buffer, FILTER_WINDOW_SIZE);
    applyMovingAverageInt(raw_water, water_buffer, FILTER_WINDOW_SIZE);
    
    buffer_index = (buffer_index + 1) % FILTER_WINDOW_SIZE;
}

static float applyMovingAverage(float new_val, float* buffer, int size) {
    for (int i = 0; i < size - 1; i++) buffer[i] = buffer[i + 1];
    buffer[size - 1] = new_val;
    
    float sum = 0;
    for (int i = 0; i < size; i++) sum += buffer[i];
    return sum / size;
}

static int applyMovingAverageInt(int new_val, int* buffer, int size) {
    for (int i = 0; i < size - 1; i++) buffer[i] = buffer[i + 1];
    buffer[size - 1] = new_val;
    
    long sum = 0;
    for (int i = 0; i < size; i++) sum += buffer[i];
    return (int)(sum / size);
}

float analogToMm(int adc_value) {
    if (adc_value <= 100) return 0.0;
    else if (adc_value >= 4000) return 30.0;
    else return (adc_value * 30.0 / 4095.0);
}
