/**
 * ============================================================
 *  Auto Control - 超限自动控制模块
 * ============================================================
 * 
 * 功能概述：
 *   - 根据传感器数据自动触发外设控制
 *   - 支持温度、湿度、气体浓度、水位等阈值设置
 *   - 可配置触发条件和执行动作
 *   - 支持联动控制（一个传感器触发多个外设）
 * 
 * 支持的传感器类型：
 *   - TEMPERATURE : 温度（°C）
 *   - HUMIDITY    : 湿度（%）
 *   - GAS         : 气体浓度（MQ-2）
 *   - SMOKE       : 烟雾状态（0/1）
 *   - WATER       : 水位（ADC值）
 * 
 * 支持的外设动作：
 *   - RELAY_ON/OFF/TOGGLE  : 继电器控制
 *   - BUZZER_ON/OFF/BEEP   : 蜂鸣器控制
 *   - MOTOR_RUN/STOP       : 电机控制
 *   - SERVO_WRITE          : 舵机角度
 * 
 * 触发条件：
 *   - GREATER_THAN  : 大于阈值
 *   - LESS_THAN     : 小于阈值
 *   - EQUAL_TO      : 等于阈值
 *   - IN_RANGE      : 在范围内 [min, max]
 *   - OUT_OF_RANGE  : 超出范围
 * 
 * 使用示例：
 *   // 初始化自动控制系统
 *   auto_control_init();
 * 
 *   // 添加规则：温度超过35°C时开启继电器
 *   AutoControlRule rule;
 *   rule.sensorType = SENSOR_TEMPERATURE;
 *   rule.condition = CONDITION_GREATER_THAN;
 *   rule.threshold1 = 35.0;
 *   rule.actionType = ACTION_RELAY_ON;
 *   add_auto_control_rule(rule);
 * 
 *   // 在主循环中检查并执行
 *   check_and_execute_auto_control();
 * 
 * @author ESP32 Project Team
 * @version 5.0 (2026-04-18)
 * @note 依赖: actuators.h, sensor_data.h
 */

#ifndef AUTO_CONTROL_H
#define AUTO_CONTROL_H

#include <Arduino.h>

// ==================== 传感器类型 ====================
typedef enum {
    SENSOR_TEMPERATURE = 0,  // 温度（°C）
    SENSOR_HUMIDITY,         // 湿度（%）
    SENSOR_GAS,              // 气体浓度（MQ-2原始值）
    SENSOR_SMOKE,            // 烟雾状态（0=无，1=有）
    SENSOR_WATER,            // 水位（ADC值）
    SENSOR_COUNT             // 传感器类型总数
} SensorType;

// ==================== 触发条件 ====================
typedef enum {
    CONDITION_GREATER_THAN = 0,  // 大于阈值
    CONDITION_LESS_THAN,         // 小于阈值
    CONDITION_EQUAL_TO,          // 等于阈值
    CONDITION_IN_RANGE,          // 在范围内 [threshold1, threshold2]
    CONDITION_OUT_OF_RANGE,      // 超出范围
    CONDITION_CHANGED,           // 数值变化（任意变化）
    CONDITION_COUNT              // 条件类型总数
} TriggerCondition;

// ==================== 动作类型 ====================
typedef enum {
    ACTION_RELAY_ON = 0,     // 继电器开启
    ACTION_RELAY_OFF,        // 继电器关闭
    ACTION_RELAY_TOGGLE,     // 继电器切换
    ACTION_BUZZER_ON,        // 蜂鸣器开启
    ACTION_BUZZER_OFF,       // 蜂鸣器关闭
    ACTION_BUZZER_BEEP,      // 蜂鸣器短鸣
    ACTION_MOTOR_RUN,        // 电机运行
    ACTION_MOTOR_STOP,       // 电机停止
    ACTION_SERVO_WRITE,      // 舵机设置角度
    ACTION_NONE,             // 无动作（禁用）
    ACTION_COUNT             // 动作类型总数
} ActionType;

// ==================== 规则结构体 ====================
#define RULE_NAME_MAX_LEN 4    // 规则名称最大长度
#define MAX_AUTO_RULES 3        // 最大规则数量

typedef struct {
    // 按大小排序，减少内存填充
    unsigned long lastTriggerTime;       // 上次触发时间（4字节）
    unsigned long lastActionTime;        // 上次执行动作时间（4字节）
    float threshold1;                    // 阈值1（4字节）
    float threshold2;                    // 阈值2（4字节）
    char name[RULE_NAME_MAX_LEN];        // 规则名称（16字节）
    uint16_t debounceMs;                 // 去抖动时间（2字节）
    uint16_t actionDurationMs;           // 动作持续时间（2字节）
    int16_t actionParam;                 // 动作参数（2字节）- 改为int16节省空间
    uint8_t id;                          // 规则ID（1字节）
    SensorType sensorType;               // 传感器类型（1字节）
    TriggerCondition condition;          // 触发条件（1字节）
    ActionType actionType;               // 动作类型（1字节）
    bool enabled;                        // 是否启用（1字节）
    bool lastTriggered;                  // 上次触发状态（1字节）
} AutoControlRule;

// ==================== 初始化函数 ====================
/**
 * 初始化自动控制系统
 * 加载默认规则，准备执行环境
 */
void auto_control_init();

// ==================== 规则管理 ====================
/**
 * 添加自动控制规则
 * @param rule 规则结构体
 * @return 规则ID（>=0成功，-1失败）
 */
int add_auto_control_rule(const AutoControlRule& rule);

/**
 * 更新自动控制规则
 * @param id 规则ID
 * @param rule 新的规则结构体
 * @return true=成功，false=失败（ID不存在）
 */
bool update_auto_control_rule(uint8_t id, const AutoControlRule& rule);

/**
 * 删除自动控制规则
 * @param id 规则ID
 * @return true=成功，false=失败
 */
bool delete_auto_control_rule(uint8_t id);

/**
 * 启用/禁用规则
 * @param id 规则ID
 * @param enabled true=启用，false=禁用
 * @return true=成功，false=失败
 */
bool set_rule_enabled(uint8_t id, bool enabled);

/**
 * 获取规则
 * @param id 规则ID
 * @return 规则指针（NULL表示不存在）
 */
AutoControlRule* get_auto_control_rule(uint8_t id);

/**
 * 获取所有规则
 * @param rules 规则数组（输出）
 * @param count 数组大小（输入/输出）
 */
void get_all_auto_control_rules(AutoControlRule* rules, int& count);

/**
 * 清除所有规则
 */
void clear_all_auto_control_rules();

// ==================== 执行控制 ====================
/**
 * 检查并执行自动控制
 * 在主循环中周期性调用（建议每1-2秒）
 */
void check_and_execute_auto_control();

/**
 * 手动触发规则（用于测试）
 * @param id 规则ID
 * @return true=成功触发，false=失败
 */
bool manual_trigger_rule(uint8_t id);

// ==================== 状态查询 ====================
/**
 * 获取自动控制系统状态（JSON格式）
 * @return JSON字符串
 */
String get_auto_control_status_json();

/**
 * 获取规则状态（JSON格式）
 * @param id 规则ID
 * @return JSON字符串
 */
String get_rule_status_json(uint8_t id);

// ==================== 预设规则 ====================
/**
 * 加载默认规则
 * 高温报警：温度>35°C → 蜂鸣器报警
 * 高湿通风：湿度>80% → 继电器开启（风扇）
 * 气体泄漏：气体>2500 → 蜂鸣器+继电器
 */
void load_default_auto_rules();

/**
 * 从JSON字符串加载规则
 * @param json JSON格式的规则数组
 * @return 成功加载的规则数量
 */
int load_rules_from_json(const char* json);

/**
 * 导出所有规则为JSON
 * @return JSON字符串
 */
String export_rules_to_json();

// ==================== 持久化存储 ====================
/**
 * 保存规则到SPIFFS文件系统
 * @return true=成功，false=失败
 */
bool save_rules_to_spiffs();

/**
 * 从SPIFFS加载规则
 * @return 成功加载的规则数量（-1表示文件不存在）
 */
int load_rules_from_spiffs();

/**
 * 检查SPIFFS中是否有保存的规则
 * @return true=有保存的规则
 */
bool has_saved_rules();

#endif // AUTO_CONTROL_H
