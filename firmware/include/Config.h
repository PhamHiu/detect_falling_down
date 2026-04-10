#ifndef CONFIG_H
#define CONFIG_H

// --- Blynk Credentials ---
#define BLYNK_TEMPLATE_ID "TMPL6nXDjldOr"
#define BLYNK_TEMPLATE_NAME "FallDetection"
#define BLYNK_AUTH_TOKEN "KveCGf33u8V1WhImVQS2E3YHGdjnCSYy"

// --- WiFi Credentials ---
const char *ssid = "Xiaomi";
const char *pass = "12345679@";

// --- API Server Settings ---
// mDNS: ESP32 tự động tìm máy tính qua tên "falldect.local"
// KHÔNG cần sửa IP thủ công nữa!
const char *SERVER_MDNS_HOST = "falldect.local"; // Tên mDNS của máy tính
const unsigned int SERVER_PORT = 8000;
const char *FALLBACK_IP = "10.186.173.225"; // IP dự phòng nếu mDNS lỗi

// server_url và reset_url sẽ được tạo động trong setup() sau khi resolve mDNS
String server_url_str = "";
String reset_url_str = "";
const char *server_url = nullptr;
const char *reset_url = nullptr;

// --- Notification Settings ---
#define NOTIFICATION_INTERVAL 2000 // 20 giây một lần ở Level 2
#define LEVEL_1_TIMEOUT 15000      // 15s
#define LEVEL_2_TIMEOUT 30000      // 5 phút

#endif
