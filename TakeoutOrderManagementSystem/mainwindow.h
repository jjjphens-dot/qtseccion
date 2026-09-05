#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "core/result.h"
#include "core/enums.h"

namespace takeout { class AppContext; class OrderTableModel; }

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(takeout::AppContext& context,
                        const takeout::Result<takeout::StartupState>& startup,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    takeout::OrderTableModel* m_orders;
};
#endif // MAINWINDOW_H
