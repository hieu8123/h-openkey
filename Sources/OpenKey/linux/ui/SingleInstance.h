//
//  SingleInstance.h
//  OpenKey cho Linux
//
//  Chi cho phep mot ban dang chay. Chay nhieu ban cung luc la loi nang: moi ban
//  deu bat phim va deu gui chu ra, nen chu bi nhan doi hoac nhan ba.
//
//  Ban thu hai khong bao loi ma gui yeu cau "mo bang dieu khien" cho ban dang
//  chay roi tu thoat. Nho vay bam vao bieu tuong ung dung trong menu la mo duoc
//  bang dieu khien — khong phai phu thuoc vao menu chuot phai o khay he thong,
//  thu ma khong phai moi desktop deu ho tro.
//

#ifndef OPENKEY_LINUX_SINGLEINSTANCE_H
#define OPENKEY_LINUX_SINGLEINSTANCE_H

#include <QLocalServer>
#include <QObject>
#include <QString>

namespace openkey {

class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject* parent = nullptr);

    // Tra ve true neu da co ban khac dang chay; khi do da gui yeu cau mo bang
    // dieu khien cho ban do va ban nay nen thoat ngay.
    bool notifyRunningInstance();

    // Gianh quyen lam ban duy nhat. Tra ve false neu khong mo duoc socket, khi
    // do van chay tiep nhung khong chan duoc ban thu hai.
    bool claim();

signals:
    void showControlPanelRequested();

private:
    static QString socketName();

    QLocalServer _server;
};

} // namespace openkey

#endif // OPENKEY_LINUX_SINGLEINSTANCE_H
