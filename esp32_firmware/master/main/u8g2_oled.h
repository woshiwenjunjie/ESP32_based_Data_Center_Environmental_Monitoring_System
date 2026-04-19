#ifndef _u8g2_oled_H
#define _u8g2_oled_H

#include "public.h"
#include <SPI.h>
#include <Wire.h>
#include "U8g2lib.h"

// OLED引脚定义（主机）
#define SCL_main  21
#define SDA_main  47

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_main;

void MyU8g2Oled_init();
void displayRemote(float temp, float humi, int smoke, int gas, int water, bool online);

#endif
