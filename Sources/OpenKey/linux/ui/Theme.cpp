//
//  Theme.cpp
//  OpenKey cho Linux
//

#include "Theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QStyleFactory>

namespace openkey {
namespace {

struct Tokens {
    QColor surface;      // nen cua so
    QColor raised;       // nen o nhap, hop chon, o danh dau
    QColor border;
    QColor text;
    QColor muted;        // chu phu, goi y
    QColor accent;       // do son mai
    QColor accentText;
};

Tokens darkTokens() {
    return {
        QColor("#1E2128"), QColor("#2A2F39"), QColor("#3E4552"),
        QColor("#E6E8EC"), QColor("#9AA1AE"), QColor("#E05A44"),
        QColor("#14161B"),
    };
}

Tokens lightTokens() {
    return {
        QColor("#F6F6F4"), QColor("#FFFFFF"), QColor("#D2D5DA"),
        QColor("#1B1D22"), QColor("#5E646F"), QColor("#C0392B"),
        QColor("#FFFFFF"),
    };
}

QPalette buildPalette(const Tokens& t) {
    QPalette p;
    p.setColor(QPalette::Window, t.surface);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.raised);
    p.setColor(QPalette::AlternateBase, t.surface);
    p.setColor(QPalette::Text, t.text);
    p.setColor(QPalette::Button, t.raised);
    p.setColor(QPalette::ButtonText, t.text);
    p.setColor(QPalette::ToolTipBase, t.raised);
    p.setColor(QPalette::ToolTipText, t.text);
    p.setColor(QPalette::PlaceholderText, t.muted);
    p.setColor(QPalette::Highlight, t.accent);
    p.setColor(QPalette::HighlightedText, t.accentText);
    p.setColor(QPalette::Link, t.accent);

    // Chu bi vo hieu hoa phai nhat RO RANG so voi chu thuong, neu khong nguoi
    // dung khong biet muc nao bam duoc — dung loi cua palette mac dinh.
    p.setColor(QPalette::Disabled, QPalette::Text, t.muted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, t.muted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.muted);
    return p;
}

QString buildStyleSheet(const Tokens& t) {
    const QString surface = t.surface.name();
    const QString raised = t.raised.name();
    const QString border = t.border.name();
    const QString text = t.text.name();
    const QString muted = t.muted.name();
    const QString accent = t.accent.name();
    const QString accentText = t.accentText.name();

    return QString(R"(
QWidget { color: %TEXT%; }

QGroupBox {
    border: 1px solid %BORDER%;
    border-radius: 8px;
    margin-top: 14px;
    padding: 14px 12px 12px 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 6px;
    color: %MUTED%;
    font-weight: 600;
}

QComboBox {
    background: %RAISED%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 6px 10px;
    min-height: 20px;
    color: %TEXT%;
}
QComboBox:hover { border-color: %ACCENT%; }
QComboBox:focus { border-color: %ACCENT%; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: %RAISED%;
    border: 1px solid %BORDER%;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
    outline: none;
}

QLineEdit, QPlainTextEdit, QTableWidget {
    background: %RAISED%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 5px 8px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
}
QLineEdit:focus, QPlainTextEdit:focus { border-color: %ACCENT%; }

QPushButton {
    background: %RAISED%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 7px 16px;
    color: %TEXT%;
}
QPushButton:hover { border-color: %ACCENT%; }
QPushButton:pressed { background: %SURFACE%; }
QPushButton:default {
    background: %ACCENT%;
    border-color: %ACCENT%;
    color: %ACCENTTEXT%;
    font-weight: 600;
}
QPushButton:disabled { border-color: %BORDER%; color: %MUTED%; }

/* Khong ghi de phan ve chi bao: QSS khong ve duoc hinh tron lan dau tich tu te,
   nen radio ra hinh vuong giong checkbox va checkbox thi mat dau tich. Fusion tu
   ve dung ca hai, va no lay mau tu palette nen van theo tong mau nhan. */
QCheckBox, QRadioButton { spacing: 9px; padding: 3px 0; }
QCheckBox:hover, QRadioButton:hover { color: %ACCENT%; }

QTabWidget::pane {
    border: 1px solid %BORDER%;
    border-radius: 8px;
    top: -1px;
}
QTabBar::tab {
    background: transparent;
    border: 1px solid transparent;
    border-bottom: 2px solid transparent;
    padding: 8px 18px;
    color: %MUTED%;
}
QTabBar::tab:hover { color: %TEXT%; }
QTabBar::tab:selected {
    color: %TEXT%;
    border-bottom: 2px solid %ACCENT%;
    font-weight: 600;
}

QHeaderView::section {
    background: %SURFACE%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    padding: 7px 8px;
    color: %MUTED%;
    font-weight: 600;
}
QTableWidget { gridline-color: %BORDER%; }

QToolTip {
    background: %RAISED%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    padding: 5px 8px;
}
)")
        .replace("%SURFACE%", surface)
        .replace("%RAISED%", raised)
        .replace("%BORDER%", border)
        .replace("%TEXT%", text)
        .replace("%MUTED%", muted)
        .replace("%ACCENTTEXT%", accentText)
        .replace("%ACCENT%", accent);
}

} // namespace

void applyTheme(QApplication& app) {
    // Fusion la style duy nhat co san o moi noi va nhan palette day du, nen bang
    // dieu khien trong nhu nhau tren COSMIC, GNOME, KDE.
    if (QStyle* fusion = QStyleFactory::create("Fusion")) {
        app.setStyle(fusion);
    }

    const bool dark = app.palette().color(QPalette::Window).lightness() < 128;
    const Tokens tokens = dark ? darkTokens() : lightTokens();

    app.setPalette(buildPalette(tokens));
    app.setStyleSheet(buildStyleSheet(tokens));
}

} // namespace openkey
