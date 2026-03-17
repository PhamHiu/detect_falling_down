#include "Config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <WiFi.h>

// Trạng thái hệ thống
enum State { IDLE, FALL_DETECTED, LEVEL_1, LEVEL_2, SOS_TRIGGERED };

State currentState = IDLE;
unsigned long fallStartTime = 0;
unsigned long lastNotifyTime = 0;
bool isAwaitingSafe = false;

// Khai báo hàm
void checkApiStatus();
void handleStateLogic();
void resetSystem();

// Blynk: Nút bấm "Tôi ổn" từ app
BLYNK_WRITE(V1) {
  int value = param.asInt();
  if (value == 1) {
    Serial.println("[BLYNK] Người dùng xác nhận: TÔI ỔN");
    resetSystem();
  }
}

// Blynk: Nút bấm "CẤP CỨU" từ app
BLYNK_WRITE(V2) {
  int value = param.asInt();
  if (value == 1) {
    Serial.println("[BLYNK] Người thân yêu cầu: CẤP CỨU KHẨN CẤP");
    currentState = SOS_TRIGGERED;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Kết nối WiFi và Blynk
  Serial.println("Đang kết nối WiFi...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Serial.print("WiFi connected! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Hệ thống khởi động thành công!");
}

void loop() {
  Blynk.run();

  // Kiểm tra API mỗi 2 giây nếu đang ở trạng thái IDLE
  static unsigned long lastApiCheck = 0;
  if (millis() - lastApiCheck > 2000) {
    lastApiCheck = millis();
    if (currentState == IDLE) {
      checkApiStatus();
    }
  }

  handleStateLogic();
}

void checkApiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(server_url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);

      bool fallDetected = doc["fall_detected"];
      if (fallDetected && currentState == IDLE) {
        currentState = FALL_DETECTED;
        fallStartTime = millis();
        Serial.println("[CẢNH BÁO] Phát hiện ngã từ AI Server!");
        Blynk.logEvent("fall_alert",
                       "Phát hiện có người ngã! Đang kiểm tra trạng thái...");
      }
    } else {
      Serial.printf("[API] GET failed, error: %s (Code: %d)\n",
                    http.errorToString(httpCode).c_str(), httpCode);
    }
    http.end();
  }
}

void handleStateLogic() {
  unsigned long elapsed = millis() - fallStartTime;

  switch (currentState) {
  case FALL_DETECTED:
    Serial.println("[LEVEL 1] Đang hỏi: 'Bạn có ổn không?'");
    // Giả lập phát loa qua Serial hoặc board DAC
    currentState = LEVEL_1;
    break;

  case LEVEL_1:
    if (elapsed > LEVEL_1_TIMEOUT) {
      Serial.println("[LEVEL 1] Hết thời gian chờ. Chuyển sang LEVEL 2.");
      currentState = LEVEL_2;
      lastNotifyTime = 0; // Gửi thông báo ngay lập tức
    }
    break;

  case LEVEL_2:
    if (millis() - lastNotifyTime > NOTIFICATION_INTERVAL) {
      lastNotifyTime = millis();
      Serial.println("[LEVEL 2] Gửi thông báo khẩn cấp tới Blynk...");
      Blynk.logEvent("emergency_push",
                     "CẢNH BÁO: Người thân ngã lâu không phản hồi!");
    }

    if (elapsed > LEVEL_2_TIMEOUT) {
      currentState = SOS_TRIGGERED;
    }
    break;

  case SOS_TRIGGERED:
    Serial.println("[SOS] TỰ ĐỘNG KÍCH HOẠT CẤP CỨU!");
    Blynk.logEvent("sos_alert", "SOS: Đã kích hoạt báo động cấp cứu tự động!");
    // Thêm logic gửi SMS hoặc gọi điện ở đây
    delay(5000);
    resetSystem();
    break;

  case IDLE:
  default:
    break;
  }
}

void resetSystem() {
  Serial.println("Resetting system to IDLE...");
  currentState = IDLE;

  // Gọi API Reset để xóa trạng thái trên Server
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(reset_url);
    http.POST("");
    http.end();
  }

  Blynk.virtualWrite(V1, 0); // Reset nút trên app
  Blynk.virtualWrite(V2, 0);
}
