# TrashPet AI Desktop Assistant

## 1. Giới thiệu dự án

**TrashPet AI Desktop Assistant** là một ứng dụng chatbot 2D chạy trên màn hình desktop Windows. Ứng dụng có hình dạng là một nhân vật 2D do người dùng tự custom, có thể nổi trên màn hình, kéo thả tự do, trả lời câu hỏi bằng **Gemini API**, đóng gói thành file `.exe`, và có thêm tính năng vui là **“ăn file trong thùng rác”**.

Trong dự án này, “ăn file trong thùng rác” được hiểu là: nhân vật 2D kiểm tra Recycle Bin, báo cho người dùng biết trong thùng rác đang có bao nhiêu file, tổng dung lượng bao nhiêu, sau đó hỏi xác nhận. Nếu người dùng đồng ý, nhân vật sẽ chạy animation ăn rác và chương trình tiến hành dọn Recycle Bin.

> Lưu ý quan trọng: Không nên tự động xóa file trong thùng rác nếu chưa có xác nhận của người dùng, vì sau khi dọn Recycle Bin thì việc khôi phục file sẽ khó hơn.

---

## 2. Mục tiêu dự án

Xây dựng một ứng dụng desktop bằng **C++** có các khả năng sau:

- Hiển thị nhân vật 2D trên desktop.
- Nhân vật có thể được custom bằng ảnh PNG, GIF hoặc sprite sheet.
- Có cửa sổ chat nhỏ để người dùng nhập câu hỏi.
- Chatbot trả lời bằng cách kết nối với **Gemini API**.
- Có thể build thành file `.exe` chạy trên Windows.
- Có thể kiểm tra và dọn Recycle Bin.
- Khi dọn Recycle Bin, nhân vật chạy animation “ăn rác”.
- Có file cấu hình để thay API key, model AI, tên nhân vật, trạng thái always-on-top.

---

## 3. Công nghệ sử dụng

### 3.1. Ngôn ngữ lập trình

```txt
C++17 hoặc C++20
```

Nên dùng C++17 trở lên để code dễ tổ chức hơn, hỗ trợ thư viện hiện đại hơn và thuận tiện khi dùng CMake.

### 3.2. Giao diện desktop

Khuyến nghị dùng:

```txt
Qt 6
```

Lý do nên dùng Qt 6:

- Hỗ trợ giao diện desktop tốt.
- Có thể tạo cửa sổ trong suốt.
- Có thể tạo cửa sổ không viền.
- Dễ hiển thị ảnh PNG, GIF, animation.
- Dễ tạo textbox, button, khung chat.
- Hỗ trợ HTTP request thông qua `QNetworkAccessManager`.
- Có thể build ứng dụng Windows thành `.exe`.

Có thể dùng Win32 API thuần, nhưng sẽ khó hơn nếu bạn chưa quen lập trình giao diện Windows bằng C++.

### 3.3. Kết nối Gemini API

Có 2 hướng:

#### Cách 1: Dùng Qt Network

Nếu đã dùng Qt 6 cho giao diện, nên dùng luôn:

```txt
QNetworkAccessManager
QNetworkRequest
QNetworkReply
QJsonDocument
QJsonObject
QJsonArray
```

Ưu điểm:

- Không cần thêm libcurl.
- Tích hợp tốt với Qt.
- Xử lý async dễ hơn.

#### Cách 2: Dùng libcurl

Nếu muốn tách riêng phần HTTP khỏi Qt, có thể dùng:

```txt
libcurl
nlohmann/json
```

Ưu điểm:

- Phổ biến.
- Dùng được với cả app không dùng Qt.
- Dễ kiểm soát request.

Khuyến nghị cho dự án này:

```txt
Qt 6 + Qt Network + QJsonDocument
```

### 3.4. Tương tác Recycle Bin

Dùng Windows Shell API:

```cpp
SHQueryRecycleBinW
SHEmptyRecycleBinW
```

Chức năng:

- `SHQueryRecycleBinW`: kiểm tra số file và tổng dung lượng trong Recycle Bin.
- `SHEmptyRecycleBinW`: dọn Recycle Bin.

---

## 4. Mô tả tổng quan ứng dụng

Ứng dụng gồm 3 phần chính:

### 4.1. Nhân vật 2D trên desktop

Nhân vật xuất hiện trên màn hình như một desktop pet.

Yêu cầu:

- Cửa sổ không viền.
- Nền trong suốt.
- Có thể kéo thả.
- Có thể luôn nằm trên các cửa sổ khác.
- Có thể click để mở khung chat.
- Có nhiều trạng thái animation.

Các trạng thái nhân vật:

```txt
idle       - Đứng yên
thinking   - Đang suy nghĩ
chatting   - Đang nói chuyện
eating     - Đang ăn rác
happy      - Vui vẻ
sleeping   - Ngủ khi không hoạt động
error      - Báo lỗi
```

### 4.2. Khung chat

Khung chat xuất hiện khi người dùng click vào nhân vật hoặc bấm phím tắt.

Khung chat gồm:

- Vùng hiển thị hội thoại.
- Ô nhập câu hỏi.
- Nút gửi.
- Nút dọn Recycle Bin.
- Nút cài đặt.
- Nút thu nhỏ.

Luồng chat:

```txt
Người dùng nhập câu hỏi
        ↓
Ứng dụng gửi prompt đến Gemini API
        ↓
Gemini API trả về JSON
        ↓
Ứng dụng parse câu trả lời
        ↓
Hiển thị câu trả lời trong khung chat
        ↓
Nhân vật chuyển sang animation talking/happy
```

### 4.3. Tính năng ăn file trong thùng rác

Luồng xử lý:

```txt
Người dùng bấm nút "Ăn rác"
        ↓
Ứng dụng kiểm tra Recycle Bin
        ↓
Hiển thị số file và dung lượng
        ↓
Hỏi người dùng xác nhận
        ↓
Nếu đồng ý, nhân vật chạy animation eating
        ↓
Ứng dụng gọi API dọn Recycle Bin
        ↓
Nhân vật báo "Mình ăn xong rồi!"
```

---

## 5. Yêu cầu chức năng chi tiết

## 5.1. Chức năng nhân vật 2D

### Yêu cầu bắt buộc

- Nhân vật được hiển thị bằng ảnh trong thư mục `assets/character`.
- Hỗ trợ ảnh PNG hoặc GIF.
- Cửa sổ nhân vật phải có nền trong suốt.
- Cửa sổ nhân vật không có thanh title bar.
- Có thể kéo thả nhân vật bằng chuột.
- Click chuột trái vào nhân vật để mở hoặc ẩn khung chat.
- Click chuột phải để mở menu nhanh.

### Menu chuột phải nên có

```txt
Mở chat
Ăn rác
Đổi nhân vật
Cài đặt
Thoát
```

### Các file hình ảnh nhân vật

Thư mục đề xuất:

```txt
assets/character/
├── idle.png
├── thinking.gif
├── talking.gif
├── eating.gif
├── happy.png
├── sleeping.png
└── error.png
```

Nếu chưa có GIF, có thể dùng PNG tạm thời.

---

## 5.2. Chức năng chat AI

### Yêu cầu bắt buộc

- Người dùng nhập câu hỏi trong khung chat.
- Ứng dụng gửi câu hỏi đến Gemini API.
- Nhận phản hồi từ Gemini API.
- Hiển thị câu trả lời trong khung chat.
- Không để API key trực tiếp trong source code.
- API key phải đọc từ file cấu hình.
- Nếu lỗi mạng, hiển thị thông báo dễ hiểu.
- Nếu API key sai, báo người dùng kiểm tra lại config.

### Trạng thái nhân vật khi chat

```txt
Khi người dùng gửi câu hỏi: thinking
Khi AI trả lời: talking
Khi trả lời xong: idle hoặc happy
Khi lỗi API: error
```

### Ví dụ hội thoại

```txt
User: Hôm nay tôi nên học gì?
Bot: Nếu bạn đang làm dự án C++ desktop, hôm nay nên học Qt Widget, signal-slot và cách gọi API HTTP.
```

---

## 5.3. Chức năng Gemini API

### Cách lưu API key

Không viết như sau trong code:

```cpp
std::string apiKey = "AIza...";
```

Nên lưu trong file:

```txt
config/config.json
```

Ví dụ:

```json
{
  "gemini_api_key": "YOUR_API_KEY_HERE",
  "gemini_model": "gemini-2.5-flash",
  "character_name": "TrashPet",
  "always_on_top": true,
  "confirm_before_empty_trash": true,
  "language": "vi"
}
```

### GeminiClient cần làm gì?

Class `GeminiClient` có nhiệm vụ:

- Đọc API key từ `ConfigManager`.
- Tạo request JSON.
- Gửi request đến Gemini API.
- Nhận response JSON.
- Parse text trả lời.
- Trả text về cho `ChatWidget`.

### Request mẫu dạng ý tưởng

```json
{
  "contents": [
    {
      "parts": [
        {
          "text": "Xin chào, bạn là ai?"
        }
      ]
    }
  ]
}
```

---

## 5.4. Chức năng ăn Recycle Bin

### Yêu cầu bắt buộc

- Có nút “Ăn rác” trong khung chat.
- Có menu chuột phải “Ăn rác”.
- Trước khi dọn rác phải kiểm tra Recycle Bin.
- Phải hiển thị số lượng file và dung lượng.
- Phải hỏi xác nhận trước khi xóa.
- Sau khi xác nhận mới được dọn Recycle Bin.
- Khi đang dọn rác, nhân vật chuyển sang trạng thái `eating`.
- Nếu thùng rác trống, không gọi lệnh xóa.

### Câu thoại gợi ý

Khi Recycle Bin trống:

```txt
Không có gì để ăn hết á.
```

Khi có file:

```txt
Trong thùng rác đang có 25 file, tổng dung lượng khoảng 1.2 GB. Bạn có muốn mình ăn hết không?
```

Khi đang ăn:

```txt
Đợi mình ăn xíu nha...
```

Khi ăn xong:

```txt
Mình ăn xong rồi!
```

Khi lỗi:

```txt
Hình như mình chưa ăn được. Bạn thử chạy app bằng quyền Administrator hoặc kiểm tra lại quyền hệ thống nha.
```

### Code C++ mẫu kiểm tra Recycle Bin

```cpp
#include <windows.h>
#include <shellapi.h>
#include <iostream>

void checkRecycleBin() {
    SHQUERYRBINFO rbInfo;
    rbInfo.cbSize = sizeof(SHQUERYRBINFO);

    HRESULT hr = SHQueryRecycleBinW(nullptr, &rbInfo);

    if (SUCCEEDED(hr)) {
        std::wcout << L"So file: " << rbInfo.i64NumItems << std::endl;
        std::wcout << L"Dung luong MB: " << rbInfo.i64Size / (1024 * 1024) << std::endl;
    } else {
        std::wcout << L"Khong the doc Recycle Bin." << std::endl;
    }
}
```

### Code C++ mẫu dọn Recycle Bin

```cpp
#include <windows.h>
#include <shellapi.h>

bool emptyRecycleBin(HWND hwnd) {
    HRESULT hr = SHEmptyRecycleBinW(
        hwnd,
        nullptr,
        SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI
    );

    return SUCCEEDED(hr);
}
```

> Ghi chú: Chỉ nên dùng `SHERB_NOCONFIRMATION` sau khi app đã có hộp thoại xác nhận riêng.

---

## 6. Yêu cầu phi chức năng

### 6.1. An toàn dữ liệu

- Không tự động xóa file nếu người dùng chưa xác nhận.
- Không dọn Recycle Bin bằng câu lệnh mơ hồ.
- Không gửi nội dung file cá nhân lên Gemini API.
- Không đọc file trong Recycle Bin nếu không cần thiết.
- Chỉ kiểm tra số lượng file và dung lượng.

### 6.2. Bảo mật API key

- Không hard-code API key.
- Không upload file config chứa API key lên GitHub.
- Thêm `config/config.json` vào `.gitignore`.
- Tạo file mẫu `config.example.json`.

Ví dụ `.gitignore`:

```gitignore
build/
*.user
*.exe
config/config.json
```

### 6.3. Hiệu năng

- App phải nhẹ.
- Không gọi Gemini API liên tục khi người dùng chưa gửi câu hỏi.
- Không kiểm tra Recycle Bin liên tục.
- Animation không được làm giật máy.
- Cửa sổ nhân vật nên dùng kích thước hợp lý, ví dụ 160x160 hoặc 256x256.

### 6.4. Trải nghiệm người dùng

- Chatbot nên trả lời tiếng Việt mặc định.
- Câu trả lời nên ngắn gọn, dễ hiểu.
- Lỗi phải báo thân thiện.
- Có thể thu nhỏ hoặc thoát app dễ dàng.
- Có thể bật/tắt always-on-top.

---

## 7. Kiến trúc chương trình

## 7.1. Sơ đồ tổng quan

```txt
+-------------------+
|    MainWindow     |
+-------------------+
          |
          | quản lý
          v
+-------------------+       +-------------------+
|  CharacterWidget  |       |    ChatWidget     |
+-------------------+       +-------------------+
          |                           |
          | đổi trạng thái             | gửi câu hỏi
          v                           v
+-------------------+       +-------------------+
| AnimationManager  |       |   GeminiClient    |
+-------------------+       +-------------------+
                                      |
                                      | đọc config
                                      v
                            +-------------------+
                            |   ConfigManager   |
                            +-------------------+

+----------------------+
|  RecycleBinManager   |
+----------------------+
          |
          v
 Windows Shell API
```

---

## 7.2. Danh sách class đề xuất

### MainWindow

Nhiệm vụ:

- Khởi tạo ứng dụng.
- Quản lý nhân vật.
- Quản lý khung chat.
- Kết nối signal-slot giữa các thành phần.
- Xử lý thoát app.

### CharacterWidget

Nhiệm vụ:

- Hiển thị nhân vật 2D.
- Đổi hình/animation theo trạng thái.
- Cho phép kéo thả.
- Bắt sự kiện click chuột.
- Hiển thị menu chuột phải.

Hàm gợi ý:

```cpp
void setState(CharacterState state);
void loadCharacterAssets(const QString& folderPath);
void showContextMenu(const QPoint& position);
```

### ChatWidget

Nhiệm vụ:

- Hiển thị khung chat.
- Nhận input từ người dùng.
- Gửi câu hỏi đến GeminiClient.
- Hiển thị câu trả lời.
- Có nút “Ăn rác”.

Hàm gợi ý:

```cpp
void sendMessage();
void appendUserMessage(const QString& message);
void appendBotMessage(const QString& message);
void onTrashButtonClicked();
```

### GeminiClient

Nhiệm vụ:

- Gửi request đến Gemini API.
- Nhận response.
- Parse JSON.
- Trả kết quả qua signal.

Hàm gợi ý:

```cpp
void sendPrompt(const QString& prompt);
signal void responseReceived(const QString& text);
signal void errorOccurred(const QString& errorMessage);
```

### RecycleBinManager

Nhiệm vụ:

- Kiểm tra số lượng file trong Recycle Bin.
- Kiểm tra dung lượng Recycle Bin.
- Dọn Recycle Bin.

Hàm gợi ý:

```cpp
RecycleBinInfo queryRecycleBin();
bool emptyRecycleBin();
```

Struct gợi ý:

```cpp
struct RecycleBinInfo {
    long long itemCount;
    long long sizeBytes;
};
```

### ConfigManager

Nhiệm vụ:

- Đọc file `config.json`.
- Lấy API key.
- Lấy model Gemini.
- Lấy setting giao diện.
- Lưu thay đổi setting nếu cần.

Hàm gợi ý:

```cpp
bool loadConfig(const QString& path);
QString geminiApiKey() const;
QString geminiModel() const;
bool alwaysOnTop() const;
bool confirmBeforeEmptyTrash() const;
```

---

## 8. Cấu trúc thư mục dự án

```txt
TrashPetAI/
├── CMakeLists.txt
├── README.md
├── .gitignore
│
├── src/
│   ├── main.cpp
│   ├── MainWindow.h
│   ├── MainWindow.cpp
│   ├── CharacterWidget.h
│   ├── CharacterWidget.cpp
│   ├── ChatWidget.h
│   ├── ChatWidget.cpp
│   ├── GeminiClient.h
│   ├── GeminiClient.cpp
│   ├── RecycleBinManager.h
│   ├── RecycleBinManager.cpp
│   ├── ConfigManager.h
│   ├── ConfigManager.cpp
│   ├── AnimationManager.h
│   └── AnimationManager.cpp
│
├── assets/
│   └── character/
│       ├── idle.png
│       ├── thinking.gif
│       ├── talking.gif
│       ├── eating.gif
│       ├── happy.png
│       ├── sleeping.png
│       └── error.png
│
├── config/
│   ├── config.example.json
│   └── config.json
│
└── docs/
    ├── setup.md
    ├── build.md
    └── character_custom.md
```

---

## 9. File cấu hình mẫu

Tạo file:

```txt
config/config.example.json
```

Nội dung:

```json
{
  "gemini_api_key": "PUT_YOUR_GEMINI_API_KEY_HERE",
  "gemini_model": "gemini-2.5-flash",
  "character_name": "TrashPet",
  "language": "vi",
  "always_on_top": true,
  "confirm_before_empty_trash": true,
  "character_asset_folder": "assets/character",
  "window_width": 220,
  "window_height": 220
}
```

Người dùng copy file này thành:

```txt
config/config.json
```

Sau đó thay API key thật vào.

---

## 10. Luồng hoạt động chính

## 10.1. Khi mở app

```txt
Start app
  ↓
Đọc config.json
  ↓
Load assets nhân vật
  ↓
Tạo cửa sổ trong suốt
  ↓
Hiển thị nhân vật trạng thái idle
  ↓
Chờ người dùng tương tác
```

## 10.2. Khi người dùng chat

```txt
Người dùng nhập câu hỏi
  ↓
ChatWidget nhận input
  ↓
CharacterWidget chuyển sang thinking
  ↓
GeminiClient gửi request
  ↓
Nhận response
  ↓
Parse text
  ↓
ChatWidget hiển thị câu trả lời
  ↓
CharacterWidget chuyển sang talking
  ↓
Sau vài giây quay lại idle
```

## 10.3. Khi người dùng bấm “Ăn rác”

```txt
Người dùng bấm nút Ăn rác
  ↓
RecycleBinManager gọi SHQueryRecycleBinW
  ↓
Nếu thùng rác trống:
      Bot nói: "Không có gì để ăn hết á."
  ↓
Nếu có file:
      Hiển thị số file + dung lượng
      Hỏi xác nhận
  ↓
Nếu người dùng hủy:
      Không làm gì
  ↓
Nếu người dùng đồng ý:
      CharacterWidget chuyển sang eating
      RecycleBinManager gọi SHEmptyRecycleBinW
  ↓
Nếu thành công:
      Bot nói: "Mình ăn xong rồi!"
  ↓
Nếu thất bại:
      Bot báo lỗi thân thiện
```

---

## 11. Gợi ý giao diện

## 11.1. Nhân vật

Kích thước gợi ý:

```txt
160x160
220x220
256x256
```

Phong cách có thể là:

- Robot mini.
- Mèo AI.
- Slime ăn rác.
- Thùng rác biết nói.
- Bé ma dễ thương.
- Pet pixel art.
- Anime chibi.

## 11.2. Khung chat

Khung chat nên nhỏ gọn:

```txt
Chiều rộng: 360px - 420px
Chiều cao: 420px - 560px
Bo góc: 12px - 20px
Nền: trắng hoặc tối
Có bóng đổ nhẹ
```

Bố cục:

```txt
+--------------------------------+
| TrashPet AI              [x]   |
+--------------------------------+
| Bot: Xin chào, cần mình giúp gì?|
| User: ...                      |
| Bot: ...                       |
|                                |
+--------------------------------+
| [Nhập câu hỏi...]       [Gửi]  |
+--------------------------------+
| [Ăn rác] [Cài đặt]             |
+--------------------------------+
```

---

## 12. CMakeLists.txt mẫu ý tưởng

```cmake
cmake_minimum_required(VERSION 3.20)

project(TrashPetAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(TrashPetAI
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/CharacterWidget.cpp
    src/CharacterWidget.h
    src/ChatWidget.cpp
    src/ChatWidget.h
    src/GeminiClient.cpp
    src/GeminiClient.h
    src/RecycleBinManager.cpp
    src/RecycleBinManager.h
    src/ConfigManager.cpp
    src/ConfigManager.h
)

target_link_libraries(TrashPetAI
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        Shell32
)
```

---

## 13. Prompt hoàn chỉnh cho AI code

Bạn có thể copy prompt này đưa cho Claude Code, Codex hoặc AI code khác.

```md
Hãy xây dựng một ứng dụng Windows Desktop bằng C++ tên là "TrashPet AI Desktop Assistant".

Mục tiêu:
Tạo một chatbot 2D nổi trên màn hình desktop, có nhân vật do người dùng custom, có thể trả lời câu hỏi bằng Gemini API, xuất bản thành file .exe, và có tính năng "ăn file trong thùng rác" tức là dọn Recycle Bin sau khi người dùng xác nhận.

Công nghệ yêu cầu:
- Ngôn ngữ: C++17 hoặc C++20
- GUI: Qt 6 Widgets
- HTTP request: Qt Network
- JSON: QJsonDocument, QJsonObject, QJsonArray
- Build system: CMake
- Nền tảng: Windows
- Có thể build ra file .exe

Yêu cầu giao diện:
1. Ứng dụng hiển thị một nhân vật 2D nổi trên desktop.
2. Cửa sổ nhân vật không viền, nền trong suốt, có thể always-on-top.
3. Người dùng có thể kéo thả nhân vật trên màn hình.
4. Khi click vào nhân vật, mở một khung chat nhỏ.
5. Khung chat có:
   - Ô nhập câu hỏi
   - Nút gửi
   - Vùng hiển thị hội thoại
   - Nút "Ăn rác" để dọn Recycle Bin
   - Nút cài đặt
6. Nhân vật có các trạng thái animation:
   - idle
   - thinking
   - talking
   - eating
   - happy
   - sleeping
   - error
7. Cho phép người dùng thay ảnh nhân vật bằng cách thay file trong thư mục assets/character.

Yêu cầu kết nối Gemini API:
1. Đọc API key từ file config/config.json, không hard-code API key trong source.
2. Gửi prompt của người dùng đến Gemini API bằng REST API.
3. Nhận phản hồi JSON, parse text trả lời và hiển thị trong khung chat.
4. Khi đang chờ API trả lời, nhân vật chuyển sang trạng thái thinking.
5. Khi có câu trả lời, nhân vật chuyển sang talking.
6. Nếu lỗi mạng hoặc lỗi API key, hiển thị thông báo lỗi thân thiện.
7. Mặc định bot trả lời bằng tiếng Việt.

Yêu cầu tính năng Recycle Bin:
1. Thêm nút "Ăn rác".
2. Khi bấm nút này, app kiểm tra số lượng file và dung lượng trong Recycle Bin bằng Windows Shell API SHQueryRecycleBinW.
3. Hiển thị thông báo:
   "Trong thùng rác có X file, tổng dung lượng Y MB. Bạn có chắc muốn cho bot ăn hết không?"
4. Chỉ khi người dùng xác nhận thì mới gọi SHEmptyRecycleBinW để dọn Recycle Bin.
5. Không được tự động xóa rác nếu người dùng chưa xác nhận.
6. Khi đang dọn rác, nhân vật chuyển sang animation eating.
7. Sau khi dọn xong, nhân vật nói:
   "Mình ăn xong rồi!"
8. Nếu Recycle Bin trống, nhân vật nói:
   "Không có gì để ăn hết á."
9. Nếu dọn rác thất bại, hiện lỗi thân thiện.

Yêu cầu cấu trúc thư mục:
- /src
  - main.cpp
  - MainWindow.h
  - MainWindow.cpp
  - CharacterWidget.h
  - CharacterWidget.cpp
  - ChatWidget.h
  - ChatWidget.cpp
  - GeminiClient.h
  - GeminiClient.cpp
  - RecycleBinManager.h
  - RecycleBinManager.cpp
  - ConfigManager.h
  - ConfigManager.cpp
  - AnimationManager.h
  - AnimationManager.cpp
- /assets/character
  - idle.png
  - thinking.gif
  - talking.gif
  - eating.gif
  - happy.png
  - sleeping.png
  - error.png
- /config
  - config.example.json
  - config.json
- CMakeLists.txt
- README.md
- .gitignore

Yêu cầu code:
1. Code rõ ràng, chia class đúng chức năng.
2. Có xử lý lỗi đầy đủ.
3. Không để API key lộ trong source.
4. Có README hướng dẫn build bằng CMake + Visual Studio.
5. Có hướng dẫn cấu hình Gemini API key.
6. Có hướng dẫn thay nhân vật 2D.
7. Có comment những phần quan trọng.
8. Dự án phải build được trên Windows.

Hãy tạo toàn bộ source code mẫu có thể build được.
```

---

## 14. Lộ trình triển khai từng bước

## Giai đoạn 1: Làm nhân vật hiện trên desktop

Mục tiêu:

- Tạo app Qt cơ bản.
- Hiển thị cửa sổ trong suốt.
- Load ảnh `idle.png`.
- Cho phép kéo thả nhân vật.

Kết quả cần đạt:

```txt
Mở app lên thấy nhân vật 2D nằm trên desktop và kéo đi được.
```

## Giai đoạn 2: Làm khung chat

Mục tiêu:

- Click nhân vật để mở khung chat.
- Có ô nhập câu hỏi.
- Có nút gửi.
- Hiển thị tin nhắn user và bot.

Kết quả cần đạt:

```txt
Người dùng nhập gì đó, app hiển thị lại trong khung chat.
```

## Giai đoạn 3: Kết nối Gemini API

Mục tiêu:

- Đọc API key từ config.
- Gửi prompt đến Gemini.
- Nhận câu trả lời.
- Hiển thị câu trả lời trong chat.

Kết quả cần đạt:

```txt
Người dùng hỏi, bot trả lời được bằng Gemini API.
```

## Giai đoạn 4: Thêm animation trạng thái

Mục tiêu:

- Khi chờ API: thinking.
- Khi trả lời: talking.
- Khi bình thường: idle.
- Khi lỗi: error.

Kết quả cần đạt:

```txt
Nhân vật thay đổi biểu cảm theo trạng thái.
```

## Giai đoạn 5: Thêm tính năng ăn rác

Mục tiêu:

- Kiểm tra Recycle Bin.
- Hiển thị số file và dung lượng.
- Hỏi xác nhận.
- Dọn Recycle Bin.
- Chạy animation eating.

Kết quả cần đạt:

```txt
Bấm "Ăn rác", bot hỏi xác nhận rồi dọn thùng rác an toàn.
```

## Giai đoạn 6: Đóng gói EXE

Mục tiêu:

- Build release.
- Copy assets và config đi kèm.
- Dùng `windeployqt` để gom thư viện Qt.
- Chạy được file `.exe` trên máy khác.

Kết quả cần đạt:

```txt
Có thư mục release chứa file TrashPetAI.exe chạy được.
```

---

## 15. Những lỗi dễ gặp

### 15.1. Không hiện nền trong suốt

Nguyên nhân có thể:

- Chưa set `Qt::FramelessWindowHint`.
- Chưa set `Qt::WA_TranslucentBackground`.
- Ảnh nhân vật không có nền trong suốt.

### 15.2. Không kéo được nhân vật

Nguyên nhân có thể:

- Chưa override `mousePressEvent`.
- Chưa override `mouseMoveEvent`.
- Chưa lưu vị trí chuột ban đầu.

### 15.3. Gemini API không trả lời

Nguyên nhân có thể:

- API key sai.
- Chưa bật quyền Gemini API.
- Sai endpoint.
- JSON request sai format.
- Máy không có internet.

### 15.4. Không dọn được Recycle Bin

Nguyên nhân có thể:

- App không đủ quyền.
- Recycle Bin đang bị khóa bởi hệ thống.
- File trong thùng rác đang được process khác sử dụng.
- Gọi sai Windows Shell API.

### 15.5. Build xong EXE chạy không được trên máy khác

Nguyên nhân có thể:

- Thiếu DLL của Qt.
- Chưa chạy `windeployqt`.
- Chưa copy thư mục `assets`.
- Chưa copy thư mục `config`.

---

## 16. Gợi ý nâng cấp sau này

Sau khi bản cơ bản chạy được, có thể nâng cấp thêm:

- Nhận lệnh bằng giọng nói.
- Bot đọc câu trả lời bằng text-to-speech.
- Cho phép thay skin nhân vật trong giao diện.
- Thêm biểu cảm theo nội dung câu trả lời.
- Thêm bộ nhớ hội thoại cục bộ.
- Thêm command đặc biệt, ví dụ:

```txt
/clear-trash
/change-skin
/sleep
/wake
/settings
```

- Thêm hiệu ứng nhân vật đi lại trên màn hình.
- Thêm chức năng nhắc việc.
- Thêm đồng hồ, lịch, thời tiết.
- Thêm chế độ offline với câu trả lời mặc định.

---

## 17. Kết luận

Dự án **TrashPet AI Desktop Assistant** hoàn toàn có thể làm bằng C++. Cách triển khai hợp lý nhất là dùng:

```txt
C++17 hoặc C++20
Qt 6 Widgets
Qt Network
Gemini API
Windows Shell API
CMake
```

Bản đầu tiên nên tập trung vào 5 chức năng chính:

1. Nhân vật 2D nổi trên desktop.
2. Kéo thả nhân vật.
3. Mở khung chat.
4. Chat được với Gemini API.
5. Ăn/dọn Recycle Bin sau khi xác nhận.

Khi 5 phần này chạy ổn, mới nên nâng cấp thêm skin, giọng nói, animation phức tạp và các tính năng trợ lý cá nhân khác.
