#ifndef CONFIG_H
#define CONFIG_H

// --- Blynk Credentials ---
#define BLYNK_TEMPLATE_ID "TMPL6nXDjldOr"
#define BLYNK_TEMPLATE_NAME "FallDetection"
#define BLYNK_AUTH_TOKEN "KveCGf33u8V1WhImVQS2E3YHGdjnCSYy"

// --- WiFi Credentials ---
const char *ssid = "Xiaomi13pro";
const char *pass = "123456780@";

// --- API Server Settings ---
// LƯU Ý: Thay đổi IP này thành IP máy tính của bạn (chạy FastAPI)
// Ví dụ: "http://192.168.1.10:8000"
const char *server_url = "http://10.221.65.225:8000/status";
const char *reset_url = "http://10.221.65.225:8000/reset";

// --- Notification Settings ---
#define NOTIFICATION_INTERVAL 20000 // 20 giây một lần ở Level 2
#define LEVEL_1_TIMEOUT 120000      // 2 phút
#define LEVEL_2_TIMEOUT 300000      // 5 phút

#endif
