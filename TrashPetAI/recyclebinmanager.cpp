#include "recyclebinmanager.h"

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <ole2.h>
#include <QDebug>

// Định nghĩa thủ công vì MinGW headers có thể thiếu các key này

// PKEY_Size: {B725F130-47EF-101A-A5F1-02608C9EEBAC} pid=12
static const PROPERTYKEY kPKEY_Size = {
    {0xB725F130, 0x47EF, 0x101A, {0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC}}, 12
};

// PKEY_DateDeleted: {9B174B33-40FF-11D2-A27E-00C04FC30871} pid=3
static const PROPERTYKEY kPKEY_DateDeleted = {
    {0x9B174B33, 0x40FF, 0x11D2, {0xA2, 0x7E, 0x00, 0xC0, 0x4F, 0xC3, 0x08, 0x71}}, 3
};

// Chuyển FILETIME Windows → QDateTime
static QDateTime filetimeToQDateTime(const FILETIME &ft) {
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // FILETIME: 100ns intervals từ 1601-01-01
    // Unix epoch: từ 1970-01-01 → hiệu = 116444736000000000 * 100ns
    constexpr quint64 kEpochDiff = 116444736000000000ULL;
    if (uli.QuadPart < kEpochDiff) return QDateTime();
    qint64 unixSecs = static_cast<qint64>((uli.QuadPart - kEpochDiff) / 10000000ULL);
    return QDateTime::fromSecsSinceEpoch(unixSecs);
}

// Helper: lấy property UINT64 từ IShellItem2
static quint64 getItemSize(IShellItem2 *pItem2) {
    PROPVARIANT pv = {};
    PropVariantInit(&pv);
    HRESULT hr = pItem2->GetProperty(kPKEY_Size, &pv);
    quint64 size = 0;
    if (SUCCEEDED(hr)) {
        if (pv.vt == VT_UI8)       size = pv.uhVal.QuadPart;
        else if (pv.vt == VT_UI4)  size = pv.ulVal;
        else if (pv.vt == VT_UI2)  size = pv.uiVal;
    }
    PropVariantClear(&pv);
    return size;
}

// Helper: lấy ngày xóa từ IShellItem2
static QDateTime getDeletedAt(IShellItem2 *pItem2) {
    PROPVARIANT pv = {};
    PropVariantInit(&pv);
    QDateTime result;
    HRESULT hr = pItem2->GetProperty(kPKEY_DateDeleted, &pv);
    if (SUCCEEDED(hr) && pv.vt == VT_FILETIME) {
        result = filetimeToQDateTime(pv.filetime);
    }
    PropVariantClear(&pv);
    return result;
}

QVector<RecycleBinManager::Item> RecycleBinManager::listItems() {
    m_lastError.clear();
    QVector<Item> result;

    // Lấy IShellItem của Recycle Bin (virtual folder)
    IShellItem *pBinItem = nullptr;
    HRESULT hr = SHGetKnownFolderItem(FOLDERID_RecycleBinFolder,
                                       KF_FLAG_DEFAULT,
                                       nullptr,
                                       IID_PPV_ARGS(&pBinItem));
    if (FAILED(hr)) {
        m_lastError = QString("Không mở được Recycle Bin (0x%1)").arg(hr, 8, 16, QChar('0'));
        return result;
    }

    // Lấy enumerator — BHID_EnumItems là GUID cố định
    // {94F60519-2850-4924-AA5A-D15E84868039}
    static const GUID kBHID_EnumItems = {
        0x94F60519, 0x2850, 0x4924,
        {0xAA, 0x5A, 0xD1, 0x5E, 0x84, 0x86, 0x80, 0x39}
    };

    IEnumShellItems *pEnum = nullptr;
    hr = pBinItem->BindToHandler(nullptr, kBHID_EnumItems, IID_PPV_ARGS(&pEnum));
    pBinItem->Release();

    if (FAILED(hr)) {
        // Thùng rác trống → S_FALSE, hoặc lỗi thật
        if (hr != S_FALSE)
            m_lastError = QString("Không thể liệt kê Recycle Bin (0x%1)").arg(hr, 8, 16, QChar('0'));
        return result;
    }

    IShellItem *pItem = nullptr;
    while (pEnum->Next(1, &pItem, nullptr) == S_OK) {
        // Nâng lên IShellItem2 để lấy property
        IShellItem2 *pItem2 = nullptr;
        hr = pItem->QueryInterface(IID_PPV_ARGS(&pItem2));

        if (SUCCEEDED(hr) && pItem2) {
            Item entry;
            entry.sizeBytes = getItemSize(pItem2);
            entry.deletedAt = getDeletedAt(pItem2);
            pItem2->Release();

            // Tên hiển thị (tên gốc trước khi xóa)
            LPWSTR pszDisplay = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pszDisplay))) {
                entry.displayName = QString::fromWCharArray(pszDisplay);
                CoTaskMemFree(pszDisplay);
            }

            // Parsing name (đường dẫn thật trong $Recycle.Bin)
            LPWSTR pszParsing = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pszParsing))) {
                entry.parsingName = QString::fromWCharArray(pszParsing);
                CoTaskMemFree(pszParsing);
            }

            if (!entry.parsingName.isEmpty()) {
                result.append(entry);
            }
        }

        pItem->Release();
    }

    pEnum->Release();
    return result;
}

bool RecycleBinManager::deleteItem(const Item &item) {
    m_lastError.clear();

    if (item.parsingName.isEmpty()) {
        m_lastError = "Không có đường dẫn để xóa.";
        return false;
    }

    // Tạo IShellItem từ parsing name
    IShellItem *pDeleteItem = nullptr;
    const std::wstring wPath = item.parsingName.toStdWString();
    HRESULT hr = SHCreateItemFromParsingName(wPath.c_str(), nullptr, IID_PPV_ARGS(&pDeleteItem));
    if (FAILED(hr)) {
        m_lastError = QString("Không tạo được IShellItem cho: %1 (0x%2)")
                          .arg(item.displayName).arg(hr, 8, 16, QChar('0'));
        return false;
    }

    // Tạo IFileOperation để xóa vĩnh viễn (không qua xác nhận shell)
    IFileOperation *pfo = nullptr;
    hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
    if (FAILED(hr)) {
        pDeleteItem->Release();
        m_lastError = "Không khởi tạo được IFileOperation.";
        return false;
    }

    // FOF_NO_UI + FOFX_NOMINIMIZEBOX: không hiện dialog, không recycle
    pfo->SetOperationFlags(FOF_NO_UI | FOFX_NOMINIMIZEBOX);
    pfo->DeleteItem(pDeleteItem, nullptr);
    hr = pfo->PerformOperations();

    BOOL anyAborted = FALSE;
    pfo->GetAnyOperationsAborted(&anyAborted);

    pfo->Release();
    pDeleteItem->Release();

    if (FAILED(hr) || anyAborted) {
        m_lastError = QString("Xóa thất bại: %1").arg(item.displayName);
        return false;
    }
    return true;
}
