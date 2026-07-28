//
//  TrayIcon.h
//  OpenKey cho Linux
//
//  Bang dieu khien day du thuoc giai doan 2. O giai doan nay chi can du de
//  dung hang ngay: doi ngon ngu, kieu go, bang ma va thoat.
//

#ifndef OPENKEY_LINUX_TRAYICON_H
#define OPENKEY_LINUX_TRAYICON_H

#include <QObject>
#include <QSystemTrayIcon>

#include <functional>

class QAction;
class QActionGroup;
class QMenu;

namespace openkey {

class Config;
class OpenKeyCore;

class TrayIcon : public QObject {
    Q_OBJECT

public:
    TrayIcon(Config& config, OpenKeyCore& core, QObject* parent = nullptr);

    void show();
    void refresh();

signals:
    void controlPanelRequested();

private:
    void rebuildIcon();
    void onSettingChanged();

    Config& _config;
    OpenKeyCore& _core;
    QSystemTrayIcon _tray;
    QMenu* _menu = nullptr;
    QAction* _languageAction = nullptr;
    QActionGroup* _inputTypeGroup = nullptr;
    QActionGroup* _codeTableGroup = nullptr;
};

} // namespace openkey

#endif // OPENKEY_LINUX_TRAYICON_H
