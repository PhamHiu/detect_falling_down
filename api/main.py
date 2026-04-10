from fastapi import FastAPI, File, UploadFile, Form
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from datetime import datetime
import shutil
import os
import time
import socket
import threading
import requests

# ── mDNS: Đăng ký tên "falldect.local" lên mạng LAN ──────────────────────────
def _get_local_ip() -> str:
    """Lấy IP LAN thực của máy (không phải 127.0.0.1)"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

def _register_mdns():
    try:
        from zeroconf import Zeroconf, ServiceInfo
        local_ip = _get_local_ip()
        zeroconf = Zeroconf()
        info = ServiceInfo(
            "_http._tcp.local.",
            "FallDetection._http._tcp.local.",
            addresses=[socket.inet_aton(local_ip)],
            port=8000,
            properties={"path": "/gate"},
            server="falldect.local.",
        )
        zeroconf.register_service(info)
        print(f"[mDNS] ✅ Đã đăng ký falldect.local → {local_ip}:8000")
        threading.Event().wait()   # Giữ thread chạy mãi
    except ImportError:
        print("[mDNS] ⚠️  Thư viện 'zeroconf' chưa được cài. Chạy: pip install zeroconf")
    except Exception as e:
        print(f"[mDNS] ❌ Lỗi đăng ký mDNS: {e}")

# Khởi động mDNS trong background thread ngay khi module được import
threading.Thread(target=_register_mdns, daemon=True, name="mdns-register").start()
# ─────────────────────────────────────────────────────────────────────────────

app = FastAPI()

# Cấu hình lưu trữ ảnh
STORAGE_DIR = "api/storage"
if not os.path.exists(STORAGE_DIR):
    os.makedirs(STORAGE_DIR)

# Mount thư mục storage để có thể truy cập ảnh qua URL
app.mount("/evidence", StaticFiles(directory=STORAGE_DIR), name="evidence")

from typing import Optional

# ── Cấu hình Telegram ────────────────────────────────────────────────────────
TELEGRAM_BOT_TOKEN = "8471843401:AAFkEdigc4GtKRUi6Xu_A36SDoRq7y8AcUU"
TELEGRAM_CHAT_ID = "5348036208"

def send_telegram_alert(timestamp: str, image_path: str):
    if TELEGRAM_BOT_TOKEN == "ĐIỀN_TOKEN_CỦA_BẠN_VÀO_ĐÂY" or not TELEGRAM_BOT_TOKEN:
        print("[Telegram] ⚠️  Chưa cấu hình Token, bỏ qua gửi tin nhắn.")
        return
        
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendPhoto"
    caption = (
        f"🚨 <b>CẢNH BÁO PHÁT HIỆN NGÃ!</b> 🚨\n"
        f"⏰ <b>Thời gian:</b> {timestamp}\n"
        f"📍 <b>Địa chỉ:</b> Nhà số 123, Đường ABC, Quận XYZ, TP. Hà Nội\n"
        f"📞 <b>SDT Người thân:</b> 0123456789 / 0987654321\n"
        f"📸 <b>Trạng thái hiện tại:</b> Hình ảnh đính kèm bên trên"
    )
    
    try:
        with open(image_path, "rb") as img:
            files = {"photo": img}
            data = {"chat_id": TELEGRAM_CHAT_ID, "caption": caption, "parse_mode": "HTML"}
            response = requests.post(url, data=data, files=files, timeout=15)
            if response.status_code == 200:
                print("[Telegram] ✅ Đã gửi cảnh báo và hình ảnh thành công!")
            else:
                print(f"[Telegram] ❌ Lỗi từ Telegram: {response.text}")
    except Exception as e:
        print(f"[Telegram] ❌ Lỗi kết nối gửi tin nhắn: {e}")
# ─────────────────────────────────────────────────────────────────────────────

# Trạng thái hiện tại
class FallStatus:
    def __init__(self):
        self.fall_detected = False
        self.fall_time = ""
        self.evidence_url = ""
        
        # Safe mode properties
        self.safe_mode: bool = False
        self.safe_start_time: float = 0.0
        self.safe_duration: float = 0.0  # in seconds internally
        self.last_action: Optional[str] = None

    def start_cooldown(self, duration_minutes: float):
        """ Kích hoạt độ an toàn (safe mode) """
        self.safe_mode = True
        self.safe_start_time = time.time()
        self.safe_duration = duration_minutes * 60.0
        self.fall_detected = False  # Reset cờ ngã khi vào mode an toàn
        print(f"[API] Đã kích hoạt Safe Mode trong {duration_minutes} phút.")

    def stop_cooldown(self):
        """ Hủy chế độ an toàn """
        self.safe_mode = False
        self.safe_start_time = 0.0
        self.safe_duration = 0.0
        print(f"[API] Đã tắt chế độ Safe Mode.")

    def is_safe_mode(self) -> bool:
        """ Kiểm tra xem hiện tại có đang trong thời gian an toàn không """
        if not self.safe_mode:
            return False
            
        elapsed = time.time() - self.safe_start_time
        if elapsed < self.safe_duration:
            return True
        else:
            # Hết thời gian an toàn, tự động tắt
            self.stop_cooldown()
            return False

    def get_remaining_cooldown(self) -> float:
        """ Lấy thời gian an toàn còn lại (giây) """
        if not self.is_safe_mode():
            return 0.0
        return max(0.0, self.safe_duration - (time.time() - self.safe_start_time))

current_status = FallStatus()

@app.post("/report")
async def report_fall(
    track_id: int = Form(...),
    image: UploadFile = File(...),
    action: Optional[str] = Form(None)
):
    """
    Endpoint dành cho AI báo cáo khi phát hiện ngã hoặc cập nhật action tĩnh.
    """
    current_status.last_action = action
    
    # Logic tự động tắt safe mode nếu ai đó đã khỏe và đứng dậy
    if current_status.is_safe_mode():
        if action and action not in ["Fall Down", "Lying Down", "Fall", "Lying"]:
            print(f"[API] Phát hiện người dùng đã dậy (Action: {action}). Tự động tắt Safe Mode.")
            current_status.stop_cooldown()
            current_status.fall_detected = False
            return {"status": "success", "message": "Safe mode disabled because user is up"}
        else:
            print(f"[API] Đang trong Safe Mode: Bỏ qua báo cáo ngã từ ID {track_id}")
            return {"status": "success", "message": "Ignored fall during safe mode"}

    # Lưu file ảnh
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"fall_{track_id}_{timestamp}.jpg"
    file_path = os.path.join(STORAGE_DIR, filename)
    
    with open(file_path, "wb") as buffer:
        shutil.copyfileobj(image.file, buffer)
    
    # Cập nhật trạng thái ngã
    current_status.fall_detected = True
    current_status.fall_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    current_status.evidence_url = f"/evidence/{filename}"
    
    print(f"[API] Đã nhận báo cáo ngã: ID {track_id}, Ảnh: {filename}, Action: {action}")
    
    return {"status": "success", "message": "Fall reported"}

@app.post("/trigger-sos")
async def trigger_sos():
    """
    Endpoint mới để thụ lý lệnh SOS tử bên ngoài (Ví dụ: từ ESP32 bóp còi SOS).
    Khi gọi vào đây thì Telegram mới thực sự được bắn đi.
    """
    if current_status.evidence_url:
        file_path = os.path.join(STORAGE_DIR, os.path.basename(current_status.evidence_url))
        if os.path.exists(file_path):
            threading.Thread(target=send_telegram_alert, args=(current_status.fall_time, file_path), daemon=True).start()
            return {"status": "success", "message": "SOS sent to Telegram"}
    return {"status": "error", "message": "No recent fall evidence"}

@app.get("/gate")
async def get_gate_data():
    """
    Endpoint cổng chính dành cho ESP32 lấy dữ liệu.
    Dựa vào trạng thái cooldown sẽ trả về API tương ứng.
    """
    if current_status.is_safe_mode():
        # Đang trong thời gian an toàn
        remaining = current_status.get_remaining_cooldown()
        return {
            "type": "apiSendSafe",
            "safe": True,
            "safe_remaining_seconds": round(remaining, 1)
        }
    else:
        # Không trong thời gian an toàn, trả về trạng thái ngã trực tiếp (không tự gạt cờ)
        return {
            "type": "apiSendFall",
            "fall_detected": current_status.fall_detected,
            "fall_time": current_status.fall_time,
            "evidence": current_status.evidence_url
        }

class ResponseFromESP32(BaseModel):
    safe: bool
    safe_duration_minutes: float = 0.0

@app.post("/response-from-esp32")
async def handle_esp32_response(data: ResponseFromESP32):
    """
    Endpoint để ESP32 phản hồi trạng thái an toàn của người dùng.
    """
    if data.safe:
        current_status.start_cooldown(data.safe_duration_minutes)
        return {"status": "success", "message": f"Safe mode activated for {data.safe_duration_minutes} minutes"}
    else:
        current_status.stop_cooldown()
        return {"status": "success", "message": "Safe mode deactivated"}

@app.post("/reset")
async def reset_status():
    """
    Endpoint để reset trạng thái.
    """
    current_status.fall_detected = False
    current_status.fall_time = ""
    current_status.evidence_url = ""
    current_status.stop_cooldown()
    return {"status": "success"}

if __name__ == "__main__":
    import uvicorn
    local_ip = _get_local_ip()
    print(f"[SERVER] 🚀 Khởi động FastAPI tại http://{local_ip}:8000")
    print(f"[SERVER] 📡 ESP32 sẽ tự tìm qua mDNS: falldect.local:8000")
    uvicorn.run(app, host="0.0.0.0", port=8000)
