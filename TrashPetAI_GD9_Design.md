# TrashPet AI — GĐ 9 Design (System Health Monitor)

> Mục tiêu: thêm 1 tính năng nhỏ, khớp với ẩn dụ "thú cưng" sẵn có, để bài báo ICEIR
> có thêm 1 đóng góp rõ ràng mà không lạc đề (không quét virus, không tự sửa máy).
> Ý tưởng: pet **chủ động cảnh báo** khi ổ đĩa/RAM gần đầy, thúc đẩy user dùng tính
> năng "ăn rác" đã có sẵn — biến việc dọn máy thành tương tác với pet thay vì thao
> tác thủ công.

---

## 0. Phạm vi (chốt trước khi code)

| Điểm | Quyết định |
|---|---|
| Theo dõi gì | Dung lượng trống **tất cả ổ đĩa cứng gắn trong máy** (không chỉ ổ C:) + % RAM đang dùng |
| Cách lấy dữ liệu | Windows API thuần: `GetLogicalDrives`+`GetDriveTypeW` (liệt kê ổ `DRIVE_FIXED`), `GetDiskFreeSpaceExW` mỗi ổ, `GlobalMemoryStatusEx` cho RAM — **không** cần thư viện ngoài |
| Ổ nào bị loại | `DRIVE_CDROM`, `DRIVE_REMOTE` (ổ mạng), `DRIVE_REMOVABLE` không có media → bỏ qua để tránh lỗi/treo khi gọi API trên ổ rỗng |
| Không làm | Quét virus, tự động sửa lỗi máy, tự xoá file ngoài Recycle Bin |
| Hiển thị cảnh báo | Tái dùng animation `Error` có sẵn (không cần vẽ sprite mới) + bong bóng chat text |
| Tần suất check | Timer riêng, mặc định 30s (test mode), không phát lại cảnh báo liên tục (giống pattern `mildNotified/hungryNotified` của `HungerManager`), theo dõi riêng từng ổ đĩa (mỗi ổ có notified-flag riêng) |
| Ngưỡng cảnh báo | Đĩa: còn < 10% dung lượng trống (hoặc < 5 GB, lấy giá trị nào chạm trước) → cảnh báo, báo rõ tên ổ (VD "Ổ D: sắp đầy"). RAM: đang dùng > **60%** → cảnh báo (máy user 32GB, để 90% khó demo) |
| User tự xem thủ công | Có — thêm menu chuột phải "💻 Tình trạng máy" mở `HealthDialog` liệt kê tất cả ổ đĩa + RAM hiện tại (không cần đợi ngưỡng). **Làm ngay trong v1.** |
| Liên kết với ăn rác | Bong bóng cảnh báo có gợi ý "Cho mình ăn rác đi!" — không tự động mở EatDialog, để user chủ động bấm |

---

## 1. File mới

### `healthconfig.h` (namespace hằng số, giống `hungerconfig.h`)
```cpp
namespace HealthCfg {
    constexpr int     kCheckMs            = 30 * 1000;  // 30s/tick (test mode)
    constexpr double  kDiskFreePercentMin = 10.0;        // < 10% free → warning
    constexpr quint64 kDiskFreeBytesMin   = 5ULL << 30;  // < 5 GB free → warning
    constexpr double  kRamUsedPercentMax  = 60.0;        // > 60% used → warning (test mode, máy 32GB)
}
```

### `systemmonitor.h / .cpp`
- Class `SystemMonitor : public QObject`, giống pattern `HungerManager`.
- `start()` / `stop()` — QTimer nội bộ, tick mỗi `HealthCfg::kCheckMs`.
- `struct DiskInfo { QString driveLetter; quint64 freeBytes; quint64 totalBytes; double freePercent; };`
- `QList<DiskInfo> listFixedDrives() const` — `GetLogicalDrives()` liệt kê bitmask ổ, lọc `GetDriveTypeW() == DRIVE_FIXED`, gọi `GetDiskFreeSpaceExW` từng ổ.
- `RamInfo currentRamInfo() const` — gọi `GlobalMemoryStatusEx`, trả `{usedPercent, totalBytes, availBytes}`.
- Mỗi ổ đĩa giữ 1 `bool notified` riêng trong `QMap<QString, bool> m_driveNotified` (reset về false khi ổ đó trở lại bình thường), tránh spam cảnh báo lặp — cùng pattern `m_mildNotified`/`m_hungryNotified` của `HungerManager`.
- Signals:
  - `diskWarning(QString driveLetter, quint64 freeBytes, quint64 totalBytes)` — phát khi 1 ổ vượt ngưỡng lần đầu (chưa notified).
  - `ramWarning(double usedPercent)` — tương tự, 1 flag `m_ramNotified` dùng chung.
- Không phụ thuộc Qt Network — chỉ dùng `<windows.h>` (đã có sẵn trong project do dùng COM ở `recyclebinmanager.cpp`).

### `healthdialog.h / .cpp`
- Dialog đơn giản, tái dùng style `eatdialog.cpp` (dark theme, bo góc, cùng font/màu ChatWidget).
- Nội dung: liệt kê từng ổ `DRIVE_FIXED` (tên ổ, dung lượng trống/tổng, thanh progress bar hoặc %), và 1 dòng RAM (đang dùng/tổng, %).
- Có nút "Làm mới" gọi lại `SystemMonitor::listFixedDrives()`/`currentRamInfo()` (không cần đợi timer tick).
- Mở qua menu chuột phải "💻 Tình trạng máy" — làm ngay trong v1 (đã chốt), không phải để sau.

---

## 2. Sửa file hiện có

- **`characterwidget.h/.cpp`**:
  - Thêm menu item "💻 Tình trạng máy" vào menu chuột phải, mở `HealthDialog`.
  - Khởi tạo `SystemMonitor`, connect `diskWarning`/`ramWarning` → slot hiển thị cảnh báo.
  - Khi có cảnh báo: gọi `setState(CharacterState::Error)` (tái dùng, tự revert về `Idle` sau ~2.5s như cơ chế đã có) + gọi `m_chatWidget->appendMessage(...)` để hiện bong bóng cảnh báo kèm gợi ý ăn rác.
- **`main.cpp`**: khởi động `SystemMonitor::start()` cùng lúc với `HungerManager::start()`.
- **`CMakeLists.txt`**: thêm `systemmonitor.cpp/.h`, `healthdialog.cpp/.h`, `healthconfig.h` vào target sources.

---

## 3. Vì sao thiết kế thế này (cho phần "Design" của bài báo)

- **Không cần asset mới** → tái dùng state `Error` sẵn có, giảm rủi ro kỹ thuật, dễ hoàn thành đúng hạn nộp bài.
- **Nhất quán với cơ chế đã có**: dùng lại pattern threshold-notify của `HungerManager` (không spam cảnh báo), dùng lại `QTimer` + `QObject` signal/slot đã dùng xuyên suốt project.
- **Không chồng lấn với AV/system tool có sẵn của Windows** → tránh câu hỏi "sao không dùng Windows Defender/Disk Cleanup" từ reviewer; đây thuần là lớp UX gamification đặt trên API hệ thống sẵn có của Windows, không phải reimplement chức năng OS.
- **Đóng góp cho bài báo**: cho phép viết 1 đoạn "Proactive System Health Nudging via Virtual Pet Metaphor" — pet không chỉ phản ứng theo lệnh user (chat, ăn rác thủ công) mà còn **chủ động khởi xướng tương tác** dựa trên tình trạng máy, đóng vai trò như 1 "ambient notification agent" nhẹ.

---

## 4. Việc cần làm (thứ tự code)

1. `healthconfig.h` — hằng số ngưỡng.
2. `systemmonitor.h/.cpp` — logic đọc disk/RAM + timer + signal ngưỡng.
3. Sửa `CMakeLists.txt` thêm file mới.
4. Sửa `characterwidget.h/.cpp` — khởi tạo monitor, connect signal, thêm menu "Tình trạng máy".
5. `healthdialog.h/.cpp` — dialog xem thủ công (có thể làm sau cùng, không bắt buộc cho v1).
6. Build test (Debug qua Qt Creator hoặc CMake+Ninja), test bằng cách hạ ngưỡng tạm thời (vd RAM > 1%) để trigger cảnh báo dễ quan sát.
7. Cập nhật `TrashPetAI_Progress_v2.md` sau khi xong.

---

## 5. Câu hỏi mở — ĐÃ CHỐT

- [x] Theo dõi tất cả ổ `DRIVE_FIXED` hiện có trên máy, báo riêng theo tên ổ.
- [x] `HealthDialog` làm ngay trong v1 (không để sau).
- [x] Ngưỡng RAM hạ xuống **60%** (thay vì 90%) — phù hợp máy 32GB của user để demo dễ trigger.

