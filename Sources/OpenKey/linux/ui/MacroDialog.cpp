//
//  MacroDialog.cpp
//  OpenKey cho Linux
//

#include "MacroDialog.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <string>
#include <vector>

#include "Config.h"
#include "Engine.h"
#include "Macro.h"

namespace openkey {

MacroDialog::MacroDialog(Config& config, QWidget* parent)
    : QDialog(parent), _config(config) {
    setWindowTitle(tr("Bảng gõ tắt"));
    resize(620, 440);

    _table = new QTableWidget(this);
    _table->setColumnCount(2);
    _table->setHorizontalHeaderLabels({tr("Gõ tắt"), tr("Thay bằng")});
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(_table, &QTableWidget::itemSelectionChanged, this,
            &MacroDialog::onSelectionChanged);

    _keyEdit = new QLineEdit(this);
    _keyEdit->setPlaceholderText(tr("vd: vn"));
    _contentEdit = new QLineEdit(this);
    _contentEdit->setPlaceholderText(tr("vd: Việt Nam"));

    auto* addButton = new QPushButton(tr("Thêm / Cập nhật"), this);
    addButton->setDefault(true);
    connect(addButton, &QPushButton::clicked, this, &MacroDialog::addOrUpdate);

    _removeButton = new QPushButton(tr("Xoá"), this);
    _removeButton->setEnabled(false);
    connect(_removeButton, &QPushButton::clicked, this, &MacroDialog::removeSelected);

    auto* form = new QGridLayout;
    form->addWidget(new QLabel(tr("Gõ tắt")), 0, 0);
    form->addWidget(_keyEdit, 0, 1);
    form->addWidget(new QLabel(tr("Thay bằng")), 1, 0);
    form->addWidget(_contentEdit, 1, 1);
    form->setColumnStretch(1, 1);

    auto* importButton = new QPushButton(tr("Nhập từ tệp…"), this);
    auto* exportButton = new QPushButton(tr("Xuất ra tệp…"), this);
    auto* closeButton = new QPushButton(tr("Đóng"), this);
    connect(importButton, &QPushButton::clicked, this, &MacroDialog::importFile);
    connect(exportButton, &QPushButton::clicked, this, &MacroDialog::exportFile);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(importButton);
    buttons->addWidget(exportButton);
    buttons->addWidget(closeButton);

    auto* root = new QVBoxLayout(this);
    root->addWidget(_table, 1);
    root->addLayout(form);
    root->addLayout(buttons);

    reload();
}

void MacroDialog::reload() {
    std::vector<std::vector<Uint32>> keys;
    std::vector<std::string> texts;
    std::vector<std::string> contents;
    getAllMacro(keys, texts, contents);

    _table->setRowCount(static_cast<int>(texts.size()));
    for (size_t i = 0; i < texts.size(); i++) {
        const int row = static_cast<int>(i);
        _table->setItem(row, 0,
                        new QTableWidgetItem(QString::fromStdString(texts[i])));
        _table->setItem(row, 1,
                        new QTableWidgetItem(QString::fromStdString(contents[i])));
    }
    onSelectionChanged();
}

void MacroDialog::onSelectionChanged() {
    const auto rows = _table->selectionModel()->selectedRows();
    _removeButton->setEnabled(!rows.isEmpty());
    if (rows.isEmpty()) {
        return;
    }
    const int row = rows.first().row();
    _keyEdit->setText(_table->item(row, 0)->text());
    _contentEdit->setText(_table->item(row, 1)->text());
}

void MacroDialog::addOrUpdate() {
    const QString key = _keyEdit->text().trimmed();
    const QString content = _contentEdit->text();

    if (key.isEmpty() || content.isEmpty()) {
        QMessageBox::information(this, tr("Bảng gõ tắt"),
                                 tr("Cần nhập cả phần gõ tắt lẫn phần thay thế."));
        return;
    }

    // addMacro cua engine khong ghi de macro da co, nen phai xoa truoc de nut
    // nay hoat dong dung nghia "Them / Cap nhat".
    const std::string keyText = key.toStdString();
    if (hasMacro(keyText)) {
        deleteMacro(keyText);
    }
    if (!addMacro(keyText, content.toStdString())) {
        QMessageBox::warning(this, tr("Bảng gõ tắt"), tr("Không thêm được mục này."));
        return;
    }

    _config.saveMacroTable();
    reload();
    _keyEdit->clear();
    _contentEdit->clear();
    _keyEdit->setFocus();
}

void MacroDialog::removeSelected() {
    const auto rows = _table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    const QString key = _table->item(rows.first().row(), 0)->text();
    deleteMacro(key.toStdString());
    _config.saveMacroTable();
    reload();
}

void MacroDialog::importFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Nhập bảng gõ tắt"), QString(), tr("Tệp gõ tắt (*.dat *.txt);;Tất cả (*)"));
    if (path.isEmpty()) {
        return;
    }
    // Nhap them vao bang hien co thay vi thay the, de khong mat cong go lai.
    readFromFile(path.toStdString(), true);
    onTableCodeChange();
    _config.saveMacroTable();
    reload();
}

void MacroDialog::exportFile() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Xuất bảng gõ tắt"), QString(), tr("Tệp gõ tắt (*.dat);;Tất cả (*)"));
    if (path.isEmpty()) {
        return;
    }
    saveToFile(path.toStdString());
}

} // namespace openkey
