// Installer helper cho TrashPetAI_Setup.exe.
// IExpress (SFX builder có sẵn của Windows) chỉ hỗ trợ ổn định khi AppLaunched
// trỏ thẳng vào một .exe thật — trỏ vào .bat/.cmd khiến nó cố gọi qua
// "Command.com" (không tồn tại trên Windows 64-bit) và báo lỗi. Nên toàn bộ
// logic cài đặt (copy file, tạo shortcut, khởi chạy app) được viết ở đây
// thay vì trong install.bat.
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <utility>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

static std::wstring exeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    return s.substr(0, s.find_last_of(L'\\'));
}

// IExpress (SFX builder) LÀM PHẲNG toàn bộ cây thư mục khi giải nén — mọi file
// trong [SourceFilesN] dù khai báo với đường dẫn con (vd "platforms\qwindows.dll")
// đều bị đặt phẳng vào cùng một thư mục temp, mất hết cấu trúc con. Hệ quả:
// Qt không tìm thấy qwindows.dll (phải nằm trong platforms\) → lỗi
// "no Qt platform plugin could be initialized". Nên ở đây phải chủ động đặt
// lại từng file vào đúng thư mục con theo cấu trúc mà windeployqt yêu cầu.
static const std::pair<const wchar_t *, const wchar_t *> kSubfolderMap[] = {
    {L"qwindows.dll",           L"platforms"},
    {L"qmodernwindowsstyle.dll", L"styles"},
    {L"qsvgicon.dll",           L"iconengines"},
    {L"qgif.dll",               L"imageformats"},
    {L"qico.dll",               L"imageformats"},
    {L"qjpeg.dll",              L"imageformats"},
    {L"qsvg.dll",               L"imageformats"},
    {L"qtuiotouchplugin.dll",   L"generic"},
    {L"qnetworklistmanager.dll", L"networkinformation"},
    {L"qcertonlybackend.dll",   L"tls"},
    {L"qschannelbackend.dll",   L"tls"},
    {L"logo.ico",               L"imgs"},
    {L"config.json",            L"config"},
    {L"config.example.json",    L"config"},
    {L"pet_state.json",         L"config"},
    {L"CREDITS.md",             L"assets\\costumes"},
    {L"goblin_knight.png",      L"assets\\costumes"},
    {L"goblin_render.png",      L"assets\\costumes"},
    {L"human_hero.png",         L"assets\\costumes"},
    {L"human_player.png",       L"assets\\costumes"},
    {L"human_rpg.png",          L"assets\\costumes"},
    {L"knight.png",             L"assets\\costumes"},
    {L"orc_female.png",         L"assets\\costumes"},
    {L"orc_hero.png",           L"assets\\costumes"},
    {L"orc_warrior.png",        L"assets\\costumes"},
    {L"skeleton.png",           L"assets\\costumes"},
    {L"eating.gif",             L"assets\\character"},
    {L"error.png",              L"assets\\character"},
    {L"happy.png",              L"assets\\character"},
    {L"idle.gif",               L"assets\\character"},
    {L"idle.png",               L"assets\\character"},
    {L"sleeping.png",           L"assets\\character"},
    {L"talking.gif",            L"assets\\character"},
    {L"thinking.gif",           L"assets\\character"},
};

static const wchar_t *subfolderFor(const std::wstring &fileName) {
    for (const auto &entry : kSubfolderMap) {
        if (_wcsicmp(fileName.c_str(), entry.first) == 0)
            return entry.second;
    }
    // qt_*.qm (translations) — nhiều file, xử lý theo tiền tố thay vì liệt kê hết.
    if (fileName.size() > 6 && fileName.compare(0, 3, L"qt_") == 0 &&
        fileName.compare(fileName.size() - 3, 3, L".qm") == 0) {
        return L"translations";
    }
    return nullptr; // ở lại thư mục gốc (exe, DLL runtime chính, v.v.)
}

static bool copyTree(const std::wstring &from, const std::wstring &to) {
    WIN32_FIND_DATAW fd;
    const std::wstring pattern = from + L"\\*";
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;

    bool ok = true;
    do {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L".." ||
            (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        const wchar_t *sub = subfolderFor(name);
        std::wstring destDir = sub ? (to + L"\\" + sub) : to;
        SHCreateDirectoryExW(nullptr, destDir.c_str(), nullptr);

        const std::wstring src  = from + L"\\" + name;
        const std::wstring dest = destDir + L"\\" + name;
        if (!CopyFileW(src.c_str(), dest.c_str(), FALSE))
            ok = false;
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return ok;
}

static void createShortcut(const std::wstring &targetExe, const std::wstring &workDir,
                            const std::wstring &shortcutPath) {
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_IShellLinkW, reinterpret_cast<void **>(&link))))
        return;

    link->SetPath(targetExe.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetIconLocation(targetExe.c_str(), 0);

    IPersistFile *file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file)))) {
        file->Save(shortcutPath.c_str(), TRUE);
        file->Release();
    }
    link->Release();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    CoInitialize(nullptr);

    const std::wstring source = exeDir();

    wchar_t localAppData[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData);
    const std::wstring installDir = std::wstring(localAppData) + L"\\Programs\\TrashPetAI";

    SHCreateDirectoryExW(nullptr, installDir.c_str(), nullptr);
    copyTree(source, installDir);

    // Cài đặt xong: file installer_helper.exe cũng bị copy theo cùng cây thư mục
    // (vì copyTree copy nguyên source) — xóa bản copy này khỏi thư mục cài để
    // không lẫn với TrashPetAI.exe thật.
    DeleteFileW((installDir + L"\\installer_helper.exe").c_str());

    wchar_t desktop[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop);
    const std::wstring shortcutPath = std::wstring(desktop) + L"\\TrashPet AI.lnk";
    const std::wstring targetExe    = installDir + L"\\TrashPetAI.exe";
    createShortcut(targetExe, installDir, shortcutPath);

    ShellExecuteW(nullptr, L"open", targetExe.c_str(), nullptr, installDir.c_str(), SW_SHOWNORMAL);

    CoUninitialize();
    return 0;
}
