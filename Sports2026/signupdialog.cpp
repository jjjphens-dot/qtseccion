#include "signupdialog.h"
#include "ui_signupdialog.h"
#include <QMessageBox>
#include <QPushButton>

SignupDialog::SignupDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignupDialog)
{
    ui->setupUi(this);

    // 出生日期：弹出日历选择，只显示年月（题目要求"出身年月"），并限制合理范围
    ui->dateEditBirth->setCalendarPopup(true);
    ui->dateEditBirth->setDisplayFormat("yyyy-MM");
    ui->dateEditBirth->setMinimumDate(QDate(1940, 1, 1));
    ui->dateEditBirth->setMaximumDate(QDate::currentDate());
    ui->dateEditBirth->setDate(QDate::currentDate().addYears(-20));
}

SignupDialog::~SignupDialog()
{
    delete ui;
}

void SignupDialog::SetExistingNumbers(const QSet<int> &numbers)
{
    m_existingNumbers = numbers;
}

void SignupDialog::accept()
{
    // 验证号码（非空、正整数、不重复）
    QString numStr = ui->lineEditNumber->text().trimmed();
    if (numStr.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("请输入运动员号码！"));
        ui->lineEditNumber->setFocus();
        return;
    }
    bool numOk = false;
    int number = numStr.toInt(&numOk);
    if (!numOk || number <= 0)
    {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("号码必须为正整数！"));
        ui->lineEditNumber->setFocus();
        ui->lineEditNumber->selectAll();
        return;
    }
    if (m_existingNumbers.contains(number))
    {
        QMessageBox::warning(this, QStringLiteral("报名号码重复"),
                             QStringLiteral("号码 %1 已存在，不允许重复录入！").arg(number));
        ui->lineEditNumber->setFocus();
        ui->lineEditNumber->selectAll();
        return;
    }

    // 验证姓名
    if (ui->lineEditName->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("输入错误"), QStringLiteral("请输入运动员姓名！"));
        ui->lineEditName->setFocus();
        return;
    }

    // 验证身高
    bool ok;
    float height = ui->lineEditHeight->text().toFloat(&ok);
    if (!ok || height < 100.0f || height > 250.0f)
    {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
                             QStringLiteral("身高应在100-250厘米之间！"));
        ui->lineEditHeight->setFocus();
        return;
    }

    // 验证体重
    float weight = ui->lineEditWeight->text().toFloat(&ok);
    if (!ok || weight < 30.0f || weight > 200.0f)
    {
        QMessageBox::warning(this, QStringLiteral("输入错误"),
                             QStringLiteral("体重应在30-200公斤之间！"));
        ui->lineEditWeight->setFocus();
        return;
    }

    // 验证通过
    QDialog::accept();
}

QString SignupDialog::Name() const
{
    return ui->lineEditName->text().trimmed();
}

int SignupDialog::Number() const
{
    return ui->lineEditNumber->text().toInt();
}

QDate SignupDialog::BirthDate() const
{
    return ui->dateEditBirth->date();
}

float SignupDialog::SportsmanHeight() const
{
    return ui->lineEditHeight->text().toFloat();
}

float SignupDialog::SportsmanWeight() const
{
    return ui->lineEditWeight->text().toFloat();
}