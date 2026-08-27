#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "sportsmaninfotable.h"
#include <QStandardItemModel>
#include <QModelIndex>
#include "sortableheaderview.h"

class ReadOnlyDelegate;
class DateEditDelegate;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void ShowSignupTable();
    void ShowGradeTable();
    void ShowWinList();
    void ShowSpecialList(int typeID, int RatioID, float valuedata);

    // 自定义角色：单元格关联的运动员编号（唯一）
    static const int AthleteNumberRole = Qt::UserRole + 1;

private slots:
    // 文件菜单
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionExit_triggered();

    // 报名管理
    void on_actionSignup_triggered();
    void on_actionResultInput_triggered();
    void on_actionViewSignup_triggered();

    // 成绩查询
    void on_actionViewGrade_triggered();
    void on_actionViewWinners_triggered();
    void on_actionFilter_triggered();

    // 帮助
    void on_actionAbout_triggered();

    // 表格编辑
    void on_ShowInfotableView_changed();
    // 双击成绩总表姓名，弹出成绩录入对话框
    void on_ShowInfotableView_doubleClicked(const QModelIndex &index);
    // 搜索框：实时按姓名过滤当前表格
    void on_searchEdit_textChanged(const QString &text);

private:
    void UpdateStatusBar();
    QStandardItem* MakeNumericItem(float value, int decimals = 1);
    QStandardItem* MakeIntItem(int value);
    // 姓名单元格：携带排序键与运动员编号
    QStandardItem* MakeNameItem(const CSportMan &man);
    // 总分单元格：加粗 + 蓝色
    QStandardItem* MakeTotalScoreItem(float total);
    // 名次单元格：加粗，前3名金/银/铜配色
    QStandardItem* MakePlaceItem(int place, bool tintOthers);
    // 列宽策略：所有列等比拉伸
    void ApplyColumnSizing();
    // 清空搜索框但不触发过滤
    void ClearSearchEdit();
    // 将前 colCount 列全部设为只读委托
    void SetColumnsReadOnly(int colCount);
    // 弹出成绩录入对话框编辑指定运动员
    void EditAthleteResults(int athleteIdx);
    // 按搜索框文字过滤当前表格行
    void applySearchFilter();

    Ui::MainWindow *ui;
    QStandardItemModel *m_sportsmanInfoModel;
    SortableHeaderView *m_header;
    ReadOnlyDelegate *m_roDelegate;
    DateEditDelegate *m_dateDelegate;
    SportsManInfoTable m_infoTable;
    int m_curTable; // 0=初始, 1=报名表, 2=成绩表, 3=领奖名单, 4=筛选名单
};

#endif // MAINWINDOW_H