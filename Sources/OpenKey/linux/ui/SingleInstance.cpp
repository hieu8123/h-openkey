//
//  SingleInstance.cpp
//  OpenKey cho Linux
//

#include "SingleInstance.h"

#include <QDir>
#include <QLocalSocket>

#include <unistd.h>

namespace openkey {
namespace {

constexpr char kShowPanelMessage[] = "show-control-panel";

} // namespace

SingleInstance::SingleInstance(QObject* parent) : QObject(parent) {
    connect(&_server, &QLocalServer::newConnection, this, [this] {
        QLocalSocket* client = _server.nextPendingConnection();
        if (!client) {
            return;
        }
        connect(client, &QLocalSocket::readyRead, this, [this, client] {
            const QByteArray message = client->readAll();
            if (message.startsWith(kShowPanelMessage)) {
                emit showControlPanelRequested();
            }
            client->disconnectFromServer();
        });
        connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });
}

QString SingleInstance::socketName() {
    // Rieng cho tung nguoi dung: nhieu nguoi dung dang nhap cung luc thi moi
    // nguoi co mot ban OpenKey rieng, khong duoc chan nhau.
    return QString("h-openkey-%1").arg(getuid());
}

bool SingleInstance::notifyRunningInstance() {
    QLocalSocket socket;
    socket.connectToServer(socketName());
    if (!socket.waitForConnected(300)) {
        return false;
    }
    socket.write(kShowPanelMessage);
    socket.flush();
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::claim() {
    if (_server.listen(socketName())) {
        return true;
    }

    // Socket cu con sot lai sau mot lan tat khong sach. Khong co ban nao tra
    // loi (da kiem o notifyRunningInstance) nen xoa di roi thu lai.
    QLocalServer::removeServer(socketName());
    return _server.listen(socketName());
}

} // namespace openkey
