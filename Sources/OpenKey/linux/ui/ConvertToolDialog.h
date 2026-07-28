//
//  ConvertToolDialog.h
//  OpenKey cho Linux
//
//  Chuyen ma van ban qua lai giua cac bang ma. Toan bo viec chuyen do
//  ConvertTool.cpp cua engine lam; day chi la giao dien.
//

#ifndef OPENKEY_LINUX_CONVERTTOOLDIALOG_H
#define OPENKEY_LINUX_CONVERTTOOLDIALOG_H

#include <QDialog>

#include <vector>

class QCheckBox;
class QComboBox;
class QPlainTextEdit;

namespace openkey {

class ConvertToolDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConvertToolDialog(QWidget* parent = nullptr);

private:
    void convert();
    void convertClipboard();
    void applyOptions();

    QComboBox* _fromCode = nullptr;
    QComboBox* _toCode = nullptr;
    QPlainTextEdit* _source = nullptr;
    QPlainTextEdit* _result = nullptr;
    std::vector<std::pair<QCheckBox*, bool*>> _options;
};

} // namespace openkey

#endif // OPENKEY_LINUX_CONVERTTOOLDIALOG_H
