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
#include <QString>
#include <QSystemTrayIcon>

#include <functional>

class QAction;
class QActionGroup;
class QMenu;

namespace openkey {

class Config;
class OpenKeyCore;
class UpdateChecker;

class TrayIcon : public QObject {
    Q_OBJECT

public:
    TrayIcon(Config& config, OpenKeyCore& core, UpdateChecker& updates,
             QObject* parent = nullptr);

    void show();
    void refresh();
    void showWarning(const QString& message);
    void setRuntimeWarning(const QString& message);

signals:
    void controlPanelRequested();

private:
    void rebuildIcon();
    void onSettingChanged();

    Config& _config;
    OpenKeyCore& _core;
    UpdateChecker& _updates;
    QSystemTrayIcon _tray;
    QMenu* _menu = nullptr;
    QAction* _languageAction = nullptr;
    QAction* _updateAction = nullptr;
    QActionGroup* _inputTypeGroup = nullptr;
    QActionGroup* _codeTableGroup = nullptr;
    QString _runtimeWarning;
    bool _updateNotificationPending = false;
};

} // namespace openkey

#endif // OPENKEY_LINUX_TRAYICON_H
