//
//  IBusTypes.cpp
//  OpenKey cho Linux
//

#include "IBusTypes.h"

#include <QDBusMetaType>

namespace openkey {

QDBusArgument& operator<<(QDBusArgument& arg, const IBusEngineDescStruct& v) {
    arg.beginStructure();
    arg << v.className << v.attachments << v.name << v.longname << v.description
        << v.language << v.license << v.author << v.icon << v.layout << v.rank
        << v.hotkeys << v.symbol << v.setup << v.layoutVariant << v.layoutOption
        << v.version << v.textdomain << v.iconPropKey;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, IBusEngineDescStruct& v) {
    arg.beginStructure();
    arg >> v.className >> v.attachments >> v.name >> v.longname >> v.description >>
        v.language >> v.license >> v.author >> v.icon >> v.layout >> v.rank >>
        v.hotkeys >> v.symbol >> v.setup >> v.layoutVariant >> v.layoutOption >>
        v.version >> v.textdomain >> v.iconPropKey;
    arg.endStructure();
    return arg;
}

QDBusArgument& operator<<(QDBusArgument& arg, const IBusComponentStruct& v) {
    arg.beginStructure();
    arg << v.className << v.attachments << v.name << v.description << v.version
        << v.license << v.author << v.homepage << v.exec << v.textdomain
        << v.observedPaths << v.engines;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, IBusComponentStruct& v) {
    arg.beginStructure();
    arg >> v.className >> v.attachments >> v.name >> v.description >> v.version >>
        v.license >> v.author >> v.homepage >> v.exec >> v.textdomain >>
        v.observedPaths >> v.engines;
    arg.endStructure();
    return arg;
}

QDBusArgument& operator<<(QDBusArgument& arg, const IBusAttrListStruct& v) {
    arg.beginStructure();
    arg << v.className << v.attachments << v.attributes;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, IBusAttrListStruct& v) {
    arg.beginStructure();
    arg >> v.className >> v.attachments >> v.attributes;
    arg.endStructure();
    return arg;
}

QDBusArgument& operator<<(QDBusArgument& arg, const IBusTextStruct& v) {
    arg.beginStructure();
    arg << v.className << v.attachments << v.text << v.attributes;
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>>(const QDBusArgument& arg, IBusTextStruct& v) {
    arg.beginStructure();
    arg >> v.className >> v.attachments >> v.text >> v.attributes;
    arg.endStructure();
    return arg;
}

void registerIBusTypes() {
    qDBusRegisterMetaType<IBusEngineDescStruct>();
    qDBusRegisterMetaType<IBusComponentStruct>();
    qDBusRegisterMetaType<IBusAttrListStruct>();
    qDBusRegisterMetaType<IBusTextStruct>();
    qDBusRegisterMetaType<QList<QDBusVariant>>();
}

IBusTextStruct makeIBusText(const QString& text) {
    IBusTextStruct out;
    out.text = text;
    out.attributes = QDBusVariant(QVariant::fromValue(IBusAttrListStruct{}));
    return out;
}

} // namespace openkey
