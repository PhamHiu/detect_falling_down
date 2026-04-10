#include "Audio.h"
#include "Config.h"
#include "SD_MMC.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
// Note off my tssk in 21/3/2026
/**
 i will update user interface to connect with code 3 api
 i will  creat al new slider to control safe duration
 next i will  creat a new button to control the system v5
 last this is fixx the picture not show in user interface and connect audio and
 microphone  to system

 you will remember this
 continue to break promblem from big step to esayest step . this is a think you
 can make now on your power. this is a good way to solve promblem  :)))

 ok let sleep now :)))
 */

// --- Audio Paths ---
const char *AUDIO_FILE_1 = "/level1.mp3"; // File âm thanh cho Level 1 (Thẻ SD)
const char *AUDIO_FILE_2 = "/level2.mp3"; // File âm thanh cho Level 2 (Thẻ SD)

// Bảng chân I2S cho Audio (Tham chiếu)
#define I2S_EN_PIN 1
#define I2S_MCLK_PIN 4
#define I2S_BCLK_PIN 5
#define I2S_DOUT_PIN 6
#define I2S_LRCK_PIN 7

// Chân nút bấm trên ESP (Ví dụ nút BOOT)
#define HW_BUTTON_PIN 0

// Trạng thái hệ thống
enum State { IDLE, LEVEL_1, LEVEL_2, SOS_TRIGGERED, WAIT_FOR_BUTTON };

State currentState = IDLE;

// Các biến lưu trữ dữ liệu từ API
String fallTimeStr = "";
String evidenceUrl = "";

// Các cờ và biến thời gian
float custom_safe_duration = 0.0; // Lưu trữ thời gian safe từ Blynk

unsigned long level1StartTime = 0;
bool level1Notified = false;
unsigned long level2StartTime = 0;
bool level2Notified = false;

int blynk_v0_state = 0; // Lưu trạng thái nút V0 (I'm OK) trên blynk
unsigned long lastV6Write =
    0; // Timer cho việc giữ đèn V6 trong WAIT_FOR_BUTTON

// Khởi tạo đối tượng Audio và I2S
Audio audio;

// Danh sách số điện thoại nhận SOS (Cài đặt cố định)
String SDT[] = {"0123456789", "0987654321"};

// Khai báo hàm
void checkApiStatus();
void sendSafeStatusToServer();
void playAudio(int caseType);
void resetSystem();
void sendSOS();
void handleStateLogic();
void triggerSOS(); // Wrapper: sendSOS() + chuyển sang WAIT_FOR_BUTTON

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

      if (typeStr == "apiSendSafe") {
        float remaining_seconds = doc["safe_remaining_seconds"].as<float>();
        Serial.printf("[HỆ THỐNG] Đang trong trạng thái AN TOÀN. Thời gian duy "
                      "trì còn lại: %.1f giây\n",
                      remaining_seconds);
      } else if (typeStr == "apiSendFall" && fallDetected &&
                 currentState == IDLE) {
        fallTimeStr = doc["fall_time"].as<String>();

        // Lấy domain gốc (cắt bỏ phần đuôi đằng sau mốc Port :8000)
        String baseUrl = String(server_url);
        int portIndex = baseUrl.indexOf(":8000");
        if (portIndex > 0) {
          baseUrl = baseUrl.substring(0, portIndex + 5);
        }

        evidenceUrl = baseUrl + doc["evidence"].as<String>();

        currentState = LEVEL_1;
        level1StartTime = millis();
        level1Notified = false; // Reset cờ thông báo lúc rơi ngã

        Serial.println("[CẢNH BÁO] Phát hiện ngã từ API Server!");
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
    String baseUrl = String(server_url);
    int portIndex = baseUrl.indexOf(":8000");
    if (portIndex > 0) {
      baseUrl = baseUrl.substring(0, portIndex + 5);
    }
    String postUrl = baseUrl + "/response-from-esp32";

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

// BƯỚC 3: Hàm phát âm thanh dựa trên tệp trong thẻ SD
void playAudio(int caseType) {
  // Nếu đang phát rồi thì không làm gì cả để tránh bị vấp (stutter)
  if (audio.isRunning())
    return;

  // BẬT Audio Enable (Mức logic LOW = enable)
  digitalWrite(I2S_EN_PIN, LOW);

  if (caseType == 1) {
    audio.connecttoFS(SD_MMC, AUDIO_FILE_1);
  } else if (caseType == 2) {
    audio.connecttoFS(SD_MMC, AUDIO_FILE_2);
  }
}

// BƯỚC 5: Hàm reset về trạng thái ban đầu cho hệ thống
void resetSystem() {
  Serial.println("[HỆ THỐNG] Resetting system to IDLE...");

  // Dừng phát âm thanh ngay lập tức nếu đang phát
  if (audio.isRunning()) {
    audio.stopSong();
  }
  digitalWrite(I2S_EN_PIN, HIGH); // Tắt loa

  currentState = IDLE;
  level1Notified = false;
  level2Notified = false;
  blynk_v0_state = 0;

  Blynk.virtualWrite(V1,
                     "\nHệ thống an toàn.\n"); // Xóa thông báo cũ trên màn hình

  // Tắt giao diện Blynk (đưa nút về trạng thái tắt)
  evidenceUrl = "";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(reset_url);
    http.POST("");
    http.end();
  }

  // Khôi phục TẤT CẢ các chân Blynk về trạng thái an toàn ban đầu
  Blynk.virtualWrite(V0, 0);         // Tắt nút I'm OK
  Blynk.virtualWrite(V1, "");        // Xóa Terminal cảnh báo
  Blynk.setProperty(V3, "urls", ""); // Xóa ảnh bằng chứng
  Blynk.virtualWrite(V6, 0);         // Tắt đèn SOS khẩn cấp
  Blynk.virtualWrite(V4, 0);         // Tắt nút Gọi Cấp Cứu (Level 1)
  Blynk.virtualWrite(V11, 0);        // Tắt nút An Toàn
  Blynk.virtualWrite(V12, 0);        // Tắt nút SOS
}

// BƯỚC 9: Xây dựng hàm SOS() — Chỉ gửi thông tin, KHÔNG block vòng lặp
void sendSOS() {
  Serial.println("==================================");
  Serial.println("THỰC HIỆN GỬI SOS ĐẾN CƠ QUAN Y TẾ VÀ NGƯỜI NHÀ");

  String msg = "Thông cáo: Bạn có người nhà đang nguy hiểm!!!\n";
  msg += "Thời gian: " + fallTimeStr + "\n";
  msg += "Bằng chứng: " + evidenceUrl + "\n";
  msg += "SĐT người thân: " + SDT[0] + "\n";

  Serial.println(msg);

  for (int i = 0; i < sizeof(SDT) / sizeof(SDT[0]); i++) {
    Serial.println("-> Đang gửi thông báo đến SĐT: " + SDT[i]);
  }
  Serial.println("==================================");
  // Sau khi gửi SOS, nơi gọi hàm phải set currentState = WAIT_FOR_BUTTON
  // Việc bật V6 liên tục sẽ do handleStateLogic() xử lý (non-blocking)
}

// Wrapper tiện ích: Gọi SOS và chuyển sang trạng thái chờ xác nhận
void triggerSOS() {
  sendSOS();
  currentState = WAIT_FOR_BUTTON;
  lastV6Write = 0; // Reset timer để V6 bật ngay lập tức
  Blynk.virtualWrite(V1, "ĐÃ KÍCH HOẠT CHƯƠNG TRÌNH SOS!");
}

// BƯỚC 6, 7, 8: Hàm xử lý loop logic cho hệ thống
void handleStateLogic() {
  switch (currentState) {

  // ── GIAI ĐOẠN 1: PHÁT HIỆN NGÃ, NẠN NHÂN CÒN NHẬN THỨC ──
  // Phát âm thanh báo động tuần tự lặp đi lặp lại
  case LEVEL_1: {
    // Thông báo 1 lần duy nhất khi mới vào Level 1
    if (!level1Notified) {
      Serial.println("[LEVEL 1] Phát hiện ngã! Đang phát âm thanh cảnh báo...");
      Blynk.virtualWrite(V1, "Bạn có ổn không?"); // Hiển thị trên màn hình V1
      Blynk.virtualWrite(V0,
                         1); // Bật nút I'm OK (đỏ) — nạn nhân gạt về 0 nếu ổn
      Blynk.virtualWrite(V4, 1); // Bật nút Gọi Cấp Cứu (Level 1)
      level1Notified = true;
    }

    // Liên tục phát âm thanh cảnh báo ngã ở mức Level 1
    if (!audio.isRunning()) {
      playAudio(1);
    }

    // Sau 2 phút (LEVEL_1_TIMEOUT ms)
    if (millis() - level1StartTime >= LEVEL_1_TIMEOUT) {
      // Hết 2 phút không ai bấm nút → Chuyển qua LEVEL 2
      Serial.println("[LEVEL 1] Hết 2 phút, không có phản hồi từ Blynk. Chuyển "
                     "sang LEVEL 2.");
      currentState = LEVEL_2;
      level2StartTime = millis();
      level2Notified = false;
    }
    break;
  }

  // ── GIAI ĐOẠN 2: NẠN NHÂN KHÔNG NHẬN THỨC — CHỜ NGƯỜI NHÀ ──
  case LEVEL_2: {
    // Đẩy thông báo & bật các nút hành động 1 lần khi vào Level 2
    if (!level2Notified) {
      String msg = "Cảnh báo: Có người bị ngã hãy check cam ngay\n";
      msg += "Giờ ngã: " + fallTimeStr;
      Blynk.virtualWrite(V1, msg); // Gửi vào Terminal V1
      Blynk.setProperty(V3, "urls", evidenceUrl);
      Serial.println(evidenceUrl);
      Blynk.virtualWrite(V11, 1); // Bật nút "An Toàn" (xanh) cho người nhà
      Blynk.virtualWrite(V12, 1); // Bật nút "SOS" (đỏ) cho người nhà

      level2Notified = true;
    }

    // Tiếp tục dồn ép và cảnh báo liên tục (Còi hú nguy cấp)
    if (!audio.isRunning()) {
      playAudio(2); // File Level2
    }

    // Quá thời gian Level 2 mà người nhà không bấm → Tự động gửi SOS
    if (millis() - level2StartTime >= LEVEL_2_TIMEOUT) {
      Serial.println("[LEVEL 2] Đã quá thời hạn chờ. Tự động kích hoạt SOS!");
      triggerSOS();
    }
    // Xử lý nút V11 (An Toàn) và V12 (SOS) do BLYNK_WRITE() đảm nhiệm
    break;
  }

  // ── TRẠNG THÁI CHỜ XÁC NHẬN SAU SOS ──
  case WAIT_FOR_BUTTON: {
    // Giữ đèn V6 luôn sáng đỏ, cứ mỗi 500ms ghi 1 lần (non-blocking)
    unsigned long now = millis();
    if (now - lastV6Write >= 500) {
      Blynk.virtualWrite(V6, 1); // Duy trì đèn đỏ khẩn cấp
      lastV6Write = now;
    }
    // Xử lý nút V6 (xác nhận đã biết) do BLYNK_WRITE() đảm nhiệm
    break;
  }

  case IDLE:
  default:
    break;
  }
}

// ================= XỬ LÝ SỰ KIỆN TỪ BLYNK APP =================

// [V0] Nút "I'm OK" (Switch) — Nạn nhân xác nhận an toàn tại LEVEL_1
// Hệ thống ghi V0=1 (bật đỏ). Nạn nhân gạt về OFF (=0) → xác nhận ổn.
BLYNK_WRITE(V0) {
  blynk_v0_state = param.asInt();
  if (blynk_v0_state == 0 && currentState == LEVEL_1) {
    Serial.println(
        "[LEVEL 1] Nạn nhân xác nhận AN TOÀN qua nút I'm OK trên Blynk!");
    sendSafeStatusToServer(); // Gửi trạng thái an toàn lên API
    resetSystem();            // Đưa hệ thống về IDLE
  }
}

// [V4] Nút "Gọi Cấp Cứu" (Push) — Nạn nhân tự kích hoạt SOS tại LEVEL_1
BLYNK_WRITE(V4) {
  if (param.asInt() == 1 && currentState == LEVEL_1) {
    Serial.println("[LEVEL 1] Nạn nhân tự kích hoạt SOS khẩn cấp qua V4!");
    triggerSOS(); // Gọi SOS và chuyển sang WAIT_FOR_BUTTON
  }
}

// [V11] Nút "An Toàn" (Push) — Người nhà xác nhận nạn nhân ổn
BLYNK_WRITE(V11) {
  if (param.asInt() == 1 &&
      (currentState == LEVEL_1 || currentState == LEVEL_2)) {
    Serial.println("[HỆ THỐNG] Người nhà xác nhận AN TOÀN qua V11!");
    Blynk.virtualWrite(V12, 0); // Tắt nút SOS còn lại nếu có
    sendSafeStatusToServer();   // Gửi trạng thái an toàn lên API
    resetSystem();              // Đưa hệ thống về IDLE (tự động tắt loa)
  }
}

// [V12] Nút "SOS" (Push) — Người nhà chủ động gọi cấp cứu tại LEVEL_2
BLYNK_WRITE(V12) {
  if (param.asInt() == 1 && currentState == LEVEL_2) {
    Serial.println("[LEVEL 2] Người nhà chủ động kích hoạt SOS qua V12!");
    triggerSOS(); // Gọi SOS và chuyển sang WAIT_FOR_BUTTON
  }
}

// [V5] Nhận thời gian an toàn tùy chỉnh từ Blynk Slider/Numeric Input
BLYNK_WRITE(V5) {
  custom_safe_duration = param.asFloat();
  Serial.printf("[BLYNK] Đã cập nhật thời gian an toàn tùy chỉnh: %.1f phút\n",
                custom_safe_duration);
  sendSafeStatusToServer(); // Gửi ngay lên server khi thay đổi giá trị
}

// [V6] Đèn SOS khẩn cấp — Người nhà xác nhận đã biết, reset hệ thống
BLYNK_WRITE(V6) {
  int v6_state = param.asInt();
  // Khi người dùng bấm nút V6 (giá trị = 1) để xác nhận đã biết thông tin
  if (v6_state == 1 && currentState == WAIT_FOR_BUTTON) {
    Serial.println(
        "[HỆ THỐNG] Người nhà xác nhận ĐÃ BIẾT tin cấp cứu. Reset về IDLE.");
    resetSystem();
  }
}

void setup() {
  Serial.begin(115200);

  // Cấu hình nút bấm trên ESP (Thường PullUp nội bên trong)
  pinMode(HW_BUTTON_PIN, INPUT_PULLUP);

  // Cấu hình chân Audio Enable (Mức logic theo bảng: High level disable, Low
  // level enable) // BƯỚC 3
  pinMode(I2S_EN_PIN, OUTPUT);
  digitalWrite(I2S_EN_PIN, HIGH);

  // Kết nối WiFi (Non-blocking hoặc Timeout ngắn)
  Serial.println("Đang cấu hình WiFi...");
  WiFi.begin(ssid, pass);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi connected! IP address: ");
    Serial.println(WiFi.localIP());

    // --- mDNS: Tự động tìm IP máy tính qua tên "falldect.local" ---
    String resolvedBaseUrl = "";
    if (MDNS.begin("esp32-falldect")) {
      Serial.println("[mDNS] Đang query falldect.local...");
      IPAddress serverIP =
          MDNS.queryHost(SERVER_MDNS_HOST, 3000); // Chờ tối đa 3s
      if (serverIP != INADDR_NONE && serverIP[0] != 0) {
        resolvedBaseUrl =
            "http://" + serverIP.toString() + ":" + String(SERVER_PORT);
        Serial.println("[mDNS] ✅ Tìm thấy server tại: " + resolvedBaseUrl);
      } else {
        Serial.println(
            "[mDNS] ⚠️ Không tìm thấy falldect.local, dùng IP fallback.");
        resolvedBaseUrl =
            "http://" + String(FALLBACK_IP) + ":" + String(SERVER_PORT);
      }
    } else {
      Serial.println("[mDNS] ❌ Lỗi khởi tạo mDNS, dùng IP fallback.");
      resolvedBaseUrl =
          "http://" + String(FALLBACK_IP) + ":" + String(SERVER_PORT);
    }
    server_url_str = resolvedBaseUrl + "/gate";
    reset_url_str = resolvedBaseUrl + "/reset";
    server_url = server_url_str.c_str();
    reset_url = reset_url_str.c_str();
    Serial.println("[CONFIG] server_url = " + server_url_str);
    Serial.println("[CONFIG] reset_url  = " + reset_url_str);
    // ─────────────────────────────────────────────────────────────

    // --- Khởi tạo Audio & SD Card & I2S Recording ---
    audio.setPinout(I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);
    audio.setVolume(21); // Âm lượng tối đa

    // Cấu hình chân SDIO cho ESP32-S3
    SD_MMC.setPins(38, 40, 39, 41, 48, 47);
    if (!SD_MMC.begin("/sdcard", false)) { // false = 4-bit mode
      Serial.println("[LỖI] Không thể mount thẻ nhớ SD!");
    } else {
      Serial.println("[HỆ THỐNG] Đã mount thẻ nhớ SD thành công.");
    }

    // Cấu hình Blynk (KHÔNG BLOCK HỆ THỐNG nếu WiFi rớt)
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk
        .connect(); // Sẽ cố gắng connect, nếu không được vẫn thoát ra chay tiep
  } else {
    Serial.println(
        "\n[LỖI] Không thể kết nối WiFi, vào mode offline theo dõi.");
  }

  Serial.println("Hệ thống khởi động hoàn tất!");
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

  // Cập nhật Audio và Loa
  audio.loop();

  // Tắt Audio EN khi không phát nữa
  static bool lastRunning = false;
  bool isRunning = audio.isRunning();
  if (lastRunning && !isRunning) {
    digitalWrite(I2S_EN_PIN, HIGH);
    Serial.println("[AUDIO] Đã phát xong, tắt Audio EN.");
  }
  lastRunning = isRunning;
}