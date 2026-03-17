from fastapi import FastAPI, File, UploadFile, Form
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from datetime import datetime
import shutil
import os

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

current_status = FallStatus()

@app.post("/report")
async def report_fall(
    track_id: int = Form(...),
    image: UploadFile = File(...)
):
    """
    Endpoint dành cho AI báo cáo khi phát hiện ngã.
    """
    # Lưu file ảnh
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"fall_{track_id}_{timestamp}.jpg"
    file_path = os.path.join(STORAGE_DIR, filename)
    
    with open(file_path, "wb") as buffer:
        shutil.copyfileobj(image.file, buffer)
    
    # Cập nhật trạng thái
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
    return {
        "fall_detected": current_status.fall_detected,
        "fall_time": current_status.fall_time,
        "evidence": current_status.evidence_url
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
