#include "u8g2_oled.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_slave1(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL_slave1, /* data=*/ SDA_slave1);

void MyU8g2Oled_init(){
  oled_slave1.setI2CAddress(0x3C << 1);
  oled_slave1.begin();
  
  oled_slave1.clearBuffer();
  oled_slave1.setFont(u8g2_font_ncenB10_tr);
  oled_slave1.drawStr(20, 30, "System");
  oled_slave1.drawStr(30, 50, "Ready");
  oled_slave1.sendBuffer();
  delay(500);  // 减少延迟时间
}

void displayData(float temp, float humi, int smoke, int gas, int water) {
  oled_slave1.clearBuffer();
  
  oled_slave1.setFont(u8g2_font_ncenB08_tr);
  oled_slave1.drawStr(30, 10, "Sensor Data");
  oled_slave1.drawLine(0, 12, 128, 12);
  
  oled_slave1.setCursor(0, 25);
  oled_slave1.print("Temp: ");
  if (temp > -100) {
    oled_slave1.print(temp, 1);
    oled_slave1.print(" C");
  } else {
    oled_slave1.print("Error");
  }
  
  oled_slave1.setCursor(0, 38);
  oled_slave1.print("Humi: ");
  if (humi > -100) {
    oled_slave1.print(humi, 1);
    oled_slave1.print(" %");
  } else {
    oled_slave1.print("Error");
  }
  
  oled_slave1.setCursor(0, 51);
  oled_slave1.print("Smoke:");
  if (smoke == LOW) {
    oled_slave1.print("ALERT!");
    oled_slave1.drawBox(70, 42, 58, 12);
    oled_slave1.setDrawColor(0);
    oled_slave1.print("ALERT!");
    oled_slave1.setDrawColor(1);
  } else {
    oled_slave1.print(" OK");
  }
  
  oled_slave1.setCursor(0, 62);
  oled_slave1.print("Gas:");
  int barWidth = map(gas, 0, 4095, 0, 60);
  oled_slave1.drawFrame(35, 55, 62, 8);
  oled_slave1.drawBox(36, 56, barWidth, 6);
  
  oled_slave1.sendBuffer();
}

void displayDetail(int gas, int water) {
  oled_slave1.clearBuffer();
  
  oled_slave1.setFont(u8g2_font_ncenB10_tr);
  oled_slave1.drawStr(10, 15, "Detail View");
  
  oled_slave1.setFont(u8g2_font_ncenB08_tr);
  
  // 气体浓度（带进度条）
  oled_slave1.setCursor(0, 30);
  oled_slave1.print("Gas:");
  int gasPct = map(gas, 0, 4095, 0, 100);
  int gasBarWidth = map(gas, 0, 4095, 0, 60);
  oled_slave1.drawFrame(35, 23, 62, 8);
  oled_slave1.drawBox(36, 24, gasBarWidth, 6);
  oled_slave1.setCursor(100, 30);
  oled_slave1.print(gasPct);
  oled_slave1.print("%");
  
  // 水位（带进度条）
  oled_slave1.setCursor(0, 45);
  oled_slave1.print("Water:");
  int waterPct = map(water, 0, 4095, 0, 100);
  int waterBarWidth = map(water, 0, 4095, 0, 60);
  oled_slave1.drawFrame(35, 38, 62, 8);
  oled_slave1.drawBox(36, 39, waterBarWidth, 6);
  oled_slave1.setCursor(100, 45);
  oled_slave1.print(waterPct);
  oled_slave1.print("%");
  
  // ADC原始值
  oled_slave1.setCursor(0, 58);
  oled_slave1.print("ADC: G=");
  oled_slave1.print(gas);
  oled_slave1.print(" W=");
  oled_slave1.print(water);
  
  oled_slave1.sendBuffer();
}
