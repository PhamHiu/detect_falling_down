#include "Config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// --- WebServer ---
WebServer localServer(80);

// --- URL cố định ---
String server_url =
    "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT_DEFAULT) + "/gate";
String reset_url = "http://" + String(SERVER_IP) + ":" +
                   String(SERVER_PORT_DEFAULT) + "/reset";

// Chân nút bấm trên ESP (Ví dụ nút BOOT)
#define HW_BUTTON_PIN 0

// Trạng thái hệ thống
enum State { IDLE, LEVEL_1, LEVEL_2, SOS_TRIGGERED, WAIT_FOR_BUTTON };

State currentState = IDLE;

// Các biến lưu trữ dữ liệu từ API
String fallTimeStr = "";
String evidenceUrl = "";

// Các cờ và biến thời gian
float custom_safe_duration = 5.0; // Mặc định 5 phút (tránh gửi 0)

unsigned long level1StartTime = 0;
bool level1Notified = false;
unsigned long level2StartTime = 0;
bool level2Notified = false;

// [XIAOZHI TẠM TẮT] Sub-state cho LEVEL_1
// enum Level1Phase { L1_PLAYING, L1_LISTENING };
// Level1Phase level1Phase = L1_PLAYING;
// unsigned long listeningStartTime = 0;
// const unsigned long LISTENING_WINDOW = 3000;

int blynk_v0_state = 0;
unsigned long lastV6Write = 0;

// Danh sách số điện thoại nhận SOS
String SDT[] = {"0123456789", "0987654321"};

// --- WiFi Reconnect ---
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000; // Check WiFi mỗi 30 giây

// Forward declarations
void checkApiStatus();
void sendSafeStatusToServer();
void resetSystem();
void sendSOS();
void handleStateLogic();
void triggerSOS();
void setupWebServer();
void ensureWiFiConnected();

// ====================== WiFi RECONNECT ======================
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.println("[WiFi] Mất kết nối! Đang thử kết nối lại...");
  WiFi.disconnect();
  WiFi.begin(ssid, pass);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✅ Đã kết nối lại! IP: %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] ❌ Không thể kết nối lại.");
  }
}

// BƯỚC 1: Hàm checkApiStatus để nhận các thông tin từ server
void checkApiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(server_url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);

      String typeStr = doc["type"].as<String>();
      bool fallDetected = doc["fall_detected"];

      if (currentState == IDLE) {
        Serial.println("[HỆ THỐNG] Đang trong trạng thái AN TOÀN. ");
      }
      if (typeStr == "apiSendSafe") {
        float remaining_seconds = doc["safe_remaining_seconds"].as<float>();
        Blynk.virtualWrite(V5, remaining_seconds);
        Serial.printf("Thời gian duy trì còn lại: %.1f phút\n",
                      remaining_seconds / 60);
      } else if (typeStr == "apiSendFall" && fallDetected &&
                 currentState == IDLE) {
        fallTimeStr = doc["fall_time"].as<String>();

        // Build evidence URL
        String baseUrl =
            "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT_DEFAULT);
        evidenceUrl = baseUrl + doc["evidence"].as<String>();

        currentState = LEVEL_1;
        level1StartTime = millis();
        level1Notified = false;

        Serial.println("[CẢNH BÁO] Phát hiện có người bị ngã trong nhà ");
        Serial.println("Thời gian: ");
        Serial.println(fallTimeStr);
      }
    } else {
      Serial.printf("[API] GET failed, error: %s (Code: %d)\n",
                    http.errorToString(httpCode).c_str(), httpCode);
    }
    http.end();
  }
}

// BƯỚC 2: Hàm thực hiện POST xác nhận an toàn lên Server API
void sendSafeStatusToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    String postUrl = "http://" + String(SERVER_IP) + ":" +
                     String(SERVER_PORT_DEFAULT) + "/response-from-esp32";

    HTTPClient http;
    http.begin(postUrl);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"safe\": true, \"safe_duration_minutes\": " +
                     String(custom_safe_duration) + "}";

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.printf("[API] Đã gửi xác nhận an toàn lên Server API (%.1f phút), "
                    "code: %d\n",
                    custom_safe_duration, httpCode);
    } else {
      Serial.printf("[API] Lỗi khi gửi xác nhận an toàn: %s\n",
                    http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void setupWebServer() {
  // Thêm endpoint /ping để kiểm tra ESP32 còn sống
  localServer.on("/ping", HTTP_GET, []() {
    localServer.send(200, "application/json",
                     "{\"status\":\"ok\",\"device\":\"esp32-fall-detect\"}");
  });

  localServer.begin();
  Serial.println("[HỆ THỐNG] WebServer nội bộ đã khởi chạy ở cổng 80.");
}

// BƯỚC 5: Hàm reset về trạng thái ban đầu cho hệ thống
void resetSystem() {
  Serial.println("[HỆ THỐNG] Resetting system to IDLE...");
  currentState = IDLE;
  level1Notified = false;
  level2Notified = false;
  blynk_v0_state = 0;
  lastV6Write = 0;
  fallTimeStr = "";
  evidenceUrl = "";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(reset_url);
    http.POST("");
    http.end();
  }

  // Khôi phục TẤT CẢ các chân Blynk về trạng thái an toàn ban đầu
  Blynk.virtualWrite(V0, 0);         // Tắt nút I'm OK
  Blynk.virtualWrite(V1, "clr");     // Xóa Terminal cảnh báo
  Blynk.setProperty(V3, "urls", ""); // Xóa ảnh bằng chứng
  Blynk.virtualWrite(V6, 0);         // Tắt đèn SOS khẩn cấp
  Blynk.virtualWrite(V4, 0);         // Tắt nút Gọi Cấp Cứu (Level 1)
  Blynk.virtualWrite(V11, 0);        // Tắt nút An Toàn
  Blynk.virtualWrite(V12, 0);        // Tắt nút SOS
}

// BƯỚC 9: Xây dựng hàm SOS() — Gửi qua Telegram
void sendSOS() {
  Serial.println("==================================");
  Serial.println(
      "THỰC HIỆN GỬI SOS ĐẾN CƠ QUAN Y TẾ VÀ NGƯỜI NHÀ QUA TELEGRAM");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Bỏ qua SSL Certificate do Telegram dùng HTTPS

    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) +
                 "/sendMessage";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    String textMsg = "🚨 <b>CẢNH BÁO PHÁT HIỆN NGÃ!</b> 🚨\n"
                     "⏰ <b>Thời gian:</b> " +
                     fallTimeStr +
                     "\n"
                     "📍 <b>Địa chỉ:</b> " +
                     String(DIA_CHI_NHA) +
                     "\n"
                     "📞 <b>SDT Người thân:</b> " +
                     SDT[0] + " / " + SDT[1] +
                     "\n"
                     "📸 <b>Trạng thái hiện tại:</b> <a href='" +
                     evidenceUrl + "'>Nhấn vào đây để xem ảnh</a>";

    Serial.println("Nội dung chuẩn bị gửi:\n" + textMsg);

    JsonDocument doc;
    doc["chat_id"] = TELEGRAM_CHAT_ID;
    doc["text"] = textMsg;
    doc["parse_mode"] = "HTML";

    String payload;
    serializeJson(doc, payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200) {
      Serial.println("[TELEGRAM] Đã gửi tin nhắn SOS thành công!");
    } else {
      Serial.printf("[TELEGRAM] Lỗi gửi SOS, HTTP Code: %d\n",
                    httpResponseCode);
      String response = http.getString();
      Serial.println("Chi tiết lỗi từ Telegram: " + response);
    }
    http.end();
  } else {
    Serial.println("[LỖI] Không có kết nối WiFi để gửi Telegram SOS!");
  }
  Serial.println("==================================");
}

// Wrapper: Gọi SOS và chuyển sang trạng thái chờ xác nhận
void triggerSOS() {
  Blynk.virtualWrite(V1, "\n🚨Đã kích hoạt chương trình cứu hộ khẩn cấp🚨\n");
  sendSOS();
  currentState = WAIT_FOR_BUTTON;
  lastV6Write = 0;
}

// BƯỚC 6, 7, 8: Hàm xử lý loop logic cho hệ thống
void handleStateLogic() {
  switch (currentState) {

  // ── GIAI ĐOẠN 1: PHÁT HIỆN NGÃ — CHỜ NẠN NHÂN XÁC NHẬN ──
  // [XIAOZHI TẠM TẮT] Không gọi Xiaozhi, chỉ hiện nút trên Blynk
  case LEVEL_1: {
    if (!level1Notified) {
      Serial.println(
          "[LEVEL 1] Phát hiện ngã! Đang chờ phản hồi từ nạn nhân...");
      Blynk.virtualWrite(V0, 1); // Bật nút I'm OK
      Blynk.virtualWrite(V4, 1); // Bật nút Gọi Cấp Cứu

      // Gửi thông báo lên terminal V1
      String msg = "⚠️ Phát hiện ngã lúc " + fallTimeStr +
                   "\nBạn có ổn không? Hãy bấm nút I'm OK!";
      Blynk.virtualWrite(V1, msg);
      level1Notified = true;
    }

    // Trong vòng 2 phút (LEVEL_1_TIMEOUT), chờ phản hồi
    if (millis() - level1StartTime >= LEVEL_1_TIMEOUT) {
      Serial.println("[LEVEL 1] Hết 2 phút, nạn nhân không phản hồi. Chuyển "
                     "sang LEVEL 2.");
      currentState = LEVEL_2;
      level2StartTime = millis();
      level2Notified = false;
    }
    // Xử lý nút V0 (I'm OK) và V4 (Gọi Cấp Cứu) do BLYNK_WRITE() đảm nhiệm
    break;
  }

  // ── GIAI ĐOẠN 2: NẠN NHÂN KHÔNG NHẬN THỨC — CHỜ NGƯỜI NHÀ ──
  case LEVEL_2: {
    if (!level2Notified) {
      String msg = "\n🚨 Cảnh báo: Có người bị ngã, hãy check cam ngay!\n";
      msg += "Giờ ngã: " + fallTimeStr;
      Blynk.virtualWrite(V1, msg);
      Blynk.setProperty(V3, "urls", evidenceUrl);
      Serial.println(evidenceUrl);
      Blynk.virtualWrite(V11, 1); // Bật nút "An Toàn"
      Blynk.virtualWrite(V12, 1); // Bật nút "SOS"
      level2Notified = true;
    }

    // Quá 3 phút mà người nhà không bấm → Tự động gửi SOS
    if (millis() - level2StartTime >= LEVEL_2_TIMEOUT) {
      Serial.println(
          "[LEVEL 2] Quá thời gian không phản hồi. Tự động kích hoạt SOS!");
      triggerSOS();
    }
    break;
  }

  // ── TRẠNG THÁI CHỜ XÁC NHẬN SAU SOS ──
  case WAIT_FOR_BUTTON: {
    unsigned long now = millis();
    if (now - lastV6Write >= 500) {
      Blynk.virtualWrite(V6, 1); // Duy trì đèn đỏ khẩn cấp
      lastV6Write = now;
    }
    break;
  }

  case IDLE:
  default:
    break;
  }
}

// ================= XỬ LÝ SỰ KIỆN TỪ BLYNK APP =================

// [V0] Nút "I'm OK" — Nạn nhân xác nhận an toàn tại LEVEL_1
BLYNK_WRITE(V0) {
  blynk_v0_state = param.asInt();
  if (blynk_v0_state == 0 && currentState == LEVEL_1) {
    Serial.println(
        "[LEVEL 1] Nạn nhân xác nhận AN TOÀN qua nút I'm OK trên Blynk!");
    Blynk.virtualWrite(V1, "clr");
    sendSafeStatusToServer();
    resetSystem();
  }
}

// [V4] Nút "Gọi Cấp Cứu" — Nạn nhân tự kích hoạt SOS tại LEVEL_1
BLYNK_WRITE(V4) {
  if (param.asInt() == 1 && currentState == LEVEL_1) {
    Serial.println("[LEVEL 1] Nạn nhân tự kích hoạt SOS khẩn cấp qua V4!");
    triggerSOS();
  }
}

// [V11] Nút "An Toàn" — Người nhà xác nhận nạn nhân ổn tại LEVEL_2
BLYNK_WRITE(V11) {
  if (param.asInt() == 1 && currentState == LEVEL_2) {
    Serial.println("[LEVEL 2] Người nhà xác nhận AN TOÀN qua V11!");
    Blynk.virtualWrite(V12, 0);
    Blynk.virtualWrite(V1, "clr");
    sendSafeStatusToServer();
    resetSystem();
  }
}

// [V12] Nút "SOS" — Người nhà chủ động gọi cấp cứu tại LEVEL_2
BLYNK_WRITE(V12) {
  if (param.asInt() == 1 && currentState == LEVEL_2) {
    Serial.println("[LEVEL 2] Người nhà chủ động kích hoạt SOS qua V12!");
    triggerSOS();
  }
}

// [V5] Nhận thời gian an toàn tùy chỉnh từ Blynk Slider
BLYNK_WRITE(V5) {
  custom_safe_duration = param.asFloat();
  Serial.printf("[BLYNK] Đã cập nhật thời gian an toàn tùy chỉnh: %.1f phút\n",
                custom_safe_duration);
  sendSafeStatusToServer();
}

// [V6] Đèn SOS khẩn cấp — Người nhà xác nhận đã biết, reset hệ thống
BLYNK_WRITE(V6) {
  int v6_state = param.asInt();
  if (v6_state == 1 && currentState == WAIT_FOR_BUTTON) {
    Serial.println(
        "[HỆ THỐNG] Người nhà xác nhận ĐÃ BIẾT tin cấp cứu. Reset về IDLE.");
    resetSystem();
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("   HỆ THỐNG PHÁT HIỆN NGÃ - ESP32-S3");
  Serial.println("   (Static IP - Hotspot Connection)");
  Serial.println("========================================\n");

  // Cấu hình nút bấm
  pinMode(HW_BUTTON_PIN, INPUT_PULLUP);

  // Kết nối WiFi
  Serial.println("[WiFi] Đang kết nối WiFi...");
  WiFi.begin(ssid, pass);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED &&
         attempt < 60) { // Tăng thời gian chờ lên 30 giây (60 * 500ms) vì
                         // Windows Hotspot cấp IP hơi chậm
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✅ Đã kết nối! IP ESP32: %s\n",
                  WiFi.localIP().toString().c_str());
    Serial.printf("[API]  ✅ Server IP: %s\n", SERVER_IP);
    Serial.printf("[API]  ✅ Server URL: %s\n", server_url.c_str());

    // Khởi chạy WebServer
    setupWebServer();

    // Cấu hình Blynk
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
  } else {
    Serial.println("\n[LỖI] Không thể kết nối WiFi. Sẽ thử kết nối lại...");
  }

  Serial.println("[HỆ THỐNG] Khởi động hoàn tất!\n");
}

// ================= MAIN LOOP =================
void loop() {
  // --- WiFi reconnect ---
  if (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    ensureWiFiConnected();
  }

  // --- Blynk ---
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }

  // --- Kiểm tra API mỗi 2 giây nếu đang ở trạng thái IDLE ---
  static unsigned long lastApiCheck = 0;
  if (millis() - lastApiCheck > 2000) {
    lastApiCheck = millis();
    if (currentState == IDLE) {
      checkApiStatus();
    }
  }

  // --- State machine ---
  handleStateLogic();

  // --- WebServer ---
  localServer.handleClient();
}