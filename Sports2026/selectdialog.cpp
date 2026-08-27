#include "selectdialog.h"
#include "ui_selectdialog.h"
#include <QButtonGroup>

SelectDialog::SelectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectDialog)
{
    ui->setupUi(this);

    RadioGroup = new QButtonGroup(this);
    RadioGroup->addButton(ui->radioGreater, 0);
    RadioGroup->addButton(ui->radioEqual, 1);
    RadioGroup->addButton(ui->radioLower, 2);

    m_iSelectIndex = 0;
    m_iRadioID = 0;

    connect(RadioGroup, &QButtonGroup::idClicked,
            this, [this](int id) { m_iRadioID = id; });

    connect(ui->comboSpecialItem, &QComboBox::currentTextChanged,
            this, &SelectDialog::on_comboSpecialItem_currentIndexChanged);
}

SelectDialog::~SelectDialog()
{
    delete ui;
    delete RadioGroup;
}

float SelectDialog::GetValue()
{
    return ui->lineEditSelectedValue->text().toFloat();
}

void SelectDialog::on_comboSpecialItem_currentIndexChanged(const QString &arg1)
{
    if (arg1 == QStringLiteral("身高"))
    {
        m_iSelectIndex = 0;
        ui->label_unit->setText(QStringLiteral("厘米"));
    }
    else if (arg1 == QStringLiteral("体重"))
    {
        m_iSelectIndex = 1;
        ui->label_unit->setText(QStringLiteral("公斤"));
    }
}


