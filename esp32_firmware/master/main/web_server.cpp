/**
 * ============================================================
 *  ESP32-CAM 智能环境监测站 - Web服务器核心模块 * ============================================================
 * 
 * 功能概述：
 *   - MJPEG 视频流服务（支持多客户端并发）
 *   - JPEG 拍照捕获（支持分辨率动态切换）
 *   - AI 视觉识别（对OpenAI / 通义千问 / Kimi / DeepSeek / 豆包等）
 *   - 传感器数据采集与 JSON API
 *   - 系统状态监控（内存/PSRAM/CPU/WiFi状态）
 *   - 配置持久化存储 *         
 * 架构设计：
 *   - 被动帧缓存架构：视频流持续缓存最新帧，拍照后AI识别复用缓存帧，避免摄像头冲突
 *   - PSRAM优先内存管理：大缓冲区自动使用PSRAM，小缓冲区使用堆内存
 *   - Mutex互斥锁保护共享资源，确保线程安全
 * 
 * API 接口列表：
 *   GET  /              -> 主页HTML
 *   GET  /stream        -> MJPEG视频流
 *   GET  /capture       -> JPEG照片                
 *   POST /ask           -> AI视觉识别
 *   GET  /sensors       -> 传感器数据JSON
 *   GET  /status        -> 系统状态JSON
 *   POST /config        -> 配置更新
 *   OPTIONS *           -> CORS预检
 * 
 * 作者： 现代交换技术实验项目组
 * 版本: v3.0 (PSRAM优化版)
 * ============================================================
 */

#include "web_server.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "sensors.h"
#include <HTTPClient.h>
#include "actuators.h"
#include "public.h"
#ifdef ENABLE_AUTO_CONTROL
#include "auto_control.h"
#endif

#include <atomic>
#include "freertos/semphr.h"
#include "mbedtls/base64.h"

/* ==================== 全局变量与资源管理 ==================== */

// MJPEG流边界定义（用于multipart响应格式）
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// HTTP服务器句柄
httpd_handle_t server = NULL;

// 并发客户端计数（原子操作，线程安全）
static std::atomic<int> stream_client_count(0);
static std::atomic<int> ws_client_count(0);
#define MAX_STREAM_CLIENTS 2  // 最大视频流并发客户端数

// ==================== 摄像头资源管理：被动帧缓存 + 互斥保护 ====================
// 设计原理：
//   传统方式：拍照时需要锁定摄像头 -> 视频流中断
//   被动缓存：视频流持续将最新JPEG帧写入共享缓冲区 -> 拍照直接读取缓冲区，无需锁摄像头
static SemaphoreHandle_t camera_mutex = NULL;
static SemaphoreHandle_t frame_buf_mutex = NULL;
static uint8_t *shared_frame_buf = NULL;
static size_t shared_frame_len = 0;
static int shared_frame_width = 0;
static int shared_frame_height = 0;
static int64_t shared_frame_time = 0;
#define SHARED_FRAME_MAX_SIZE (300 * 1024)
#define MIN_FREE_HEAP_THRESHOLD (30 * 1024)

// ==================== 默认摄像头配置 ====================
framesize_t default_framesize = FRAMESIZE_VGA;
int default_quality = 10;

// ==================== PSRAM智能内存管理系统 ====================
// ESP32-S3拥有8MB PSRAM，用于存储大图像缓冲区
// 策略：>4KB的分配优先使用PSRAM，小分配使用内部SRAM（更快）
static void* smart_malloc(size_t size) {
    void *ptr = NULL;
    if (psramFound() && size > 4096) {
        ptr = ps_malloc(size);
        if (ptr) return ptr;
    }
    if (ESP.getFreeHeap() > size + MIN_FREE_HEAP_THRESHOLD) {
        ptr = malloc(size);
    }
    return ptr;
}

static void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

static bool check_memory_available(size_t needed) {
    size_t free_heap = ESP.getFreeHeap();
    size_t free_psram = psramFound() ? ESP.getFreePsram() : 0;

    if (needed < 4096) {
        return free_heap > needed + MIN_FREE_HEAP_THRESHOLD;
    }

    if (free_psram > needed + 1024) return true;
    if (free_heap > needed + MIN_FREE_HEAP_THRESHOLD) return true;

    DEBUG_PRINTF("[Memory] Not enough! Need: %u, Heap: %u, PSRAM: %u\n",
                  needed, free_heap, free_psram);
    return false;
}

static void cleanup_shared_frame() {
    if (frame_buf_mutex && xSemaphoreTake(frame_buf_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        safe_free((void**)&shared_frame_buf);
        shared_frame_len = 0;
        xSemaphoreGive(frame_buf_mutex);
    }
}

// ==================== 统一帧缓冲区管理系统 ====================
// 解决内存泄漏问题：使用RAII风格的帧包装器
typedef struct {
    camera_fb_t* fb;
    uint8_t* jpg_buf;
    size_t jpg_len;
    bool is_shared_copy;
    bool is_jpeg_converted;
} FrameWrapper;

/**
 * 初始化FrameWrapper（必须在使用前调用）
 */
static void frame_wrapper_init(FrameWrapper* wrapper) {
    if (!wrapper) return;
    
    wrapper->fb = NULL;
    wrapper->jpg_buf = NULL;
    wrapper->jpg_len = 0;
    wrapper->is_shared_copy = false;
    wrapper->is_jpeg_converted = false;
}

/**
 * 安全释放FrameWrapper中的所有资�? * 统一清理，避免内存泄�? */
static void frame_wrapper_cleanup(FrameWrapper* wrapper) {
    if (!wrapper) return;
    
    // 释放JPEG缓冲区（如果是动态分配的）
    if (wrapper->jpg_buf && wrapper->is_jpeg_converted) {
        free(wrapper->jpg_buf);
        wrapper->jpg_buf = NULL;
    }
    
    // 释放帧缓冲区
    if (wrapper->fb) {
        if (wrapper->is_shared_copy) {
            // 共享帧的副本需要手动释放buf和结构体
            if (wrapper->fb->buf) {
                free(wrapper->fb->buf);
                wrapper->fb->buf = NULL;
            }
            free(wrapper->fb);
        } else {
            // 正常的摄像头帧，使用官方释放函数
            esp_camera_fb_return(wrapper->fb);
        }
        wrapper->fb = NULL;
    }
    
    // 重置状态
    wrapper->jpg_len = 0;
    wrapper->is_shared_copy = false;
    wrapper->is_jpeg_converted = false;
}

/**
 * 从共享帧创建FrameWrapper副本
 * 返回true表示成功，false表示失败
 */
static bool frame_wrapper_from_shared(FrameWrapper* wrapper) {
    if (!wrapper) return false;
    
    frame_wrapper_init(wrapper);
    
    if (!frame_buf_mutex || xSemaphoreTake(frame_buf_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    // 检查共享帧是否有效
    if (!shared_frame_buf || shared_frame_len == 0 ||
        (esp_timer_get_time() - shared_frame_time) > 2000000) {  // 2秒过�?        xSemaphoreGive(frame_buf_mutex);
        return false;
    }
    
    // 创建帧副本
    wrapper->fb = (camera_fb_t*)calloc(1, sizeof(camera_fb_t));
    if (!wrapper->fb) {
        xSemaphoreGive(frame_buf_mutex);
        return false;
    }
    
    // 分配并复制图像数据
    wrapper->fb->buf = (uint8_t*)smart_malloc(shared_frame_len);
    if (!wrapper->fb->buf) {
        free(wrapper->fb);
        wrapper->fb = NULL;
        xSemaphoreGive(frame_buf_mutex);
        return false;
    }
    
    memcpy(wrapper->fb->buf, shared_frame_buf, shared_frame_len);
    wrapper->fb->len = shared_frame_len;
    wrapper->fb->width = shared_frame_width;
    wrapper->fb->height = shared_frame_height;
    wrapper->fb->format = PIXFORMAT_JPEG;
    
    wrapper->is_shared_copy = true;
    
    xSemaphoreGive(frame_buf_mutex);
    
    DEBUG_PRINTF("[Frame] Shared copy: %dx%d, %u bytes\n",
                  wrapper->fb->width, wrapper->fb->height, wrapper->fb->len);
    
    return true;
}

/**
 * 捕获新帧到FrameWrapper
 */
static bool frame_wrapper_capture(FrameWrapper* wrapper, int skip_count = 1) {
    if (!wrapper) return false;
    
    frame_wrapper_init(wrapper);
    
    // 跳过指定数量的旧帧
    for (int i = 0; i < skip_count; i++) {
        camera_fb_t* temp_fb = esp_camera_fb_get();
        if (temp_fb) {
            esp_camera_fb_return(temp_fb);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    // 捕获新帧
    wrapper->fb = esp_camera_fb_get();
    return (wrapper->fb != NULL);
}

// ==================== AI视觉识别服务配置 ====================
// 支持的AI服务商（OpenAI兼容API格式）：
//   - OpenAI:  GPT-4o / GPT-4o-Mini / GPT-3.5-Turbo
//   - 阿里云:  通义千问 Qwen-Turbo/Plus/Max (dashscope.aliyuncs.com)
//   - 月之暗面: Kimi (api.moonshot.cn)
//   - DeepSeek: Chat / Reasoner (api.deepseek.com)
//   - 火山引擎: 豆包 Pro/Lite (ark.cn-beijing.volces.com)
static String ai_service_url = "";
static String ai_api_key = "";
static int ai_timeout_ms = 30000;
static String ai_model = "gpt-4o-mini";

// ==================== AI 识别模式配置 ====================
// 支持三种模式: "llm"=云端大模型(OpenAI/千问/Kimi) | "yolo"=YOLO目标检测 | "local"=本地简单识别
static String ai_mode = "llm";
// YOLO服务地址 (可选，用于YOLO目标检测模式)
#ifdef ENABLE_YOLO_DETECT
static String yolo_service_url = "";
#endif

// 系统日志缓冲区
#ifdef ENABLE_LOGS
#define LOG_BUFFER_SIZE 2048
static char log_buffer[LOG_BUFFER_SIZE];
static int log_write_pos = 0;
static SemaphoreHandle_t log_mutex = NULL;
#endif

// ==================== PROGMEM 常量（节省RAM，存入Flash）====================
const char ERR_CAMERA_BUSY[] PROGMEM = "{\"error\":\"Camera busy\"}";
const char ERR_CAMERA_CAPTURE_FAIL[] PROGMEM = "{\"error\":\"Camera capture failed\"}";
const char ERR_CAMERA_BUSY_RETRY[] PROGMEM = "{\"error\":\"Camera busy, please retry\"}";
const char ERR_CAMERA_BUSY_STOP[] PROGMEM = "{\"error\":\"Camera busy, stop stream first\"}";
const char ERR_READ_BODY_FAIL[] PROGMEM = "{\"error\":\"Failed to read body\"}";
const char ERR_INVALID_JSON[] PROGMEM = "{\"error\":\"Invalid JSON\"}";
const char ERR_METHOD_NOT_ALLOWED[] PROGMEM = "{\"error\":\"Method not allowed\"}";
const char ERR_AI_NOT_CONFIGURED[] PROGMEM = "{\"error\":\"AI service not configured\"}";

typedef struct {
    framesize_t framesize;
    int quality;
    int64_t capture_time_ms;
    size_t image_size;
    int width;
    int height;
} capture_stats_t;

static capture_stats_t last_capture_stats = {FRAMESIZE_VGA, 10, 0, 0, 0, 0};

// 日志记录函数
#ifdef ENABLE_LOGS
static void web_log(const char* format, ...) {
    if (!log_mutex) return;
    
    if (xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        char temp[256];
        va_list args;
        va_start(args, format);
        vsnprintf(temp, sizeof(temp), format, args);
        va_end(args);
        
        // 添加时间戳
        char log_entry[300];
        snprintf(log_entry, sizeof(log_entry), "[%lld] %s\n", esp_timer_get_time() / 1000000, temp);
        
        int len = strlen(log_entry);
        if (log_write_pos + len >= LOG_BUFFER_SIZE) {
            log_write_pos = 0; // 循环覆盖
        }
        
        memcpy(log_buffer + log_write_pos, log_entry, len);
        log_write_pos += len;
        if (log_write_pos < LOG_BUFFER_SIZE) {
            log_buffer[log_write_pos] = '\0';
        }
        
        xSemaphoreGive(log_mutex);
    }
}
#else
#define web_log(format, ...) ((void)0)
#endif

#ifdef ENABLE_WEBSOCKET
static esp_err_t ws_handler(httpd_req_t *req) {
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        DEBUG_PRINTF("[WS] Rx fail: %s\n", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len) {
        buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            DEBUG_PRINTLN("[WS] Alloc fail");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            DEBUG_PRINTF("[WS] Data fail: %s\n", esp_err_to_name(ret));
            free(buf);
            return ret;
        }
        buf[ws_pkt.len] = '\0';
    }

    String msg = String((char*)buf);
    DEBUG_PRINTF("[WS] Msg: %s\n", msg.c_str());

    String response = "";
    if (msg == "capture") {
        response = "{\"type\":\"ack\",\"command\":\"capture\"}";
    } else if (msg == "status") {
        char status[256];
        snprintf(status, sizeof(status),
            "{\"type\":\"status\",\"heap\":%u,\"clients\":%d,\"uptime\":%lld}",
            ESP.getFreeHeap(), stream_client_count.load(), esp_timer_get_time() / 1000000);
        response = String(status);
    } else {
        response = "{\"type\":\"echo\",\"message\":\"" + msg + "\"}";
    }

    httpd_ws_frame_t ws_resp;
    memset(&ws_resp, 0, sizeof(httpd_ws_frame_t));
    ws_resp.type = HTTPD_WS_TYPE_TEXT;
    ws_resp.payload = (uint8_t*)response.c_str();
    ws_resp.len = response.length();
    ret = httpd_ws_send_frame(req, &ws_resp);

    if (buf) free(buf);
    return ret;
}

static void ws_send_to_all(const char* message) {
    if (!server) return;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)message;
    ws_pkt.len = strlen(message);

    size_t max_clients = 7;
    size_t clients = max_clients;
    int client_fds[max_clients];

    if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
        for (size_t i = 0; i < clients; i++) {
            int sock = client_fds[i];
            if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
                httpd_ws_send_frame_async(server, sock, &ws_pkt);
            }
        }
    }
}
#endif

static const char* framesize_to_string(framesize_t fs) {
    switch (fs) {
        case FRAMESIZE_QQVGA: return "QQVGA";
        case FRAMESIZE_QVGA:  return "QVGA";
        case FRAMESIZE_CIF:   return "CIF";
        case FRAMESIZE_VGA:   return "VGA";
        case FRAMESIZE_SVGA:  return "SVGA";
        case FRAMESIZE_XGA:   return "XGA";
        case FRAMESIZE_SXGA:  return "SXGA";
        case FRAMESIZE_UXGA:  return "UXGA";
        default:              return "UNKNOWN";
    }
}

static framesize_t string_to_framesize(const char* str) {
    if (!str) return default_framesize;
    if (strcmp(str, "QQVGA") == 0 || strcmp(str, "qqvga") == 0) return FRAMESIZE_QQVGA;
    if (strcmp(str, "QVGA") == 0  || strcmp(str, "qvga") == 0)  return FRAMESIZE_QVGA;
    if (strcmp(str, "CIF") == 0   || strcmp(str, "cif") == 0)   return FRAMESIZE_CIF;
    if (strcmp(str, "VGA") == 0   || strcmp(str, "vga") == 0)   return FRAMESIZE_VGA;
    if (strcmp(str, "SVGA") == 0  || strcmp(str, "svga") == 0)  return FRAMESIZE_SVGA;
    if (strcmp(str, "XGA") == 0   || strcmp(str, "xga") == 0)   return FRAMESIZE_XGA;
    if (strcmp(str, "SXGA") == 0  || strcmp(str, "sxga") == 0)  return FRAMESIZE_SXGA;
    if (strcmp(str, "UXGA") == 0  || strcmp(str, "uxga") == 0)  return FRAMESIZE_UXGA;
    return default_framesize;
}

static void framesize_to_wh(framesize_t fs, int *w, int *h) {
    switch (fs) {
        case FRAMESIZE_QQVGA: *w = 160; *h = 120; break;
        case FRAMESIZE_QVGA:  *w = 320; *h = 240; break;
        case FRAMESIZE_CIF:   *w = 400; *h = 296; break;
        case FRAMESIZE_VGA:   *w = 640; *h = 480; break;
        case FRAMESIZE_SVGA:  *w = 800; *h = 600; break;
        case FRAMESIZE_XGA:   *w = 1024; *h = 768; break;
        case FRAMESIZE_SXGA:  *w = 1280; *h = 1024; break;
        case FRAMESIZE_UXGA:  *w = 1600; *h = 1200; break;
        default:              *w = 640; *h = 480; break;
    }
}

// CORS预检响应
static esp_err_t send_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    return ESP_OK;
}

static esp_err_t read_post_body(httpd_req_t *req, String &body) {
    char buf[512];
    int ret, remaining = req->content_len;

    if (remaining <= 0) return ESP_OK;

    while (remaining > 0) {
        int to_read = (remaining > sizeof(buf) - 1) ? sizeof(buf) - 1 : remaining;
        ret = httpd_req_recv(req, buf, to_read);

        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (ret <= 0) {
            return ESP_FAIL;
        }

        buf[ret] = '\0';
        body += buf;
        remaining -= ret;
    }
    return ESP_OK;
}

static camera_fb_t* capture_latest_frame(int skip_count) {
    camera_fb_t *fb = NULL;

    for (int i = 0; i < skip_count; i++) {
        fb = esp_camera_fb_get();
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    fb = esp_camera_fb_get();
    return fb;
}

static bool switch_resolution(framesize_t new_size, int new_quality, framesize_t *old_size, int *old_quality) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;

    *old_size = default_framesize;
    *old_quality = default_quality;

    if (new_size != default_framesize) {
        s->set_framesize(s, new_size);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (new_quality != default_quality) {
        s->set_quality(s, new_quality);
    }

    return true;
}

static void restore_resolution(framesize_t old_size, int old_quality) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return;

    s->set_framesize(s, old_size);
    s->set_quality(s, old_quality);
    vTaskDelay(pdMS_TO_TICKS(50));
}

// 将图片转换为base64
// ==================== JSON 安全转义工具 ====================
// 将字符串中的控制字符和特殊字符转义，确保嵌入JSON时不会破坏结构
static String escapeJsonString(const String &input) {
    String output;
    output.reserve(input.length() * 2);
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b";  break;
            case '\f': output += "\\f";  break;
            case '\n': output += "\\n";  break;
            case '\r': output += "\\r";  break;
            case '\t': output += "\\t";  break;
            default:
                if (c >= 0 && c <= 0x1F) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    output += buf;
                } else {
                    output += c;
                }
        }
    }
    return output;
}

static String image_to_base64(camera_fb_t *fb) {
    if (!fb || !fb->buf || fb->len == 0) return "";

    size_t b64_len = ((fb->len + 2) / 3) * 4 + 1;
    char *b64_buf = (char*)smart_malloc(b64_len);
    if (!b64_buf) return "";

    size_t out_len = 0;
    int ret = mbedtls_base64_encode((unsigned char*)b64_buf, b64_len, &out_len, fb->buf, fb->len);
    if (ret != 0) {
        free(b64_buf);
        return "";
    }
    b64_buf[out_len] = '\0';
    String result = String(b64_buf);
    free(b64_buf);
    return result;
}

// CORS预检处理
static esp_err_t options_handler(httpd_req_t *req) {
    send_cors_headers(req);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t capture_handler(httpd_req_t *req) {
    int64_t start_time = esp_timer_get_time();

    framesize_t target_size = default_framesize;
    int target_quality = default_quality;
    bool json_mode = false;
    bool base64_only = false;

    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        char *query = (char*)malloc(query_len);
        if (query) {
            if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
                char param[64];

                if (httpd_query_key_value(query, "resolution", param, sizeof(param)) == ESP_OK) {
                    target_size = string_to_framesize(param);
                }
                if (httpd_query_key_value(query, "quality", param, sizeof(param)) == ESP_OK) {
                    int q = atoi(param);
                    if (q >= 4 && q <= 63) {
                        target_quality = q;
                    }
                }
                if (httpd_query_key_value(query, "format", param, sizeof(param)) == ESP_OK) {
                    if (strcmp(param, "json") == 0) {
                        json_mode = true;
                    } else if (strcmp(param, "base64") == 0) {
                        base64_only = true;
                    }
                }
            }
            free(query);
        }
    }

    DEBUG_PRINTF("[Cap] Params: res=%s, q=%d, JSON=%s, B64=%s\n",
                  framesize_to_string(target_size), target_quality,
                  json_mode ? "Y" : "N", base64_only ? "Y" : "N");
    DEBUG_PRINTF("[Cap] Stream clients: %d\n", stream_client_count.load());

    camera_fb_t *fb = NULL;
    bool used_shared_frame = false;
    bool need_resolution_change = (target_size != default_framesize || target_quality != default_quality);
    framesize_t old_size = default_framesize;
    int old_quality = default_quality;
    bool resolution_changed = false;

    if (stream_client_count.load() > 0 && !need_resolution_change) {
        DEBUG_PRINTLN("[Cap] Stream active, try cache");
        
        if (frame_buf_mutex && xSemaphoreTake(frame_buf_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (shared_frame_buf && shared_frame_len > 0 &&
                (esp_timer_get_time() - shared_frame_time) < 2000000) {
                fb = (camera_fb_t*)calloc(1, sizeof(camera_fb_t));
                if (fb) {
                    fb->buf = (uint8_t*)smart_malloc(shared_frame_len);
                    if (fb->buf) {
                        memcpy(fb->buf, shared_frame_buf, shared_frame_len);
                        fb->len = shared_frame_len;
                        fb->width = shared_frame_width;
                        fb->height = shared_frame_height;
                        fb->format = PIXFORMAT_JPEG;
                        used_shared_frame = true;
                        DEBUG_PRINTF("[Cap] Cache: %dx%d, %u bytes\n",
                                      fb->width, fb->height, fb->len);
                    } else {
                        free(fb);
                        fb = NULL;
                    }
                }
            } else {
                DEBUG_PRINTLN("[Cap] Cache invalid/expire, fallback");
            }
            xSemaphoreGive(frame_buf_mutex);
        }
    }

    if (!fb) {
        if (camera_mutex) {
            if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
                DEBUG_PRINTLN("[Cap] Camera mutex timeout");
                send_cors_headers(req);
                httpd_resp_set_status(req, "503 Service Unavailable");
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, ERR_CAMERA_BUSY_RETRY, -1);
                return ESP_OK;
            }
        }

        if (need_resolution_change) {
            resolution_changed = switch_resolution(target_size, target_quality, &old_size, &old_quality);
            DEBUG_PRINTF("[Cap] Res: %s->%s, Q: %d->%d\n",
                          framesize_to_string(old_size), framesize_to_string(target_size),
                          old_quality, target_quality);
        }

        fb = capture_latest_frame(1);

        if (!fb) {
            DEBUG_PRINTLN("[Cap] First frame fail, retry...");
            vTaskDelay(pdMS_TO_TICKS(100));
            fb = esp_camera_fb_get();
        }

        if (!fb) {
            DEBUG_PRINTLN("[Cap] Get frame fail");
            if (resolution_changed) {
                restore_resolution(old_size, old_quality);
            }
            if (camera_mutex) {
                xSemaphoreGive(camera_mutex);
            }
            send_cors_headers(req);
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, ERR_CAMERA_CAPTURE_FAIL, -1);
            return ESP_OK;
        }
    }

    int64_t capture_time = esp_timer_get_time();

    last_capture_stats.framesize = target_size;
    last_capture_stats.quality = target_quality;
    last_capture_stats.capture_time_ms = (capture_time - start_time) / 1000;
    last_capture_stats.image_size = fb->len;
    last_capture_stats.width = fb->width;
    last_capture_stats.height = fb->height;

    DEBUG_PRINTF("[Cap] OK: %dx%d, sz=%dB, t=%lldms, mode=%s\n",
                  fb->width, fb->height, fb->len, (capture_time - start_time) / 1000,
                  used_shared_frame ? "帧共享" : "直接拍照");

    esp_err_t res = ESP_OK;

    if (base64_only) {
        String base64Img = image_to_base64(fb);

        if (used_shared_frame) {
            free(fb->buf);
            free(fb);
            fb = NULL;
        } else {
            esp_camera_fb_return(fb);
            fb = NULL;
            if (resolution_changed) {
                restore_resolution(old_size, old_quality);
            }
            if (camera_mutex) xSemaphoreGive(camera_mutex);
        }

        send_cors_headers(req);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, base64Img.c_str(), base64Img.length());
    } else if (json_mode) {
        String base64Img = image_to_base64(fb);

        char json_header[512];
        snprintf(json_header, sizeof(json_header),
                 "{\"success\":true,\"width\":%d,\"height\":%d,\"size\":%u,\"quality\":%d,"
                 "\"resolution\":\"%s\",\"capture_time_ms\":%lld,\"timestamp\":%lld,\"image\":\"",
                 fb->width, fb->height, fb->len, target_quality,
                 framesize_to_string(target_size), (capture_time - start_time) / 1000,
                 esp_timer_get_time() / 1000000);

        if (used_shared_frame) {
            free(fb->buf);
            free(fb);
            fb = NULL;
        } else {
            esp_camera_fb_return(fb);
            fb = NULL;
            if (resolution_changed) {
                restore_resolution(old_size, old_quality);
            }
            if (camera_mutex) xSemaphoreGive(camera_mutex);
        }

        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Connection", "close");

        res = httpd_resp_send_chunk(req, json_header, strlen(json_header));
        if (res == ESP_OK && base64Img.length() > 0) {
            const int chunk_size = 4096;
            int offset = 0;
            while (offset < base64Img.length() && res == ESP_OK) {
                int send_len = base64Img.length() - offset;
                if (send_len > chunk_size) send_len = chunk_size;
                res = httpd_resp_send_chunk(req, base64Img.c_str() + offset, send_len);
                offset += chunk_size;
            }
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, "\"}", 2);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, NULL, 0);
        }
    } else {
        send_cors_headers(req);
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_set_hdr(req, "X-Image-Width", String(fb->width).c_str());
        httpd_resp_set_hdr(req, "X-Image-Height", String(fb->height).c_str());
        httpd_resp_set_hdr(req, "X-Image-Size", String(fb->len).c_str());
        httpd_resp_set_hdr(req, "X-Capture-Time-Ms", String((capture_time - start_time) / 1000).c_str());

        res = httpd_resp_send(req, (const char*)fb->buf, fb->len);

        if (used_shared_frame) {
            free(fb->buf);
            free(fb);
            fb = NULL;
        } else {
            esp_camera_fb_return(fb);
            fb = NULL;
            if (resolution_changed) {
                restore_resolution(old_size, old_quality);
            }
            if (camera_mutex) xSemaphoreGive(camera_mutex);
        }
    }

    int64_t end_time = esp_timer_get_time();
    DEBUG_PRINTF("[Cap] Total: %lldms (incl tx)\n", (end_time - start_time) / 1000);

    return res;
}

// 批量拍照接口
#ifdef ENABLE_BURST_CAPTURE
static esp_err_t burst_capture_handler(httpd_req_t *req) {
    int64_t start_time = esp_timer_get_time();

    int count = 3;
    int delay_ms = 500;
    framesize_t target_size = default_framesize;
    int target_quality = default_quality;

    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        char *query = (char*)malloc(query_len);
        if (query) {
            if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
                char param[64];
                if (httpd_query_key_value(query, "count", param, sizeof(param)) == ESP_OK) {
                    count = atoi(param);
                    if (count < 1) count = 1;
                    if (count > 10) count = 10;
                }
                if (httpd_query_key_value(query, "delay", param, sizeof(param)) == ESP_OK) {
                    delay_ms = atoi(param);
                    if (delay_ms < 100) delay_ms = 100;
                }
                if (httpd_query_key_value(query, "resolution", param, sizeof(param)) == ESP_OK) {
                    target_size = string_to_framesize(param);
                }
                if (httpd_query_key_value(query, "quality", param, sizeof(param)) == ESP_OK) {
                    int q = atoi(param);
                    if (q >= 4 && q <= 63) {
                        target_quality = q;
                    }
                }
            }
            free(query);
        }
    }

    DEBUG_PRINTF("[Burst] n=%d, interval=%dms, res=%s, q=%d\n",
                  count, delay_ms, framesize_to_string(target_size), target_quality);

    // 批量拍照需要独占摄像头，先暂停视频流
    DEBUG_PRINTF("[Burst] Stream clients: %d\n", stream_client_count.load());

    if (camera_mutex) {
        if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(8000)) != pdTRUE) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, ERR_CAMERA_BUSY_STOP, -1);
            return ESP_OK;
        }
    }

    framesize_t old_size = default_framesize;
    int old_quality = default_quality;
    bool resolution_changed = false;

    if (target_size != default_framesize || target_quality != default_quality) {
        resolution_changed = switch_resolution(target_size, target_quality, &old_size, &old_quality);
    }

    // 构建JSON响应
    send_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");

    char json_start[256];
    snprintf(json_start, sizeof(json_start),
             "{\"success\":true,\"count\":%d,\"resolution\":\"%s\",\"quality\":%d,\"images\":[",
             count, framesize_to_string(target_size), target_quality);
    httpd_resp_send_chunk(req, json_start, strlen(json_start));

    for (int i = 0; i < count; i++) {
        if (i > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        camera_fb_t *fb = capture_latest_frame(1);
        if (!fb) {
            continue;
        }

        String base64Img = image_to_base64(fb);
        esp_camera_fb_return(fb);

        char img_json[512];
        snprintf(img_json, sizeof(img_json),
                 "%s{\"index\":%d,\"width\":%d,\"height\":%d,\"size\":%u,\"image\":\"%s\"}",
                 i > 0 ? "," : "", i, 0, 0, 0, base64Img.c_str());

        httpd_resp_send_chunk(req, img_json, strlen(img_json));

        DEBUG_PRINTF("[Burst] %d/%d done\n", i + 1, count);
    }

    char json_end[128];
    int64_t total_time = (esp_timer_get_time() - start_time) / 1000;
    snprintf(json_end, sizeof(json_end), "],\"total_time_ms\":%lld}", total_time);
    httpd_resp_send_chunk(req, json_end, strlen(json_end));
    httpd_resp_send_chunk(req, NULL, 0);

    if (resolution_changed) {
        restore_resolution(old_size, old_quality);
    }

    if (camera_mutex) {
        xSemaphoreGive(camera_mutex);
    }

    DEBUG_PRINTF("[Burst] Done, total: %lldms\n", total_time);
    return ESP_OK;
}
#endif

static esp_err_t stream_handler(httpd_req_t *req) {
    int current = stream_client_count.load();
    if (current >= MAX_STREAM_CLIENTS) {
        DEBUG_PRINTF("[Stream] Reject, clients: %d\n", current);
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, "Too many stream clients", -1);
        return ESP_OK;
    }

    stream_client_count++;
    DEBUG_PRINTF("[Stream] Client connected, total: %d\n", stream_client_count.load());

    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];
    int64_t last_activity = esp_timer_get_time();
    const int64_t timeout_us = 30 * 1000000;
    int frame_count = 0;
    int error_count = 0;
    const int max_errors = 10;
    
    // ==================== 增强版帧率控制系统 =====================
    int target_fps = 15;
    int64_t frame_interval_us = 1000000 / target_fps;
    int64_t last_frame_time = esp_timer_get_time();
    
    // 自适应帧率控制参数
    const int SMOOTHING_WINDOW = 5;           // 帧间隔平滑窗       
    int64_t frame_times[SMOOTHING_WINDOW] = {0}; // 最近N帧的时间戳
    int frame_time_index = 0;                  // 当前帧索引
    // 性能监控变量
    float avg_processing_us = 0;               // 平均处理耗时（微秒）
    int consecutive_fast_frames = 0;           // 连续快速帧计数
    int consecutive_slow_frames = 0;           // 连续慢速帧计数
    const int MAX_FAST_FRAMES = 10;            // 最大连续快速帧阈值
    const int MAX_SLOW_FRAMES = 3;             // 最大连续慢速帧阈值
    DEBUG_PRINTF("[Stream] FPS:%d dt=%lldus\n", target_fps, frame_interval_us);

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        DEBUG_PRINTLN("[Stream] Set type fail");
        goto cleanup;
    }

    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_hdr(req, "X-Accel-Buffering", "no");
    httpd_resp_set_hdr(req, "Connection", "close");

    DEBUG_PRINTLN("[Stream] Start tx");

    while (true) {
        if (esp_timer_get_time() - last_activity > timeout_us) {
            DEBUG_PRINTLN("[Stream] Timeout, close");
            break;
        }

        if (error_count >= max_errors) {
            DEBUG_PRINTF("[视频流] 连续错误%d次，关闭连接\n", error_count);
            res = ESP_FAIL;
            break;
        }

        fb = esp_camera_fb_get();
        if (!fb) {
            DEBUG_PRINTLN("[Stream] Get frame fail");
            error_count++;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        error_count = 0;
        last_activity = esp_timer_get_time();
        frame_count++;

        bool need_free_jpg = false;
        if (fb->format != PIXFORMAT_JPEG) {
            int cached_fb_width = fb->width;
            int cached_fb_height = fb->height;
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if (!jpeg_converted) {
                DEBUG_PRINTLN("[Stream] JPEG conv fail");
                error_count++;
                continue;
            }
            need_free_jpg = true;

            if (frame_buf_mutex && xSemaphoreTake(frame_buf_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                safe_free((void**)&shared_frame_buf);
                shared_frame_len = 0;
                if (_jpg_buf_len <= SHARED_FRAME_MAX_SIZE && check_memory_available(_jpg_buf_len)) {
                    shared_frame_buf = (uint8_t*)smart_malloc(_jpg_buf_len);
                    if (shared_frame_buf) {
                        memcpy(shared_frame_buf, _jpg_buf, _jpg_buf_len);
                        shared_frame_len = _jpg_buf_len;
                        shared_frame_width = cached_fb_width;
                        shared_frame_height = cached_fb_height;
                        shared_frame_time = esp_timer_get_time();
                    }
                }
                xSemaphoreGive(frame_buf_mutex);
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
            need_free_jpg = false;

            if (frame_buf_mutex && xSemaphoreTake(frame_buf_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                safe_free((void**)&shared_frame_buf);
                shared_frame_len = 0;
                if (fb->len <= SHARED_FRAME_MAX_SIZE && check_memory_available(fb->len)) {
                    shared_frame_buf = (uint8_t*)smart_malloc(fb->len);
                    if (shared_frame_buf) {
                        memcpy(shared_frame_buf, fb->buf, fb->len);
                        shared_frame_len = fb->len;
                        shared_frame_width = fb->width;
                        shared_frame_height = fb->height;
                        shared_frame_time = esp_timer_get_time();
                    }
                }
                xSemaphoreGive(frame_buf_mutex);
            }
        }

        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            if (fb) esp_camera_fb_return(fb);
            if (need_free_jpg && _jpg_buf) free(_jpg_buf);
            break;
        }

        res = httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len);
        if (res != ESP_OK) {
            if (fb) esp_camera_fb_return(fb);
            if (need_free_jpg && _jpg_buf) free(_jpg_buf);
            break;
        }

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        if (need_free_jpg && _jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK) {
            break;
        }

        // ==================== 增强版帧率控制 ====================
        int64_t now = esp_timer_get_time();
        int64_t elapsed_us = now - last_frame_time;
        
        // 记录帧时间用于平滑计算
        frame_times[frame_time_index] = now;
        frame_time_index = (frame_time_index + 1) % SMOOTHING_WINDOW;
        
        // 计算平滑后的帧间隔（使用最近几帧的平均值）
        if (frame_count >= SMOOTHING_WINDOW) {
            float sum_interval = 0;
            for (int i = 1; i < SMOOTHING_WINDOW; i++) {
                int idx = (frame_time_index + i) % SMOOTHING_WINDOW;
                int prev_idx = (frame_time_index + i - 1) % SMOOTHING_WINDOW;
                if (frame_times[idx] > 0 && frame_times[prev_idx] > 0) {
                    sum_interval += (frame_times[idx] - frame_times[prev_idx]);
                }
            }
            avg_processing_us = sum_interval / (SMOOTHING_WINDOW - 1);
        } else {
            avg_processing_us = elapsed_us;
        }
        
        // 自适应延迟策略
        if (elapsed_us < frame_interval_us) {
            // 帧处理快于目标帧率，需要等
            int64_t delay_us = frame_interval_us - elapsed_us;
            
            // 使用高精度微秒级延迟（更精确）
            if (delay_us >= 1000) {
                // 大延迟：使用vTaskDelay
                vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
                // 补偿剩余的微秒
                int64_t remaining_us = delay_us % 1000;
                if (remaining_us > 0) {
                    delayMicroseconds(remaining_us);
                }
            } else {
                // 小延迟（<1ms）：使用delayMicroseconds
                delayMicroseconds((uint32_t)delay_us);
            }
            
            consecutive_fast_frames++;
            consecutive_slow_frames = 0;
            
            // 如果连续快速帧过多，可能目标FPS设置过低
            if (consecutive_fast_frames >= MAX_FAST_FRAMES && target_fps < 30) {
                DEBUG_PRINTLN("[Stream] Perf OK, can raise FPS");
                consecutive_fast_frames = 0;
            }
        } else {
            // 帧处理慢于目标帧率，不需要等
            consecutive_slow_frames++;
            consecutive_fast_frames = 0;
            
            // 如果连续慢帧过多，记录警告
            if (consecutive_slow_frames >= MAX_SLOW_FRAMES) {
                DEBUG_PRINTF("[Stream] Slow#%d (target:%.1fms actual:%.2fms)\n",
                             MAX_SLOW_FRAMES, 
                             frame_interval_us / 1000.0,
                             avg_processing_us / 1000.0);
                
                // 自动降低帧率以保持流畅性
                if (target_fps > 5 && avg_processing_us > frame_interval_us * 1.5) {
                    target_fps = max(5, target_fps - 2);
                    frame_interval_us = 1000000 / target_fps;
                    DEBUG_PRINTF("[Stream] Auto FPS->%d\n", target_fps);
                }
                
                consecutive_slow_frames = 0;
            }
            
            taskYIELD();  // 让出CPU给其他任务
        }
        
        last_frame_time = esp_timer_get_time();

        // 定期输出性能统计
        if (frame_count % 50 == 0 && frame_count > 0) {
            float actual_fps = 1000000.0f / max(avg_processing_us, (float)1);
            size_t free_heap = ESP.getFreeHeap();
            size_t free_psram = psramFound() ? ESP.getFreePsram() : 0;
            
            DEBUG_PRINTF("[Stream] #%d | FPS=%.1f | tgt=%d | proc=%.2fms | heap=%u | psram=%u\n",
                          frame_count, actual_fps, target_fps,
                          avg_processing_us / 1000.0,
                          free_heap, free_psram);
        }
    }

cleanup:
    if (fb) {
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    if (_jpg_buf) {
        free(_jpg_buf);
        _jpg_buf = NULL;
    }

    stream_client_count--;
    DEBUG_PRINTF("[Stream] Client disconnect, left:%d, frames:%d\n",
                  stream_client_count.load(), frame_count);

    return res;
}

static esp_err_t sensors_handler(httpd_req_t *req) {
    SensorData data = readSensors();

    // 支持查询参数过滤
    bool temp_only = false, hum_only = false, mq2_only = false, water_only = false;

    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        char *query = (char*)malloc(query_len);
        if (query) {
            if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
                char param[32];
                if (httpd_query_key_value(query, "field", param, sizeof(param)) == ESP_OK) {
                    if (strcmp(param, "temperature") == 0) temp_only = true;
                    else if (strcmp(param, "humidity") == 0) hum_only = true;
                    else if (strcmp(param, "mq2") == 0) mq2_only = true;
                    else if (strcmp(param, "water_level") == 0) water_only = true;
                }
            }
            free(query);
        }
    }

    char json[256];
    if (temp_only) {
        snprintf(json, sizeof(json), "{\"temperature\":%.1f}", data.temperature);
    } else if (hum_only) {
        snprintf(json, sizeof(json), "{\"humidity\":%.1f}", data.humidity);
    } else if (mq2_only) {
        snprintf(json, sizeof(json), "{\"mq2\":%d}", data.mq2);
    } else if (water_only) {
        snprintf(json, sizeof(json), "{\"water_level\":%.1f}", data.water_level);
    } else {
        snprintf(json, sizeof(json),
                 "{\"temperature\":%.1f,\"humidity\":%.1f,\"mq2\":%d,\"water_level\":%.1f,\"timestamp\":%lld}",
                 data.temperature, data.humidity, data.mq2, data.water_level,
                 esp_timer_get_time() / 1000000);
    }

    send_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    char json[1024];

    // 获取摄像头当前状态
    sensor_t *s = esp_camera_sensor_get();
    framesize_t current_size = default_framesize;
    int current_quality = default_quality;
    if (s) {
        current_size = s->status.framesize;
        current_quality = s->status.quality;
    }

    snprintf(json, sizeof(json),
             "{\"success\":true,\"system\":{\"heap_free\":%u,\"heap_total\":%u,\"psram_free\":%u,"
             "\"psram_size\":%u,\"psram_used\":%u,"
             "\"cpu_freq_mhz\":%d,\"chip_model\":\"%s\",\"chip_rev\":%d,\"sdk_version\":\"%s\","
             "\"uptime_sec\":%lld},\"camera\":{\"current_resolution\":\"%s\",\"current_quality\":%d,"
             "\"default_resolution\":\"%s\",\"default_quality\":%d},\"streaming\":{\"clients\":%d,"
             "\"max_clients\":%d},\"last_capture\":{\"resolution\":\"%s\",\"quality\":%d,"
             "\"size\":%u,\"width\":%d,\"height\":%d,\"time_ms\":%lld},"
             "\"memory\":{\"shared_frame_buf\":%s,\"shared_frame_size\":%u},"
             "\"wifi\":{\"connected\":%s,\"rssi\":%d,\"ip\":\"%s\"}}",
             ESP.getFreeHeap(),
             ESP.getHeapSize(),
             ESP.getPsramSize() > 0 ? ESP.getFreePsram() : 0,
             ESP.getPsramSize(),
             ESP.getPsramSize() > 0 ? (ESP.getPsramSize() - ESP.getFreePsram()) : 0,
             ESP.getCpuFreqMHz(),
             ESP.getChipModel(),
             ESP.getChipRevision(),
             ESP.getSdkVersion(),
             esp_timer_get_time() / 1000000,
             framesize_to_string(current_size),
             current_quality,
             framesize_to_string(default_framesize),
             default_quality,
             stream_client_count.load(),
             MAX_STREAM_CLIENTS,
             framesize_to_string(last_capture_stats.framesize),
             last_capture_stats.quality,
             last_capture_stats.image_size,
             last_capture_stats.width,
             last_capture_stats.height,
             last_capture_stats.capture_time_ms,
             shared_frame_buf ? "active" : "null",
             shared_frame_len,
             WiFi.status() == WL_CONNECTED ? "true" : "false",
             WiFi.RSSI(),
             WiFi.localIP().toString().c_str());

    send_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

// 摄像头配置接口
// 获取当前配置
// 设置新配置
// 返回错误信息
static esp_err_t config_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    if (req->method == HTTP_GET) {
        // 获取当前配置
        sensor_t *s = esp_camera_sensor_get();
        char json[1024];
        snprintf(json, sizeof(json),
                 "{\"success\":true,\"default_framesize\":\"%s\",\"default_quality\":%d,"
                 "\"available_resolutions\":[\"QQVGA\",\"QVGA\",\"CIF\",\"VGA\",\"SVGA\",\"XGA\",\"SXGA\",\"UXGA\"],"
                 "\"quality_range\":{\"min\":4,\"max\":63},"
                 "\"ai_api_url\":\"%s\",\"ai_model\":\"%s\"}",
                 framesize_to_string(default_framesize), default_quality,
                 escapeJsonString(ai_service_url).c_str(),
                 escapeJsonString(ai_model).c_str());
        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        return ESP_OK;
    }

    if (req->method == HTTP_POST) {
        String body;
        if (read_post_body(req, body) != ESP_OK) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, ERR_READ_BODY_FAIL, -1);
            return ESP_OK;
        }

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, ERR_INVALID_JSON, -1);
            return ESP_OK;
        }

        bool changed = false;

        if (doc["default_framesize"]) {
            const char* fs_str = doc["default_framesize"];
            framesize_t new_fs = string_to_framesize(fs_str);
            if (new_fs != default_framesize) {
                default_framesize = new_fs;
                changed = true;
            }
        }

        if (doc["default_quality"]) {
            int new_q = doc["default_quality"];
            if (new_q >= 4 && new_q <= 63 && new_q != default_quality) {
                default_quality = new_q;
                changed = true;
            }
        }

        // 处理AI配置字段
        if (doc["ai_api_url"]) {
            String new_url = doc["ai_api_url"];
            if (new_url != ai_service_url) {
                ai_service_url = new_url;
                changed = true;
                DEBUG_PRINTF("[AI-Cfg] URL: %s\n", ai_service_url.c_str());
            }
        }

        if (doc["ai_api_key"]) {
            String new_key = doc["ai_api_key"];
            if (new_key != ai_api_key) {
                ai_api_key = new_key;
                changed = true;
                DEBUG_PRINTLN("[AI-Cfg] Key updated");
            }
        }

        if (doc["ai_model"]) {
            String new_model = doc["ai_model"];
            if (new_model != ai_model) {
                ai_model = new_model;
                changed = true;
                DEBUG_PRINTF("[AI-Cfg] Model: %s\n", ai_model.c_str());
            }
        }

        // 立即应用配置到摄像头
        if (changed && camera_mutex) {
            if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
                sensor_t *s = esp_camera_sensor_get();
                if (s) {
                    s->set_framesize(s, default_framesize);
                    s->set_quality(s, default_quality);
                }
                xSemaphoreGive(camera_mutex);
            }
        }

        char response[512];
        snprintf(response, sizeof(response),
                 "{\"success\":true,\"changed\":%s,\"default_framesize\":\"%s\",\"default_quality\":%d,\"ai_api_url\":\"%s\",\"ai_model\":\"%s\"}",
                 changed ? "true" : "false",
                 framesize_to_string(default_framesize), default_quality,
                 escapeJsonString(ai_service_url).c_str(),
                 escapeJsonString(ai_model).c_str());

        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    send_cors_headers(req);
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ERR_METHOD_NOT_ALLOWED, -1);
    return ESP_OK;
}

// 调用外部AI服务进行图像分析
static String call_ai_service(const String& base64_image, const String& question, const String& model_type) {
    if (ai_service_url.isEmpty()) {
        return ERR_AI_NOT_CONFIGURED;
    }

    HTTPClient http;
    http.setTimeout(ai_timeout_ms);
    
    String payload = "{"
        "\"model\":\"" + ai_model + "\","
        "\"messages\":["
            "{"
                "\"role\":\"user\","
                "\"content\":["
                    "{\"type\":\"text\",\"text\":\"" + question + "\"},"
                    "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/jpeg;base64," + base64_image + "\"}}"
                "]"
            "}"
        "],"
        "\"max_tokens\":1000"
    "}";

    http.begin(ai_service_url);
    http.addHeader("Content-Type", "application/json");
    if (!ai_api_key.isEmpty()) {
        http.addHeader("Authorization", "Bearer " + ai_api_key);
    }

    int httpCode = http.POST(payload);
    String response = "";

    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
        DEBUG_PRINTF("[AI-Svc] OK, len=%d\n", response.length());
    } else {
        DEBUG_PRINTF("[AI-Svc] Fail, HTTP:%d\n", httpCode);
        response = "{\"error\":\"AI service request failed\",\"http_code\":" + String(httpCode) + "}";
    }

    http.end();
    return response;
}

// ==================== AI 响应文本提取工具 ====================
// 从LLM返回的JSON中提取纯文本回复内容
// 支持格式：OpenAI / 通义千问 / DeepSeek / Kimi / 豆包 / Claude 等
static String clean_ai_text(const String& text);

static String extract_ai_text(const String& raw_response) {
    if (raw_response.isEmpty()) {
        return "{\"error\":\"空响应\"}";
    }

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, raw_response);
    
    if (err) {
        DEBUG_PRINTF("[AI-Txt] JSON fail: %s, retry clean\n", err.c_str());
        // 尝试清理响应中的控制字符后重新解析
        String cleaned = raw_response;
        cleaned.replace("\n", "\\n");
        cleaned.replace("\r", "");
        cleaned.replace("\t", " ");
        
        DeserializationError err2 = deserializeJson(doc, cleaned);
        if (err2) {
            DEBUG_PRINTLN("[AI-Txt] Clean still fail, return raw");
            // 如果响应看起来像纯文本，直接返回
            if (!raw_response.startsWith("{") && !raw_response.startsWith("[")) {
                return raw_response;
            }
            return raw_response.substring(0, 2000);  // 截断过长的错误响应
        }
    }

    String extracted = "";

    // ========== 格式1: OpenAI/GPT标准格式 ==========
    // {"choices":[{"message":{"content":"..."}}]}
    if (doc.containsKey("choices") && doc["choices"].size() > 0) {
        JsonArray choices = doc["choices"].as<JsonArray>();
        
        // 支持多choices情况，拼接所有内容
        for (JsonObject choice : choices) {
            if (choice.containsKey("message") && choice["message"].containsKey("content")) {
                String content = choice["message"]["content"].as<String>();
                if (!content.isEmpty()) {
                    extracted += (extracted.isEmpty() ? "" : "\n") + content;
                }
            }
            
            // 某些API使用 delta 流式格式
            if (choice.containsKey("delta") && choice["delta"].containsKey("content")) {
                String deltaContent = choice["delta"]["content"].as<String>();
                if (!deltaContent.isEmpty()) {
                    extracted += deltaContent;
                }
            }
            
            // 某些API使用 text 字段
            if (choice.containsKey("text")) {
                String text = choice["text"].as<String>();
                if (!text.isEmpty()) {
                    extracted += (extracted.isEmpty() ? "" : "\n") + text;
                }
            }
        }
        
        if (!extracted.isEmpty()) {
            DEBUG_PRINTLN("[AI-Txt] OpenAI fmt OK");
            return clean_ai_text(extracted);
        }
    }

    // ========== 格式2: 通义千问 Qwen ==========
    // {"output":{"text":"..."}} 或 {"output":{"choices":[...]}}
    if (doc.containsKey("output")) {
        JsonObject output = doc["output"].as<JsonObject>();
        
        if (output.containsKey("text")) {
            extracted = output["text"].as<String>();
            if (!extracted.isEmpty()) {
                DEBUG_PRINTLN("[AI-Txt] Qwen(output.text) OK");
                return clean_ai_text(extracted);
            }
        }
        
        if (output.containsKey("choices") && output["choices"].size() > 0) {
            JsonObject firstChoice = output["choices"][0].as<JsonObject>();
            if (firstChoice.containsKey("message") && firstChoice["message"].containsKey("content")) {
                extracted = firstChoice["message"]["content"].as<String>();
                if (!extracted.isEmpty()) {
                    DEBUG_PRINTLN("[AI-Txt] Qwen(output.choices) OK");
                    return clean_ai_text(extracted);
                }
            }
        }
        
        // 千问新版可能包含 result 字段
        if (output.containsKey("result")) {
            extracted = output["result"].as<String>();
            if (!extracted.isEmpty()) {
                DEBUG_PRINTLN("[AI-Txt] Qwen(output.result) OK");
                return clean_ai_text(extracted);
            }
        }
    }

    // ========== 格式3: DeepSeek/Kimi/豆包 等国内模型 ==========
    // {"result": "..."} 或 {"data":{"result":"..."}}
    const char* resultFields[] = {"result", "reply", "response", "answer", "content", "text"};
    for (const char* field : resultFields) {
        if (doc.containsKey(field)) {
            extracted = doc[field].as<String>();
            if (!extracted.isEmpty()) {
                DEBUG_PRINTF("[AI-Tx] Field '%s' OK\n", field);
                return clean_ai_text(extracted);
            }
        }
    }

    // ========== 格式4: 豆包/火山引擎 ==========
    // {"choices":[{"messages":{"content":"..."}}]}
    if (doc.containsKey("choices") && doc["choices"].size() > 0) {
        JsonObject firstChoice = doc["choices"][0];
        
        // 豆包格式: choices[0].message.content 或 choices[0].messages.content  
        const char* msgFields[] = {"message", "messages"};
        for (const char* mf : msgFields) {
            if (firstChoice.containsKey(mf) && firstChoice[mf].containsKey("content")) {
                extracted = firstChoice[mf]["content"].as<String>();
                if (!extracted.isEmpty()) {
                    DEBUG_PRINTF("[AI-Txt] Doubao(%s.content) OK\n", mf);
                    return clean_ai_text(extracted);
                }
            }
        }
    }

    // ========== 格式5: Claude/AWS Bedrock ==========
    // {"content":[{"type":"text","text":"..."}]} 
    if (doc.containsKey("content") && doc["content"].is<JsonArray>()) {
        JsonArray contentArr = doc["content"].as<JsonArray>();
        for (JsonObject item : contentArr) {
            if (item["type"] == "text" || item.containsKey("text")) {
                String text = item["text"].as<String>();
                if (!text.isEmpty()) {
                    extracted += (extracted.isEmpty() ? "" : "\n") + text;
                }
            }
        }
        if (!extracted.isEmpty()) {
            DEBUG_PRINTLN("[AI-Txt] Claude fmt OK");
            return clean_ai_text(extracted);
        }
    }

    // ========== 格式6: Google Gemini ==========
    // {"candidates":[{"content":{"parts":[{"text":"..."}]}}]
    if (doc.containsKey("candidates") && doc["candidates"].size() > 0) {
        JsonObject candidate = doc["candidates"][0];
        if (candidate.containsKey("content") && candidate["content"].containsKey("parts")) {
            JsonArray parts = candidate["content"]["parts"];
            for (JsonObject part : parts) {
                if (part.containsKey("text")) {
                    String text = part["text"].as<String>();
                    if (!text.isEmpty()) {
                        extracted += (extracted.isEmpty() ? "" : "\n") + text;
                    }
                }
            }
        }
        if (!extracted.isEmpty()) {
            DEBUG_PRINTLN("[AI-Txt] Gemini fmt OK");
            return clean_ai_text(extracted);
        }
    }

    // ========== 兜底处理 ==========
    
    // 检查是否有错误信息
    if (doc.containsKey("error")) {
        if (doc["error"].is<JsonObject>() && doc["error"].containsKey("message")) {
            String errMsg = doc["error"]["message"].as<String>();
            DEBUG_PRINTF("[AI文本] 检测到错误信息: %s\n", errMsg.c_str());
            return "{\"error\":\"" + escapeJsonString(errMsg) + "\",\"suggestion\":\"请检查API Key和模型名称是否正确\"}";
        } else if (doc["error"].is<const char*>()) {
            String errMsg = doc["error"].as<String>();
            return "{\"error\":\"" + escapeJsonString(errMsg) + "\"}";
        }
    }
    
    // 检查 code/message 错误格式   
    if (doc.containsKey("code") || doc.containsKey("message") || doc.containsKey("msg")) {
        String errorMsg;
        if (doc.containsKey("message")) errorMsg = doc["message"].as<String>();
        else if (doc.containsKey("msg")) errorMsg = doc["msg"].as<String>();
        else errorMsg = "未知错误(code:" + doc["code"].as<String>() + ")";
        
        DEBUG_PRINTF("[AI文本] 检测到错误码格式: %s\n", errorMsg.c_str());
        return "{\"error\":\"" + escapeJsonString(errorMsg) + "\"}";
    }

    // 最后兜底：序列化整个JSON（限制长度）
    String fallback;
    serializeJson(doc, fallback);
    if (fallback.length() > 3000) {
        fallback = fallback.substring(0, 3000) + "...(截断)";
    }
    DEBUG_PRINTLN("[AI文本] 使用完整JSON作为输出");
    return fallback;
}

// AI文本清理工具 - 移除多余空白和特殊字符
static String clean_ai_text(const String& text) {
    if (text.isEmpty()) return text;
    
    String cleaned = text;
    
    // 移除开头结尾的引号（某些API会包裹）
    while ((cleaned.startsWith("\"") && cleaned.endsWith("\"")) || 
           (cleaned.startsWith("'") && cleaned.endsWith("'"))) {
        cleaned = cleaned.substring(1, cleaned.length() - 1);
    }
    
    // 清理多余换行
    cleaned.replace("\r\n", "\n");
    cleaned.replace("\r", "\n");
    
    // 压缩连续空行（超过2个的换行变成2个）
    int consecutiveNewlines = 0;
    String compressed = "";
    for (unsigned int i = 0; i < cleaned.length(); i++) {
        if (cleaned[i] == '\n') {
            consecutiveNewlines++;
            if (consecutiveNewlines <= 2) {
                compressed += '\n';
            }
        } else {
            consecutiveNewlines = 0;
            compressed += cleaned[i];
        }
    }
    
    // 移除首尾空白
    compressed.trim();
    
    // 限制最大长度（防止内存溢出）
    if (compressed.length() > 5000) {
        compressed = compressed.substring(0, 4970) + "\n...(内容过长已截断)";
    }
    
    DEBUG_PRINTF("[AI文本] 清理完成: %d -> %d 字符\n", text.length(), compressed.length());
    return compressed;
}

#ifdef ENABLE_YOLO_DETECT
// ==================== YOLO 目标检测服务调用 ====================
// 调用外部YOLO检测API，返回检测结果JSON
static String call_yolo_service(const String& base64_image) {
    if (yolo_service_url.isEmpty()) {
        return "{\"error\":\"YOLO service not configured\",\"suggestion\":\"请在配置中设置YOLO服务地址\"}";
    }

    HTTPClient http;
    http.setTimeout(ai_timeout_ms);

    String payload = "{"
        "\"image\":\"data:image/jpeg;base64," + base64_image + "\","
        "\"confidence\":0.5,"
        "\"iou_threshold\":0.45"
    "}";

    http.begin(yolo_service_url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(payload);
    String response = "";

    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
        DEBUG_PRINTF("[YOLO] 检测成功，响应长度: %d\n", response.length());
    } else {
        DEBUG_PRINTF("[YOLO] 检测失败，HTTP代码: %d\n", httpCode);
        response = "{\"error\":\"YOLO request failed\",\"http_code\":" + String(httpCode) + "}";
    }

    http.end();
    return response;
}
#endif

#ifdef ENABLE_LOCAL_ANALYSIS
// 本地简单图像分析（当没有外部AI服务或选择本地模式时使用）
// 基于图像统计信息进行基础分析：亮度、对比度、颜色分布等
static String local_image_analysis(camera_fb_t *fb, const String& question) {
    uint32_t total_brightness = 0;
    uint32_t dark_pixels = 0, bright_pixels = 0;
    uint32_t sample_count = 0;

    for (int i = 0; i < fb->len; i += 100) {
        uint8_t val = fb->buf[i];
        total_brightness += val;
        if (val < 50) dark_pixels++;
        else if (val > 200) bright_pixels++;
        sample_count++;
    }

    int avg_brightness = sample_count > 0 ? total_brightness / sample_count : 128;
    float dark_ratio = sample_count > 0 ? (float)dark_pixels / sample_count * 100 : 0;
    float bright_ratio = sample_count > 0 ? (float)bright_pixels / sample_count * 100 : 0;

    String brightness_desc = avg_brightness > 200 ? "很亮" :
                            avg_brightness > 150 ? "明亮" :
                            avg_brightness > 100 ? "正常" :
                            avg_brightness > 50 ? "较暗" : "很暗";

    String scene_type = "";
    if (dark_ratio > 60) scene_type = "可能是夜间场景或低光环境";
    else if (bright_ratio > 40 && avg_brightness > 180) scene_type = "可能是户外明亮场景";
    else if (avg_brightness >= 80 && avg_brightness <= 160) scene_type = "可能是室内正常光照环境";
    else scene_type = "光照条件不明";

    String analysis_text = "📷 图片信息\n";
    analysis_text += "分辨率: " + String(fb->width) + " × " + String(fb->height) + "\n";
    analysis_text += "文件大小: " + String(fb->len / 1024) + "KB\n";
    analysis_text += "整体亮度: " + brightness_desc + "(" + String(avg_brightness) + ")\n";
    analysis_text += "\n🔍 场景分析\n";
    analysis_text += scene_type + "\n";
    analysis_text += "暗部占比: " + String(dark_ratio, 1) + "% | 亮部占比: " + String(bright_ratio, 1) + "%\n";

    if (!question.isEmpty() && question != "描述图片") {
        analysis_text += "\n关于问题 \"" + question + "\"\n";
        analysis_text += "⚠️ 本地模式仅支持基础图像分析，无法回答复杂问题。\n";
        analysis_text += "💡 建议：切换到「云端大模型」模式获取智能回复";
    } else {
        analysis_text += "\n💡 提示: 切换至云端LLM模式可获取更详细的AI识别结果";
    }

    return analysis_text;
}
#endif

// AI识别接口 - 增强版，支持返回base64图片给大模型
static esp_err_t ask_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    DEBUG_PRINTLN("[AI识别] 开始处理请求");

    String requestBody;
    String question = "描述图片";
    String base64Image = "";
    bool usingFrontendImage = false;
    bool return_image = false;
    framesize_t target_size = FRAMESIZE_VGA;
    int target_quality = 12;
    String request_ai_mode = "";  // 前端请求的AI模式，为空则使用全局配置

    if (req->method == HTTP_POST) {
        if (read_post_body(req, requestBody) != ESP_OK) {
            DEBUG_PRINTLN("[AI识别] 读取POST body失败");
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"error\":\"Failed to read body\"}", -1);
            return ESP_FAIL;
        }

        DEBUG_PRINTF("[AI识别] 收到POST数据，长度: %d\n", requestBody.length());

        DynamicJsonDocument doc(16384);
        DeserializationError error = deserializeJson(doc, requestBody);

        if (error) {
            DEBUG_PRINTLN("[AI识别] JSON解析失败: " + String(error.c_str()));
        } else {
            if (doc["question"]) {
                question = doc["question"].as<String>();
            }
            if (doc["image"]) {
                String rawImage = doc["image"].as<String>();
                // 处理 data URI 格式 (data:image/jpeg;base64,...)
                int commaIndex = rawImage.indexOf(',');
                if (commaIndex > 0 && rawImage.startsWith("data:image")) {
                    base64Image = rawImage.substring(commaIndex + 1);
                } else {
                    base64Image = rawImage;
                }
                usingFrontendImage = !base64Image.isEmpty();
                DEBUG_PRINTF("[AI识别] 收到前端图片，原始长度: %d, 处理后长度: %d\n",
                              rawImage.length(), base64Image.length());
            }
            if (doc["return_image"]) {
                return_image = doc["return_image"].as<bool>();
            }
            if (doc["resolution"]) {
                target_size = string_to_framesize(doc["resolution"]);
            }
            if (doc["quality"]) {
                int q = doc["quality"];
                if (q >= 4 && q <= 63) {
                    target_quality = q;
                }
            }
            if (doc["ai_mode"]) {
                request_ai_mode = doc["ai_mode"].as<String>();
            }
        }
    } else if (req->method == HTTP_GET) {
        char question_raw[256] = {0};
        size_t query_len = httpd_req_get_url_query_len(req) + 1;
        if (query_len > 1) {
            char *query = (char*)malloc(query_len);
            if (query) {
                if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
                    char param[256];
                    if (httpd_query_key_value(query, "q", param, sizeof(param)) == ESP_OK) {
                        strlcpy(question_raw, param, sizeof(question_raw));
                        question = String(question_raw);
                    }
                    if (httpd_query_key_value(query, "resolution", param, sizeof(param)) == ESP_OK) {
                        target_size = string_to_framesize(param);
                    }
                    if (httpd_query_key_value(query, "quality", param, sizeof(param)) == ESP_OK) {
                        int q = atoi(param);
                        if (q >= 4 && q <= 63) {
                            target_quality = q;
                        }
                    }
                    if (httpd_query_key_value(query, "mode", param, sizeof(param)) == ESP_OK) {
                        request_ai_mode = String(param);
                    }
                }
                free(query);
            }
        }
    }

    String active_mode = request_ai_mode.isEmpty() ? ai_mode : request_ai_mode;
    DEBUG_PRINTF("[AI识别] 问题: %s, 模式: %s, 分辨率: %s, 质量: %d\n",
                  question.c_str(), active_mode.c_str(), framesize_to_string(target_size), target_quality);

    camera_fb_t *fb = NULL;
    String captured_base64 = "";
    int img_width = 0, img_height = 0;
    size_t img_size = 0;

    if (!usingFrontendImage) {
        bool need_resolution_change = (target_size != default_framesize || target_quality != default_quality);
        bool used_shared_frame = false;

        if (stream_client_count.load() > 0 && !need_resolution_change) {
            DEBUG_PRINTLN("[AI识别] 视频流运行中，尝试使用被动帧缓存");
            
            if (frame_buf_mutex && xSemaphoreTake(frame_buf_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (shared_frame_buf && shared_frame_len > 0 &&
                    (esp_timer_get_time() - shared_frame_time) < 2000000) {
                    fb = (camera_fb_t*)calloc(1, sizeof(camera_fb_t));
                    if (fb) {
                        fb->buf = (uint8_t*)smart_malloc(shared_frame_len);
                        if (fb->buf) {
                            memcpy(fb->buf, shared_frame_buf, shared_frame_len);
                            fb->len = shared_frame_len;
                            fb->width = shared_frame_width;
                            fb->height = shared_frame_height;
                            fb->format = PIXFORMAT_JPEG;
                            used_shared_frame = true;
                            DEBUG_PRINTF("[AI识别] 使用被动缓存帧: %dx%d, %u bytes\n",
                                          fb->width, fb->height, fb->len);
                        } else {
                            free(fb);
                            fb = NULL;
                        }
                    }
                } else {
                    DEBUG_PRINTLN("[AI识别] 被动帧缓存无效或过期，回退到直接拍摄");
                }
                xSemaphoreGive(frame_buf_mutex);
            }
        }

        if (!fb) {
            if (camera_mutex) {
                if (xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
                    DEBUG_PRINTLN("[AI识别] 获取摄像头互斥锁超时");
                    send_cors_headers(req);
                    httpd_resp_set_status(req, "503 Service Unavailable");
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_send(req, ERR_CAMERA_BUSY, -1);
                    return ESP_FAIL;
                }
            }

            framesize_t old_size = default_framesize;
            int old_quality = default_quality;
            bool resolution_changed = false;

            if (need_resolution_change) {
                resolution_changed = switch_resolution(target_size, target_quality, &old_size, &old_quality);
            }

            fb = capture_latest_frame(2);

            if (!fb) {
                DEBUG_PRINTLN("[AI识别] 拍照失败");
                if (resolution_changed) {
                    restore_resolution(old_size, old_quality);
                }
                if (camera_mutex) {
                    xSemaphoreGive(camera_mutex);
                }
                send_cors_headers(req);
                httpd_resp_set_status(req, "503 Service Unavailable");
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, ERR_CAMERA_CAPTURE_FAIL, -1);
                return ESP_FAIL;
            }

            DEBUG_PRINTF("[AI识别] 直接拍照成功: %dx%d, %d bytes\n", fb->width, fb->height, fb->len);

            img_width = fb->width;
            img_height = fb->height;
            img_size = fb->len;

            if (return_image) {
                captured_base64 = image_to_base64(fb);
            }

            esp_camera_fb_return(fb);
            fb = NULL;

            if (resolution_changed) {
                restore_resolution(old_size, old_quality);
            }

            if (camera_mutex) {
                xSemaphoreGive(camera_mutex);
            }
        } else {
            img_width = fb->width;
            img_height = fb->height;
            img_size = fb->len;

            if (return_image) {
                captured_base64 = image_to_base64(fb);
            }

            free(fb->buf);
            free(fb);
            fb = NULL;
        }
    } else {
        DEBUG_PRINTLN("[AI识别] 使用前端传来的图像");
        captured_base64 = base64Image;
    }

    // 构建响应
    char json_start[1024];
    snprintf(json_start, sizeof(json_start),
             "{\"success\":true,\"question\":\"%s\",\"using_frontend_image\":%s,"
             "\"image_width\":%d,\"image_height\":%d,\"image_size\":%u,"
             "\"resolution\":\"%s\",\"quality\":%d,\"timestamp\":%lld",
             question.c_str(),
             usingFrontendImage ? "true" : "false",
             img_width, img_height, img_size,
             framesize_to_string(target_size), target_quality,
             esp_timer_get_time() / 1000000);

    send_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");

    esp_err_t res = httpd_resp_send_chunk(req, json_start, strlen(json_start));

    // 添加图片数据（仅在LLM模式且显式要求时返回）
    bool should_return_image = return_image && (active_mode == "llm") && captured_base64.length() > 0;
    if (res == ESP_OK && should_return_image) {
        res = httpd_resp_send_chunk(req, ",\"image\":\"", 11);
        if (res == ESP_OK) {
            const int chunk_size = 4096;
            int offset = 0;
            while (offset < captured_base64.length() && res == ESP_OK) {
                int send_len = captured_base64.length() - offset;
                if (send_len > chunk_size) send_len = chunk_size;
                res = httpd_resp_send_chunk(req, captured_base64.c_str() + offset, send_len);
                offset += chunk_size;
            }
        }
        res = httpd_resp_send_chunk(req, "\"", 1);
    }

    // 添加AI分析结果 - 根据active_mode选择处理方式
    String ai_result = "";
    String ai_source = "unknown";

    if (active_mode == "yolo") {
        // ========== YOLO 目标检测模式 ==========
        #ifdef ENABLE_YOLO_DETECT
        DEBUG_PRINTLN("[AI识别] 模式: YOLO目标检测");
        ai_source = "yolo";
        if (captured_base64.length() > 0) {
            ai_result = call_yolo_service(captured_base64);
        } else {
            ai_result = "{\"error\":\"没有图像数据可供YOLO检测\"}";
        }
        #else
        ai_result = "{\"error\":\"YOLO功能已禁用\"}";
        #endif
    } else if (active_mode == "local") {
        // ========== 本地分析模式 ==========
        #ifdef ENABLE_LOCAL_ANALYSIS
        DEBUG_PRINTLN("[AI识别] 模式: 本地设备端分析");
        ai_source = "local";
        
        if (camera_mutex && xSemaphoreTake(camera_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            camera_fb_t *analysis_fb = capture_latest_frame(1);
            if (analysis_fb) {
                ai_result = local_image_analysis(analysis_fb, question);
                esp_camera_fb_return(analysis_fb);
            } else {
                ai_result = "本地分析：拍照失败，无法获取图像数据";
            }
            xSemaphoreGive(camera_mutex);
        } else {
            ai_result = "本地分析：摄像头忙碌，请稍后重试";
        }
        #else
        ai_result = "{\"error\":\"本地分析功能已禁用\"}";
        #endif
    } else {
        // ========== LLM 云端大模型模式 ==========
        DEBUG_PRINTLN("[AI识别] 模式: 云端LLM大模型");
        ai_source = "llm";
        if (ai_service_url.isEmpty()) {
            ai_result = "{\"mode\":\"llm\",\"error\":\"LLM服务未配置，请先在测试面板配置API地址和Key\",\"suggestion\":\"可在前端LLM测试面板配置后同步到ESP32\"}";
        } else if (captured_base64.length() > 0) {
            String raw_llm_response = call_ai_service(captured_base64, question, ai_model);
            // 从LLM响应中提取纯文本
            ai_result = extract_ai_text(raw_llm_response);
            DEBUG_PRINTF("[AI识别] LLM文本提取完成，长度: %d\n", ai_result.length());
        } else {
            ai_result = "{\"mode\":\"llm\",\"error\":\"没有图像数据可供LLM分析\"}";
        }
    }

    DEBUG_PRINTF("[AI识别] 处理完成，来源: %s，结果长度: %d\n", ai_source.c_str(), ai_result.length());
    
    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, ",\"ai_source\":\"", 16);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (ai_source + "\"").c_str(), ai_source.length() + 1);
        }
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, ",\"ai_mode\":\"", 11);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (active_mode + "\"").c_str(), active_mode.length() + 1);
        }
    }
    
    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, ",\"ai_result\":", 14);
        if (res == ESP_OK) {
            String safe_result = escapeJsonString(ai_result);
            res = httpd_resp_send_chunk(req, "\"", 1);
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, safe_result.c_str(), safe_result.length());
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, "\"", 1);
            }
        }
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, "}", 1);
    }
    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, NULL, 0);
    }

    DEBUG_PRINTLN("[AI识别] 请求处理完成");
    return res;
}

// AI配置接口
#ifdef ENABLE_AI_CONFIG
static esp_err_t ai_config_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    if (req->method == HTTP_GET) {
        char json[768];
        snprintf(json, sizeof(json),
            "{\"success\":true,\"ai_service_configured\":%s,\"ai_model\":\"%s\","
            "\"ai_timeout_ms\":%d,\"api_key_configured\":%s,"
            "\"ai_mode\":\"%s\",\"yolo_configured\":%s}",
            ai_service_url.isEmpty() ? "false" : "true",
            ai_model.c_str(),
            ai_timeout_ms,
            ai_api_key.isEmpty() ? "false" : "true",
            ai_mode.c_str(),
            #ifdef ENABLE_YOLO_DETECT
            yolo_service_url.isEmpty() ? "false" : "true"
            #else
            "false"
            #endif
            );
        
        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
        return ESP_OK;
    }

    if (req->method == HTTP_POST) {
        String body;
        if (read_post_body(req, body) != ESP_OK) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"error\":\"Failed to read body\"}", -1);
            return ESP_OK;
        }

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"error\":\"Invalid JSON\"}", -1);
            return ESP_OK;
        }

        if (doc["ai_service_url"]) {
            ai_service_url = doc["ai_service_url"].as<String>();
        }
        if (doc["ai_api_key"]) {
            ai_api_key = doc["ai_api_key"].as<String>();
        }
        if (doc["ai_model"]) {
            ai_model = doc["ai_model"].as<String>();
        }
        if (doc["ai_timeout_ms"]) {
            ai_timeout_ms = doc["ai_timeout_ms"].as<int>();
            if (ai_timeout_ms < 5000) ai_timeout_ms = 5000;
            if (ai_timeout_ms > 120000) ai_timeout_ms = 120000;
        }
        if (doc["ai_mode"]) {
            String new_mode = doc["ai_mode"].as<String>();
            #ifdef ENABLE_YOLO_DETECT
            if (new_mode == "llm" || new_mode == "yolo" || new_mode == "local") {
            #else
            if (new_mode == "llm" || new_mode == "local") {
            #endif
                ai_mode = new_mode;
                DEBUG_PRINTF("[AI配置] 模式已切换为: %s\n", ai_mode.c_str());
            }
        }
        #ifdef ENABLE_YOLO_DETECT
        if (doc["yolo_service_url"]) {
            yolo_service_url = doc["yolo_service_url"].as<String>();
        }
        #endif

        char response[384];
        snprintf(response, sizeof(response),
            "{\"success\":true,\"ai_service_configured\":%s,\"ai_model\":\"%s\","
            "\"ai_timeout_ms\":%d,\"ai_mode\":\"%s\",\"yolo_configured\":%s}",
            ai_service_url.isEmpty() ? "false" : "true",
            ai_model.c_str(),
            ai_timeout_ms,
            ai_mode.c_str(),
            #ifdef ENABLE_YOLO_DETECT
            yolo_service_url.isEmpty() ? "false" : "true"
            #else
            "false"
            #endif
            );

        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    send_cors_headers(req);
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"error\":\"Method not allowed\"}", -1);
    return ESP_OK;
}
#endif

// 日志接口
#ifdef ENABLE_LOGS
static esp_err_t logs_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    if (req->method == HTTP_GET) {
        send_cors_headers(req);
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_set_hdr(req, "Connection", "close");

        if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            httpd_resp_send(req, log_buffer, strlen(log_buffer));
            xSemaphoreGive(log_mutex);
        } else {
            httpd_resp_send(req, "[日志系统未就绪]", -1);
        }
        return ESP_OK;
    }

    if (req->method == HTTP_DELETE) {
        // 清空日志
        if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            memset(log_buffer, 0, LOG_BUFFER_SIZE);
            log_write_pos = 0;
            xSemaphoreGive(log_mutex);
        }
        
        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Logs cleared\"}", -1);
        return ESP_OK;
    }

    send_cors_headers(req);
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"error\":\"Method not allowed\"}", -1);
    return ESP_OK;
}
#endif

// 重启接口
static esp_err_t restart_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    if (req->method == HTTP_POST) {
        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":true,\"message\":\"Device restarting...\"}", -1);
        
        delay(1000);
        ESP.restart();
        return ESP_OK;
    }

    send_cors_headers(req);
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"error\":\"Method not allowed\"}", -1);
    return ESP_OK;
}

// ==================== 外设控制接口 ====================
// HTTP API: GET /actuator?device={type}&action={cmd}&value={param}
// 支持的设备类型: relay, buzzer, motor, servo
// 示例:
//   /actuator?device=relay&action=on
//   /actuator?device=buzzer&action=beep&freq=2000&duration=500
//   /actuator?device=motor&action=run&speed=128
//   /actuator?device=servo&action=write&angle=90
static esp_err_t actuator_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    if (req->method != HTTP_GET) {
        send_cors_headers(req);
        httpd_resp_set_status(req, "405 Method Not Allowed");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Only GET method allowed\"}", -1);
        return ESP_OK;
    }

    // 解析查询参数
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len <= 1) {
        // 无参数时返回所有外设状态
        String state_json = get_actuators_state_json();
        String response = "{\"success\":true,\"action\":\"status\"," + state_json + ",\"timestamp\":" + 
                          String(esp_timer_get_time() / 1000000) + "}";
        
        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response.c_str(), response.length());
        return ESP_OK;
    }

    char *query = (char*)malloc(query_len);
    if (!query) {
        send_cors_headers(req);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Memory allocation failed\"}", -1);
        return ESP_OK;
    }

    if (httpd_req_get_url_query_str(req, query, query_len) != ESP_OK) {
        free(query);
        send_cors_headers(req);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Failed to parse query string\"}", -1);
        return ESP_OK;
    }

    // 提取 device 参数
    char device[32] = {0};
    if (httpd_query_key_value(query, "device", device, sizeof(device)) != ESP_OK) {
        free(query);
        send_cors_headers(req);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Missing 'device' parameter (relay/buzzer/motor/servo)\"}", -1);
        return ESP_OK;
    }

    // 提取 action 参数
    char action[32] = {0};
    if (httpd_query_key_value(query, "action", action, sizeof(action)) != ESP_OK) {
        free(query);
        send_cors_headers(req);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"error\":\"Missing 'action' parameter\"}", -1);
        return ESP_OK;
    }

    DEBUG_PRINTF("[Actuator/API] 收到控制请求: device=%s, action=%s\n", device, action);

    String result_state = "";
    bool success = true;
    String error_msg = "";

    // ========== 根据设备类型分发处理 ==========
    if (strcmp(device, "relay") == 0) {
        if (strcmp(action, "on") == 0) {
            relay_on();
            result_state = "ON";
        } else if (strcmp(action, "off") == 0) {
            relay_off();
            result_state = "OFF";
        } else if (strcmp(action, "toggle") == 0) {
            relay_toggle();
            result_state = relay_get_state() ? "ON" : "OFF";
        } else if (strcmp(action, "status") == 0) {
            result_state = relay_get_state() ? "ON" : "OFF";
        } else {
            success = false;
            error_msg = "Invalid action for relay. Supported: on, off, toggle, status";
        }
    }
    else if (strcmp(device, "buzzer") == 0) {
        if (strcmp(action, "on") == 0) {
            // 支持频率参数，例如: /actuator?device=buzzer&action=on&freq=2000
            char freq_str[16] = {0};
            int freq = BUZZER_FREQ_DEFAULT;  // 默认使用配置的频率
            
            if (httpd_query_key_value(query, "freq", freq_str, sizeof(freq_str)) == ESP_OK) {
                freq = atoi(freq_str);
                freq = constrain(freq, 100, 10000);  // 限制频率范围
            }
            
            buzzer_on_with_freq(freq);
            result_state = "ON";
        } else if (strcmp(action, "off") == 0) {
            buzzer_off();
            result_state = "OFF";
        } else if (strcmp(action, "beep") == 0) {
            char freq_str[16] = {0}, dur_str[16] = {0};
            int freq = BUZZER_FREQ_DEFAULT;      // 默认2kHz
            int duration = 500;                   // 默认500ms
            
            if (httpd_query_key_value(query, "freq", freq_str, sizeof(freq_str)) == ESP_OK) {
                freq = atoi(freq_str);
            }
            if (httpd_query_key_value(query, "duration", dur_str, sizeof(dur_str)) == ESP_OK) {
                duration = atoi(dur_str);
            }
            
            buzzer_beep(freq, duration);
            result_state = "BEEP";
        } else if (strcmp(action, "status") == 0) {
            result_state = buzzer_get_state() ? "ON" : "OFF";
        } else {
            success = false;
            error_msg = "Invalid action for buzzer. Supported: on, off, beep, status";
        }
    }
    else if (strcmp(device, "motor") == 0) {
        if (strcmp(action, "stop") == 0) {
            motor_stop();
            result_state = "STOP";
        } else if (strcmp(action, "run") == 0) {
            char speed_str[16] = {0};
            int speed = 128;  // 默认50%速度
            
            if (httpd_query_key_value(query, "speed", speed_str, sizeof(speed_str)) == ESP_OK) {
                speed = atoi(speed_str);
            }
            
            motor_run(speed);
            result_state = "RUN@" + String(speed);
        } else if (strcmp(action, "status") == 0) {
            result_state = String(motor_get_speed());
        } else {
            success = false;
            error_msg = "Invalid action for motor. Supported: stop, run, status";
        }
    }
    else if (strcmp(device, "servo") == 0) {
        if (strcmp(action, "attach") == 0) {
            servo_attach();
            result_state = servo_is_attached() ? "ATTACHED" : "DETACHED";
        } else if (strcmp(action, "detach") == 0) {
            servo_detach();
            result_state = "DETACHED";
        } else if (strcmp(action, "write") == 0) {
            char angle_str[16] = {0};
            int angle = 90;  // 默认90°
            
            if (httpd_query_key_value(query, "angle", angle_str, sizeof(angle_str)) == ESP_OK) {
                angle = atoi(angle_str);
            }
            
            servo_write_angle(angle);
            result_state = String(servo_get_angle()) + "°";
        } else if (strcmp(action, "status") == 0) {
            result_state = servo_is_attached() ? String(servo_get_angle()) + "°" : "DETACHED";
        } else {
            success = false;
            error_msg = "Invalid action for servo. Supported: attach, detach, write, status";
        }
    }
    else {
        success = false;
        error_msg = "Unknown device. Supported: relay, buzzer, motor, servo";
    }

    free(query);

    // 构建JSON响应
    String json_response;
    if (success) {
        json_response = "{\"success\":true,\"device\":\"" + String(device) + 
                       "\",\"action\":\"" + String(action) + 
                       "\",\"state\":\"" + result_state + 
                       "\",\"timestamp\":" + String(esp_timer_get_time() / 1000000) + "}";
        
        DEBUG_PRINTF("[Actuator/API] 操作成功: %s %s ->%s\n", device, action, result_state.c_str());
    } else {
        json_response = "{\"success\":false,\"error\":\"" + error_msg + 
                       "\",\"device\":\"" + String(device) + 
                       "\",\"action\":\"" + String(action) + "\"}";
        
        DEBUG_PRINTF("[Actuator/API] 操作失败: %s\n", error_msg.c_str());
    }

    send_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, json_response.c_str(), json_response.length());

    return ESP_OK;
}

// ==================== 自动控制API处理函数 ====================

#ifdef ENABLE_AUTO_CONTROL
/**
 * GET /auto_control - 获取自动控制系统状态和规则列表
 * POST /auto_control - 添加/更新/删除规则
 *   添加规则: {"action":"add","rule":{...}}
 *   更新规则: {"action":"update","id":1,"rule":{...}}
 *   删除规则: {"action":"delete","id":1}
 *   启用规则: {"action":"enable","id":1}
 *   禁用规则: {"action":"disable","id":1}
 *   手动触发: {"action":"trigger","id":1}
 *   加载默认: {"action":"load_defaults"}
 *   清除所有: {"action":"clear"}
 *   保存配置: {"action":"save"}
 *   加载配置: {"action":"load"}
 */
static esp_err_t auto_control_handler(httpd_req_t *req) {
    if (req->method == HTTP_OPTIONS) {
        return options_handler(req);
    }

    // GET请求 - 返回状态
    if (req->method == HTTP_GET) {
        String status_json = get_auto_control_status_json();
        
        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, status_json.c_str(), status_json.length());
        return ESP_OK;
    }

    // POST请求 - 处理规则操作
    if (req->method == HTTP_POST) {
        char content[768];  // 减小缓冲区，因为规则结构体变小
        size_t recv_size = min(req->content_len, sizeof(content) - 1);
        
        int ret = httpd_req_recv(req, content, recv_size);
        if (ret <= 0) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "{\"success\":false,\"error\":\"Failed to read request body\"}", -1);
            return ESP_OK;
        }
        content[ret] = '\0';

        // 解析JSON（减小缓冲区以节省内存）
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, content);
        
        if (error) {
            send_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            String error_msg = "{\"success\":false,\"error\":\"JSON parse error: " + String(error.c_str()) + "\"}";
            httpd_resp_send(req, error_msg.c_str(), error_msg.length());
            return ESP_OK;
        }

        const char* action = doc["action"] | "";
        int rule_id = doc["id"] | 0;
        
        String response;
        bool success = false;

        if (strcmp(action, "add") == 0) {
            // 添加新规则
            JsonObject ruleObj = doc["rule"];
            if (ruleObj.isNull()) {
                response = "{\"success\":false,\"error\":\"Missing rule data\"}";
            } else {
                AutoControlRule rule;
                memset(&rule, 0, sizeof(rule));
                
                // 解析规则字段
                strncpy(rule.name, ruleObj["name"] | "未命名", RULE_NAME_MAX_LEN - 1);
                rule.enabled = ruleObj["enabled"] | true;
                
                const char* sensor = ruleObj["sensor"] | "temperature";
                if (strcmp(sensor, "temperature") == 0) rule.sensorType = SENSOR_TEMPERATURE;
                else if (strcmp(sensor, "humidity") == 0) rule.sensorType = SENSOR_HUMIDITY;
                else if (strcmp(sensor, "gas") == 0) rule.sensorType = SENSOR_GAS;
                else if (strcmp(sensor, "smoke") == 0) rule.sensorType = SENSOR_SMOKE;
                else if (strcmp(sensor, "water") == 0) rule.sensorType = SENSOR_WATER;
                
                const char* condition = ruleObj["condition"] | ">";
                if (strcmp(condition, ">") == 0) rule.condition = CONDITION_GREATER_THAN;
                else if (strcmp(condition, "<") == 0) rule.condition = CONDITION_LESS_THAN;
                else if (strcmp(condition, "=") == 0) rule.condition = CONDITION_EQUAL_TO;
                else if (strcmp(condition, "in_range") == 0) rule.condition = CONDITION_IN_RANGE;
                else if (strcmp(condition, "out_of_range") == 0) rule.condition = CONDITION_OUT_OF_RANGE;
                
                rule.threshold1 = ruleObj["threshold1"] | 0.0f;
                rule.threshold2 = ruleObj["threshold2"] | 0.0f;
                rule.debounceMs = ruleObj["debounce_ms"] | 1000;
                
                const char* act = ruleObj["action"] | "none";
                if (strcmp(act, "relay_on") == 0) rule.actionType = ACTION_RELAY_ON;
                else if (strcmp(act, "relay_off") == 0) rule.actionType = ACTION_RELAY_OFF;
                else if (strcmp(act, "relay_toggle") == 0) rule.actionType = ACTION_RELAY_TOGGLE;
                else if (strcmp(act, "buzzer_on") == 0) rule.actionType = ACTION_BUZZER_ON;
                else if (strcmp(act, "buzzer_off") == 0) rule.actionType = ACTION_BUZZER_OFF;
                else if (strcmp(act, "buzzer_beep") == 0) rule.actionType = ACTION_BUZZER_BEEP;
                else if (strcmp(act, "motor_run") == 0) rule.actionType = ACTION_MOTOR_RUN;
                else if (strcmp(act, "motor_stop") == 0) rule.actionType = ACTION_MOTOR_STOP;
                else if (strcmp(act, "servo_write") == 0) rule.actionType = ACTION_SERVO_WRITE;
                else rule.actionType = ACTION_NONE;
                
                rule.actionParam = ruleObj["action_param"] | 0;
                rule.actionDurationMs = ruleObj["duration_ms"] | 0;
                
                int new_id = add_auto_control_rule(rule);
                if (new_id >= 0) {
                    success = true;
                    response = "{\"success\":true,\"id\":" + String(new_id) + ",\"message\":\"Rule added\"}";
                } else {
                    response = "{\"success\":false,\"error\":\"Failed to add rule (max reached)\"}";
                }
            }
        }
        else if (strcmp(action, "update") == 0) {
            // 更新规则
            if (rule_id <= 0) {
                response = "{\"success\":false,\"error\":\"Invalid rule ID\"}";
            } else {
                JsonObject ruleObj = doc["rule"];
                if (ruleObj.isNull()) {
                    response = "{\"success\":false,\"error\":\"Missing rule data\"}";
                } else {
                    AutoControlRule rule;
                    memset(&rule, 0, sizeof(rule));
                    
                    strncpy(rule.name, ruleObj["name"] | "未命名", RULE_NAME_MAX_LEN - 1);
                    rule.enabled = ruleObj["enabled"] | true;
                    
                    const char* sensor = ruleObj["sensor"] | "temperature";
                    if (strcmp(sensor, "temperature") == 0) rule.sensorType = SENSOR_TEMPERATURE;
                    else if (strcmp(sensor, "humidity") == 0) rule.sensorType = SENSOR_HUMIDITY;
                    else if (strcmp(sensor, "gas") == 0) rule.sensorType = SENSOR_GAS;
                    else if (strcmp(sensor, "smoke") == 0) rule.sensorType = SENSOR_SMOKE;
                    else if (strcmp(sensor, "water") == 0) rule.sensorType = SENSOR_WATER;
                    
                    const char* condition = ruleObj["condition"] | ">";
                    if (strcmp(condition, ">") == 0) rule.condition = CONDITION_GREATER_THAN;
                    else if (strcmp(condition, "<") == 0) rule.condition = CONDITION_LESS_THAN;
                    else if (strcmp(condition, "=") == 0) rule.condition = CONDITION_EQUAL_TO;
                    else if (strcmp(condition, "in_range") == 0) rule.condition = CONDITION_IN_RANGE;
                    else if (strcmp(condition, "out_of_range") == 0) rule.condition = CONDITION_OUT_OF_RANGE;
                    
                    rule.threshold1 = ruleObj["threshold1"] | 0.0f;
                    rule.threshold2 = ruleObj["threshold2"] | 0.0f;
                    rule.debounceMs = ruleObj["debounce_ms"] | 1000;
                    
                    const char* act = ruleObj["action"] | "none";
                    if (strcmp(act, "relay_on") == 0) rule.actionType = ACTION_RELAY_ON;
                    else if (strcmp(act, "relay_off") == 0) rule.actionType = ACTION_RELAY_OFF;
                    else if (strcmp(act, "relay_toggle") == 0) rule.actionType = ACTION_RELAY_TOGGLE;
                    else if (strcmp(act, "buzzer_on") == 0) rule.actionType = ACTION_BUZZER_ON;
                    else if (strcmp(act, "buzzer_off") == 0) rule.actionType = ACTION_BUZZER_OFF;
                    else if (strcmp(act, "buzzer_beep") == 0) rule.actionType = ACTION_BUZZER_BEEP;
                    else if (strcmp(act, "motor_run") == 0) rule.actionType = ACTION_MOTOR_RUN;
                    else if (strcmp(act, "motor_stop") == 0) rule.actionType = ACTION_MOTOR_STOP;
                    else if (strcmp(act, "servo_write") == 0) rule.actionType = ACTION_SERVO_WRITE;
                    else rule.actionType = ACTION_NONE;
                    
                    rule.actionParam = ruleObj["action_param"] | 0;
                    rule.actionDurationMs = ruleObj["duration_ms"] | 0;
                    
                    if (update_auto_control_rule(rule_id, rule)) {
                        success = true;
                        response = "{\"success\":true,\"message\":\"Rule updated\"}";
                    } else {
                        response = "{\"success\":false,\"error\":\"Rule not found\"}";
                    }
                }
            }
        }
        else if (strcmp(action, "delete") == 0) {
            // 删除规则
            if (delete_auto_control_rule(rule_id)) {
                success = true;
                response = "{\"success\":true,\"message\":\"Rule deleted\"}";
            } else {
                response = "{\"success\":false,\"error\":\"Rule not found\"}";
            }
        }
        else if (strcmp(action, "enable") == 0) {
            // 启用规则
            if (set_rule_enabled(rule_id, true)) {
                success = true;
                response = "{\"success\":true,\"message\":\"Rule enabled\"}";
            } else {
                response = "{\"success\":false,\"error\":\"Rule not found\"}";
            }
        }
        else if (strcmp(action, "disable") == 0) {
            // 禁用规则
            if (set_rule_enabled(rule_id, false)) {
                success = true;
                response = "{\"success\":true,\"message\":\"Rule disabled\"}";
            } else {
                response = "{\"success\":false,\"error\":\"Rule not found\"}";
            }
        }
        else if (strcmp(action, "trigger") == 0) {
            // 手动触发规则
            if (manual_trigger_rule(rule_id)) {
                success = true;
                response = "{\"success\":true,\"message\":\"Rule triggered manually\"}";
            } else {
                response = "{\"success\":false,\"error\":\"Rule not found or disabled\"}";
            }
        }
        else if (strcmp(action, "load_defaults") == 0) {
            // 加载默认规则
            load_default_auto_rules();
            success = true;
            response = "{\"success\":true,\"message\":\"Default rules loaded\"}";
        }
        else if (strcmp(action, "clear") == 0) {
            // 清除所有规则
            clear_all_auto_control_rules();
            success = true;
            response = "{\"success\":true,\"message\":\"All rules cleared\"}";
        }
        else if (strcmp(action, "save") == 0) {
            // 保存规则到SPIFFS
            if (save_rules_to_spiffs()) {
                success = true;
                response = "{\"success\":true,\"message\":\"Rules saved to storage\"}";
            } else {
                response = "{\"success\":false,\"error\":\"Failed to save rules\"}";
            }
        }
        else if (strcmp(action, "load") == 0) {
            // 从SPIFFS加载规则
            int loaded = load_rules_from_spiffs();
            if (loaded >= 0) {
                success = true;
                response = "{\"success\":true,\"count\":" + String(loaded) + ",\"message\":\"Rules loaded from storage\"}";
            } else {
                response = "{\"success\":false,\"error\":\"No saved rules found\"}";
            }
        }
        else {
            response = "{\"success\":false,\"error\":\"Unknown action\"}";
        }

        send_cors_headers(req);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response.c_str(), response.length());
        return ESP_OK;
    }

    send_cors_headers(req);
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Only GET/POST allowed\"}", -1);
    return ESP_OK;
}
#else
// 自动控制功能被禁用时，返回空处理函数
static esp_err_t auto_control_handler(httpd_req_t *req) {
    send_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Auto control disabled\"}", -1);
    return ESP_OK;
}
#endif

void startCameraServer() {
    camera_mutex = xSemaphoreCreateMutex();
    #ifdef ENABLE_LOGS
    log_mutex = xSemaphoreCreateMutex();
    #endif
    frame_buf_mutex = xSemaphoreCreateMutex();
    if (camera_mutex == NULL) {
        DEBUG_PRINTLN("[Web] 创建摄像头互斥锁失败!");
    }
    if (frame_buf_mutex == NULL) {
        DEBUG_PRINTLN("[Web] 创建帧缓冲区互斥锁失败");
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_open_sockets = 7;
    config.max_uri_handlers = 30;
    config.max_resp_headers = 20;
    config.backlog_conn = 5;
    config.lru_purge_enable = true;
    config.stack_size = 8192;
    config.send_wait_timeout = 30;
    config.recv_wait_timeout = 30;
    config.keep_alive_enable = true;
    config.keep_alive_idle = 5;
    config.keep_alive_interval = 3;
    config.keep_alive_count = 3;

    // 定义所有URI处理函数
    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t capture_options_uri = {
        .uri       = "/capture",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    #ifdef ENABLE_BURST_CAPTURE
    httpd_uri_t burst_uri = {
        .uri       = "/burst",
        .method    = HTTP_GET,
        .handler   = burst_capture_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t burst_options_uri = {
        .uri       = "/burst",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    #endif
    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t sensors_uri = {
        .uri       = "/sensors",
        .method    = HTTP_GET,
        .handler   = sensors_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t sensors_options_uri = {
        .uri       = "/sensors",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t status_options_uri = {
        .uri       = "/status",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t config_uri = {
        .uri       = "/config",
        .method    = HTTP_GET,
        .handler   = config_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t config_post_uri = {
        .uri       = "/config",
        .method    = HTTP_POST,
        .handler   = config_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t config_options_uri = {
        .uri       = "/config",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t ask_uri = {
        .uri       = "/ask",
        .method    = HTTP_GET,
        .handler   = ask_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t ask_post_uri = {
        .uri       = "/ask",
        .method    = HTTP_POST,
        .handler   = ask_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t ask_options_uri = {
        .uri       = "/ask",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    #ifdef ENABLE_AI_CONFIG
    httpd_uri_t ai_config_uri = {
        .uri       = "/ai_config",
        .method    = HTTP_GET,
        .handler   = ai_config_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t ai_config_post_uri = {
        .uri       = "/ai_config",
        .method    = HTTP_POST,
        .handler   = ai_config_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t ai_config_options_uri = {
        .uri       = "/ai_config",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    #endif
    #ifdef ENABLE_LOGS
    httpd_uri_t logs_uri = {
        .uri       = "/logs",
        .method    = HTTP_GET,
        .handler   = logs_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t logs_delete_uri = {
        .uri       = "/logs",
        .method    = HTTP_DELETE,
        .handler   = logs_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t logs_options_uri = {
        .uri       = "/logs",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    #endif
    httpd_uri_t restart_uri = {
        .uri       = "/restart",
        .method    = HTTP_POST,
        .handler   = restart_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t restart_options_uri = {
        .uri       = "/restart",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    #ifdef ENABLE_WEBSOCKET
    httpd_uri_t ws_uri = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = NULL,
        .is_websocket = true
    };
    #endif
    httpd_uri_t actuator_uri = {
        .uri       = "/actuator",
        .method    = HTTP_GET,
        .handler   = actuator_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t actuator_options_uri = {
        .uri       = "/actuator",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t auto_control_uri = {
        .uri       = "/auto_control",
        .method    = HTTP_GET,
        .handler   = auto_control_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t auto_control_post_uri = {
        .uri       = "/auto_control",
        .method    = HTTP_POST,
        .handler   = auto_control_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t auto_control_options_uri = {
        .uri       = "/auto_control",
        .method    = HTTP_OPTIONS,
        .handler   = options_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        // 注册所有URI处理函数
        httpd_register_uri_handler(server, &capture_uri);
        httpd_register_uri_handler(server, &capture_options_uri);
        #ifdef ENABLE_BURST_CAPTURE
        httpd_register_uri_handler(server, &burst_uri);
        httpd_register_uri_handler(server, &burst_options_uri);
        #endif
        httpd_register_uri_handler(server, &stream_uri);
        httpd_register_uri_handler(server, &sensors_uri);
        httpd_register_uri_handler(server, &sensors_options_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &status_options_uri);
        httpd_register_uri_handler(server, &config_uri);
        httpd_register_uri_handler(server, &config_post_uri);
        httpd_register_uri_handler(server, &config_options_uri);
        httpd_register_uri_handler(server, &ask_uri);
        httpd_register_uri_handler(server, &ask_post_uri);
        httpd_register_uri_handler(server, &ask_options_uri);
        #ifdef ENABLE_AI_CONFIG
        httpd_register_uri_handler(server, &ai_config_uri);
        httpd_register_uri_handler(server, &ai_config_post_uri);
        httpd_register_uri_handler(server, &ai_config_options_uri);
        #endif
        #ifdef ENABLE_LOGS
        httpd_register_uri_handler(server, &logs_uri);
        httpd_register_uri_handler(server, &logs_delete_uri);
        httpd_register_uri_handler(server, &logs_options_uri);
        #endif
        httpd_register_uri_handler(server, &restart_uri);
        httpd_register_uri_handler(server, &restart_options_uri);
        #ifdef ENABLE_WEBSOCKET
        httpd_register_uri_handler(server, &ws_uri);
        #endif
        httpd_register_uri_handler(server, &actuator_uri);
        httpd_register_uri_handler(server, &actuator_options_uri);
        httpd_register_uri_handler(server, &auto_control_uri);
        httpd_register_uri_handler(server, &auto_control_post_uri);
        httpd_register_uri_handler(server, &auto_control_options_uri);

        DEBUG_PRINTLN(F("========================================"));
        DEBUG_PRINTLN(F("Web服务器启动成功(Web+AI适配器 v4.0)"));
        DEBUG_PRINTF(F("  最大连接数: %d\n"), config.max_open_sockets);
        DEBUG_PRINTLN(F("  API列表:"));
        DEBUG_PRINTLN(F("    GET|OPTIONS /capture?resolution=VGA&quality=10&format=json|base64"));
        #ifdef ENABLE_BURST_CAPTURE
        DEBUG_PRINTLN(F("    GET|OPTIONS /burst?count=3&delay=500&resolution=VGA"));
        #endif
        DEBUG_PRINTLN(F("    GET       /stream"));
        DEBUG_PRINTLN(F("    GET|OPTIONS /sensors?field=temperature"));
        DEBUG_PRINTLN(F("    GET|OPTIONS /status"));
        DEBUG_PRINTLN(F("    GET|POST|OPTIONS /config"));
        DEBUG_PRINTLN(F("    GET|POST|OPTIONS /ask?q=question&resolution=VGA&return_image=true"));
        #ifdef ENABLE_AI_CONFIG
        DEBUG_PRINTLN(F("    GET|POST|OPTIONS /ai_config"));
        #endif
        #ifdef ENABLE_LOGS
        DEBUG_PRINTLN(F("    GET|DELETE|OPTIONS /logs"));
        #endif
        DEBUG_PRINTLN(F("    POST|OPTIONS /restart"));
        #ifdef ENABLE_WEBSOCKET
        DEBUG_PRINTLN(F("    WebSocket   /ws"));
        #endif
        DEBUG_PRINTLN(F("    GET|OPTIONS /actuator?device=relay&action=on"));
        DEBUG_PRINTLN(F("    GET|POST|OPTIONS /auto_control"));
        DEBUG_PRINTLN(F("========================================"));
        
        DEBUG_PRINTLN(F("Web Server started successfully (Web+AI v4.0)"));
        web_log("WebServer started");
    } else {
        DEBUG_PRINTLN("Web Server start failed");
    }
}
