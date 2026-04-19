/**
 * ============================================================
 *  Actuators - 外设控制模块实现
 * ============================================================
 * 
 * 实现继电器、蜂鸣器、直流电机、舵机的完整控制逻辑
 * 包含状态管理、参数校验、日志输出等功能
 */

#include "actuators.h"

// ==================== 模块内部状态变量 ====================
static Servo myServo;                    // 舵机对象
static bool servo_attached = false;      // 舵机附加状态标志
static bool relay_state = false;         // 继电器当前状态（false=OFF, true=ON）
static bool buzzer_state = false;        // 蜂鸣器当前状态
static int motor_speed = 0;              // 电机当前速度（0-255）
static int servo_angle = 90;             // 舵机当前角度（0-180）

// 异步蜂鸣器相关变量
static bool async_buzzer_active = false;
static unsigned long async_buzzer_start_time = 0;
static unsigned long async_buzzer_duration = 0;

// ==================== 初始化函数 ====================

void actuators_init() {
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("[Actuators] ===== 初始化外设控制模块 =====");
    
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    relay_state = false;
    DEBUG_PRINTLN("[Actuators] ✓ 继电器初始化完成 (GPIO" + String(RELAY_PIN) + ")");
    
    if (ledcAttach(BUZZER_PIN, BUZZER_FREQ_DEFAULT, BUZZER_RESOLUTION)) {
        ledcWrite(BUZZER_PIN, 0);
        buzzer_state = false;
        DEBUG_PRINTLN("[Actuators] ✓ 蜂鸣器初始化完成 (GPIO" + String(BUZZER_PIN) + ")");
    } else {
        DEBUG_PRINTLN("[Actuators] ✗ 蜂鸣器LEDC通道分配失败!");
    }
    
    if (ledcAttach(MOTOR_PIN, MOTOR_FREQ_DEFAULT, MOTOR_RESOLUTION)) {
        ledcWrite(MOTOR_PIN, 0);
        motor_speed = 0;
        DEBUG_PRINTLN("[Actuators] ✓ 电机初始化完成 (GPIO" + String(MOTOR_PIN) + ")");
    } else {
        DEBUG_PRINTLN("[Actuators] ✗ 电机LEDC通道分配失败!");
    }
    
    myServo.attach(SERVO_PIN);
    servo_attached = true;
    servo_angle = 90;
    myServo.write(servo_angle);
    delay(100);
    DEBUG_PRINTLN("[Actuators] ✓ 舵机初始化完成 (GPIO" + String(SERVO_PIN) + ")");
    
    DEBUG_PRINTLN("[Actuators] ===== 所有外设初始化完成 =====");
    DEBUG_PRINTLN();
}

// ==================== 继电器控制实现 ====================

void relay_on() {
    digitalWrite(RELAY_PIN, HIGH);
    relay_state = true;
    DEBUG_PRINTLN("[Relay] ON");
}

void relay_off() {
    digitalWrite(RELAY_PIN, LOW);
    relay_state = false;
    DEBUG_PRINTLN("[Relay] OFF");
}

void relay_toggle() {
    bool new_state = !digitalRead(RELAY_PIN);
    digitalWrite(RELAY_PIN, new_state ? HIGH : LOW);
    relay_state = new_state;
    DEBUG_PRINTF("[Relay] TOGGLE → %s\n", new_state ? "ON" : "OFF");
}

bool relay_get_state() {
    return relay_state;
}

// ==================== 蜂鸣器控制实现 ====================

void buzzer_on() {
    ledcWriteTone(BUZZER_PIN, BUZZER_FREQ_DEFAULT);
    buzzer_state = true;
    DEBUG_PRINTLN("[Buzzer] ON");
}

void buzzer_on_with_freq(int freq) {
    freq = constrain(freq, 100, 10000);
    ledcWriteTone(BUZZER_PIN, freq);
    buzzer_state = true;
    DEBUG_PRINTF("[Buzzer] ON with freq: %dHz\n", freq);
}

void buzzer_off() {
    ledcWrite(BUZZER_PIN, 0);
    buzzer_state = false;
    async_buzzer_active = false;
    DEBUG_PRINTLN("[Buzzer] OFF");
}

void buzzer_beep(int freq, int duration_ms) {
    freq = constrain(freq, 100, 10000);
    duration_ms = constrain(duration_ms, 10, 5000);
    
    DEBUG_PRINTF("[Buzzer] BEEP: %dHz, %dms\n", freq, duration_ms);
    
    ledcWriteTone(BUZZER_PIN, freq);
    buzzer_state = true;
    delay(duration_ms);
    ledcWrite(BUZZER_PIN, 0);
    buzzer_state = false;
    
    DEBUG_PRINTLN("[Buzzer] BEEP done");
}

void buzzer_beep_async(int freq, int duration_ms) {
    freq = constrain(freq, 100, 10000);
    duration_ms = constrain(duration_ms, 10, 5000);
    
    ledcWriteTone(BUZZER_PIN, freq);
    buzzer_state = true;
    async_buzzer_active = true;
    async_buzzer_start_time = millis();
    async_buzzer_duration = duration_ms;
    
    DEBUG_PRINTF("[Buzzer] ASYNC-BEEP: %dHz, %dms\n", freq, duration_ms);
}

void buzzer_update() {
    if (async_buzzer_active && (millis() - async_buzzer_start_time >= async_buzzer_duration)) {
        ledcWrite(BUZZER_PIN, 0);
        buzzer_state = false;
        async_buzzer_active = false;
        DEBUG_PRINTLN("[Buzzer] ASYNC-BEEP stopped");
    }
}

bool buzzer_get_state() {
    return buzzer_state;
}

// ==================== 直流电机控制实现 ====================

void motor_stop() {
    ledcWrite(MOTOR_PIN, 0);
    motor_speed = 0;
    DEBUG_PRINTLN("[Motor] STOP");
}

void motor_run(int speed) {
    speed = constrain(speed, 0, 255);
    ledcWrite(MOTOR_PIN, speed);
    motor_speed = speed;
    DEBUG_PRINTF("[Motor] RUN: %d (%d%%)\n", speed, map(speed, 0, 255, 0, 100));
}

void motor_run_percent(int percent) {
    percent = constrain(percent, 0, 100);
    motor_run(map(percent, 0, 100, 0, 255));
}

int motor_get_speed() {
    return motor_speed;
}

// ==================== 舵机控制实现 ====================

void servo_attach() {
    if (!servo_attached) {
        myServo.attach(SERVO_PIN);
        servo_attached = true;
        DEBUG_PRINTLN("[Servo] ATTACH");
        delay(50);
        myServo.write(servo_angle);
    } else {
        DEBUG_PRINTLN("[Servo] ATTACH - already attached");
    }
}

void servo_detach() {
    if (servo_attached) {
        myServo.detach();
        servo_attached = false;
        DEBUG_PRINTLN("[Servo] DETACH");
    } else {
        DEBUG_PRINTLN("[Servo] DETACH - not attached");
    }
}

void servo_write_angle(int angle) {
    angle = constrain(angle, 0, 180);
    
    if (!servo_attached) {
        DEBUG_PRINTLN("[Servo] WRITE - auto attach...");
        servo_attach();
        delay(100);
    }
    
    myServo.write(angle);
    servo_angle = angle;
    DEBUG_PRINTF("[Servo] WRITE: %d°\n", angle);
}

int servo_get_angle() {
    if (!servo_attached) return -1;
    return servo_angle;
}

bool servo_is_attached() {
    return servo_attached;
}

// ==================== 状态查询实现 ====================

String get_actuators_state_json() {
    String json = "{";
    json += "\"relay\":\"" + String(relay_state ? "ON" : "OFF") + "\",";
    json += "\"buzzer\":\"" + String(buzzer_state ? "ON" : "OFF") + "\",";
    json += "\"motor\":" + String(motor_speed) + ",";
    json += "\"motor_percent\":" + String(map(motor_speed, 0, 255, 0, 100)) + ",";
    json += "\"servo\":" + String(servo_angle) + ",";
    json += "\"servo_attached\":" + String(servo_attached ? "true" : "false");
    json += "}";
    return json;
}
