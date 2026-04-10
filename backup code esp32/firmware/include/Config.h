#ifndef CONFIG_H
#define CONFIG_H

// --- Blynk Credentials ---
#define BLYNK_TEMPLATE_ID "TMPL6nXDjldOr"
#define BLYNK_TEMPLATE_NAME "FallDetection"
#define BLYNK_AUTH_TOKEN "KveCGf33u8V1WhImVQS2E3YHGdjnCSYy"

// --- WiFi Credentials ---
// LƯU Ý: Đây PHẢI là Tên mạng và Mật khẩu của HOTSPOT mà Laptop bạn phát ra
const char *ssid = "Xiaomi";    // Đổi thành tên Mobile Hotspot của Laptop
const char *pass = "12345679@"; // Đổi thành mật khẩu Hotspot của Laptop

// --- Server Settings ---
// Vì Laptop phát Hotspot nên IP luôn cố định là 192.168.137.1
#define SERVER_IP "192.168.137.1"
#define SERVER_PORT_DEFAULT 8000

// --- Notification Settings ---
#define NOTIFICATION_INTERVAL 2000 // 20 giây một lần ở Level 2
#define LEVEL_1_TIMEOUT 15000      // 15 giây
#define LEVEL_2_TIMEOUT 30000      // 30 giây

// --- Telegram Settings ---
#define TELEGRAM_BOT_TOKEN "8271771967:AAHmku0WWhAZnFm-GALfTEhB80FSmkwLKXQ"
#define TELEGRAM_CHAT_ID "5348036208"
#define DIA_CHI_NHA "Nhà số 123, Đường ABC, Quận XYZ, TP. Hà Nội"

// --- Xiaozhi AI Settings (TẠM TẮT) ---
// #define XIAOZHI_ALERT_URL "http://192.168.1.100/alert"

#endif
