#include "resultinputdialog.h"
#include "ui_resultinputdialog.h"
#include <QCompleter>

ResultInputDialog::ResultInputDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResultInputDialog)
{
    ui->setupUi(this);

    ui->comboAthlete->setEditable(true);
    ui->comboAthlete->setInsertPolicy(QComboBox::NoInsert);   // 不把输入文字当作新项
    QCompleter *completer = new QCompleter(ui->comboAthlete->model(), this);
    completer->setFilterMode(Qt::MatchContains);               // 包含匹配
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    ui->comboAthlete->setCompleter(completer);

    connect(ui->comboAthlete, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ResultInputDialog::onAthleteChanged);
}

ResultInputDialog::~ResultInputDialog()
{
    delete ui;
}

void ResultInputDialog::SetAthleteList(const QVector<CSportMan> &athletes)
{
    m_athletes = athletes;
    ui->comboAthlete->clear();
    for (int i = 0; i < athletes.size(); i++)
    {
        QString text = QString::number(athletes[i].m_number) + " - " + athletes[i].m_name;
        ui->comboAthlete->addItem(text);
    }
    if (athletes.size() > 0)
        onAthleteChanged(0);
}

void ResultInputDialog::SelectAthlete(int index)
{
    if (index >= 0 && index < ui->comboAthlete->count())
        ui->comboAthlete->setCurrentIndex(index);   // 触发 onAthleteChanged 加载该运动员成绩
}

int ResultInputDialog::GetSelectedAthleteIndex() const
{
    // editable combo：优先按当前文本精确匹配，避免用户输入未选中时返回错误索引
    int idx = ui->comboAthlete->findText(ui->comboAthlete->currentText());
    if (idx >= 0)
        return idx;
    return ui->comboAthlete->currentIndex();
}

void ResultInputDialog::GetResults(float results[5]) const
{
    results[0] = static_cast<float>(ui->spin100m->value());
    results[1] = static_cast<float>(ui->spin110mh->value());
    results[2] = static_cast<float>(ui->spin1500m->value());
    results[3] = static_cast<float>(ui->spinHighJump->value());
    results[4] = static_cast<float>(ui->spinShotPut->value());
}

// 窗口加载已有成绩
void ResultInputDialog::onAthleteChanged(int index)
{
    if (index < 0 || index >= m_athletes.size())
        return;

    const CSportMan &athlete = m_athletes[index];

    ui->spin100m->setValue(static_cast<double>(athlete.m_score.m_record[0].m_record));
    ui->spin110mh->setValue(static_cast<double>(athlete.m_score.m_record[1].m_record));
    ui->spin1500m->setValue(static_cast<double>(athlete.m_score.m_record[2].m_record));
    ui->spinHighJump->setValue(static_cast<double>(athlete.m_score.m_record[3].m_record));
    ui->spinShotPut->setValue(static_cast<double>(athlete.m_score.m_record[4].m_record));
}