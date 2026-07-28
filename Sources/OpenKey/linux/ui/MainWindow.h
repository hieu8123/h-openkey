//
//  MainWindow.h
//  OpenKey cho Linux
//
//  Bang dieu khien. Bo cuc theo ban EVKey: nhom "Dieu khien" o tren, ba tab
//  ben duoi, checkbox xep hai cot.
//
//  Moi tuy chon deu buoc thang vao mot bien v* cua engine. De them mot tuy
//  chon moi, khai bao them mot dong trong bang o MainWindow.cpp thay vi viet
//  tay tung ket noi.
//

#ifndef OPENKEY_LINUX_MAINWINDOW_H
#define OPENKEY_LINUX_MAINWINDOW_H

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QRadioButton;

namespace openkey {

class Config;
class OpenKeyCore;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    MainWindow(Config& config, OpenKeyCore& core, QWidget* parent = nullptr);

    // Nap lai gia tri tu cac bien engine len giao dien.
    void refreshFromState();

signals:
    void settingsChanged();

private:
    // Mot o danh dau buoc vao mot bien int cua engine.
    struct BoundCheck {
        QCheckBox* box = nullptr;
        int* value = nullptr;
    };

    QWidget* buildControlGroup();
    QWidget* buildBasicTab();
    QWidget* buildHotkeyTab();
    void readHotkeyFromUi();
    void writeHotkeyToUi();
    QWidget* buildSystemTab();

    QCheckBox* addCheck(QGridLayout* grid, int row, int column, const QString& text,
                        int* value, const QString& tip = {});

    void applyToEngine();
    void restoreDefaults();
    void save();

    Config& _config;
    OpenKeyCore& _core;

    QComboBox* _codeTable = nullptr;
    QComboBox* _inputType = nullptr;
    QRadioButton* _backendAuto = nullptr;
    QRadioButton* _backendWayland = nullptr;
    QRadioButton* _backendX11 = nullptr;

    QCheckBox* _hotkeyCtrl = nullptr;
    QCheckBox* _hotkeyAlt = nullptr;
    QCheckBox* _hotkeyShift = nullptr;
    QCheckBox* _hotkeySuper = nullptr;
    QComboBox* _hotkeyKey = nullptr;
    QLabel* _hotkeyPreview = nullptr;

    std::vector<BoundCheck> _checks;
    bool _loading = false;
};

} // namespace openkey

#endif // OPENKEY_LINUX_MAINWINDOW_H
