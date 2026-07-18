#ifndef RECYCLEBINMANAGER_H
#define RECYCLEBINMANAGER_H

#include <QString>
#include <QDateTime>
#include <QVector>

class RecycleBinManager {
public:
    struct Item {
        QString  displayName;  // Tên gốc trước khi xóa (vd: "photo.jpg")
        quint64  sizeBytes;    // Dung lượng file
        QDateTime deletedAt;   // Thời điểm xóa vào thùng rác
        QString  parsingName;  // Đường dẫn thật trong $Recycle.Bin (để xóa)
    };

    // Liệt kê tất cả item trong Recycle Bin
    // Trả về list rỗng nếu thùng rác trống hoặc có lỗi
    QVector<Item> listItems();

    // Xóa vĩnh viễn 1 item ra khỏi Recycle Bin
    bool deleteItem(const Item &item);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // RECYCLEBINMANAGER_H
