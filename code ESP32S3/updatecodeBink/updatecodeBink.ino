// --- Blynk Credentials ---
#define BLYNK_TEMPLATE_ID "TMPL6nXDjldOr"
#define BLYNK_TEMPLATE_NAME "FallDetection"
#define BLYNK_AUTH_TOKEN "KveCGf33u8V1WhImVQS2E3YHGdjnCSYy"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <WiFi.h>



// --- WiFi Credentials ---
const char ssid[] = "Xiaomi";
const char pass[] = "12345679@";

// --- API Server Settings ---
// LƯU Ý: Thay đổi IP này thành IP máy tính của bạn (chạy FastAPI)
// Ví dụ: "http://192.168.1.10:8000"
const char *server_url = "http://10.128.150.225:8000/status";
const char *reset_url = "http://10.128.150.225:8000/reset";

// --- Notification Settings ---
#define NOTIFICATION_INTERVAL 20000 // 20 giây một lần ở Level 2
#define LEVEL_1_TIMEOUT 120000      // 2 phút
#define LEVEL_2_TIMEOUT 300000      // 5 phút

// Bảng chân I2S cho Audio (Tham chiếu)
#define I2S_EN_PIN 1
#define I2S_MCLK_PIN 4
#define I2S_BCLK_PIN 5
#define I2S_DOUT_PIN 6
#define I2S_LRCK_PIN 7
#define I2S_DIN_PIN 8

// Chân nút bấm trên ESP (Ví dụ nút BOOT)
#define HW_BUTTON_PIN 0

// Trạng thái hệ thống
enum State { IDLE, LEVEL_1, LEVEL_2, SOS_TRIGGERED, WAIT_FOR_BUTTON };

State currentState = IDLE;

// Các biến lưu trữ dữ liệu từ API
String fallTimeStr = "";
String evidenceUrl = "";

// Các cờ và biến thời gian
bool isApiPaused = false;
unsigned long apiPauseStartTime = 0;
const unsigned long API_PAUSE_DURATION = 15 * 60 * 1000; // 15 phút

unsigned long level1StartTime = 0;
bool level1Notified = false;
unsigned long level2StartTime = 0;
bool level2Notified = false;

int blynk_v0_state = 0; // Lưu trạng thái nút V0 (I'm OK) trên blynk
unsigned long lastV6Write =
    0; // Timer cho việc giữ đèn V6 trong WAIT_FOR_BUTTON

// Danh sách số điện thoại nhận SOS (Cài đặt cố định)
String SDT[] = {"0123456789", "0987654321"};

// Khai báo hàm
void checkApiStatus();
void pauseApiRetrieval();
void playAudio(int caseType);
bool checkVoiceCommand();
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

      bool fallDetected = doc["fall_detected"];

      if (fallDetected && currentState == IDLE) {
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

// BƯỚC 2: Hàm không truy xuất API trong 15 phút
void pauseApiRetrieval() {
  isApiPaused = true;
  apiPauseStartTime = millis();
  Serial.println("[HỆ THỐNG] Tạm dừng truy xuất API trong 15 phút.");
}

// BƯỚC 3: Hàm phát âm thanh dựa trên case
void playAudio(int caseType) {
  // Low level enable Audio dựa theo bảng I2S En
  digitalWrite(I2S_EN_PIN, LOW);

  if (caseType == 1) {
    // Phát âm thanh case 1
    // TODO: Bổ sung code đọc file am thanh 1 ra I2S tại đây
  } else if (caseType == 2) {
    // Phát âm thanh case 2
    // TODO: Bổ sung code đọc file am thanh 2 ra I2S tại đây
  }

  // High level disable Audio sau khi dùng
  digitalWrite(I2S_EN_PIN, HIGH);
}

// BƯỚC 4: Hàm ghi âm thanh bằng mic của esp
bool checkVoiceCommand() {
  // Ghi âm bằng I2S (I08 làm DIN) và phân tích giọng nói "tôi ổn"
  // Logic giả lập trả về False hiện tại
  return false;
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
  Blynk.virtualWrite(V1, "");        // Xóa label cảnh báo
  Blynk.virtualWrite(V2, "");        // Xóa label thời gian
  Blynk.setProperty(V3, "urls", ""); // Xóa ảnh bằng chứng
  Blynk.virtualWrite(V6, 0);         // Tắt đèn SOS khẩn cấp
  Blynk.virtualWrite(V10, 0);        // Tắt nút Gọi Cấp Cứu (Level 1)
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
}

// BƯỚC 6, 7, 8: Hàm xử lý loop logic cho hệ thống
void handleStateLogic() {
  switch (currentState) {

  // ── GIAI ĐOẠN 1: PHÁT HIỆN NGÃ, NẠN NHÂN CÒN NHẬN THỨC ──
  case LEVEL_1: {
    // Thông báo 1 lần duy nhất khi mới vào Level 1
    if (!level1Notified) {
      Serial.println(
          "[LEVEL 1] Phát hiện ngã! Đang chờ phản hồi từ nạn nhân...");
      Blynk.virtualWrite(V0,
                         1); // Bật nút I'm OK (đỏ) — nạn nhân gạt về 0 nếu ổn
      Blynk.virtualWrite(V10, 1); // Bật nút Gọi Cấp Cứu (Level 1)
      level1Notified = true;
    }

    // Trong vòng 2 phút (120,000 ms): phát âm thanh + lắng nghe giọng nói
    if (millis() - level1StartTime < 120000) {
      playAudio(1); // Phát âm thanh cảnh báo Level 1
      bool saySafe =
          checkVoiceCommand(); // Ghi âm và đối chiếu "tôi ổn" (Non-blocking)

      if (saySafe) {
        Serial.println("[LEVEL 1] Xác nhận an toàn từ giọng nói!");
        pauseApiRetrieval();
        resetSystem();
      }
      // Xử lý nút V0 (I'm OK) và V10 (Gọi Cấp Cứu) do BLYNK_WRITE() đảm nhiệm
    } else {
      // Hết 2 phút không phản hồi → Chuyển qua LEVEL 2
      Serial.println("[LEVEL 1] Hết 2 phút, nạn nhân không phản hồi. Chuyển "
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
      Blynk.virtualWrite(V1, "Cảnh báo: Có người bị ngã hãy check cam ngay");
      Blynk.virtualWrite(V2, fallTimeStr);
      Blynk.setProperty(V3, "urls", evidenceUrl); // Tải URL ảnh bằng chứng
      Blynk.virtualWrite(V11, 1); // Bật nút "An Toàn" (xanh) cho người nhà
      Blynk.virtualWrite(V12, 1); // Bật nút "SOS" (đỏ) cho người nhà
      level2Notified = true;
      Serial.println("[LEVEL 2] Đã đẩy cảnh báo lên Blynk (V1, V2, V3, V11, "
                     "V12). Đang đếm 3 phút...");
    }

    // Quá 3 phút (180,000 ms) mà người nhà không bấm → Tự động gửi SOS
    if (millis() - level2StartTime >= 180000) {
      Serial.println(
          "[LEVEL 2] Quá 3 phút không phản hồi. Tự động kích hoạt SOS!");
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
    pauseApiRetrieval(); // Tạm dừng truy xuất API 15 phút
    resetSystem();       // Đưa hệ thống về IDLE
  }
}

// [V10] Nút "Gọi Cấp Cứu" (Push) — Nạn nhân tự kích hoạt SOS tại LEVEL_1
BLYNK_WRITE(V10) {
  if (param.asInt() == 1 && currentState == LEVEL_1) {
    Serial.println("[LEVEL 1] Nạn nhân tự kích hoạt SOS khẩn cấp qua V10!");
    triggerSOS(); // Gọi SOS và chuyển sang WAIT_FOR_BUTTON
  }
}

// [V11] Nút "An Toàn" (Push) — Người nhà xác nhận nạn nhân ổn tại LEVEL_2
BLYNK_WRITE(V11) {
  if (param.asInt() == 1 && currentState == LEVEL_2) {
    Serial.println("[LEVEL 2] Người nhà xác nhận AN TOÀN qua V11!");
    Blynk.virtualWrite(V12, 0); // Tắt nút SOS còn lại
    pauseApiRetrieval();        // Tạm dừng truy xuất API 15 phút
    resetSystem();              // Đưa hệ thống về IDLE
  }
}

// [V12] Nút "SOS" (Push) — Người nhà chủ động gọi cấp cứu tại LEVEL_2
BLYNK_WRITE(V12) {
  if (param.asInt() == 1 && currentState == LEVEL_2) {
    Serial.println("[LEVEL 2] Người nhà chủ động kích hoạt SOS qua V12!");
    triggerSOS(); // Gọi SOS và chuyển sang WAIT_FOR_BUTTON
  }
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

  // BƯỚC 2: Kiểm tra cờ tạm dừng truy xuất API (15 phút)
  if (isApiPaused) {
    if (millis() - apiPauseStartTime > API_PAUSE_DURATION) {
      isApiPaused = false;
      Serial.println(
          "[HỆ THỐNG] Đã hết 15 phút tạm dừng API. Bắt đầu theo dõi lại.");
    }
  }

  // Kiểm tra API mỗi 2 giây nếu đang ở trạng thái IDLE và không bị tạm dừng
  static unsigned long lastApiCheck = 0;
  if (!isApiPaused && (millis() - lastApiCheck > 2000)) {
    lastApiCheck = millis();
    if (currentState == IDLE) {
      checkApiStatus();
    }
  }

  handleStateLogic();
}