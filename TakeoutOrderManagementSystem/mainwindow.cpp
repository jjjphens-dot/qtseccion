#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "app/appcontext.h"
#include "delegates/moneydelegate.h"
#include "delegates/orderstatusdelegate.h"
#include "dialogs/logindialog.h"
#include "dialogs/registerdialog.h"
#include "models/orderfilterproxymodel.h"
#include "models/ordertablemodel.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>

MainWindow::MainWindow(takeout::AppContext &context,
                       const takeout::Result<takeout::StartupState> &startup,
                       QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_context(context),
      m_orders(new takeout::OrderTableModel(this)),
      m_startupState(startup.ok()
                         ? std::optional<takeout::StartupState>(startup.value())
                         : std::nullopt),
      m_stateLabel(new QLabel(this)), m_sessionLabel(new QLabel(this)),
      m_loginButton(new QPushButton(QStringLiteral("登录"), this)),
      m_registerButton(new QPushButton(this)),
      m_logoutButton(new QPushButton(QStringLiteral("注销"), this)),
      m_navigation(nullptr), m_pages(nullptr), m_roleContent(nullptr),
      m_loggedOutFiller(new QWidget(this)) {
  using namespace takeout;
  ui->setupUi(this);
  setWindowTitle(QStringLiteral("外卖订单管理系统 · W04 认证版 0.5.0"));
  resize(1200, 800);
  setMinimumSize(960, 640);
  auto *layout = new QVBoxLayout(ui->centralwidget);
  auto *heading = new QLabel(QStringLiteral("外卖订单管理系统"), this);
  heading->setObjectName("heading");
  heading->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  layout->addWidget(heading);
  auto *notice = new QLabel(
      QStringLiteral(
          "本地多账号系统 · 注册和登录已启用；业务页面按当前登录角色路由。"),
      this);
  notice->setWordWrap(true);
  notice->setObjectName("notice");
  notice->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  layout->addWidget(notice);

  m_stateLabel->setObjectName("startupState");
  m_stateLabel->setTextFormat(Qt::PlainText);
  m_stateLabel->setWordWrap(true);
  m_stateLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  if (!startup.ok())
    m_stateLabel->setText(QStringLiteral("启动受阻：") +
                          startup.error().message);
  layout->addWidget(m_stateLabel);
  auto *authentication = new QHBoxLayout;
  m_sessionLabel->setObjectName("sessionState");
  m_sessionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_loginButton->setObjectName("loginButton");
  m_registerButton->setObjectName("registerButton");
  m_logoutButton->setObjectName("logoutButton");
  authentication->addWidget(m_sessionLabel, 1);
  authentication->addWidget(m_registerButton);
  authentication->addWidget(m_loginButton);
  authentication->addWidget(m_logoutButton);
  layout->addLayout(authentication);

  m_roleContent = new QSplitter(this);
  m_roleContent->setObjectName("roleContent");
  m_navigation = new QListWidget(m_roleContent);
  m_navigation->setObjectName("roleNavigation");
  m_navigation->setMaximumWidth(180);
  m_pages = new QStackedWidget(m_roleContent);
  m_pages->setObjectName("rolePages");
  const QVector<Role> roles{Role::Customer, Role::Merchant, Role::Rider,
                            Role::Admin};
  const QStringList scopes{
      QStringLiteral("W05：店铺、菜品、购物车；W06：本人订单与确认收货。"),
      QStringLiteral("账号与店铺已原子注册；W05：菜品管理；W06：订单处理。"),
      QStringLiteral("W06：配送池、认领和标记送达。"),
      QStringLiteral("W07：账号管理与统计；W08：备份恢复。")};
  for (qsizetype i = 0; i < roles.size(); ++i) {
    m_navigation->addItem(roleLabel(roles.at(i)));
    auto *page = new QWidget(m_pages);
    auto *body = new QVBoxLayout(page);
    auto *title =
        new QLabel(roleLabel(roles.at(i)) + QStringLiteral("模块"), page);
    title->setObjectName("pageTitle");
    body->addWidget(title);
    auto *scope = new QLabel(scopes.at(i), page);
    scope->setWordWrap(true);
    body->addWidget(scope);
    if (roles.at(i) != Role::Admin) {
      auto *search = new QLineEdit(page);
      search->setPlaceholderText(
          QStringLiteral("订单号筛选（仅影响视图，不触发保存）"));
      body->addWidget(search);
      auto *proxy = new OrderFilterProxyModel(page);
      proxy->setSourceModel(m_orders);
      connect(search, &QLineEdit::textChanged, proxy,
              [proxy](const QString &text) {
                OrderFilter filter;
                filter.keyword = text;
                proxy->setFilter(filter);
              });
      auto *table = new QTableView(page);
      table->setModel(proxy);
      table->setItemDelegateForColumn(OrderTableModel::Total,
                                      new MoneyDelegate(table));
      table->setItemDelegateForColumn(OrderTableModel::Status,
                                      new OrderStatusDelegate(table));
      table->setSelectionBehavior(QAbstractItemView::SelectRows);
      table->setEditTriggers(QAbstractItemView::NoEditTriggers);
      table->setSortingEnabled(true);
      table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
      body->addWidget(table, 1);
      body->addWidget(
          new QLabel(QStringLiteral("当前工作包尚未建立订单业务数据。"), page));
    } else {
      body->addWidget(new QLabel(
          QStringLiteral(
              "管理员账号已通过首次初始化建立；账号管理将在 W07 实现。"),
          page));
      body->addStretch();
    }
    m_pages->addWidget(page);
  }
  m_roleContent->setStretchFactor(1, 1);
  layout->addWidget(m_roleContent, 1);
  m_loggedOutFiller->setSizePolicy(QSizePolicy::Preferred,
                                   QSizePolicy::Expanding);
  layout->addWidget(m_loggedOutFiller, 1);
  connect(m_navigation, &QListWidget::currentRowChanged, m_pages,
          &QStackedWidget::setCurrentIndex);
  m_navigation->setEnabled(false);
  connect(m_loginButton, &QPushButton::clicked, this, &MainWindow::openLogin);
  connect(m_registerButton, &QPushButton::clicked, this,
          &MainWindow::openRegistration);
  connect(m_logoutButton, &QPushButton::clicked, this, [this] {
    const auto result = m_context.auth().logout();
    Q_ASSERT(result.ok());
  });

  auto reload = [this] {
    m_orders->replaceProjection({});
    const auto result = m_context.orderQuery().visibleOrders();
    if (result.ok())
      m_orders->replaceProjection(result.value());
    refreshAuthenticationUi();
  };
  connect(&context.session(), &SessionContext::changed, this, reload);
  connect(&context.store(), &DataStore::committed, this,
          [reload](qint64) { reload(); });
  reload();
  statusBar()->showMessage(QStringLiteral("数据目录：") +
                           context.paths().directory);
}

void MainWindow::refreshAuthenticationUi() {
  using namespace takeout;
  if (!m_startupState) {
    m_sessionLabel->setText(QStringLiteral("认证不可用"));
    m_loginButton->setEnabled(false);
    m_registerButton->setEnabled(false);
    m_logoutButton->setEnabled(false);
    m_roleContent->setVisible(false);
    m_loggedOutFiller->setVisible(true);
    return;
  }
  const auto session = m_context.session().current();
  if (*m_startupState == StartupState::NeedsAdminBootstrap) {
    m_stateLabel->setText(
        QStringLiteral("首次运行：请先创建唯一的初始管理员账号。"));
    m_sessionLabel->setText(QStringLiteral("尚未初始化管理员"));
    m_registerButton->setText(QStringLiteral("初始化管理员"));
    m_registerButton->setEnabled(true);
    m_loginButton->setEnabled(false);
    m_logoutButton->setEnabled(false);
    m_roleContent->setVisible(false);
    m_loggedOutFiller->setVisible(true);
    return;
  }
  m_stateLabel->setText(QStringLiteral("数据层：Ready"));
  m_registerButton->setText(QStringLiteral("注册用户/商家/骑手"));
  m_loginButton->setEnabled(!session.has_value());
  m_registerButton->setEnabled(!session.has_value());
  m_logoutButton->setEnabled(session.has_value());
  m_roleContent->setVisible(session.has_value());
  m_loggedOutFiller->setVisible(!session.has_value());
  if (!session) {
    m_sessionLabel->setText(QStringLiteral("尚未登录"));
    m_navigation->setCurrentRow(-1);
    return;
  }
  m_sessionLabel->setText(
      QStringLiteral("当前账号：%1（%2）")
          .arg(session->displayName, roleLabel(session->role)));
  m_navigation->setCurrentRow(int(session->role));
  m_pages->setCurrentIndex(int(session->role));
}

void MainWindow::openLogin() {
  using namespace takeout;
  if (!m_startupState || *m_startupState != StartupState::Ready)
    return;
  LoginDialog dialog(this);
  for (;;) {
    const int result = dialog.exec();
    if (result == LoginDialog::RegisterRequested) {
      openRegistration();
      return;
    }
    if (result != QDialog::Accepted)
      return;
    const auto login = m_context.auth().login(dialog.loginName(),
                                              dialog.password(), dialog.role());
    if (login.ok())
      return;
    dialog.setError(login.error().message);
  }
}

void MainWindow::openRegistration() {
  using namespace takeout;
  if (!m_startupState || m_context.session().current())
    return;
  const bool bootstrap = *m_startupState == StartupState::NeedsAdminBootstrap;
  RegisterDialog dialog(bootstrap ? RegisterDialog::Mode::BootstrapAdmin
                                  : RegisterDialog::Mode::RegisterAccount,
                        this);
  for (;;) {
    if (dialog.exec() != QDialog::Accepted)
      return;
    Result<void> result =
        bootstrap ? m_context.auth().bootstrapAdmin(dialog.adminRequest())
        : dialog.accountRequest().role == Role::Merchant
            ? m_context.catalog().createMerchantWithShop(
                  dialog.merchantRequest())
            : m_context.auth().registerAccount(dialog.accountRequest());
    if (!result.ok()) {
      dialog.setError(result.error().message);
      continue;
    }
    if (bootstrap)
      m_startupState = StartupState::Ready;
    refreshAuthenticationUi();
    return;
  }
}

MainWindow::~MainWindow() { delete ui; }
