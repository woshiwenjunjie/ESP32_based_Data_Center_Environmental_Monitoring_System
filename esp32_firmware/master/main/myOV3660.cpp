/**
 * ============================================================
 *  OV3660 摄像头驱动 - 初始化与配置模块
 * ============================================================
 * 
 * 功能：
 *   - 配置OV3660摄像头引脚映射（ESP32-S3 CAM开发板）
 *   - 自动检测PSRAM并选择最优配置
 *   - 支持多种分辨率：QVGA(320x240) / VGA(640x480) / SVGA(800x600)
 *   - JPEG格式直出（减少CPU处理负担）
 * 
 * 分辨率策略：
 *   有PSRAM (8MB): SVGA/VGA + 高质量JPEG (quality=10)
 *   无PSRAM:      QVGA only + 中等质量 (quality=20) 防止内存溢出
 * 
 * 引脚定义来源: ESP32-S3-WROOM-1 CAM开发板原理图
 * 
 * ============================================================
 */

#include "myOV3660.h"
#include "public.h"

/**
 * myov3660_init() - 摄像头完整初始化流程
 * 
 * 初始化步骤：
 *   1. 设置LEDC PWM定时器（用于摄像头像素时钟）
 *   2. 配置D0-D7数据引脚、XCLK/PCLK/VSYNC/HREF时序引脚
 *   3. 配置SCCB（I2C兼容）控制引脚用于传感器寄存器配置
 *   4. 设置帧缓冲区位置（优先PSRAM，回退DRAM）
 *   5. 调用esp_camera_init()完成硬件初始化
 *   6. 应用OV3660特定参数（翻转/亮度/饱和度）
 */
void myov3660_init(){
  
  camera_config_t config;
  
  // ---- LEDC PWM配置（摄像头像素时钟源）----
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  
  // ---- 引脚定义（基于ESP32-S3 普中科技开发板）----
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;//20MHz时钟
  config.frame_size = FRAMESIZE_SVGA;//分辨率：SVGA(1280x720)
  config.pixel_format = PIXFORMAT_JPEG;//YUV422,GRAYSCALE,RGB565,JPEG
  config.grab_mode = CAMERA_GRAB_LATEST;  // 最新帧模式
  config.fb_location = CAMERA_FB_IN_PSRAM;  // 先尝试PSRAM
  config.jpeg_quality = 12;//0-63, 数值越低质量越低
  config.fb_count = 2;//大于1时启用连续模式，提高帧率
  
  // 检测PSRAM并配置
  bool has_psram = psramFound();
  DEBUG_PRINTF("[Camera] PSRAM detected: %s\n", has_psram ? "YES" : "NO");
  
  if (has_psram) {
    // 有PSRAM：使用更高配置
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.frame_size = FRAMESIZE_SVGA;  // 分辨率：SVGA(1280x720)
  } else {
    // 无PSRAM：严格限制使用内部RAM
    DEBUG_PRINTLN("[Camera] Warning: No PSRAM! Using DRAM with limited settings");
    config.frame_size = FRAMESIZE_QVGA;  // 必须限制为QVGA
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_count = 2;
    config.jpeg_quality = 20;  // 降低质量减少内存
  }

  // camera 初始化
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    DEBUG_PRINTF("Camera init failed with error 0x%x\n", err);
    // 如果失败且尝试的是PSRAM，回退到DRAM
    if (has_psram && config.fb_location == CAMERA_FB_IN_PSRAM) {
      DEBUG_PRINTLN("[Camera] Retrying with DRAM...");
      config.fb_location = CAMERA_FB_IN_DRAM;
      config.frame_size = FRAMESIZE_QVGA;
      config.fb_count = 2;
      err = esp_camera_init(&config);
      if (err != ESP_OK) {
        DEBUG_PRINTF("Camera retry failed: 0x%x\n", err);
        return;
      }
    } else {
      return;
    }
  }
  DEBUG_PRINTLN("摄像头初始化成功");
  // 获取传感器句柄
  sensor_t * s = esp_camera_sensor_get();
  // 应用OV3660特定设置
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
    DEBUG_PRINTLN("已应用OV3660 特定设置");
  }
  
  // 设置最终帧大小
  if (has_psram) {
    s->set_framesize(s, FRAMESIZE_VGA);//SVGA(1280x7240)
    s->set_quality(s, 10);//0-63, 数值越低质量越低
    DEBUG_PRINTLN("帧大小： SVGA (PSRAM)");
  } else {
    s->set_framesize(s, FRAMESIZE_QVGA);
    s->set_quality(s, 20);
    DEBUG_PRINTLN("帧大小： QVGA (DRAM)");  
  }
}
