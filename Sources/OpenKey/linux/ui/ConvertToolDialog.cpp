//
//  ConvertToolDialog.cpp
//  OpenKey cho Linux
//

#include "ConvertToolDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "ConvertTool.h"
#include "Engine.h"

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

QComboBox* makeCodeBox(QWidget* parent, int selected) {
    auto* box = new QComboBox(parent);
    for (const auto& choice : kCodeTables) {
        box->addItem(QString::fromUtf8(choice.label), choice.value);
    }
    const int index = box->findData(selected);
    box->setCurrentIndex(index >= 0 ? index : 0);
    return box;
}

} // namespace

ConvertToolDialog::ConvertToolDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Công cụ chuyển mã"));
    resize(720, 520);

    _fromCode = makeCodeBox(this, convertToolFromCode);
    _toCode = makeCodeBox(this, convertToolToCode);

    auto* codeRow = new QGridLayout;
    codeRow->addWidget(new QLabel(tr("Từ bảng mã")), 0, 0);
    codeRow->addWidget(_fromCode, 0, 1);
    codeRow->addWidget(new QLabel(tr("Sang bảng mã")), 0, 2);
    codeRow->addWidget(_toCode, 0, 3);
    codeRow->setColumnStretch(1, 1);
    codeRow->setColumnStretch(3, 1);

    auto* optionGrid = new QGridLayout;
    const struct {
        const char* label;
        bool* value;
        int row;
        int column;
    } options[] = {
        {"CHỮ HOA TẤT CẢ", &convertToolToAllCaps, 0, 0},
        {"chữ thường tất cả", &convertToolToAllNonCaps, 0, 1},
        {"Viết hoa chữ đầu câu", &convertToolToCapsFirstLetter, 1, 0},
        {"Viết Hoa Đầu Mỗi Từ", &convertToolToCapsEachWord, 1, 1},
        {"Bỏ dấu tiếng Việt", &convertToolRemoveMark, 2, 0},
    };
    for (const auto& option : options) {
        auto* box = new QCheckBox(tr(option.label), this);
        box->setChecked(*option.value);
        optionGrid->addWidget(box, option.row, option.column);
        _options.emplace_back(box, option.value);
    }

    _source = new QPlainTextEdit(this);
    _source->setPlaceholderText(tr("Dán văn bản cần chuyển vào đây"));
    _result = new QPlainTextEdit(this);
    _result->setReadOnly(true);
    _result->setPlaceholderText(tr("Kết quả"));

    auto* convertButton = new QPushButton(tr("Chuyển mã"), this);
    convertButton->setDefault(true);
    auto* clipboardButton = new QPushButton(tr("Chuyển nội dung clipboard"), this);
    auto* copyButton = new QPushButton(tr("Chép kết quả"), this);
    auto* closeButton = new QPushButton(tr("Đóng"), this);

    connect(convertButton, &QPushButton::clicked, this, &ConvertToolDialog::convert);
    connect(clipboardButton, &QPushButton::clicked, this,
            &ConvertToolDialog::convertClipboard);
    connect(copyButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(_result->toPlainText());
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(convertButton);
    buttons->addWidget(clipboardButton);
    buttons->addWidget(copyButton);
    buttons->addStretch(1);
    buttons->addWidget(closeButton);

    auto* root = new QVBoxLayout(this);
    root->addLayout(codeRow);
    root->addLayout(optionGrid);
    root->addWidget(new QLabel(tr("Văn bản gốc")));
    root->addWidget(_source, 1);
    root->addWidget(new QLabel(tr("Kết quả")));
    root->addWidget(_result, 1);
    root->addLayout(buttons);
}

void ConvertToolDialog::applyOptions() {
    convertToolFromCode = static_cast<Uint8>(_fromCode->currentData().toInt());
    convertToolToCode = static_cast<Uint8>(_toCode->currentData().toInt());
    for (const auto& [box, value] : _options) {
        *value = box->isChecked();
    }
}

void ConvertToolDialog::convert() {
    applyOptions();
    _result->setPlainText(
        QString::fromStdString(convertUtil(_source->toPlainText().toStdString())));
}

void ConvertToolDialog::convertClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    _source->setPlainText(clipboard->text());
    convert();
    // Chuyen xong thi tra thang vao clipboard: dung nhat la dan de len cho cu.
    clipboard->setText(_result->toPlainText());
}

} // namespace openkey
