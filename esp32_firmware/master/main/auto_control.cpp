/**
 * ============================================================
 *  Auto Control - 超限自动控制模块实现
 * ============================================================
 */

#include "auto_control.h"
#include "actuators.h"
#include "sensor_data.h"
#include <ArduinoJson.h>

// ==================== 全局变量 ====================
static AutoControlRule autoRules[MAX_AUTO_RULES];
static int ruleCount = 0;
static bool autoControlEnabled = true;

// ==================== 传感器名称映射 ====================
static const char* sensorTypeNames[] = {
    "temperature", "humidity", "gas", "smoke", "water"
};

static const char* conditionNames[] = {
    ">", "<", "=", "in_range", "out_of_range", "changed"
};

static const char* actionTypeNames[] = {
    "relay_on", "relay_off", "relay_toggle",
    "buzzer_on", "buzzer_off", "buzzer_beep",
    "motor_run", "motor_stop", "servo_write", "none"
};

// ==================== 辅助函数 ====================
/**
 * 获取当前传感器值
 */
static float get_sensor_value(SensorType type) {
    switch (type) {
        case SENSOR_TEMPERATURE:
            return remoteTemp;
        case SENSOR_HUMIDITY:
            return remoteHumi;
        case SENSOR_GAS:
            return (float)remoteGas;
        case SENSOR_SMOKE:
            return (float)remoteSmoke;
        case SENSOR_WATER:
            return (float)remoteWater;
        default:
            return 0.0f;
    }
}

/**
 * 检查条件是否满足
 */
static bool check_condition(float value, TriggerCondition condition, float threshold1, float threshold2) {
    switch (condition) {
        case CONDITION_GREATER_THAN:
            return value > threshold1;
        case CONDITION_LESS_THAN:
            return value < threshold1;
        case CONDITION_EQUAL_TO:
            return fabs(value - threshold1) < 0.001f;
        case CONDITION_IN_RANGE:
            return value >= threshold1 && value <= threshold2;
        case CONDITION_OUT_OF_RANGE:
            return value < threshold1 || value > threshold2;
        case CONDITION_CHANGED:
            return true; // 由调用者处理变化检测
        default:
            return false;
    }
}

/**
 * 执行动作
 */
static void execute_action(ActionType action, int param, uint16_t durationMs) {
    switch (action) {
        case ACTION_RELAY_ON:
            relay_on();
            break;
        case ACTION_RELAY_OFF:
            relay_off();
            break;
        case ACTION_RELAY_TOGGLE:
            relay_toggle();
            break;
        case ACTION_BUZZER_ON:
            if (param > 0) {
                buzzer_on_with_freq(param);
            } else {
                buzzer_on();
            }
            break;
        case ACTION_BUZZER_OFF:
            buzzer_off();
            break;
        case ACTION_BUZZER_BEEP:
            buzzer_beep_async(param > 0 ? param : 2000, durationMs > 0 ? durationMs : 500);
            break;
        case ACTION_MOTOR_RUN:
            motor_run(param > 0 ? param : 128);
            break;
        case ACTION_MOTOR_STOP:
            motor_stop();
            break;
        case ACTION_SERVO_WRITE:
            servo_write_angle(param);
            break;
        case ACTION_NONE:
        default:
            break;
    }
}

/**
 * 获取动作对应的设备名称
 */
static const char* get_action_device_name(ActionType action) {
    switch (action) {
        case ACTION_RELAY_ON:
        case ACTION_RELAY_OFF:
        case ACTION_RELAY_TOGGLE:
            return "relay";
        case ACTION_BUZZER_ON:
        case ACTION_BUZZER_OFF:
        case ACTION_BUZZER_BEEP:
            return "buzzer";
        case ACTION_MOTOR_RUN:
        case ACTION_MOTOR_STOP:
            return "motor";
        case ACTION_SERVO_WRITE:
            return "servo";
        default:
            return "none";
    }
}

// ==================== 初始化函数 ====================
void auto_control_init() {
    // 清空所有规则
    memset(autoRules, 0, sizeof(autoRules));
    ruleCount = 0;
    autoControlEnabled = true;
    
    // 尝试从SPIFFS加载保存的规则
    if (has_saved_rules()) {
        int loaded = load_rules_from_spiffs();
        if (loaded > 0) {
            Serial.printf(F("[AutoControl] 已从SPIFFS加载 %d 条规则\n"), loaded);
        } else {
            Serial.println(F("[AutoControl] SPIFFS规则加载失败，使用默认规则"));
            load_default_auto_rules();
        }
    } else {
        Serial.println(F("[AutoControl] 无保存的规则，加载默认规则"));
        load_default_auto_rules();
    }
    
    Serial.println(F("[AutoControl] 自动控制系统初始化完成"));
    Serial.printf(F("[AutoControl] 最大规则数: %d\n"), MAX_AUTO_RULES);
}

// ==================== 规则管理 ====================
int add_auto_control_rule(const AutoControlRule& rule) {
    if (ruleCount >= MAX_AUTO_RULES) {
        Serial.println(F("[AutoControl] 错误：规则数量已达上限"));
        return -1;
    }
    
    // 查找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == 0 && strlen(autoRules[i].name) == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        slot = ruleCount;
    }
    
    // 复制规则
    memcpy(&autoRules[slot], &rule, sizeof(AutoControlRule));
    
    // 分配ID（如果没有指定）
    if (autoRules[slot].id == 0) {
        autoRules[slot].id = slot + 1;
    }
    
    // 初始化内部状态
    autoRules[slot].lastTriggered = false;
    autoRules[slot].lastTriggerTime = 0;
    autoRules[slot].lastActionTime = 0;
    
    ruleCount++;
    
    Serial.printf(F("[AutoControl] 规则已添加: ID=%d, 名称=%s\n"), 
                  autoRules[slot].id, autoRules[slot].name);
    
    return autoRules[slot].id;
}

bool update_auto_control_rule(uint8_t id, const AutoControlRule& rule) {
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == id) {
            // 保留内部状态
            bool lastTriggered = autoRules[i].lastTriggered;
            unsigned long lastTriggerTime = autoRules[i].lastTriggerTime;
            unsigned long lastActionTime = autoRules[i].lastActionTime;
            
            memcpy(&autoRules[i], &rule, sizeof(AutoControlRule));
            autoRules[i].id = id;
            autoRules[i].lastTriggered = lastTriggered;
            autoRules[i].lastTriggerTime = lastTriggerTime;
            autoRules[i].lastActionTime = lastActionTime;
            
            Serial.printf(F("[AutoControl] 规则已更新: ID=%d\n"), id);
            return true;
        }
    }
    
    Serial.printf(F("[AutoControl] 错误：规则不存在 ID=%d\n"), id);
    return false;
}

bool delete_auto_control_rule(uint8_t id) {
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == id) {
            memset(&autoRules[i], 0, sizeof(AutoControlRule));
            ruleCount--;
            return true;
        }
    }
    return false;
}

bool set_rule_enabled(uint8_t id, bool enabled) {
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == id) {
            autoRules[i].enabled = enabled;
            return true;
        }
    }
    return false;
}

AutoControlRule* get_auto_control_rule(uint8_t id) {
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == id) {
            return &autoRules[i];
        }
    }
    
    return NULL;
}

void get_all_auto_control_rules(AutoControlRule* rules, int& count) {
    count = 0;
    for (int i = 0; i < MAX_AUTO_RULES && count < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id != 0) {
            memcpy(&rules[count], &autoRules[i], sizeof(AutoControlRule));
            count++;
        }
    }
}

void clear_all_auto_control_rules() {
    memset(autoRules, 0, sizeof(autoRules));
    ruleCount = 0;
}

// ==================== 执行控制 ====================
void check_and_execute_auto_control() {
    if (!autoControlEnabled) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        AutoControlRule& rule = autoRules[i];
        
        // 跳过无效和禁用的规则
        if (rule.id == 0 || !rule.enabled) {
            continue;
        }
        
        // 获取传感器值
        float sensorValue = get_sensor_value(rule.sensorType);
        
        // 检查条件
        bool conditionMet = check_condition(sensorValue, rule.condition, 
                                            rule.threshold1, rule.threshold2);
        
        // 去抖动处理
        if (conditionMet && !rule.lastTriggered) {
            if (currentTime - rule.lastTriggerTime >= rule.debounceMs) {
                // 条件满足且通过去抖动
                rule.lastTriggered = true;
                rule.lastTriggerTime = currentTime;
                rule.lastActionTime = currentTime;
                
                // 执行动作
                execute_action(rule.actionType, rule.actionParam, rule.actionDurationMs);
                
                }
        } else if (!conditionMet && rule.lastTriggered) {
            rule.lastTriggered = false;
            if (rule.actionDurationMs > 0) {
                switch (rule.actionType) {
                    case ACTION_RELAY_ON:
                        relay_off();
                        break;
                    case ACTION_BUZZER_ON:
                        buzzer_off();
                        break;
                    case ACTION_MOTOR_RUN:
                        motor_stop();
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

bool manual_trigger_rule(uint8_t id) {
    AutoControlRule* rule = get_auto_control_rule(id);
    if (rule == NULL || !rule->enabled) {
        return false;
    }
    execute_action(rule->actionType, rule->actionParam, rule->actionDurationMs);
    return true;
}

// ==================== 状态查询 ====================
String get_auto_control_status_json() {
    StaticJsonDocument<512> doc;
    
    doc["enabled"] = autoControlEnabled;
    doc["rule_count"] = ruleCount;
    doc["max_rules"] = MAX_AUTO_RULES;
    
    JsonArray rulesArray = doc.createNestedArray("rules");
    
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == 0) continue;
        
        JsonObject ruleObj = rulesArray.createNestedObject();
        ruleObj["id"] = autoRules[i].id;
        ruleObj["name"] = autoRules[i].name;
        ruleObj["enabled"] = autoRules[i].enabled;
        ruleObj["sensor"] = sensorTypeNames[autoRules[i].sensorType];
        ruleObj["condition"] = conditionNames[autoRules[i].condition];
        ruleObj["threshold1"] = autoRules[i].threshold1;
        ruleObj["threshold2"] = autoRules[i].threshold2;
        ruleObj["action"] = actionTypeNames[autoRules[i].actionType];
        ruleObj["action_param"] = autoRules[i].actionParam;
        ruleObj["triggered"] = autoRules[i].lastTriggered;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}

String get_rule_status_json(uint8_t id) {
    AutoControlRule* rule = get_auto_control_rule(id);
    if (rule == NULL) {
        return "{}";
    }
    // 构建JSON文档
    StaticJsonDocument<256> doc;
    
    doc["id"] = rule->id;
    doc["name"] = rule->name;
    doc["enabled"] = rule->enabled;
    doc["sensor"] = sensorTypeNames[rule->sensorType];
    doc["sensor_value"] = get_sensor_value(rule->sensorType);
    doc["condition"] = conditionNames[rule->condition];
    doc["threshold1"] = rule->threshold1;
    doc["threshold2"] = rule->threshold2;
    doc["action"] = actionTypeNames[rule->actionType];
    doc["action_param"] = rule->actionParam;
    doc["triggered"] = rule->lastTriggered;
    doc["last_trigger_time"] = rule->lastTriggerTime;
    
    String result;
    serializeJson(doc, result);
    return result;
}

// ==================== 预设规则 ====================
void load_default_auto_rules() {
    clear_all_auto_control_rules();
    
    AutoControlRule rule1;
    memset(&rule1, 0, sizeof(rule1));
    strncpy(rule1.name, "高温", RULE_NAME_MAX_LEN - 1);
    rule1.enabled = true;
    rule1.sensorType = SENSOR_TEMPERATURE;
    rule1.condition = CONDITION_GREATER_THAN;
    rule1.threshold1 = 35.0f;
    rule1.debounceMs = 3000;
    rule1.actionType = ACTION_BUZZER_BEEP;
    rule1.actionParam = 2000;
    rule1.actionDurationMs = 1000;
    add_auto_control_rule(rule1);
    
    AutoControlRule rule2;
    memset(&rule2, 0, sizeof(rule2));
    strncpy(rule2.name, "气体", RULE_NAME_MAX_LEN - 1);
    rule2.enabled = true;
    rule2.sensorType = SENSOR_GAS;
    rule2.condition = CONDITION_GREATER_THAN;
    rule2.threshold1 = 2500.0f;
    rule2.debounceMs = 1000;
    rule2.actionType = ACTION_BUZZER_ON;
    rule2.actionParam = 3000;
    rule2.actionDurationMs = 0;
    add_auto_control_rule(rule2);
}

int load_rules_from_json(const char* json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        return 0;
    }
    
    JsonArray rulesArray = doc.as<JsonArray>();
    if (rulesArray.isNull()) {
        return 0;
    }
    
    int loadedCount = 0;
    
    for (JsonObject ruleObj : rulesArray) {
        if (loadedCount >= MAX_AUTO_RULES) break;
        
        AutoControlRule rule;
        memset(&rule, 0, sizeof(rule));
        
        rule.id = ruleObj["id"] | 0;
        strncpy(rule.name, ruleObj["name"] | "rule", RULE_NAME_MAX_LEN - 1);
        rule.enabled = ruleObj["enabled"] | true;
        
        const char* sensorStr = ruleObj["sensor"];
        if (sensorStr) {
            for (int i = 0; i < SENSOR_COUNT; i++) {
                if (strcmp(sensorStr, sensorTypeNames[i]) == 0) {
                    rule.sensorType = (SensorType)i;
                    break;
                }
            }
        }
        
        const char* conditionStr = ruleObj["condition"];
        if (conditionStr) {
            for (int i = 0; i < CONDITION_COUNT; i++) {
                if (strcmp(conditionStr, conditionNames[i]) == 0) {
                    rule.condition = (TriggerCondition)i;
                    break;
                }
            }
        }
        
        rule.threshold1 = ruleObj["threshold1"] | 0.0f;
        rule.threshold2 = ruleObj["threshold2"] | 0.0f;
        rule.debounceMs = ruleObj["debounce_ms"] | 1000;
        
        const char* actionStr = ruleObj["action"];
        if (actionStr) {
            for (int i = 0; i < ACTION_COUNT; i++) {
                if (strcmp(actionStr, actionTypeNames[i]) == 0) {
                    rule.actionType = (ActionType)i;
                    break;
                }
            }
        }
        
        rule.actionParam = ruleObj["action_param"] | 0;
        rule.actionDurationMs = ruleObj["duration_ms"] | 0;
        
        if (add_auto_control_rule(rule) >= 0) {
            loadedCount++;
        }
    }
    return loadedCount;
}

String export_rules_to_json() {
    StaticJsonDocument<1024> doc;
    JsonArray rulesArray = doc.to<JsonArray>();
    
    for (int i = 0; i < MAX_AUTO_RULES; i++) {
        if (autoRules[i].id == 0) continue;
        
        JsonObject ruleObj = rulesArray.createNestedObject();
        ruleObj["id"] = autoRules[i].id;
        ruleObj["name"] = autoRules[i].name;
        ruleObj["enabled"] = autoRules[i].enabled;
        ruleObj["sensor"] = sensorTypeNames[autoRules[i].sensorType];
        ruleObj["condition"] = conditionNames[autoRules[i].condition];
        ruleObj["threshold1"] = autoRules[i].threshold1;
        ruleObj["threshold2"] = autoRules[i].threshold2;
        ruleObj["debounce_ms"] = autoRules[i].debounceMs;
        ruleObj["action"] = actionTypeNames[autoRules[i].actionType];
        ruleObj["action_param"] = autoRules[i].actionParam;
        ruleObj["duration_ms"] = autoRules[i].actionDurationMs;
    }
    
    String result;
    serializeJson(rulesArray, result);
    return result;
}

// ==================== 持久化存储实现 ====================
#include <SPIFFS.h>
#define RULES_FILE "/auto_rules.json"
bool save_rules_to_spiffs() {
    if (!SPIFFS.begin(true)) return false;
    
    String json = export_rules_to_json();
    File file = SPIFFS.open(RULES_FILE, FILE_WRITE);
    if (!file) return false;
    
    size_t written = file.print(json);
    file.close();
    return written == json.length();
}

int load_rules_from_spiffs() {
    if (!SPIFFS.begin(true)) return -1;
    if (!SPIFFS.exists(RULES_FILE)) return -1;
    
    File file = SPIFFS.open(RULES_FILE, FILE_READ);
    if (!file) return -1;
    
    String json = file.readString();
    file.close();
    
    clear_all_auto_control_rules();
    return load_rules_from_json(json.c_str());
}

bool has_saved_rules() {
    if (!SPIFFS.begin(true)) return false;
    return SPIFFS.exists(RULES_FILE);
}
