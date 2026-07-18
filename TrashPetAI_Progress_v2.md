# TrashPet AI Desktop Assistant — Progress (Updated)

> Codex update 2026-07-06: GĐ 5.1, 5.2, 5.3 và 5.5 đã code xong. User dùng Qt Creator để build/chạy nên bỏ qua lỗi build terminal; runtime ăn rác từng file đã được user test trước khi làm Phase 5.3.

## 0. Trạng thái hiện tại

- [x] GĐ 1: Pet 2D nổi trên desktop, kéo thả, menu chuột phải.
- [x] GĐ 2: Khung chat.
- [x] GĐ 3: Gemini API thật.
- [x] GĐ 4: Animation theo state.
- [x] GĐ 5.1: Hunger + persist + COM Recycle Bin + EatDialog + integrate ăn từng file.
- [x] GĐ 5.2: Size system, mập khi ăn dư, gầy khi đói lâu.
- [x] GĐ 5.3: Visual eating feedback `+N MB` khi pet ăn/xóa từng file.
- [x] GĐ 5.5: Pet bò quanh màn hình khi idle, sprite lật theo hướng bò — **đã test xong, không bug**.
- [x] GĐ 6: Đóng gói `.exe` (Release build + windeployqt + assets/config) — **đã test chạy standalone, ổn**.
- [x] GĐ 7 (ngoài roadmap gốc): Hệ thống trang phục — 10 costume tải từ OpenGameArt.org (người, orc, goblin, skeleton, kỵ sĩ), đổi qua menu chuột phải, persist trong pet_state.json.
- [x] GĐ 8 (ngoài roadmap gốc): Đa AI provider — đổi qua lại Gemini / ChatGPT (OpenAI) / Claude (Anthropic) qua menu chuột phải "🤖 Đổi AI", mỗi provider xử lý request/response riêng, persist lựa chọn trong pet_state.json.

> Cập nhật ngày 15/07/2026 — GĐ 5 (toàn bộ, gồm 5.5), GĐ 6, đổi trang phục (GĐ 7), và đa AI provider (GĐ 8) đã xong. Build Release thành công.

---

## 1. Môi trường & công cụ

| Thành phần | Phiên bản / Đường dẫn |
|---|---|
| OS | Windows |
| IDE | Qt Creator (chính) + Visual Studio 2026 Insiders (phụ) |
| Qt | **Qt 6.11.1** tại `C:\Qt\6.11.1\mingw_64` |
| Compiler | MinGW 64-bit |
| Build system | CMake |
| AI Provider | Google Gemini API — `gemini-3.5-flash` |

---

## 2. Cấu trúc thư mục hiện tại

```
TrashPetAI/
├── CMakeLists.txt
├── main.cpp
├── characterstate.h              ← GĐ 4
├── characterwidget.h / .cpp      ← GĐ 4: dùng QMovie/QPixmap
├── chatwidget.h / .cpp           ← GĐ 4: nối với CharacterWidget
├── configmanager.h / .cpp
├── geminiclient.h / .cpp         ← GĐ 4: thêm systemInstruction
├── config/
│   ├── config.json
│   └── config.example.json
├── assets/
│   └── character/                ← Slime sprite
│       ├── idle.gif / idle.png
│       ├── thinking.gif
│       ├── talking.gif
│       ├── eating.gif
│       ├── happy.png
│       ├── sleeping.png
│       └── error.png
└── build/
```

---

## 3. CMakeLists.txt hiện tại

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(TrashPetAI
    WIN32 MACOSX_BUNDLE
    main.cpp
    characterwidget.cpp characterwidget.h
    chatwidget.cpp      chatwidget.h
    configmanager.cpp   configmanager.h
    geminiclient.cpp    geminiclient.h
    characterstate.h
)

target_link_libraries(TrashPetAI PRIVATE Qt6::Widgets Qt6::Network)
```

---

## 4. Tiến độ giai đoạn

| GĐ | Nội dung | Trạng thái |
|---|---|---|
| 1 | Pet 2D nổi trên desktop, kéo thả, menu chuột phải | ✅ Xong |
| 2 | Khung chat, click trái mở chat | ✅ Xong |
| 3 | Gemini API thật, retry 503/429 | ✅ Xong |
| 4 | Animation theo state + polish | ✅ Xong |
| 5 | Ăn rác (Hunger + Size system, ăn từng file + visual feedback) | ✅ Xong tới Phase 5.3 |
| 5.5 | Pet bò quanh màn hình + lật sprite theo hướng bò | ✅ Xong, đã test |
| 6 | Đóng gói .exe (windeployqt) | ✅ Xong, đã test standalone |

---

## 5. Chi tiết những việc đã làm

### 5.1. GĐ 1 — `CharacterWidget` cơ bản
- Cửa sổ `FramelessWindowHint | WindowStaysOnTopHint | Tool`
- `WA_TranslucentBackground` — nền trong suốt
- `resize(160, 160)`
- Phân biệt **click** vs **drag** bằng `manhattanLength() > 5px`
- Mouse trái: drag pet HOẶC click → `toggleChat()`
- Mouse phải → menu: Mở chat / Ăn rác / Cài đặt / Thoát
- Giữ con trỏ `ChatWidget *m_chatWidget`

### 5.2. GĐ 2 — `ChatWidget`
- Cửa sổ frameless, 380×480, bo góc, theme tối
- Layout: title bar → vùng chat (QTextEdit) → input + nút Gửi → nút "Ăn rác"
- `appendMessage`: Bot xanh lá, Bạn xanh dương, có `toHtmlEscaped()`
- Enter trong input → gửi
- `showNearCharacter(pos)` → tính vị trí thông minh (xem 5.7)

### 5.3. GĐ 3 — `ConfigManager` + `GeminiClient`
- ConfigManager singleton, 3 path fallback
- GeminiClient async với `QNetworkAccessManager`
- Auto-retry 503/429, tối đa 3 lần, đợi 2s giữa các lần
- Signals `responseReceived` / `errorOccurred`

### 5.4. GĐ 4 — Animation theo state ✨ MỚI

**File mới: `characterstate.h`**
```cpp
enum class CharacterState {
    Idle, Thinking, Talking, Eating, Happy, Sleeping, Error
};
```

**`CharacterWidget` viết lại:**
- Bỏ `QPainter` vẽ hình tròn → dùng `QLabel + QMovie + QPixmap`
- `loadAssets()` — load 4 GIF + 4 PNG, scale 160x160
- `setState(CharacterState)` — đổi animation
- Auto-revert timing:
  - `Talking` → `Happy` sau 3s
  - `Happy` → `Idle` sau 2s
  - `Error` → `Idle` sau 2.5s
- Auto-sleeping sau 5 phút rảnh
- Click pet đang ngủ → đánh thức về Idle

**`ChatWidget` cập nhật:**
- Thêm `setCharacterWidget()`, `triggerEatTrash()` (stub)
- Sửa 3 slot: `onSendClicked` → Thinking, `onGeminiResponse` → Talking, `onGeminiError` → Error

**`GeminiClient` polish:**
- Thêm `systemInstruction` vào JSON request
- System prompt định nghĩa cá tính TrashPet:
  - Vui vẻ, thân thiện, xưng "mình"
  - Trả lời tiếng Việt
  - Ngắn gọn (2-4 câu)
  - Không markdown
  - Có thể nhắc tới tính năng "ăn rác"

### 5.5. Polish layout chat

**`showNearCharacter(QPoint)` cải tiến:**
- Lấy `availableGeometry()` của màn hình chứa pet (multi-screen safe)
- Ưu tiên trái → phải → đè giữa nếu cả 2 không đủ
- Căn dọc giữa pet (không phải top)
- Clamp 4 phía với margin 10px → không tràn ra ngoài màn hình + không bị taskbar che

---

## 6. Luồng state pet hiện tại

```
        Sleeping ◄── (5 phút rảnh) ── Idle
            │                           ▲
            └── (click pet) ────────────┤
                                        │
                user gửi message        │
                        ▼               │
                    Thinking            │
                        │               │
                    response về         │
                        ▼               │
                    Talking ──(3s)──► Happy ──(2s)──┘
                        
                    (lỗi API)
                        ▼
                    Error ──(2.5s)──► Idle
```

---

## 7. Các kỹ thuật Qt/C++ đã dùng

- Forward declaration trong .h
- Signal-slot Qt async
- Singleton pattern (ConfigManager, sắp tới PetState)
- `QNetworkAccessManager` / `QJsonDocument`
- `QMovie` + `QPixmap` cho animation
- `QTimer::singleShot` cho auto-revert state
- Lambda capture cho callback async
- `QScreen::availableGeometry()` cho multi-monitor

---

## 8. GĐ 5 — Đang làm (Hunger + Size + ăn từng file)

> Xem chi tiết: `TrashPetAI_GD5_Design_v3.md`

### Tóm tắt design
- Pet có chỉ số **hunger [0,100]** tăng dần theo thời gian
- 2 ngưỡng popup: 40 (hơi đói), 70 (đói)
- Ăn rác = **ăn từng file** theo dung lượng:
  - 1 MB = -1 hunger; no full = 100 MB
  - File > 1 GB → skip (báo tên + size)
  - User chọn thứ tự: Cũ nhất / Lớn nhất trước
- Pet có chỉ số **size** [80, 320] px:
  - Ăn dư khi no → mập (50 MB dư = +1 px)
  - Đói lâu → gầy đi (làm ở Phase 5.2)
- Persist: `config/pet_state.json`

### Phân chia
- **Part 1:** PetState + HungerManager *(xong)*
- **Part 2:** Refactor RecycleBinManager với COM API *(xong)*
- **Part 3:** Dialog + integrate *(xong)*
- **Phase 5.3:** Visual eating feedback `+N MB` *(xong)*

### File sẽ tạo mới
- `petstate.h / .cpp`
- `hungermanager.h / .cpp`
- `recyclebinmanager.h / .cpp`
- `eatdialog.h / .cpp`
- `config/pet_state.json` (auto-generate khi chạy lần đầu)

---

## 9. Lưu ý kỹ thuật / lỗi đã gặp

1. **Qt 5 vs Qt 6:** Dùng Qt 6.11.1.
2. **PowerShell + curl.exe:** Test API bằng `Invoke-RestMethod` thay curl.
3. **Model name:** `gemini-3.5-flash` (release 19/5/2026), không phải `gemini-2.5-flash`.
4. **HTTP 503:** Đã có retry tự động 3 lần.
5. **Config path:** 3 path fallback (debug exe nằm trong build/.../Debug/).
6. **API key:** Đã đổi key mới sau khi lộ trong screenshot.
7. **GĐ 4:** Tên biến drag thay đổi (`m_dragging` thay `m_isDragging`) → phải sync giữa .h và .cpp.
8. **multiple definition:** Khi paste nhầm code class này vào file class khác.
9. **Layout chat tràn màn hình:** Đã fix bằng `availableGeometry()` + clamp 4 phía.

---

## 10. Việc tiếp theo

### GĐ 5 đã xong tới Phase 5.3
- `petstate.h/.cpp`: singleton load/save JSON.
- `hungermanager.h/.cpp`: QTimer hunger, popup threshold, feed theo MB, size signal.
- `recyclebinmanager.h/.cpp`: COM enumerate Recycle Bin + delete từng item.
- `eatdialog.h/.cpp`: dialog chọn thứ tự ăn, ước tính dung lượng, skip file > 1GB.
- `chatwidget::triggerEatTrash`: flow ăn từng file đã integrate.
- `characterwidget`: size system + visual eating feedback `+N MB`.

### Phase 5.5 — Hoàn tất, đã test
- Pet bò quanh màn hình khi `Idle`.
- Tự đổi hướng ngẫu nhiên và bật lại khi chạm mép màn hình.
- Tự lật GIF/PNG theo hướng bò để mắt nhìn cùng hướng di chuyển.
- Tạm dừng khi chat mở, user đang drag, hoặc pet đang ở state khác `Idle`.
- User đã test trong Qt Creator: idle roam, mắt nhìn đúng hướng trái/phải, mở/đóng chat, drag pet, chat Gemini, ăn rác — tất cả OK, không bug.

### GĐ 6 — Hoàn tất
- Build Release qua CMake + Ninja + MinGW (`build/Release/`)
- `windeployqt6 --release` gom DLL vào `release/`
- Copy `assets/` + `config/` vào `release/`
- Test chạy độc lập `release/TrashPetAI.exe` — chạy ổn định, không crash
- Lưu ý: `release/config/config.json` chứa API key thật — không chia sẻ/zip thư mục release ra ngoài khi còn key thật

---

## 10.5. GĐ 7 — Hệ thống trang phục (mới)

- File mới: `costume.h` (catalog id + tên hiển thị), sửa `petstate.h/.cpp` (persist `costume`),
  sửa `characterwidget.h/.cpp` (`setCostume()`, `costumePixmap()` lazy-load + cache, override
  `setState()` khi có trang phục).
- Menu chuột phải → "👕 Đổi trang phục" → submenu checkable liệt kê 10 trang phục + Slime mặc định.
- Khi chọn trang phục khác Slime: pet hiển thị ảnh tĩnh trang phục đó thay cho animation slime
  (do chỉ có 1 pose/trang phục, không phải full animation set); các timer auto-revert/sleep vẫn
  hoạt động bình thường, chỉ phần hình ảnh bị thay.
- Trang phục lưu trong `config/pet_state.json` field `costume`, load lại khi mở app.
- 10 file sprite trong `assets/costumes/`, tải từ OpenGameArt.org (giấy phép CC0/CC-BY-SA/OGA-BY
  3.0), xem chi tiết nguồn + yêu cầu ghi công trong `assets/costumes/CREDITS.md`.
- Danh sách: human_hero, human_rpg, human_player (người), goblin_knight, goblin_render (goblin),
  orc_hero, orc_warrior, orc_female (orc), skeleton, knight.

---

## 10.6. GĐ 8 — Đa AI provider (mới)

- File mới: `aiclient.h` (interface chung `AiClient` — QObject, `sendMessage()`, signals
  `responseReceived`/`errorOccurred`), `openaiclient.h/.cpp` (ChatGPT), `claudeclient.h/.cpp`
  (Claude/Anthropic). `geminiclient.h/.cpp` refactor để kế thừa `AiClient` thay vì `QObject` trực tiếp.
- `configmanager.h/.cpp`: thêm `ai_provider`, `openai_api_key`, `openai_model`, `claude_api_key`,
  `claude_model` (đọc từ config.json, có default an toàn nếu thiếu field).
- `petstate.h/.cpp`: persist `ai_provider` đang chọn (field mới trong pet_state.json).
- `chatwidget.h/.cpp`: giữ cả 3 client (Gemini/OpenAI/Claude), dispatch tin nhắn tới client đang
  active dựa theo `m_aiProvider`; `setAiProvider()` đổi provider + lưu vào PetState + báo trong chat.
- `characterwidget.cpp`: menu chuột phải → "🤖 Đổi AI" → submenu checkable Gemini/ChatGPT/Claude.
- Xử lý API khác nhau theo từng provider:
  - **Gemini**: `generativelanguage.googleapis.com`, `systemInstruction` + `contents[].parts[].text`,
    retry 503/429.
  - **ChatGPT (OpenAI)**: `api.openai.com/v1/chat/completions`, `Authorization: Bearer`, messages
    array với role system+user, đọc `choices[0].message.content`, retry 429/5xx.
  - **Claude (Anthropic)**: `api.anthropic.com/v1/messages`, header `x-api-key` +
    `anthropic-version: 2023-06-01`, field `system` riêng, đọc `content[].text`, xử lý
    `stop_reason == "refusal"`, retry 429/529/5xx. Model mặc định `claude-opus-4-8`.
- Build Release qua CMake+Ninja+MinGW test OK, không lỗi. Cần user tự điền `openai_api_key` /
  `claude_api_key` vào `config/config.json` để dùng 2 provider mới (Gemini vẫn là default nếu
  chưa cấu hình).

---

## 11. Tài liệu liên quan

- `TrashPet_AI_Desktop_Assistant.md` — Spec gốc
- `TrashPetAI_GD5_Design_v3.md` — Design chi tiết GĐ 5 (FINAL)
- `TrashPetAI_Progress.md` — File này
