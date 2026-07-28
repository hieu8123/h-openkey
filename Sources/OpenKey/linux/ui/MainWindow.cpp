//
//  MainWindow.cpp
//  OpenKey cho Linux
//

#include "MainWindow.h"

#include <QButtonGroup>
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

#include "AppState.h"
#include "Config.h"
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

void selectValue(QComboBox* box, int value) {
    const int index = box->findData(value);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

MainWindow::MainWindow(Config& config, OpenKeyCore& core, QWidget* parent)
    : QWidget(parent), _config(config), _core(core) {
    setWindowTitle(tr("OpenKey"));

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildBasicTab(), tr("Cơ bản"));
    tabs->addTab(buildHotkeyTab(), tr("Phím tắt"));
    tabs->addTab(buildSystemTab(), tr("Hệ thống"));

    auto* defaults = new QPushButton(tr("Thiết lập mặc định"));
    auto* saveButton = new QPushButton(tr("Lưu"));
    saveButton->setDefault(true);
    connect(defaults, &QPushButton::clicked, this, &MainWindow::restoreDefaults);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::save);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(defaults);
    bottom->addStretch(1);
    bottom->addWidget(saveButton);

    auto* root = new QVBoxLayout(this);
    root->addWidget(buildControlGroup());
    root->addWidget(tabs, 1);
    root->addLayout(bottom);

    refreshFromState();
}

QWidget* MainWindow::buildControlGroup() {
    auto* group = new QGroupBox(tr("Điều khiển"), this);
    auto* grid = new QGridLayout(group);

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
    _backendX11->setEnabled(false);
    _backendX11->setToolTip(tr("Chưa có trong bản này"));

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
    addCheck(grid, row++, 1, tr("Tạm tắt OpenKey bằng Alt"), &vTempOffOpenKey);

    outer->addLayout(grid);

    auto* macroGroup = new QGroupBox(page);
    auto* macroRow = new QHBoxLayout(macroGroup);
    auto* macroGrid = new QGridLayout;
    addCheck(macroGrid, 0, 0, tr("Cho phép gõ tắt"), &vUseMacro);
    macroRow->addLayout(macroGrid);

    auto* macroButton = new QPushButton(tr("Bảng gõ tắt"), macroGroup);
    macroButton->setEnabled(false);
    macroButton->setToolTip(tr("Sắp có"));
    macroRow->addWidget(macroButton);

    auto* macroGrid2 = new QGridLayout;
    addCheck(macroGrid2, 0, 0, tr("Vẫn gõ tắt khi ở chế độ tiếng Anh"),
             &vUseMacroInEnglishMode);
    addCheck(macroGrid2, 1, 0, tr("Tự viết hoa theo cách gõ (btw → By the way)"),
             &vAutoCapsMacro);
    macroRow->addLayout(macroGrid2);
    macroRow->addStretch(1);

    outer->addWidget(macroGroup);
    outer->addStretch(1);
    return page;
}

QWidget* MainWindow::buildHotkeyTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* note = new QLabel(
        tr("Phím chuyển giữa tiếng Việt và tiếng Anh hiện là Alt + Z.\n"
           "Phần cho phép tự chọn tổ hợp phím đang được làm."),
        page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    return page;
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

void MainWindow::save() {
    _config.save();
    close();
}

} // namespace openkey
