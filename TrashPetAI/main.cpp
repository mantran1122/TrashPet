#include "characterwidget.h"
#include "configmanager.h"
#include "petstate.h"
#include "setupdialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QStandardPaths>
#include <QIcon>
#include <windows.h>

int main(int argc, char *argv[])
{
    // COM cần init trước QApplication (STA thread model cho Shell API)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    QCoreApplication::setOrganizationName("TrashPetAI");
    QCoreApplication::setApplicationName("TrashPetAI");
    QApplication a(argc, argv);

    // Icon app (taskbar/Alt-Tab). File .ico đã nhúng sẵn vào exe qua resource.rc,
    // đây là fallback khi chạy debug (exe chưa nhúng resource nếu build kiểu khác).
    const QStringList iconPaths = {
        QCoreApplication::applicationDirPath() + "/imgs/logo.ico",
        QCoreApplication::applicationDirPath() + "/../imgs/logo.ico",
        QCoreApplication::applicationDirPath() + "/../../imgs/logo.ico",
    };
    for (const QString &p : iconPaths) {
        if (QFile::exists(p)) { a.setWindowIcon(QIcon(p)); break; }
    }

    // Config lưu ở thư mục ghi được của user (%APPDATA%/TrashPetAI/...), không phải cạnh exe.
    // Cần thiết vì app cài ở Program Files thì thư mục cài đặt không ghi được nếu không chạy admin.
    const QString userDataDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(userDataDir);
    const QString configPath = userDataDir + "/config.json";

    if (!QFile::exists(configPath)) {
        ConfigManager::writeDefaultConfig(configPath);
    }

    ConfigManager::instance().load(configPath);  // có thể fail nếu chưa có key — kiểm tra riêng bên dưới

    if (!ConfigManager::instance().hasValidApiKeyForCurrentProvider()) {
        SetupDialog dlg;
        if (dlg.exec() != QDialog::Accepted) {
            CoUninitialize();
            return 0;
        }
        ConfigManager::instance().saveProviderApiKey(configPath, dlg.selectedProvider(), dlg.apiKey());
    }

    if (!ConfigManager::instance().hasValidApiKeyForCurrentProvider()) {
        QMessageBox::critical(nullptr, "Lỗi cấu hình",
                              "Chưa có API key hợp lệ, TrashPet không thể khởi động.\n\n"
                                  + ConfigManager::instance().lastError());
        CoUninitialize();
        return 1;
    }

    // Load pet state (cùng thư mục với config.json)
    const QString petStatePath = userDataDir + "/pet_state.json";
    PetState::instance().load(petStatePath);

    CharacterWidget w;
    w.show();
    const int ret = a.exec();

    // Lưu pet state khi thoát
    PetState::instance().save();

    CoUninitialize();
    return ret;
}
