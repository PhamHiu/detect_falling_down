from fastapi import FastAPI, File, UploadFile, Form
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from datetime import datetime
import shutil
import os
import time

app = FastAPI()

# Cấu hình lưu trữ ảnh
STORAGE_DIR = "api/storage"
if not os.path.exists(STORAGE_DIR):
    os.makedirs(STORAGE_DIR)

# Mount thư mục storage để có thể truy cập ảnh qua URL
app.mount("/evidence", StaticFiles(directory=STORAGE_DIR), name="evidence")

# Trạng thái hiện tại
class FallStatus:
    def __init__(self):
        self.fall_detected = False
        self.fall_time = ""
        self.evidence_url = ""
        self.cooldown_start_time: float = 0.0  # Thời điểm bắt đầu 5 phút khóa (tính từ khi ESP đọc được)

    def is_in_cooldown(self) -> bool:
        """ Kiểm tra xem hiện tại có đang trong thời gian khóa 5 phút không """
        return (time.time() - self.cooldown_start_time) < COOLDOWN_SECONDS

COOLDOWN_SECONDS = 5 * 60  # 5 phút = 300 giây

current_status = FallStatus()

@app.post("/report")
async def report_fall(
    track_id: int = Form(...),
    image: UploadFile = File(...)
):
    """
    Endpoint dành cho AI báo cáo khi phát hiện ngã.
    """
    # Kiểm tra cooldown: Nếu đang trong thời gian khóa 5 phút thì bỏ qua
    if current_status.is_in_cooldown():
        remaining = int(COOLDOWN_SECONDS - (time.time() - current_status.cooldown_start_time))
        print(f"[API] Đang trong cooldown 5 phút, bỏ qua báo cáo ngã ({remaining}s còn lại).")
        return {"status": "skipped", "message": f"Cooldown active, {remaining}s remaining"}

    # Lưu file ảnh
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"fall_{track_id}_{timestamp}.jpg"
    file_path = os.path.join(STORAGE_DIR, filename)
    
    with open(file_path, "wb") as buffer:
        shutil.copyfileobj(image.file, buffer)
    
    # Cập nhật trạng thái (cooldown chưa bắt đầu tại đây, sẽ bắt đầu khi ESP đọc được)
    current_status.fall_detected = True
    current_status.fall_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    current_status.evidence_url = f"/evidence/{filename}"
    
    print(f"[API] Đã nhận báo cáo ngã: ID {track_id}, Ảnh: {filename}")
    
    return {"status": "success", "message": "Fall reported"}

@app.get("/status")
async def get_status():
    """
    Endpoint dành cho ESP32 truy vấn trạng thái.
    """
    # Lấy trạng thái hiện tại
    detected = current_status.fall_detected
    time_str = current_status.fall_time
    evi_url = current_status.evidence_url

    # Nếu ESP32 vừa nhận được thông tin ngã lần đầu tiên:
    # -> Gạt cờ về False ngay lập tức
    # -> KHỚI ĐỘNG đồng hồ khóa 5 phút (cooldown_start_time)
    # Đảm bảo fall_detected luôn giữ mức False suốt 5 phút kế tiếp
    if detected:
        current_status.fall_detected = False
        current_status.cooldown_start_time = time.time()  # Bắt đầu đồng hồ 5 phút
        print(f"[API] ESP32 đã nhận tin ngã. Khóa cờ 5 phút bắt đầu.")

    return {
        "fall_detected": detected,
        "fall_time": time_str,
        "evidence": evi_url
    }

@app.post("/reset")
async def reset_status():
    """
    Endpoint để reset trạng thái (ví dụ sau khi đã xử lý xong).
    """
    current_status.fall_detected = False
    current_status.fall_time = ""
    current_status.evidence_url = ""
    return {"status": "success"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
