//
//  TrayIcon.cpp
//  OpenKey cho Linux
//

#include "TrayIcon.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

#include "Config.h"
#include "Engine.h"
#include "OpenKeyCore.h"

namespace openkey {
namespace {

struct Choice {
    const char* label;
    int value;
};

const Choice kInputTypes[] = {
    {"Telex", vTelex},
    {"VNI", vVNI},
    {"Simple Telex 1", vSimpleTelex1},
    {"Simple Telex 2", vSimpleTelex2},
};

const Choice kCodeTables[] = {
    {"Unicode dựng sẵn", 0},
    {"TCVN3 (ABC)", 1},
    {"VNI Windows", 2},
    {"Unicode tổ hợp", 3},
    {"Vietnamese Locale CP 1258", 4},
};

QIcon makeIcon(bool vietnamese) {
    // Ve chu thay vi dung file anh: bieu tuong noi luon trang thai dang go, va
    // khong phai keo theo tai nguyen nao.
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(vietnamese ? QColor(0xC0, 0x39, 0x2B) : QColor(0x7F, 0x8C, 0x8D));
    painter.drawRoundedRect(pixmap.rect().adjusted(2, 2, -2, -2), 14, 14);

    QFont font = QApplication::font();
    font.setPixelSize(38);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, vietnamese ? "V" : "E");
    painter.end();

    return QIcon(pixmap);
}

} // namespace

TrayIcon::TrayIcon(Config& config, OpenKeyCore& core, QObject* parent)
    : QObject(parent), _config(config), _core(core) {
    _menu = new QMenu();

    _languageAction = _menu->addAction(tr("Gõ tiếng Việt"));
    _languageAction->setCheckable(true);
    _languageAction->setChecked(vLanguage == 1);
    connect(_languageAction, &QAction::triggered, this, [this] {
        _core.toggleLanguage();
        onSettingChanged();
    });

    connect(_menu->addAction(tr("Bảng điều khiển…")), &QAction::triggered, this,
            [this] { emit controlPanelRequested(); });

    _menu->addSeparator();

    QMenu* inputMenu = _menu->addMenu(tr("Kiểu gõ"));
    _inputTypeGroup = new QActionGroup(this);
    for (const auto& choice : kInputTypes) {
        QAction* action = inputMenu->addAction(choice.label);
        action->setData(choice.value);
        action->setCheckable(true);
        action->setChecked(vInputType == choice.value);
        _inputTypeGroup->addAction(action);
        const int value = choice.value;
        connect(action, &QAction::triggered, this, [this, value] {
            vInputType = value;
            _core.resetTypingState();
            onSettingChanged();
        });
    }

    QMenu* codeMenu = _menu->addMenu(tr("Bảng mã"));
    _codeTableGroup = new QActionGroup(this);
    for (const auto& choice : kCodeTables) {
        QAction* action = codeMenu->addAction(choice.label);
        action->setData(choice.value);
        action->setCheckable(true);
        action->setChecked(vCodeTable == choice.value);
        _codeTableGroup->addAction(action);
        const int value = choice.value;
        connect(action, &QAction::triggered, this, [this, value] {
            vCodeTable = value;
            onTableCodeChange();
            _core.resetTypingState();
            _core.rememberCurrentApp();
            onSettingChanged();
        });
    }

    _menu->addSeparator();
    connect(_menu->addAction(tr("Thoát")), &QAction::triggered, qApp, &QApplication::quit);

    _tray.setContextMenu(_menu);
    _tray.setToolTip(tr("OpenKey"));
    connect(&_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    _core.toggleLanguage();
                    onSettingChanged();
                } else if (reason == QSystemTrayIcon::DoubleClick) {
                    emit controlPanelRequested();
                }
            });
    rebuildIcon();
}

void TrayIcon::rebuildIcon() {
    _tray.setIcon(makeIcon(vLanguage == 1));
    if (_languageAction) {
        _languageAction->setChecked(vLanguage == 1);
    }
}

void TrayIcon::onSettingChanged() {
    rebuildIcon();
    _config.save();
}

void TrayIcon::refresh() {
    rebuildIcon();
    for (QActionGroup* group : {_inputTypeGroup, _codeTableGroup}) {
        if (!group) continue;
        const int current = group == _inputTypeGroup ? vInputType : vCodeTable;
        for (QAction* action : group->actions()) {
            action->setChecked(action->data().toInt() == current);
        }
    }
}

void TrayIcon::show() { _tray.show(); }

} // namespace openkey
