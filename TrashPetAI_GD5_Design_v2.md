# TrashPet AI — GĐ 5 Design (v2, đã chốt)

> Codex update 2026-07-06: GĐ 5.1, 5.2, 5.3 và 5.5 đã code xong theo flow hiện tại. User đã test runtime phân đoạn ăn rác; build terminal bỏ qua vì chạy qua Qt Creator.

## 0. Checklist tiến độ GĐ 5

- [x] Phase 5.1: `PetState`, `HungerManager`, COM `RecycleBinManager`, `EatDialog`, gắn hunger vào `CharacterWidget`, rewrite `ChatWidget::triggerEatTrash`.
- [x] Phase 5.2: Size system, `m_petSize`, scale GIF/PNG theo size, mập khi ăn dư, gầy dần khi đói lâu.
- [x] Phase 5.3: Visual eating feedback, hiện label nổi `+N MB` mỗi lần pet ăn/xóa được 1 file.
- [x] Phase 5.5: Pet bò quanh màn hình khi đang idle, tự đổi hướng, tự né mép màn hình, tự lật mắt/sprite theo hướng bò, tạm dừng khi mở chat/drag/ăn/chat/lỗi/ngủ.
- [ ] GĐ 6: Đóng gói `.exe` bằng `windeployqt`.

> Version 2: cập nhật theo confirm của user.

---

## 1. Chốt design

| Điểm | Quyết định |
|---|---|
| Thứ tự ăn default | **Cũ nhất trước** (sort by `deletedAt` ascending) |
| User chọn được không | **CÓ** — radio button trong dialog: `Cũ nhất / Lớn nhất` |
| Đơn vị ăn | **Theo dung lượng** (không phải số file) |
| Tỉ lệ | **1 MB = -1 hunger** |
| Đầy bụng | 100 MB ăn vào = giảm 100 hunger (no full từ 100 → 0) |
| File quá lớn | **> 1 GB → SKIP**, không ăn được nguyên lần, không chia nhỏ |
| Khi nào dừng ăn | hunger=0 HOẶC hết file phù hợp |
| Dư bụng → mập | (MB ăn được - hunger giảm) ÷ 10 = số px size tăng |
| Popup nhắc đói | `QMessageBox` (modal, chặn user) |
| Persist | File riêng `config/pet_state.json` |

---

## 2. Công thức tính chi tiết

### 2.1. Khi ăn 1 file size `S` MB

```
hungerBefore = m_hunger  // ví dụ 80
hungerCost   = min(S, m_hunger)
m_hunger    -= hungerCost
overflow     = S - hungerCost  // phần dư (chỉ > 0 khi hunger đã về 0)

// Mập lên do ăn dư
if (overflow > 0) {
    m_petSize += overflow / 10  // 80MB dư → +8px
    m_petSize  = min(m_petSize, kPetSizeMax)
}
```

### 2.2. Skip file lớn

```cpp
constexpr quint64 kMaxFileBytes = 1ULL * 1024 * 1024 * 1024;  // 1 GB

if (file.size > kMaxFileBytes) {
    // Skip + ghi log
    skipped.append(file);
    continue;
}
```

### 2.3. Vòng lặp ăn

```
sort items theo logic user chọn
foreach item in sortedItems:
    if item.size > 1GB:
        skip, add to skippedList
        continue
    
    eat(item)            // gọi IFileOperation::DeleteItem
    updateHunger(item.size_MB)
    showEatingAnimation()
    wait 300ms (UI feedback)
    
    if m_hunger == 0:
        break            // no rồi, dừng

emit doneEating(eatenCount, skippedList)
```

---

## 3. Dialog UX

### 3.1. Lúc bấm "🗑️ Ăn rác"

```
┌─────────────────────────────────────────┐
│  🐌 TrashPet đang đói: 75/100          │
├─────────────────────────────────────────┤
│  Thùng rác: 45 file (3.2 GB)            │
│  (3 file > 1GB sẽ bị skip)              │
│                                          │
│  Thứ tự ăn:                              │
│    ◉ Cũ nhất trước                      │
│    ○ Lớn nhất trước                     │
│                                          │
│  Ước tính ăn: ~75 MB                    │
│  (ăn đến khi no hoặc hết)                │
│                                          │
│         [🍴 Cho ăn!]    [Hủy]            │
└─────────────────────────────────────────┘
```

### 3.2. Lúc popup "đói"

**Vượt 40:**
- Title: "TrashPet hơi đói rồi 🥺"
- Body: "Bụng mình đang sôi nhẹ rồi... có rác nào không?"
- `[Cho ăn ngay]` → mở dialog 3.1 / `[Để lát]`

**Vượt 70:**
- Title: "TrashPet đói lắm rồi 🥺🥺🥺"
- Body: "Mình đói lả rồi bạn ơi..."
- `[Cho ăn ngay!]` / `[Để lát]`

---

## 4. File `pet_state.json`

```json
{
  "hunger": 35,
  "pet_size": 160,
  "last_save_iso": "2026-06-24T15:30:00",
  "lifetime_eaten_mb": 1240
}
```

Khi load lại:
- Tính `secondsSinceSave` từ `last_save_iso`
- Hunger tăng theo offline: `m_hunger += secondsSinceSave / kHungerTickSec`
- Clamp [0, 100]

---

## 5. Constants (test mode)

```cpp
// hungerconfig.h
namespace HungerCfg {
    constexpr int  kTickMs           = 10 * 1000;  // 10s/tick (TEST)
    constexpr int  kHungerPerTick    = 1;
    constexpr int  kHungerMax        = 100;
    constexpr int  kThresholdMild    = 40;
    constexpr int  kThresholdHungry  = 70;

    constexpr quint64 kMaxFileBytes  = 1ULL << 30;     // 1 GB
    constexpr int  kMBPerHunger      = 1;              // 1 MB = -1 hunger
    constexpr int  kOverflowToSize   = 10;             // 10 MB dư = +1 px size
    constexpr int  kEatDelayMs       = 300;            // mỗi file 300ms anim
}

// sizeconfig.h
namespace SizeCfg {
    constexpr int kMin     = 80;
    constexpr int kDefault = 160;
    constexpr int kMax     = 320;
}
```

---

## 6. Lộ trình code

### Phase 5.1 (TUẦN TỰ — code theo thứ tự)

1. **`petstate.h/.cpp`** — class load/save `pet_state.json`
2. **`hungermanager.h/.cpp`** — QTimer tăng hunger, emit signal khi vượt ngưỡng
3. **`recyclebinmanager.cpp` refactor** — dùng COM API, có `listItems()` và `deleteItem()`
4. **`eatdialog.h/.cpp`** — dialog mới với radio button thứ tự + ước tính
5. **`characterwidget`** — gắn HungerManager, persist trên close
6. **`chatwidget::triggerEatTrash`** — viết lại theo flow mới

### Phase 5.2 (sau)
- Size system: thay `kPetSize` constant → `m_petSize`, resize movie/pixmap mỗi `setState`
- Logic tăng/giảm size theo overflow / đói lâu

### Phase 5.3 (đã làm)
- Visual eating feedback: mỗi lần xóa/ăn được 1 file, `CharacterWidget` hiện label nổi `+N MB` trên pet rồi tự bay lên/ẩn đi.

### Phase 5.5 (đã làm)
- Pet tự bò nhẹ quanh màn hình bằng timer khi ở trạng thái `Idle`.
- Pet tự đổi hướng ngẫu nhiên, bật lại khi chạm mép vùng màn hình khả dụng.
- Sprite tự mirror theo hướng bò: asset gốc nhìn trái, khi bò sang phải thì frame GIF/PNG được lật ngang bằng code.
- Pet tạm dừng bò khi user đang giữ/drag chuột, khi chat đang mở, hoặc khi pet đang thinking/talking/eating/happy/sleeping/error.

### Cần test trong Qt Creator
- Chạy app và để pet idle: pet phải tự di chuyển nhẹ quanh màn hình.
- Khi pet bò sang trái/phải: mắt phải nhìn cùng hướng di chuyển, không còn cảm giác bò lùi.
- Mở chat: pet phải dừng bò.
- Đóng chat: pet tiếp tục bò sau đó.
- Kéo pet bằng chuột: pet không được giật khỏi tay.
- Cho pet ăn rác/chat với Gemini: pet không bò trong lúc đang ăn/suy nghĩ/trả lời.

---

## 7. Phần khó nhất

**Enum Recycle Bin với COM** — đây là phần phải code cẩn thận:

- `CoInitializeEx` trong main / init
- `SHGetKnownFolderItem(FOLDERID_RecycleBinFolder, ...)`
- `BindToHandler(BHID_EnumItems)` → `IEnumShellItems`
- Mỗi item: query property store cho `PKEY_Size`, `PKEY_Recycle_DateDeleted`
- Xóa: `IFileOperation::Initialize` → `SetOperationFlags(FOF_NO_UI)` → `DeleteItem` → `PerformOperations`

Code này dài, dễ leak COM ref. Phải xài `Microsoft::WRL::ComPtr` hoặc tự cẩn thận `Release()`.

---

## 8. Confirm cuối

Trước khi mình code, bạn confirm 2 điểm:

- [ ] **Pet state file** lưu ở `config/pet_state.json` cùng `config.json` (đề xuất)?
- [ ] **Skip file > 1GB** thì bot có nói rõ tên file không, hay chỉ "có 3 file quá lớn đã skip"?
  - **A:** Liệt kê tên ("đã skip: report.iso, ubuntu.iso, backup.zip")
  - **B:** Chỉ đếm ("3 file lớn đã skip — bạn dọn tay nhé")

Sau khi confirm 2 cái này → mình bắt đầu code Phase 5.1, bắt đầu từ `petstate` và `hungermanager` (dễ trước), rồi tới phần COM khó nhất.
