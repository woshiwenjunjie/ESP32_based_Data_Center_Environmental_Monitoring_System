/**
 * ============================================================
 *  统一配置文件 - ESP32 从机项目
 * ============================================================
 *  集中管理所有配置项，避免配置分散
 *  
 *  ⚠️ 安全提示：使用前请将所有占位符替换为实际配置
 *  
 *  【使用说明】
 *  1. 本文件包含从机运行所需的所有配置参数
 *  2. 请根据下方说明逐项填写您的实际配置
 *  3. 修改完成后保存并重新编译上传
 *  
 *  【配置步骤】
 *  步骤1: 填写WiFi信息（必须）
 *    - 将 YOUR_WIFI_SSID 替换为您的WiFi名称
 *    - 将 YOUR_WIFI_PASSWORD 替换为您的WiFi密码
 *  
 *  步骤2: 填写主机IP地址（必须）
 *    - 将 192.168.x.xxx 替换为ESP32主机的实际IP地址
 *    - 主机IP可在主机串口输出或路由器管理页面查看
 *  
 *  步骤3: 配置OneNet平台（可选，用于云平台上报）
 *    - 访问 https://open.iot.10086.cn/ 注册OneNet账号
 *    - 创建产品并添加设备，获取三要素信息
 *    - 将 PRODUCT_ID、DEVICE_NAME、DEVICE_SECRET 替换为实际值
 *    - 详细步骤请参考 onenet_config.md 文档
 *  
 *  【参数说明】
 *  - SEND_INTERVAL: 数据发送间隔，默认5秒，可根据需要调整
 *  - SENSOR_READ_INTERVAL: 传感器读取间隔，默认2秒
 *  - DISPLAY_UPDATE_INTERVAL: OLED显示更新间隔，默认1.5秒
 *  - DISPLAY_SWITCH_INTERVAL: OLED显示模式切换间隔，默认5秒
 *  
 *  【注意事项】
 *  1. 字符串配置项请保留双引号
 *  2. 数字配置项不要加引号
 *  3. 修改后需要重新编译并上传代码到ESP32
 *  4. 请勿将包含真实密码的代码提交到公共仓库
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== WiFi 配置 ====================
// WiFi 网络配置 - 【必填】请替换为您的WiFi信息
#define WIFI_SSID      "YOUR_WIFI_SSID"      // WiFi名称 - 替换为您的WiFi名称
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"  // WiFi密码 - 替换为您的WiFi密码

// ==================== 主机通信配置 ====================
// 【必填】主机IP地址 - 替换为ESP32主机的实际IP地址
// 获取方法：主机启动后会在串口输出IP地址，或在路由器查看
#define HOST_IP        "192.168.x.xxx"       // 主机IP地址
#define TCP_PORT       8888                  // 主机TCP端口（默认8888，一般无需修改）

// ==================== MQTT 配置 ====================
// OneNet 平台配置 - 【可选】如需云平台上报功能，请填写以下信息
// 获取方法：登录OneNet平台 → 产品管理 → 设备详情
#define MQTT_SERVER    "mqtts.heclouds.com"  // MQTT服务器地址（默认，无需修改）
#define MQTT_PORT      1883                  // MQTT端口（默认，无需修改）
#define PRODUCT_ID     "YOUR_PRODUCT_ID"     // 产品ID - 从OneNet平台获取
#define DEVICE_NAME    "YOUR_DEVICE_NAME"    // 设备名称 - 从OneNet平台获取
#define DEVICE_SECRET  "YOUR_DEVICE_SECRET"  // 设备密钥 - 从OneNet平台获取

// ==================== 系统配置 ====================
// 数据发送间隔（毫秒）- 默认5秒，可根据需要调整
// 说明：控制向主机发送传感器数据的时间间隔
#define SEND_INTERVAL  5000                  // 发送数据间隔（毫秒）
#define RECONNECT_INTERVAL  8000            // 重连间隔（毫秒）

// ==================== 传感器配置 ====================
// 传感器读取间隔（毫秒）- 默认2秒
// 说明：控制读取传感器硬件的频率
#define SENSOR_READ_INTERVAL  2000          // 传感器读取间隔（毫秒）

// ==================== 显示配置 ====================
// OLED显示配置 - 控制OLED屏幕的刷新和切换
#define DISPLAY_UPDATE_INTERVAL  1500         // 显示更新间隔（毫秒）
#define DISPLAY_SWITCH_INTERVAL  5000        // 显示模式切换间隔（毫秒）

#endif // CONFIG_H
