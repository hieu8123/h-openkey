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
        while (_server.hasPendingConnections()) {
            QLocalSocket* client = _server.nextPendingConnection();
            if (!client) continue;

            auto processMessage = [this, client] {
                const QByteArray message = client->readAll();
                if (message.startsWith(kShowPanelMessage)) {
                    emit showControlPanelRequested();
                    client->write("ok");
                    client->flush();
                }
                client->disconnectFromServer();
            };
            connect(client, &QLocalSocket::readyRead, this, processMessage);
            connect(client, &QLocalSocket::disconnected, client,
                    &QLocalSocket::deleteLater);

            // Client có thể đã gửi xong trước khi event loop xử lý newConnection;
            // khi đó tín hiệu readyRead đã trôi qua và phải đọc buffer ngay.
            if (client->bytesAvailable() > 0) processMessage();
        }
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
    // Giữ kết nối tới khi tiến trình chính xác nhận đã nhận yêu cầu. Nếu đóng
    // ngay sau khi ghi, dữ liệu có thể bị mất trước khi server gắn readyRead.
    socket.waitForReadyRead(1000);
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
