/**
 * ============================================================
 *  公共头文件（从机版）
 * ============================================================
 */

#ifndef _public_H
#define _public_H

#include "Arduino.h"

// ==================== 调试开关 ====================
#define DEBUG_ENABLE 1           // 调试输出（注释掉可禁用所有DEBUG输出）

#ifdef DEBUG_ENABLE
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
#endif

typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;

#endif
