# Hướng dẫn chi tiết thiết lập giao diện Blynk 
> **Template ID**: `TMPL6nXDjldOr`
> **Template Name**: `FallDetection`

Tài liệu này hướng dẫn cách cấu hình Datastream (Virtual Pins) trên Blynk Console (Web) và cách bố trí các Widget trên Blynk Mobile App để khớp 100% với logic code C++ trong `firmware/src/main.cpp`.

---

## 1. Cấu hình Datastream (Trên giao diện Web)
Truy cập **Blynk Console -> Templates -> FallDetection -> Datastreams**, tạo các biến sau:

| Tên Datastream | Virtual Pin | Kiểu dữ liệu (Data Type) | Giá trị min/max | Cấu hình thêm (nếu có) | Đối chiếu Logic Code |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **BaoDong_NanNhan** | `V0` | Integer | `0` to `1` | Default Value: `0` | Nhận tín hiệu Nạn nhân xác nhận "Tôi Ổn" trong 2 phút đầu (Level 1). Code tự bật V0=1 khi ngã, người dùng gạt V0=0 để đoạt quyền Tắt báo động. |
| **Label_ThongBao** | `V1` | String | Không áp dụng | - | Code sẽ truyền chuỗi: `"Cảnh báo: Có người bị ngã hãy check cam ngay"` ở Level 2. Mặc định gửi `""` khi reset. |
| **Label_ThoiGian** | `V2` | String | Không áp dụng | - | Hiển thị thời gian thực tế ghi nhận lúc ngã từ API (`fallTimeStr`). Mặc định gửi `""` khi reset. |
| **Hinh_Anh_Bang_Chung**| `V3` | Integer | `1` to `1` | Default Value: `0` | Dùng Widget **Image Gallery**. Logic code sẽ gọi `Blynk.setProperty(V3, "urls", evidenceUrl)` để tải ảnh từ Server xuống. |
| **BaoDong_NguoiNha_SOS**| `V6` | Integer | `0` to `1` | Default Value: `0` | Công tắc Còi hú SOS đỏ. Bật (1) khi quá 5 phút. Người nhà gạt tắt (0) để xác nhận xử lý xong việc và Reset toàn hệ thống. |
| **Nut_NgNha_AnToan** | `V11` | Integer | `0` to `1` | Default Value: `0` | Nút bấm để Người nhà báo An Toàn. Gửi lệnh Pause API 15 phút. |
| **Nut_NgNha_GoiSOS** | `V12` | Integer | `0` to `1` | Default Value: `0` | Nút bấm cướp quyền gọi SOS khẩn cấp ngay lập tức (không chờ hết 5 phút). |

---

## 2. Thiết lập Giao Diện (Widget Layout) trên Mobile App

Sắp xếp màn hình theo 3 nhóm chức năng (Group) để người nhà dễ thao tác khi hoảng loạn:

### Nhóm 1: BÁO ĐỘNG NGÃ TẠI CHỖ (Dành cho nạn nhân)
*Sử dụng khi nạn nhân còn tỉnh.*
* **Widget Button (Nút Switch)**
  - **Datastream:** Chọn `V0` (BaoDong_NanNhan)
  - **Mode:** Cài dạng **Switch**
  - **Visual:** Chọn màu Đỏ/Cam. Đặt Label Text dạng: `ONLabel = ĐANG BÁO ĐỘNG`, `OFFLabel = TÔI ỔN !`

### Nhóm 2: BẰNG CHỨNG HÌNH ẢNH (Dành cho người nhà coi cam)
*Level 2 - Sau 2 phút còi hú.*
* **Widget Labeled Value (hoặc Value Display)**
  - **Datastream:** Chọn `V1`
  - **Visual:** Đổi màu chữ đỏ đậm/gạch dưới để cảnh cáo thông tin.
* **Widget Labeled Value**
  - **Datastream:** Chọn `V2`
  - **Text:** Thêm Prefix "Giờ ngã: "
* **Widget Image Gallery (Bộ hiển thị Bức ảnh)**
  - **Datastream:** Chọn `V3`
  - *Lưu ý: Bạn không cần điền link ảnh tĩnh tĩnh vào URL đây, vì C++ đã dùng `setProperty("urls", evidenceUrl)` tự load đè lên rồi.*

### Nhóm 3: PHẢN ỨNG CỨU HỘ MỨC ĐỘ CAO (Dành cho người nhà xử lý SOS)
*Level 2 và Báo động ghim (Wait_for_Button).*
* **2 Nút Widget Button dạng nút bấm (Bấm Nhả còi - Dành cho Level 2)**
  - **Nút "An Toàn" (Datastream V11):** Chọn Mode **Push**. Cho màu Xanh Lá cây.
  - **Nút "Gọi Cấp Cứu" (Datastream V12):** Chọn Mode **Push**. Trộn màu Đỏ rực.
* **1 Nút Widget Button dạng công tắc (Tắt còi khi báo động vô thời hạn - V6)**
  - **Datastream:** Chọn `V6` (BaoDong_NguoiNha_SOS)
  - **Mode:** Chỉnh thành dạng **Switch**
  - **Phân loại tác dụng:** Lúc báo khẩn cấp sau 5 phút, nút này tự nảy lên Đỏ rực chớp tắt (v6=1). Người nhà thấy nguy hiểm tới, gạt nó về 0 để reset báo động êm ả lại (`IDLE`).

---

## 3. Hoàn tất Thiết Lập

1. Sau khi Setup Widget, hãy Restart lại Blynk App 1 lần để cập nhật mọi thay đổi từ Console.
2. Bạn có thể test logic mạch bằng cách: Bấm chay nút `V0` thử, thả `V11, V12` hoặc làm giả 1 Request lên server ảo (hoặc đổi trạng thái code test) để xem Widget `V3` bung hình.
