#ifndef SIGNUPDIALOG_H
#define SIGNUPDIALOG_H

#include <QDialog>
#include <QString>
#include <QDate>
#include <QSet>

namespace Ui {
class SignupDialog;
}

class SignupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignupDialog(QWidget *parent = nullptr);
    ~SignupDialog();

    QString Name() const;
    int     Number() const;
    QDate   BirthDate() const;
    float   SportsmanHeight() const;
    float   SportsmanWeight() const;

    // 号码于唯一性校验
    void SetExistingNumbers(const QSet<int> &numbers);

public slots:
    void accept() override;

private:
    Ui::SignupDialog *ui;
    QSet<int> m_existingNumbers;
};

#endif // SIGNUPDIALOG_H