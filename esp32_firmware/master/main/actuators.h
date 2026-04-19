/**
 * ============================================================
 *  Actuators - 外设控制模块（ESP32-S3 适配版）
 * ============================================================
 * 
 * 功能概述：
 *   - 继电器模块控制（开关/翻转）
 *   - 无源蜂鸣器控制（PWM音调/单次鸣叫）
 *   - 直流电机速度控制（LEDC PWM调速）
 *   - SG90 舵机角度控制（0-180°）
 * 
 * 引脚分配（已验证无冲突）：
 *   RELAY_PIN  = GPIO45  (继电器模块)
 *   BUZZER_PIN = GPIO40  (无源蜂鸣器，原GPIO16已被MQ2_DO占用)
 *   MOTOR_PIN  = GPIO19  (直流电机驱动)
 *   SERVO_PIN  = GPIO20  (SG90舵机)
 * 
 * 使用示例：
 *   actuators_init();           // 初始化所有外设
 *   relay_on();                 // 开启继电器
 *   buzzer_beep(2000, 500);    // 播放2kHz蜂鸣500ms
 *   motor_run_percent(50);     // 电机50%速度运行
 *   servo_write_angle(90);     // 舵机转到90度
 * 
 * @author ESP32 Project Team
 * @version 4.0 (2026-04-16)
 * @note 依赖库: ESP32Servo.h
 */

#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include "public.h"

// ==================== 引脚定义（防冲突版本） ====================
    
#define RELAY_PIN     45      // 继电器模块（高电平触发）
#define BUZZER_PIN    40     // 无源蜂鸣器（LEDC PWM输出）
#define MOTOR_PIN     19      // 直流电机驱动（LEDC PWM调速）
#define SERVO_PIN     20      // SG90舵机（PWM信号输入）

// ==================== LEDC PWM 配置 ====================
#define BUZZER_LEDC_CHANNEL    2       // 蜂鸣器使用LEDC通道2
#define BUZZER_FREQ_DEFAULT    2000    // 默认频率 2kHz
#define BUZZER_RESOLUTION      8       // 8位分辨率（0-255）

#define MOTOR_LEDC_CHANNEL     3       // 电机使用LEDC通道3
#define MOTOR_FREQ_DEFAULT     5000    // 默认频率 5kHz（电机推荐高频）
#define MOTOR_RESOLUTION        8       // 8位分辨率（0-255）

// ==================== 初始化函数 ====================
/**
 * 初始化所有外设模块
 * 设置引脚模式、配置LEDC PWM通道、初始化舵机
 * 必须在 setup() 中调用，且在 startCameraServer() 之前
 */
void actuators_init();

// ==================== 继电器控制 ====================
/**
 * 开启继电器（高电平触发）
 */
void relay_on();

/**
 * 关闭继电器（低电平）
 */
void relay_off();

/**
 * 翻转继电器状态
 * 如果当前是ON则变为OFF，反之亦然
 */
void relay_toggle();

/**
 * 获取继电器当前状态
 * @return true=开启, false=关闭
 */
bool relay_get_state();

// ==================== 蜂鸣器控制 ====================
/**
 * 开启蜂鸣器（持续发声，默认2kHz）
 */
void buzzer_on();

/**
 * 开启蜂鸣器（指定频率）
 * @param freq 频率（Hz），范围 100-10000Hz
 */
void buzzer_on_with_freq(int freq);

/**
 * 关闭蜂鸣器（静音）
 */
void buzzer_off();

/**
 * 播放单次蜂鸣声（阻塞式，会阻塞 duration_ms 毫秒）
 * @param freq 频率（Hz），建议范围 500-8000Hz
 * @param duration_ms 持续时间（毫秒），建议范围 50-2000ms
 * @warning 此函数会阻塞当前任务，不建议在实时性要求高的场景使用
 */
void buzzer_beep(int freq, int duration_ms);

/**
 * 播放非阻塞蜂鸣声（需在 loop() 中配合 buzzer_update() 使用）
 * @param freq 频率（Hz）
 * @param duration_ms 持续时间（毫秒）
 */
void buzzer_beep_async(int freq, int duration_ms);

/**
 * 更新异步蜂鸣器状态（需在 loop() 中周期性调用）
 * 检查是否到达停止时间并自动静音
 */
void buzzer_update();

/**
 * 获取蜂鸣器当前状态
 * @return true=开启, false=关闭
 */
bool buzzer_get_state();

// ==================== 直流电机控制 ====================
/**
 * 立即停止电机（PWM占空比=0）
 */
void motor_stop();

/**
 * 以指定速度运行电机
 * @param speed 速度值（0-255），映射到PWM占空比
 */
void motor_run(int speed);

/**
 * 以百分比速度运行电机
 * @param percent 百分比（0-100），自动映射到0-255
 */
void motor_run_percent(int percent);

/**
 * 获取电机当前速度
 * @return 当前速度值（0-255）
 */
int motor_get_speed();

// ==================== 舵机控制 ====================
/**
 * 附加舵机对象到指定引脚（如果未附加）
 */
void servo_attach();

/**
 * 分离舵机对象（释放PWM资源）
 */
void servo_detach();

/**
 * 设置舵机目标角度
 * @param angle 角度（0-180度），会自动 constrain 到有效范围
 *               如果舵机未附加，会先自动 attach
 */
void servo_write_angle(int angle);

/**
 * 获取舵机当前角度
 * @return 当前角度值（0-180），如果未附加返回 -1
 */
int servo_get_angle();

/**
 * 检查舵机是否已附加
 * @return true=已附加, false=未附加
 */
bool servo_is_attached();

// ==================== 状态查询 ====================
/**
 * 获取所有外设的当前状态（JSON格式字符串）
 * 用于 HTTP API 响应
 * @return JSON 字符串，格式：{"relay":"ON","buzzer":"OFF","motor":0,"servo":90}
 */
String get_actuators_state_json();

#endif // ACTUATORS_H
