#ifndef SELECTDIALOG_H
#define SELECTDIALOG_H

#include <QDialog>
#include <QButtonGroup>

namespace Ui {
class SelectDialog;
}

class SelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SelectDialog(QWidget *parent = nullptr);
    ~SelectDialog();

    float GetValue();
    int m_iSelectIndex; // 0=身高, 1=体重
    int m_iRadioID;     // 0=高于, 1=等于, 2=低于

private slots:
    void on_comboSpecialItem_currentIndexChanged(const QString &arg1);

private:
    Ui::SelectDialog *ui;
    QButtonGroup *RadioGroup;
};

#endif // SELECTDIALOG_H