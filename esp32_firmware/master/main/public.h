#ifndef _public_H
#define _public_H

#include "Arduino.h"

// ==================== 编译器优化（减少程序大小 10-15KB）====================
#pragma GCC optimize ("Os")           // 优化代码大小（而非速度）
#pragma GCC optimize ("-fdata-sections") // 数据分段，便于链接器删除未使用数据
#pragma GCC optimize ("-ffunction-sections") // 函数分段，便于删除未使用函数
#pragma GCC option (arch = "xtensa-esp32s3") // 针对ESP32-S3架构优化

// ==================== 功能开关（禁用可节省空间）====================
// #define DEBUG_ENABLE 1           // 调试输出（~20KB）
// #define ENABLE_WEBSOCKET 1       // WebSocket支持（~8KB）
// #define ENABLE_BURST_CAPTURE 1   // 批量拍照功能（~5KB）
// #define ENABLE_LOGS 1            // 系统日志功能（~3KB）
// #define ENABLE_AI_CONFIG 1       // AI配置功能（~10KB）
#define ENABLE_AUTO_CONTROL 1    // 自动控制功能（~15KB）
// #define ENABLE_YOLO_DETECT 1     // YOLO目标检测（~3KB）
// #define ENABLE_LOCAL_ANALYSIS 1  // 本地图像分析（~2KB）

#ifdef DEBUG_ENABLE
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
#endif

// 类型重定义
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#endif
