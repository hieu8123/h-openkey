//
//  MacroDialog.h
//  OpenKey cho Linux
//
//  Bang go tat. Du lieu nam trong engine (Macro.cpp), day chi la lop giao dien
//  mong: doc bang getAllMacro, ghi bang addMacro/deleteMacro, luu xuong dia
//  bang Config::saveMacroTable.
//

#ifndef OPENKEY_LINUX_MACRODIALOG_H
#define OPENKEY_LINUX_MACRODIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class QTableWidget;

namespace openkey {

class Config;

class MacroDialog : public QDialog {
    Q_OBJECT

public:
    MacroDialog(Config& config, QWidget* parent = nullptr);

private:
    void reload();
    void addOrUpdate();
    void removeSelected();
    void onSelectionChanged();
    void importFile();
    void exportFile();

    Config& _config;
    QTableWidget* _table = nullptr;
    QLineEdit* _keyEdit = nullptr;
    QLineEdit* _contentEdit = nullptr;
    QPushButton* _removeButton = nullptr;
};

} // namespace openkey

#endif // OPENKEY_LINUX_MACRODIALOG_H
