//
//  IBusTypes.h
//  OpenKey cho Linux
//
//  Các cấu trúc mà ibus-daemon mong đợi trên đường DBus. Thứ tự trường và kiểu
//  của từng trường lấy từ chính thư viện IBus, ghi trong IBusVariants.md — đoán
//  chỗ này là nguồn lỗi rất khó chẩn đoán vì daemon chỉ im lặng từ chối.
//

#ifndef OPENKEY_LINUX_IBUS_TYPES_H
#define OPENKEY_LINUX_IBUS_TYPES_H

#include <QDBusArgument>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace openkey {

// (sa{sv}ssssssssussssssss)
struct IBusEngineDescStruct {
    QString className = "IBusEngineDesc";
    QVariantMap attachments;
    QString name;
    QString longname;
    QString description;
    QString language;
    QString license;
    QString author;
    QString icon;
    QString layout;
    uint rank = 0;
    QString hotkeys;
    QString symbol;
    QString setup;
    QString layoutVariant;
    QString layoutOption;
    QString version;
    QString textdomain;
    QString iconPropKey;
};

// (sa{sv}ssssssssavav)
struct IBusComponentStruct {
    QString className = "IBusComponent";
    QVariantMap attachments;
    QString name;
    QString description;
    QString version;
    QString license;
    QString author;
    QString homepage;
    QString exec;  // rỗng: ta đăng ký động, không để daemon spawn tiến trình
    QString textdomain;
    QList<QDBusVariant> observedPaths;
    QList<QDBusVariant> engines;
};

// (sa{sv}av) — luôn rỗng vì OpenKey không dùng preedit nên không tô màu chữ nào.
struct IBusAttrListStruct {
    QString className = "IBusAttrList";
    QVariantMap attachments;
    QList<QDBusVariant> attributes;
};

// (sa{sv}sv)
struct IBusTextStruct {
    QString className = "IBusText";
    QVariantMap attachments;
    QString text;
    QDBusVariant attributes;
};

QDBusArgument& operator<<(QDBusArgument& arg, const IBusEngineDescStruct& v);
const QDBusArgument& operator>>(const QDBusArgument& arg, IBusEngineDescStruct& v);
QDBusArgument& operator<<(QDBusArgument& arg, const IBusComponentStruct& v);
const QDBusArgument& operator>>(const QDBusArgument& arg, IBusComponentStruct& v);
QDBusArgument& operator<<(QDBusArgument& arg, const IBusAttrListStruct& v);
const QDBusArgument& operator>>(const QDBusArgument& arg, IBusAttrListStruct& v);
QDBusArgument& operator<<(QDBusArgument& arg, const IBusTextStruct& v);
const QDBusArgument& operator>>(const QDBusArgument& arg, IBusTextStruct& v);

// Gọi một lần trước khi gửi bất cứ thứ gì qua DBus.
void registerIBusTypes();

// Dựng sẵn một IBusText rỗng thuộc tính từ chuỗi chữ.
IBusTextStruct makeIBusText(const QString& text);

} // namespace openkey

Q_DECLARE_METATYPE(openkey::IBusEngineDescStruct)
Q_DECLARE_METATYPE(openkey::IBusComponentStruct)
Q_DECLARE_METATYPE(openkey::IBusAttrListStruct)
Q_DECLARE_METATYPE(openkey::IBusTextStruct)

#endif // OPENKEY_LINUX_IBUS_TYPES_H
