#include "u8g2_oled.h"
#include "sensors.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_main(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL_main, /* data=*/ SDA_main);
// 初始化OLED显示
void MyU8g2Oled_init(){
  oled_main.setI2CAddress(0x3C << 1);
  oled_main.begin();
  
  oled_main.clearBuffer();
  oled_main.setFont(u8g2_font_ncenB10_tr);
  oled_main.drawStr(15, 30, "Host Ready");
  oled_main.sendBuffer();
  delay(1500);
}
// 显示远程数据
void displayRemote(float temp, float humi, int smoke, int gas, int water, bool online) {
  oled_main.clearBuffer();
  
  oled_main.setFont(u8g2_font_ncenB08_tr);
  
  if (!online) {
    // 离线状态显示
    oled_main.drawStr(20, 10, "Host Status");
    oled_main.drawLine(0, 12, 128, 12);
    
    oled_main.setCursor(0, 30);
    oled_main.print("Status: Offline");
    
    oled_main.setCursor(0, 45);
    oled_main.print("Waiting for");
    
    oled_main.setCursor(0, 58);
    oled_main.print("slave...");
    
    // 闪烁效果
    static bool blink = false;
    blink = !blink;
    if (blink) {
      oled_main.drawBox(110, 50, 15, 10);
    }
  } else {
    // 在线状态显示
    oled_main.drawStr(25, 10, "Remote Data");
    oled_main.drawLine(0, 12, 128, 12);
    
    // 温度显示
    oled_main.setCursor(0, 25);
    oled_main.print("🌡️ Temp: ");
    if (temp > -100) {
      oled_main.print(temp, 1);
      oled_main.print("°C");
    } else {
      oled_main.print("--.-°C");
    }
    
    // 湿度显示
    oled_main.setCursor(0, 38);
    oled_main.print("💧 Humi: ");
    if (humi > -100) {
      oled_main.print(humi, 1);
      oled_main.print("%");
    } else {
      oled_main.print("--.-%");
    }
    
    // 烟雾显示
    oled_main.setCursor(0, 51);
    oled_main.print("💨 Smoke:");
    if (smoke == LOW) {
      oled_main.print(" ALERT!");
      oled_main.drawBox(70, 42, 58, 12);
      oled_main.setDrawColor(0);
      oled_main.print(" ALERT!");
      oled_main.setDrawColor(1);
    } else {
      oled_main.print(" OK");
    }
    
    // 气体浓度显示
    oled_main.setCursor(0, 62);
    oled_main.print("🔥 Gas:");
    int gasPct = map(gas, 0, 4095, 0, 100);
    int barWidth = map(gas, 0, 4095, 0, 60);
    oled_main.drawFrame(35, 55, 62, 8);
    // 根据气体浓度设置颜色
    if (gasPct > 70) {
      oled_main.setDrawColor(1);
      oled_main.drawBox(36, 56, barWidth, 6);
    } else if (gasPct > 30) {
      oled_main.setDrawColor(1);
      oled_main.drawBox(36, 56, barWidth, 6);
    } else {
      oled_main.setDrawColor(1);
      oled_main.drawBox(36, 56, barWidth, 6);
    }
    oled_main.setCursor(100, 62);
    oled_main.print(gasPct);
    oled_main.print("%");
    
    // 水位显示（在第二行）
    oled_main.setCursor(64, 25);
    oled_main.print("💧 Water:");
    float waterCm = analogToMm(water);
    oled_main.print(waterCm, 1);
    oled_main.print("cm");
  }
  
  oled_main.sendBuffer();
}
