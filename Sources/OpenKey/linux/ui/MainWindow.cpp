//
//  MainWindow.cpp
//  OpenKey cho Linux
//

#include "MainWindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "MacroDialog.h"

#include "AppState.h"
#include "Config.h"
#include "DebugLog.h"
#include "Engine.h"
#include "OpenKeyCore.h"

namespace openkey {
namespace {

struct Choice {
    const char* label;
    int value;
};

const Choice kCodeTables[] = {
    {"Unicode dựng sẵn", 0},
    {"TCVN3 (ABC)", 1},
    {"VNI Windows", 2},
    {"Unicode tổ hợp", 3},
    {"Vietnamese Locale CP 1258", 4},
};

const Choice kInputTypes[] = {
    {"Telex", vTelex},
    {"VNI", vVNI},
    {"Simple Telex 1", vSimpleTelex1},
    {"Simple Telex 2", vSimpleTelex2},
};

void fill(QComboBox* box, const Choice* choices, size_t count) {
    for (size_t i = 0; i < count; i++) {
        box->addItem(QString::fromUtf8(choices[i].label), choices[i].value);
    }
}

// Bang chu cai theo keycode X11, dung chung voi platforms/linux.h.
int keycodeForLetter(char letter) {
    static const int table[26] = {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
        KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
        KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
    if (letter < 'A' || letter > 'Z') return -1;
    return table[letter - 'A'];
}

void selectValue(QComboBox* box, int value) {
    const int index = box->findData(value);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

MainWindow::MainWindow(Config& config, OpenKeyCore& core, QWidget* parent)
    : QWidget(parent), _config(config), _core(core) {
    setWindowTitle(tr("H-OpenKey — bộ gõ tiếng Việt"));

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildBasicTab(), tr("Cơ bản"));
    tabs->addTab(buildHotkeyTab(), tr("Phím tắt"));
    tabs->addTab(buildSystemTab(), tr("Hệ thống"));

    auto* defaults = new QPushButton(tr("Thiết lập mặc định"));
    auto* saveButton = new QPushButton(tr("Lưu"));
    saveButton->setDefault(true);
    connect(defaults, &QPushButton::clicked, this, &MainWindow::restoreDefaults);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::save);

    // Phai co duong thoat ngay trong cua so: menu chuot phai o khay he thong
    // khong phai desktop nao cung ho tro, va nguoi dung can thoat duoc.
    auto* quitButton = new QPushButton(tr("Thoát H-OpenKey"));
    connect(quitButton, &QPushButton::clicked, this, &MainWindow::quitApplication);

    auto* closeButton = new QPushButton(tr("Đóng"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(defaults);
    bottom->addWidget(quitButton);
    bottom->addStretch(1);
    bottom->addWidget(closeButton);
    bottom->addWidget(saveButton);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);
    root->addWidget(buildControlGroup());
    root->addWidget(tabs, 1);
    root->addLayout(bottom);

    refreshFromState();
}

QWidget* MainWindow::buildControlGroup() {
    auto* group = new QGroupBox(tr("Điều khiển"), this);
    auto* grid = new QGridLayout(group);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);

    _codeTable = new QComboBox(group);
    fill(_codeTable, kCodeTables, std::size(kCodeTables));
    grid->addWidget(new QLabel(tr("Bảng mã")), 0, 0);
    grid->addWidget(_codeTable, 0, 1);

    _inputType = new QComboBox(group);
    fill(_inputType, kInputTypes, std::size(kInputTypes));
    grid->addWidget(new QLabel(tr("Kiểu gõ")), 1, 0);
    grid->addWidget(_inputType, 1, 1);

    // "Co che" cua EVKey chinh la viec chon backend o day. Auto tu dò phien
    // lam viec; hai lua chon con lai de ep khi can chan doan.
    _backendAuto = new QRadioButton(tr("Tự động"), group);
    _backendWayland = new QRadioButton(tr("Wayland"), group);
    _backendX11 = new QRadioButton(tr("X11"), group);
    _backendX11->setToolTip(
        tr("Dùng cho phiên đăng nhập X11. Trên X11 không chặn được phím nên "
           "chữ có thể nhấp nháy nhẹ."));

    auto* backendRow = new QHBoxLayout;
    backendRow->addWidget(_backendAuto);
    backendRow->addWidget(_backendWayland);
    backendRow->addWidget(_backendX11);
    backendRow->addStretch(1);
    grid->addWidget(new QLabel(tr("Cơ chế")), 2, 0);
    grid->addLayout(backendRow, 2, 1);

    grid->setColumnStretch(1, 1);

    connect(_codeTable, &QComboBox::currentIndexChanged, this, [this] {
        if (_loading) return;
        vCodeTable = _codeTable->currentData().toInt();
        onTableCodeChange();
        _core.resetTypingState();
        _core.rememberCurrentApp();
        emit settingsChanged();
    });
    connect(_inputType, &QComboBox::currentIndexChanged, this, [this] {
        if (_loading) return;
        vInputType = _inputType->currentData().toInt();
        _core.resetTypingState();
        emit settingsChanged();
    });

    auto* backendGroup = new QButtonGroup(this);
    backendGroup->addButton(_backendAuto);
    backendGroup->addButton(_backendWayland);
    backendGroup->addButton(_backendX11);
    connect(backendGroup, &QButtonGroup::buttonToggled, this, [this] {
        if (_loading) return;
        _config.backend = _backendWayland->isChecked() ? BackendKind::Wayland
                        : _backendX11->isChecked()     ? BackendKind::X11
                                                       : BackendKind::Auto;
        emit settingsChanged();
    });

    return group;
}

QCheckBox* MainWindow::addCheck(QGridLayout* grid, int row, int column,
                                const QString& text, int* value, const QString& tip) {
    auto* box = new QCheckBox(text);
    if (!tip.isEmpty()) {
        box->setToolTip(tip);
    }
    grid->addWidget(box, row, column);
    _checks.push_back({box, value});

    connect(box, &QCheckBox::toggled, this, [this, value](bool on) {
        if (_loading) return;
        *value = on ? 1 : 0;
        applyToEngine();
        emit settingsChanged();
    });
    return box;
}

QWidget* MainWindow::buildBasicTab() {
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    auto* grid = new QGridLayout;

    int row = 0;
    addCheck(grid, row, 0, tr("Đặt dấu kiểu mới (oà, uý)"), &vUseModernOrthography,
             tr("Đặt dấu oà, uý thay vì òa, úy"));
    addCheck(grid, row++, 1, tr("Bật kiểm tra chính tả"), &vCheckSpelling);

    addCheck(grid, row, 0, tr("Đặt dấu tự do"), &vFreeMark,
             tr("Cho phép đặt dấu ở bất kỳ đâu trong từ"));
    addCheck(grid, row++, 1, tr("Khôi phục phím với từ sai"), &vRestoreIfWrongSpelling,
             tr("Gõ sai chính tả thì trả lại nguyên các phím đã bấm"));

    addCheck(grid, row, 0, tr("Cho phép 'f j w z' làm phụ âm"), &vAllowConsonantZFWJ);
    addCheck(grid, row++, 1, tr("Gõ nhanh Telex (cc=ch, gg=gi…)"), &vQuickTelex);

    addCheck(grid, row, 0, tr("Gõ tắt phụ âm đầu: f→ph, j→gi, w→qu"),
             &vQuickStartConsonant);
    addCheck(grid, row++, 1, tr("Gõ tắt phụ âm cuối: g→ng, h→nh, k→ch"),
             &vQuickEndConsonant);

    addCheck(grid, row, 0, tr("Tự động viết hoa đầu câu"), &vUpperCaseFirstChar);
    addCheck(grid, row++, 1, tr("Sửa lỗi gợi ý của trình duyệt"), &vFixRecommendBrowser);

    addCheck(grid, row, 0, tr("Tạm tắt kiểm tra chính tả bằng Ctrl"), &vTempOffSpelling);
    addCheck(grid, row++, 1, tr("Tạm tắt bộ gõ bằng Alt"), &vTempOffOpenKey);

    outer->addLayout(grid);

    auto* macroGroup = new QGroupBox(tr("Gõ tắt"), page);
    auto* macroGrid = new QGridLayout(macroGroup);

    addCheck(macroGrid, 0, 0, tr("Cho phép gõ tắt"), &vUseMacro);
    addCheck(macroGrid, 1, 0, tr("Vẫn gõ tắt khi ở chế độ tiếng Anh"),
             &vUseMacroInEnglishMode);
    addCheck(macroGrid, 2, 0, tr("Tự viết hoa theo cách gõ (btw → By the way)"),
             &vAutoCapsMacro);

    auto* macroButton = new QPushButton(tr("Mở bảng gõ tắt"), macroGroup);
    connect(macroButton, &QPushButton::clicked, this, [this] {
        MacroDialog dialog(_config, this);
        dialog.exec();
    });
    macroGrid->addWidget(macroButton, 0, 1, 3, 1, Qt::AlignRight | Qt::AlignVCenter);
    macroGrid->setColumnStretch(0, 1);

    outer->addWidget(macroGroup);
    outer->addStretch(1);
    return page;
}

QWidget* MainWindow::buildHotkeyTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* group = new QGroupBox(tr("Phím chuyển tiếng Việt / tiếng Anh"), page);
    auto* grid = new QGridLayout(group);

    _hotkeyCtrl = new QCheckBox(tr("Ctrl"), group);
    _hotkeyAlt = new QCheckBox(tr("Alt"), group);
    _hotkeyShift = new QCheckBox(tr("Shift"), group);
    _hotkeySuper = new QCheckBox(tr("Super"), group);
    grid->addWidget(_hotkeyCtrl, 0, 0);
    grid->addWidget(_hotkeyAlt, 0, 1);
    grid->addWidget(_hotkeyShift, 0, 2);
    grid->addWidget(_hotkeySuper, 0, 3);

    _hotkeyKey = new QComboBox(group);
    _hotkeyKey->addItem(tr("(chỉ phím bổ trợ)"), kSwitchKeyModifiersOnly);
    for (char c = 'A'; c <= 'Z'; c++) {
        const int keycode = keycodeForLetter(c);
        if (keycode > 0) {
            _hotkeyKey->addItem(QString(QChar(c)), keycode);
        }
    }
    grid->addWidget(new QLabel(tr("Phím")), 1, 0);
    grid->addWidget(_hotkeyKey, 1, 1, 1, 3);

    _hotkeyPreview = new QLabel(group);
    grid->addWidget(_hotkeyPreview, 2, 0, 1, 4);

    auto onChanged = [this] {
        if (_loading) return;
        readHotkeyFromUi();
        writeHotkeyToUi();
        emit settingsChanged();
    };
    for (QCheckBox* box : {_hotkeyCtrl, _hotkeyAlt, _hotkeyShift, _hotkeySuper}) {
        connect(box, &QCheckBox::toggled, this, onChanged);
    }
    connect(_hotkeyKey, &QComboBox::currentIndexChanged, this, onChanged);

    layout->addWidget(group);

    auto* note = new QLabel(
        tr("Chọn \"(chỉ phím bổ trợ)\" nếu muốn dùng kiểu Ctrl+Shift: khi đó chế độ "
           "đổi lúc bạn nhả tổ hợp ra, và sẽ không đổi nếu có phím khác được bấm xen "
           "vào giữa."),
        page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    return page;
}

void MainWindow::readHotkeyFromUi() {
    int status = _hotkeyKey->currentData().toInt() & 0xFF;
    if (_hotkeyCtrl->isChecked()) status |= 0x100;
    if (_hotkeyAlt->isChecked()) status |= 0x200;
    if (_hotkeySuper->isChecked()) status |= 0x400;
    if (_hotkeyShift->isChecked()) status |= 0x800;
    vSwitchKeyStatus = status;
}

void MainWindow::writeHotkeyToUi() {
    QStringList parts;
    if (HAS_CONTROL(vSwitchKeyStatus)) parts << "Ctrl";
    if (HAS_OPTION(vSwitchKeyStatus)) parts << "Alt";
    if (HAS_COMMAND(vSwitchKeyStatus)) parts << "Super";
    if (HAS_SHIFT(vSwitchKeyStatus)) parts << "Shift";

    const int key = GET_SWITCH_KEY(vSwitchKeyStatus);
    if (key != kSwitchKeyModifiersOnly) {
        const int index = _hotkeyKey->findData(key);
        parts << (index >= 0 ? _hotkeyKey->itemText(index) : tr("?"));
    }

    if (parts.isEmpty()) {
        _hotkeyPreview->setText(tr("Chưa đặt phím chuyển chế độ."));
    } else {
        _hotkeyPreview->setText(tr("Tổ hợp hiện tại: %1").arg(parts.join(" + ")));
    }
}

QWidget* MainWindow::buildSystemTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* grid = new QGridLayout;

    int row = 0;
    addCheck(grid, row++, 0, tr("Chuyển chế độ thông minh theo ứng dụng"),
             &vUseSmartSwitchKey,
             tr("Nhớ ứng dụng nào dùng tiếng Việt, ứng dụng nào dùng tiếng Anh"));
    addCheck(grid, row++, 0, tr("Tự nhớ bảng mã theo ứng dụng"), &vRememberCode);
    addCheck(grid, row++, 0, tr("Tắt tiếng Việt khi dùng bố cục bàn phím khác"),
             &vOtherLanguage);
    layout->addLayout(grid);

    layout->addSpacing(12);
    layout->addWidget(buildDebugGroup(page));

    auto* info = new QLabel(
        tr("Cấu hình lưu tại ~/.config/openkey/config.json\n"
           "Chỉ một bộ gõ được giữ input method của phiên Wayland: phải tắt hẳn "
           "fcitx5 và ibus thì OpenKey mới gõ được."),
        page);
    info->setWordWrap(true);
    layout->addSpacing(12);
    layout->addWidget(info);
    layout->addStretch(1);
    return page;
}

// Loi go thuong chi tai hien duoc tren may nguoi dung, va gan het la loi thu tu
// hoac dua tranh — doan mo khong ra. Nut nay de ho tu ghi lai dung luc loi xay
// ra roi gui file log di.
QWidget* MainWindow::buildDebugGroup(QWidget* parent) {
    auto* group = new QGroupBox(tr("Chẩn đoán lỗi gõ"), parent);
    auto* box = new QVBoxLayout(group);

    auto* hint = new QLabel(
        tr("Gặp lỗi gõ? Bấm nút bên dưới rồi gõ lại cho lỗi tái hiện, sau đó "
           "dừng ghi và gửi file nhật ký cho người phát triển."),
        group);
    hint->setWordWrap(true);
    box->addWidget(hint);

    _debugToggle = new QPushButton(group);
    _debugStatus = new QLabel(group);
    _debugStatus->setWordWrap(true);
    _debugStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* copyPath = new QPushButton(tr("Chép đường dẫn"), group);
    auto* openFolder = new QPushButton(tr("Mở thư mục chứa log"), group);

    auto* row = new QHBoxLayout;
    row->addWidget(_debugToggle);
    row->addWidget(copyPath);
    row->addWidget(openFolder);
    row->addStretch(1);
    box->addLayout(row);
    box->addWidget(_debugStatus);

    connect(_debugToggle, &QPushButton::clicked, this, &MainWindow::toggleDebugLogging);
    connect(copyPath, &QPushButton::clicked, this, [] {
        QApplication::clipboard()->setText(
            QString::fromStdString(openkey::debugLogPath()));
    });
    connect(openFolder, &QPushButton::clicked, this, [] {
        const QString dir =
            QFileInfo(QString::fromStdString(openkey::debugLogPath())).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    refreshDebugUi();
    return group;
}

void MainWindow::toggleDebugLogging() {
    const bool wantOn = !openkey::debugLoggingEnabled();
    if (!openkey::setDebugLogging(wantOn)) {
        QMessageBox::warning(this, tr("H-OpenKey"),
                             tr("Không mở được file nhật ký:\n%1")
                                 .arg(QString::fromStdString(openkey::debugLogPath())));
    }
    refreshDebugUi();
}

void MainWindow::refreshDebugUi() {
    const bool on = openkey::debugLoggingEnabled();
    _debugToggle->setText(on ? tr("Dừng ghi nhật ký") : tr("Bắt đầu ghi nhật ký"));
    _debugStatus->setText(
        (on ? tr("Đang ghi vào:\n%1") : tr("File nhật ký:\n%1"))
            .arg(QString::fromStdString(openkey::debugLogPath())));
}

void MainWindow::refreshFromState() {
    _loading = true;

    selectValue(_codeTable, vCodeTable);
    selectValue(_inputType, vInputType);

    switch (_config.backend) {
        case BackendKind::Wayland: _backendWayland->setChecked(true); break;
        case BackendKind::X11: _backendX11->setChecked(true); break;
        case BackendKind::Auto: _backendAuto->setChecked(true); break;
    }

    for (const auto& bound : _checks) {
        bound.box->setChecked(*bound.value != 0);
    }

    _hotkeyCtrl->setChecked(HAS_CONTROL(vSwitchKeyStatus));
    _hotkeyAlt->setChecked(HAS_OPTION(vSwitchKeyStatus));
    _hotkeySuper->setChecked(HAS_COMMAND(vSwitchKeyStatus));
    _hotkeyShift->setChecked(HAS_SHIFT(vSwitchKeyStatus));
    selectValue(_hotkeyKey, GET_SWITCH_KEY(vSwitchKeyStatus));
    writeHotkeyToUi();

    _loading = false;
}

void MainWindow::applyToEngine() {
    // Doi kiem tra chinh ta phai bao cho engine biet, neu khong trang thai cu
    // van con hieu luc cho toi het tu dang go.
    vSetCheckSpelling();
}

void MainWindow::restoreDefaults() {
    resetAppStateToDefault();
    onTableCodeChange();
    _core.resetTypingState();
    refreshFromState();
    emit settingsChanged();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    event->ignore();
    hide();
}

void MainWindow::quitApplication() {
    _config.save();
    QApplication::quit();
}

void MainWindow::save() {
    _config.save();
    close();
}

} // namespace openkey
