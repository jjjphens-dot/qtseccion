#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "core/enums.h"
#include "core/result.h"
#include <QMainWindow>
#include <optional>

class QLabel;
class QListWidget;
class QPushButton;
class QSplitter;
class QStackedWidget;

namespace takeout {
class AppContext;
class OrderTableModel;
} // namespace takeout

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(takeout::AppContext &context,
                      const takeout::Result<takeout::StartupState> &startup,
                      QWidget *parent = nullptr);
  ~MainWindow() override;

private:
  void refreshAuthenticationUi();
  void openLogin();
  void openRegistration();

  Ui::MainWindow *ui;
  takeout::AppContext &m_context;
  takeout::OrderTableModel *m_orders;
  std::optional<takeout::StartupState> m_startupState;
  QLabel *m_stateLabel;
  QLabel *m_sessionLabel;
  QPushButton *m_loginButton;
  QPushButton *m_registerButton;
  QPushButton *m_logoutButton;
  QListWidget *m_navigation;
  QStackedWidget *m_pages;
  QSplitter *m_roleContent;
  QWidget *m_loggedOutFiller;
};
#endif
