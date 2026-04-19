#ifndef _u8g2_oled_H
#define _u8g2_oled_H

#include "public.h"
#include <SPI.h>
#include <Wire.h>
#include "U8g2lib.h"

// OLED引脚定义
#define SCL_slave1   22
#define SDA_slave1   23

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_slave1;

void MyU8g2Oled_init();
void displayData(float temp, float humi, int smoke, int gas, int water);
void displayDetail(int gas, int water);

#endif
