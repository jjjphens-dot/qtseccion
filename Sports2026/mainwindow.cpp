#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qdir.h"
#include "qfiledialog.h"
#include "signupdialog.h"
#include "resultinputdialog.h"
#include "csportman.h"
#include <QStandardItem>
#include <QMessageBox>
#include <QAbstractItemDelegate>
#include <QModelIndex>
#include <QHeaderView>
#include <QSet>
#include "readonlydelegate.h"
#include "dateeditdelegate.h"
#include "selectdialog.h"


QStandardItem* MainWindow::MakeNumericItem(float value, int decimals)
{
    QStandardItem *item = new QStandardItem();
    item->setData(value, Qt::UserRole);
    item->setText(QString::number(value, 'f', decimals));
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QStandardItem* MainWindow::MakeIntItem(int value)
{
    QStandardItem *item = new QStandardItem();
    item->setData(value, Qt::UserRole);
    item->setText(QString::number(value));
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

// 姓名单元格：携带排序键与运动员编号（按编号定位，不受排序/筛选影响）
QStandardItem* MainWindow::MakeNameItem(const CSportMan &man)
{
    QStandardItem *item = new QStandardItem(man.m_name);
    item->setData(man.m_name, Qt::UserRole);   // 排序用
    item->setData(man.m_number, AthleteNumberRole);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

// 总分单元格：加粗 + 蓝色
QStandardItem* MainWindow::MakeTotalScoreItem(float total)
{
    QStandardItem *item = MakeNumericItem(total, 0);
    QFont boldFont = item->font();
    boldFont.setBold(true);
    item->setFont(boldFont);
    item->setForeground(QColor("#0D47A1"));
    return item;
}

// 名次单元格：加粗，前3名金/银/铜配色；tintOthers 为 true 时其余名次加浅色底
QStandardItem* MainWindow::MakePlaceItem(int place, bool tintOthers)
{
    QStandardItem *item = MakeIntItem(place);
    QFont boldFont = item->font();
    boldFont.setBold(true);
    item->setFont(boldFont);
    switch (place)
    {
    case 1:
        item->setBackground(QColor("#FFD54F"));
        item->setForeground(QColor("#5D4037"));
        break;
    case 2:
        item->setBackground(QColor("#CFD8DC"));
        item->setForeground(QColor("#37474F"));
        break;
    case 3:
        item->setBackground(QColor("#BCAAA4"));
        item->setForeground(QColor("#3E2723"));
        break;
    default:
        if (tintOthers)
            item->setBackground(QColor("#F5F8FC"));
        break;
    }
    return item;
}

// 清空搜索框但不触发 textChanged（避免重建时重复过滤）
void MainWindow::ClearSearchEdit()
{
    ui->searchEdit->blockSignals(true);
    ui->searchEdit->clear();
    ui->searchEdit->blockSignals(false);
}

// 将前 colCount 列全部设为只读委托
void MainWindow::SetColumnsReadOnly(int colCount)
{
    for (int col = 0; col < colCount; col++)
        ui->ShowInfotableView->setItemDelegateForColumn(col, m_roDelegate);
}

void MainWindow::ApplyColumnSizing()
{
    ui->ShowInfotableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

// 构造函数
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_header(nullptr)
    , m_roDelegate(nullptr)
    , m_dateDelegate(nullptr)
{
    ui->setupUi(this);

    // 表格设置
    ui->ShowInfotableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->ShowInfotableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->ShowInfotableView->verticalHeader()->setDefaultSectionSize(34);
    ui->ShowInfotableView->verticalHeader()->setVisible(false);
    ui->ShowInfotableView->setShowGrid(true);
    ui->ShowInfotableView->setAlternatingRowColors(true);
    ui->ShowInfotableView->setSortingEnabled(true);

    m_sportsmanInfoModel = new QStandardItemModel(this);   // 父对象负责释放
    // 以 UserRole 中的数值进行排序
    m_sportsmanInfoModel->setSortRole(Qt::UserRole);
    ui->ShowInfotableView->setModel(m_sportsmanInfoModel);

    // 自定义表头
    m_header = new SortableHeaderView(this);
    ui->ShowInfotableView->setHorizontalHeader(m_header);

    // 只读委托
    m_roDelegate = new ReadOnlyDelegate(this);

    // 日期列委托
    m_dateDelegate = new DateEditDelegate(QStringLiteral("yyyy-MM"),
                                         QDate(1940, 1, 1), QDate::currentDate(), this);

    // 信号槽
    connect(ui->ShowInfotableView->itemDelegate(), &QAbstractItemDelegate::closeEditor,
            this, &MainWindow::on_ShowInfotableView_changed);
    connect(m_dateDelegate, &QAbstractItemDelegate::closeEditor,
            this, &MainWindow::on_ShowInfotableView_changed);

    m_curTable = 0;
    UpdateStatusBar();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::UpdateStatusBar()
{
    int count = m_infoTable.GetSportsmanNum();
    QString msg = QStringLiteral("运动员总数: %1 / %2")
                      .arg(count)
                      .arg(SportsManInfoTable::MAX_SPORTSMAN);
    ui->statusbar->showMessage(msg);
}

// 文件菜单
void MainWindow::on_actionOpen_triggered()
{
    QString curPath = QDir::currentPath();
    QString dlgTitle = QStringLiteral("打开数据文件");
    QString filter = QStringLiteral("JSON 文件(*.json);;所有文件(*.*)");
    QString aFileName = QFileDialog::getOpenFileName(this, dlgTitle, curPath, filter);

    if (aFileName.isEmpty())
        return;

    if (m_infoTable.ReadSportsmanFromFile(aFileName))
    {
        ShowSignupTable();
        UpdateStatusBar();
    }
    else
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法打开文件！"));
    }
}

void MainWindow::on_actionSave_triggered()
{
    QString curPath = QDir::currentPath();
    QString dlgTitle = QStringLiteral("保存数据文件");
    QString filter = QStringLiteral("JSON 文件(*.json);;所有文件(*.*)");
    QString aFileName = QFileDialog::getSaveFileName(this, dlgTitle, curPath, filter);

    if (aFileName.isEmpty())
        return;

    // 用户没写扩展名时自动补 .json
    if (!aFileName.endsWith(".json", Qt::CaseInsensitive))
        aFileName += ".json";

    if (!m_infoTable.SaveSportsmanToFile(aFileName))
    {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("保存文件失败！"));
    }
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

// 报名管理
void MainWindow::on_actionSignup_triggered()
{
    SignupDialog dlgSignup(this);
    // 号码唯一性校验
    QSet<int> existing;
    for (int i = 0; i < m_infoTable.GetSportsmanNum(); i++)
        existing.insert(m_infoTable.GetSportMan(i).m_number);
    dlgSignup.SetExistingNumbers(existing);

    if (dlgSignup.exec() == QDialog::Accepted)
    {
        CSportMan sportman;
        sportman.m_number = dlgSignup.Number();
        sportman.m_name = dlgSignup.Name();
        sportman.m_date = dlgSignup.BirthDate();
        sportman.m_height = dlgSignup.SportsmanHeight();
        sportman.m_weight = dlgSignup.SportsmanWeight();

        if (m_infoTable.AddSportman(sportman))
        {
            ShowSignupTable();
            UpdateStatusBar();
        }
    }
}

// 成绩录入
void MainWindow::on_actionResultInput_triggered()
{
    // 预选首位运动员数据
    EditAthleteResults(0);
}

// 弹出成绩录入对话框编辑指定运动员
void MainWindow::EditAthleteResults(int athleteIdx)
{
    if (m_infoTable.GetSportsmanNum() == 0)
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先录入运动员报名信息！"));
        return;
    }
    if (athleteIdx < 0 || athleteIdx >= m_infoTable.GetSportsmanNum())
        athleteIdx = 0;

    ResultInputDialog dlg(this);
    QVector<CSportMan> athletes;
    for (int i = 0; i < m_infoTable.GetSportsmanNum(); i++)
        athletes.push_back(m_infoTable.GetSportMan(i));
    dlg.SetAthleteList(athletes);
    dlg.SelectAthlete(athleteIdx);

    if (dlg.exec() == QDialog::Accepted)
    {
        int idx = dlg.GetSelectedAthleteIndex();
        if (idx >= 0 && idx < m_infoTable.GetSportsmanNum())
        {
            float results[5];
            dlg.GetResults(results);
            CSportMan &athlete = m_infoTable.GetSportMan(idx);
            for (int i = 0; i < 5; i++)
                athlete.m_score.m_record[i].m_record = results[i];
            athlete.m_score.CalculateTotalScore();
            m_infoTable.CalculateRank();
            ShowGradeTable();
            UpdateStatusBar();
        }
    }
}

void MainWindow::on_actionViewSignup_triggered()
{
    ShowSignupTable();
}

// 成绩查询
void MainWindow::on_actionViewGrade_triggered()
{
    ShowGradeTable();
}

void MainWindow::on_actionViewWinners_triggered()
{
    m_infoTable.CalculateRank();
    ShowWinList();
}

void MainWindow::on_actionFilter_triggered()
{
    SelectDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_infoTable.CalculateRank();
        ShowSpecialList(dlg.m_iSelectIndex, dlg.m_iRadioID, dlg.GetValue());
    }
}

// 提示信息

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, QStringLiteral("关于"),
                       QStringLiteral("男子五项全能比赛信息管理系统\n\n"
                                      "比赛项目：100米、110米栏、1500米、跳高、铅球\n"
                                      "版本：2.0\n"
                                      "人数上限：50人"));
}

// 表格显示
void MainWindow::ShowSignupTable()
{
    m_curTable = 1;
    m_sportsmanInfoModel->clear();
    ui->ShowInfotableView->setSortingEnabled(false);
    ClearSearchEdit();

    QStringList headers;
    headers << QStringLiteral("号码")
            << QStringLiteral("姓名")
            << QStringLiteral("出生年月")
            << QStringLiteral("身高(cm)")
            << QStringLiteral("体重(kg)");
    m_sportsmanInfoModel->setHorizontalHeaderLabels(headers);

    int rowCnt = m_infoTable.GetSportsmanNum();
    m_sportsmanInfoModel->setRowCount(rowCnt);

    for (int i = 0; i < rowCnt; ++i)
    {
        CSportMan &man = m_infoTable.GetSportMan(i);

        QStandardItem *itemNum = MakeIntItem(man.m_number);
        itemNum->setData(man.m_number, AthleteNumberRole);
        m_sportsmanInfoModel->setItem(i, 0, itemNum);

        m_sportsmanInfoModel->setItem(i, 1, MakeNameItem(man));

        QStandardItem *itemDate = new QStandardItem(man.m_date.toString("yyyy-MM"));
        itemDate->setData(man.m_date, Qt::UserRole);    // 按日期排序
        itemDate->setTextAlignment(Qt::AlignCenter);
        m_sportsmanInfoModel->setItem(i, 2, itemDate);

        m_sportsmanInfoModel->setItem(i, 3, MakeNumericItem(man.m_height, 1));
        m_sportsmanInfoModel->setItem(i, 4, MakeNumericItem(man.m_weight, 1));
    }

    // 设定委托
    ui->ShowInfotableView->setItemDelegateForColumn(0, m_roDelegate);
    ui->ShowInfotableView->setItemDelegateForColumn(2, m_dateDelegate);
    ui->ShowInfotableView->setItemDelegateForColumn(1, nullptr);
    ui->ShowInfotableView->setItemDelegateForColumn(3, nullptr);
    ui->ShowInfotableView->setItemDelegateForColumn(4, nullptr);

    // 列宽
    ApplyColumnSizing();
    ui->ShowInfotableView->setSortingEnabled(true);
    ui->labelViewTitle->setText(QStringLiteral("当前视图：报名表（点击表头可排序）"));
    applySearchFilter();
}

void MainWindow::ShowGradeTable()
{
    // 仅当当前已是成绩表时保留排序状态（避免从其他视图切换时按错误列排序）
    const bool wasGradeTable = (m_curTable == 2);
    int prevSortCol = wasGradeTable ? m_header->sortIndicatorSection() : -1;
    Qt::SortOrder prevSortOrder = m_header->sortIndicatorOrder();

    m_curTable = 2;
    m_sportsmanInfoModel->clear();
    ui->ShowInfotableView->setSortingEnabled(false);
    // 切换到成绩表时清空搜索
    if (!wasGradeTable)
        ClearSearchEdit();

    QStringList headers;
    headers << QStringLiteral("姓名")
            << QStringLiteral("100米\n(秒)") << QStringLiteral("100米\n得分")
            << QStringLiteral("110米栏\n(秒)") << QStringLiteral("110米栏\n得分")
            << QStringLiteral("1500米\n(分)") << QStringLiteral("1500米\n得分")
            << QStringLiteral("跳高\n(米)") << QStringLiteral("跳高\n得分")
            << QStringLiteral("铅球\n(米)") << QStringLiteral("铅球\n得分")
            << QStringLiteral("总分") << QStringLiteral("名次");
    m_sportsmanInfoModel->setHorizontalHeaderLabels(headers);

    m_infoTable.CalculateRank();

    int rowCnt = m_infoTable.GetSportsmanNum();
    m_sportsmanInfoModel->setRowCount(rowCnt);

    for (int i = 0; i < rowCnt; ++i)
    {
        CSportMan &man = m_infoTable.GetSportMan(i);
        const CScore &sc = man.m_score;

        m_sportsmanInfoModel->setItem(i, 0, MakeNameItem(man));

        // 五个项目：成绩、得分
        for (int e = 0; e < 5; ++e)
        {
            // 显示2位小数
            m_sportsmanInfoModel->setItem(i, 1 + 2 * e, MakeNumericItem(sc.m_record[e].m_record, 2));
            m_sportsmanInfoModel->setItem(i, 2 + 2 * e, MakeNumericItem(sc.m_record[e].m_score, 0));
        }

        QStandardItem *itemTotal = MakeTotalScoreItem(sc.m_totalscore);
        itemTotal->setBackground(QColor("#E3F2FD"));
        m_sportsmanInfoModel->setItem(i, 11, itemTotal);

        m_sportsmanInfoModel->setItem(i, 12, MakePlaceItem(man.m_place, true));
    }

    // 全部列只读
    SetColumnsReadOnly(headers.size());

    // 列宽拉伸
    ApplyColumnSizing();
    ui->ShowInfotableView->setSortingEnabled(true);

    // 恢复排序状态，或默认按名次升序
    if (prevSortCol >= 0 && prevSortCol < headers.size())
        m_header->setSortIndicator(prevSortCol, prevSortOrder);
    else
        m_header->setSortIndicator(12, Qt::AscendingOrder);

    ui->labelViewTitle->setText(QStringLiteral("当前视图：成绩总表（双击姓名修改成绩；点击表头排序）"));
    applySearchFilter();
}

void MainWindow::ShowWinList()
{
    m_curTable = 3;
    m_sportsmanInfoModel->clear();
    ui->ShowInfotableView->setSortingEnabled(false);
    ClearSearchEdit();

    QStringList headers;
    headers << QStringLiteral("姓名") << QStringLiteral("总分") << QStringLiteral("名次");
    m_sportsmanInfoModel->setHorizontalHeaderLabels(headers);

    int totalCnt = m_infoTable.GetSportsmanNum();

    int displayed = 0;
    for (int i = 0; i < totalCnt; ++i)
    {
        CSportMan &man = m_infoTable.GetSportMan(i);
        if (man.m_place > 6)
            continue;

        m_sportsmanInfoModel->setItem(displayed, 0, MakeNameItem(man));
        m_sportsmanInfoModel->setItem(displayed, 1, MakeTotalScoreItem(man.m_score.m_totalscore));
        m_sportsmanInfoModel->setItem(displayed, 2, MakePlaceItem(man.m_place, false));
        displayed++;
    }
    m_sportsmanInfoModel->setRowCount(displayed);

    // 所有列只读
    SetColumnsReadOnly(headers.size());

    ApplyColumnSizing();
    ui->ShowInfotableView->setSortingEnabled(true);
    m_header->setSortIndicator(2, Qt::AscendingOrder);  // 默认按名次升序
    ui->labelViewTitle->setText(QStringLiteral("当前视图：领奖名单（前6名）"));
    applySearchFilter();
}

void MainWindow::ShowSpecialList(int typeID, int RatioID, float valuedata)
{
    m_curTable = 4;
    m_sportsmanInfoModel->clear();
    ui->ShowInfotableView->setSortingEnabled(false);
    ClearSearchEdit();

    QStringList headers;
    QString typeName = (typeID == 0) ? QStringLiteral("身高(cm)") : QStringLiteral("体重(kg)");
    headers << QStringLiteral("姓名") << typeName
            << QStringLiteral("100米") << QStringLiteral("110米栏")
            << QStringLiteral("1500米") << QStringLiteral("跳高")
            << QStringLiteral("铅球") << QStringLiteral("总分") << QStringLiteral("名次");
    m_sportsmanInfoModel->setHorizontalHeaderLabels(headers);

    int totalCnt = m_infoTable.GetSportsmanNum();

    int rowIdx = 0;
    for (int i = 0; i < totalCnt; i++)
    {
        CSportMan &man = m_infoTable.GetSportMan(i);
        float var = (typeID == 0) ? man.m_height : man.m_weight;

        bool match = false;
        switch (RatioID)
        {
        case 0: match = (var > valuedata); break;
        case 1: match = (var == valuedata); break;
        case 2: match = (var < valuedata); break;
        }
        if (!match) continue;

        m_sportsmanInfoModel->setItem(rowIdx, 0, MakeNameItem(man));

        m_sportsmanInfoModel->setItem(rowIdx, 1, MakeNumericItem(var, 1));
        for (int e = 0; e < 5; ++e)
            m_sportsmanInfoModel->setItem(rowIdx, 2 + e, MakeNumericItem(man.m_score.m_record[e].m_record, 2));

        m_sportsmanInfoModel->setItem(rowIdx, 7, MakeTotalScoreItem(man.m_score.m_totalscore));
        m_sportsmanInfoModel->setItem(rowIdx, 8, MakeIntItem(man.m_place));
        rowIdx++;
    }
    m_sportsmanInfoModel->setRowCount(rowIdx);

    SetColumnsReadOnly(headers.size());

    ApplyColumnSizing();

    QString ratioText;
    switch (RatioID) {
    case 0: ratioText = QStringLiteral("高于"); break;
    case 1: ratioText = QStringLiteral("等于"); break;
    case 2: ratioText = QStringLiteral("低于"); break;
    }
    ui->ShowInfotableView->setSortingEnabled(true);
    m_header->setSortIndicator(7, Qt::DescendingOrder);  // 默认按总分降序
    ui->labelViewTitle->setText(QStringLiteral("当前视图：%1%2 %3 的运动员（供研究用）")
                                    .arg(typeName, ratioText)
                                    .arg(valuedata));
    applySearchFilter();
}

// 表格编辑

void MainWindow::on_ShowInfotableView_changed()
{
    QModelIndex index = ui->ShowInfotableView->currentIndex();
    int visualRow = index.row();
    int col = index.column();

    // 按运动员编号定位
    QModelIndex idxCol0 = m_sportsmanInfoModel->index(visualRow, 0);
    int athleteIdx = m_infoTable.FindSportmanByNumber(
                m_sportsmanInfoModel->data(idxCol0, AthleteNumberRole).toInt());

    if (athleteIdx < 0 || athleteIdx >= m_infoTable.GetSportsmanNum())
        return;

    CSportMan &sportman = m_infoTable.GetSportMan(athleteIdx);
    QStandardItem *item = m_sportsmanInfoModel->itemFromIndex(index);
    QVariant data = m_sportsmanInfoModel->data(index);

    switch (m_curTable)
    {
    case 1: // 报名表（号码列只读，不会进入）
        switch (col)
        {
        case 1:
            sportman.m_name = data.toString();
            if (item) item->setData(sportman.m_name, Qt::UserRole);   // 同步排序键
            break;
        case 2:
            sportman.m_date = item ? item->data(Qt::UserRole).toDate() : data.toDate();
            break;
        case 3:
            sportman.m_height = data.toFloat();
            if (item) item->setData(sportman.m_height, Qt::UserRole);
            break;
        case 4:
            sportman.m_weight = data.toFloat();
            if (item) item->setData(sportman.m_weight, Qt::UserRole);
            break;
        }
        break;
    }
}

// 双击编辑与搜索
void MainWindow::on_ShowInfotableView_doubleClicked(const QModelIndex &index)
{
    if (m_curTable != 2 || !index.isValid() || index.column() != 0)
        return;
    int athleteIdx = m_infoTable.FindSportmanByNumber(
                m_sportsmanInfoModel->data(index, AthleteNumberRole).toInt());
    if (athleteIdx < 0)
        return;
    EditAthleteResults(athleteIdx);
}

void MainWindow::on_searchEdit_textChanged(const QString &text)
{
    Q_UNUSED(text);
    applySearchFilter();
}

void MainWindow::applySearchFilter()
{
    // 姓名列：报名表为第1列，其余视图均为第0列
    const int nameCol = (m_curTable == 1) ? 1 : 0;
    const QString key = ui->searchEdit->text().trimmed();
    const int rows = m_sportsmanInfoModel->rowCount();

    int matched = 0;
    for (int r = 0; r < rows; r++)
    {
        QStandardItem *nameItem = m_sportsmanInfoModel->item(r, nameCol);
        const QString name = nameItem ? nameItem->text() : QString();
        const bool match = key.isEmpty() || name.contains(key, Qt::CaseInsensitive);
        ui->ShowInfotableView->setRowHidden(r, !match);
        if (match) ++matched;
    }

    if (rows == 0 || key.isEmpty())
        ui->labelMatchCount->setText(key.isEmpty() && rows > 0
            ? QStringLiteral("共 %1 人").arg(rows) : QString());
    else
        ui->labelMatchCount->setText(QStringLiteral("匹配 %1 / %2").arg(matched).arg(rows));
}