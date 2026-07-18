# TrashPet AI Desktop Assistant — Tóm tắt tiến độ

## 1. Môi trường & công cụ

| Thành phần | Phiên bản / Đường dẫn |
|---|---|
| OS | Windows |
| IDE | Qt Creator (chính) + Visual Studio 2026 Insiders (phụ, có Copilot) |
| Qt | **Qt 6.11.1** tại `C:\Qt\6.11.1\mingw_64` |
| Compiler | MinGW 64-bit (đi kèm Qt) |
| Build system | CMake |
| Kit | Desktop Qt 6.11.1 MinGW 64-bit |
| AI Provider | **Google Gemini API** — model `gemini-3.5-flash` |
| Free tier | Có (qua Google AI Studio) |

**Lưu ý:** Không cài MSVC component của Qt → bắt buộc dùng MinGW + Qt Creator (không build được bằng Visual Studio cho project này).

---

## 2. Cấu trúc thư mục dự án hiện tại

```
C:\Users\Administrator\Desktop\TrashPet AI Desktop Assistant\TrashPetAI\
├── CMakeLists.txt
├── main.cpp
├── characterwidget.h / .cpp
├── chatwidget.h / .cpp
├── configmanager.h / .cpp
├── geminiclient.h / .cpp
├── config/
│   ├── config.json           ← chứa API key thật (KHÔNG commit)
│   └── config.example.json   ← bản mẫu
├── assets/
│   └── character/            ← (sắp đặt vào ở GĐ 4)
│       ├── idle.gif / idle.png
│       ├── thinking.gif
│       ├── talking.gif
│       ├── eating.gif
│       ├── happy.png
│       ├── sleeping.png
│       └── error.png
└── build/                    ← Qt Creator tự sinh
```

---

## 3. CMakeLists.txt hiện tại

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(TrashPetAI
    WIN32 MACOSX_BUNDLE
    main.cpp
    characterwidget.cpp characterwidget.h
    chatwidget.cpp chatwidget.h
    configmanager.cpp configmanager.h
    geminiclient.cpp geminiclient.h
)

target_link_libraries(TrashPetAI PRIVATE Qt6::Widgets Qt6::Network)
```

---

## 4. Tiến độ theo giai đoạn

| GĐ | Nội dung | Trạng thái |
|---|---|---|
| 1 | Pet 2D nổi trên desktop, kéo thả, menu chuột phải | ✅ Xong |
| 2 | Khung chat, click trái mở chat, gửi tin nhắn echo | ✅ Xong |
| 3 | Gemini API thật, config từ file, retry 503/429 | ✅ Xong |
| 4 | Animation theo trạng thái (load sprite slime) | ⏳ Đang làm |
| 5 | Ăn rác (dọn Recycle Bin) | ⬜ Chưa |
| 6 | Đóng gói .exe (windeployqt) | ⬜ Chưa |

---

## 5. Chi tiết từng class đã làm

### 5.1. `CharacterWidget` (GĐ 1)
- Cửa sổ `FramelessWindowHint | WindowStaysOnTopHint | Tool`
- `WA_TranslucentBackground` — nền trong suốt
- `resize(160, 160)`
- **Vẽ tay** hình tròn xanh cười bằng `QPainter` (placeholder, sẽ thay ở GĐ 4)
- Phân biệt **click** vs **drag** bằng `manhattanLength() > 5px`
- Mouse trái:
  - Drag → di chuyển pet
  - Click (không drag) → `toggleChat()` mở/ẩn `ChatWidget`
- Mouse phải → menu: Mở chat / Ăn rác / Cài đặt / Thoát
- Giữ con trỏ `ChatWidget *m_chatWidget` (tạo sẵn trong constructor)

### 5.2. `ChatWidget` (GĐ 2)
- Cửa sổ frameless, fixed size 380×480, bo góc, theme tối
- Layout: title bar → vùng chat (`QTextEdit` read-only) → input + nút Gửi → nút "Ăn rác"
- `appendMessage(sender, text)`:
  - Bot → màu xanh lá `#7ee787`
  - Bạn → màu xanh dương `#79c0ff`
  - Dùng HTML, có `toHtmlEscaped()` chống XSS
- `Enter` trong input → gửi (`returnPressed`)
- `showNearCharacter(pos)` → hiện chat bên trái pet, raise + focus input
- Có sẵn `m_gemini = new GeminiClient(this)`, kết nối signal:
  - `responseReceived` → `onGeminiResponse`
  - `errorOccurred` → `onGeminiError`
- Khi đang chờ Gemini: disable input + nút Gửi, hiện "💭 Đang suy nghĩ..."
- Lỗi: hiện "⚠️ ..."

### 5.3. `ConfigManager` (GĐ 3, Phần 1)
- **Singleton pattern** (`instance()`)
- Đọc `config.json` qua `QJsonDocument::fromJson`
- Các field đọc được:
  - `gemini_api_key`
  - `gemini_model` (default `gemini-3.5-flash`)
  - `character_name`
  - `always_on_top`
  - `confirm_before_empty_trash`
  - `language`
- `lastError()` trả về thông báo lỗi rõ ràng (tiếng Việt)
- Validate key: nếu rỗng hoặc bằng `"YOUR_API_KEY_HERE"` → báo lỗi
- `main.cpp` thử 3 đường dẫn fallback:
  1. `applicationDirPath()/config/config.json`
  2. `applicationDirPath()/../config/config.json` ← dùng khi chạy debug
  3. `applicationDirPath()/../../config/config.json`

### 5.4. `GeminiClient` (GĐ 3, Phần 2)
- Kế thừa `QObject` để dùng signal/slot
- Dùng `QNetworkAccessManager` (async, không chặn UI)
- **Endpoint:** `https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={key}`
- Build JSON request:
  ```json
  { "contents": [ { "parts": [ { "text": "..." } ] } ] }
  ```
- Parse JSON response, lấy `candidates[0].content.parts[0].text`
- **Auto-retry** khi gặp HTTP 503 (overload) hoặc 429 (rate limit):
  - Tối đa 3 lần
  - Đợi 2 giây giữa các lần (`QTimer::singleShot`)
- Signals:
  - `responseReceived(QString)` — khi có text trả lời
  - `errorOccurred(QString)` — khi có lỗi (mạng/API/JSON)
- Slot: `onReplyFinished(QNetworkReply*)` (gọi `deleteLater()` để tránh leak)

---

## 6. File config.json

```json
{
  "gemini_api_key": "AIzaSy...",
  "gemini_model": "gemini-3.5-flash",
  "character_name": "TrashPet",
  "always_on_top": true,
  "confirm_before_empty_trash": true,
  "language": "vi"
}
```

⚠️ **Lưu ý bảo mật:** API key đã từng bị lộ trong ảnh chụp màn hình → **cần đổi key mới** ở https://aistudio.google.com/apikey nếu chưa làm.

---

## 7. Sprite slime đã chuẩn bị (GĐ 4)

Gói sprite tải từ itch.io (pixel art 32×25 mỗi frame, đã scale up 5x → 160×125):

| File | Frames | Trạng thái map |
|---|---|---|
| `idle.gif` | 4 | Đứng yên (default) |
| `thinking.gif` | 4 | Khi chờ Gemini trả lời |
| `talking.gif` | 4 (nhanh) | Khi bot trả lời |
| `eating.gif` | 5 (anim attack) | Khi ăn rác |
| `happy.png` | 1 | Sau khi trả lời xong |
| `sleeping.png` | 1 | Khi rảnh lâu |
| `error.png` | 1 (anim hurt) | Khi lỗi API |
| `idle.png` | 1 | Backup nếu GIF lỗi |

Cần đặt vào: `TrashPetAI/assets/character/`

---

## 8. Các kỹ thuật Qt/C++ đã dùng

- **Forward declaration** trong `.h` (`class ChatWidget;`) — giảm phụ thuộc, build nhanh
- **Signal-slot** Qt — async event-driven
- **Singleton pattern** — `ConfigManager::instance()`
- **`QNetworkAccessManager`** + `QNetworkRequest` + `QNetworkReply` (HTTP async)
- **`QJsonDocument` / `QJsonObject` / `QJsonArray`** (JSON parse/build)
- **`QMovie`** sẽ dùng ở GĐ 4 cho GIF animation
- **`QPainter`** vẽ tay (placeholder hiện tại, sẽ thay bằng `QPixmap`/`QMovie`)
- **`QTimer::singleShot`** delay async cho retry
- **Lambda capture** `[this]() { ... }` cho retry callback
- **`deleteLater()`** tránh memory leak cho reply objects
- **`QStringList`** + loop fallback paths cho config

---

## 9. Việc tiếp theo cần làm

### Trước khi sang GĐ 4
- [ ] Đổi API key mới (key cũ lộ trong ảnh)
- [ ] Apply code retry 503 vào `geminiclient.cpp` + `.h` (nếu chưa)
- [ ] Giải nén `character.zip` → đặt vào `TrashPetAI/assets/character/`

### GĐ 4 sẽ làm
1. Tạo enum `CharacterState { Idle, Thinking, Talking, Eating, Happy, Sleeping, Error }`
2. Sửa `CharacterWidget`:
   - Bỏ code vẽ `QPainter` hình tròn
   - Dùng `QMovie` cho GIF, `QPixmap` cho PNG
   - Thêm hàm `setState(CharacterState)` đổi animation
   - Thêm `QLabel *m_imageLabel` hoặc override `paintEvent` để vẽ frame hiện tại
3. Nối với `ChatWidget`:
   - Khi gửi câu hỏi → pet state `Thinking`
   - Khi nhận response → pet state `Talking` (vài giây) → `Happy` (vài giây) → `Idle`
   - Khi lỗi → pet state `Error` (vài giây) → `Idle`
4. (Optional) Auto chuyển sang `Sleeping` sau N phút rảnh

### GĐ 5
- Class `RecycleBinManager` dùng `SHQueryRecycleBinW` + `SHEmptyRecycleBinW`
- Link `Shell32` trong CMakeLists
- Nút "🗑️ Ăn rác" trong ChatWidget → check rác → hỏi xác nhận → dọn → animation eating

### GĐ 6
- Build release
- `windeployqt` gom DLL
- Copy `assets/` và `config/` vào folder release
- Test chạy độc lập

---

## 10. Lưu ý kỹ thuật / lỗi đã gặp

1. **Qt 5 vs Qt 6:** Tưởng có Qt 5 nhưng thực ra là Qt 6.11.1 — dùng Qt 6.
2. **PowerShell + curl.exe:** Test API bằng curl trong PowerShell hay bị escape JSON sai → dùng `Invoke-RestMethod` với `ConvertTo-Json` thay thế.
3. **Model name:** `gemini-3.5-flash` là model thật (release 19/5/2026), không phải `gemini-2.5-flash`.
4. **HTTP 503:** Gemini server đôi khi overload → đã có retry tự động.
5. **Config path:** Khi chạy debug, exe nằm trong `build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/` → phải có path fallback `../config/config.json`.
6. **API key đã lộ:** Cần xóa key cũ + tạo key mới.
