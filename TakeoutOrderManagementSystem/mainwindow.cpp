#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "app/appcontext.h"
#include "models/ordertablemodel.h"
#include "models/orderfilterproxymodel.h"
#include "delegates/moneydelegate.h"
#include "delegates/orderstatusdelegate.h"
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QStatusBar>

MainWindow::MainWindow(takeout::AppContext& context,
                       const takeout::Result<takeout::StartupState>& startup, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_orders(new takeout::OrderTableModel(this))
{
    using namespace takeout;
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("外卖订单管理系统 · W03 持久化基础版 0.4.0"));
    resize(1200, 800);
    setMinimumSize(960, 640);
    auto* layout = new QVBoxLayout(ui->centralwidget);
    auto* heading = new QLabel(QStringLiteral("外卖订单管理系统"), this);
    heading->setObjectName("heading");
    layout->addWidget(heading);
    auto* notice = new QLabel(QStringLiteral("架构预览 · 角色导航不代表登录。当前没有演示订单，也不执行注册、支付或配送。"), this);
    notice->setWordWrap(true);
    notice->setObjectName("notice");
    layout->addWidget(notice);
    auto* state = new QLabel(this);
    state->setTextFormat(Qt::PlainText);
    state->setWordWrap(true);
    state->setText(!startup.ok() ? QStringLiteral("启动受阻：") + startup.error().message
        : startup.value() == StartupState::NeedsAdminBootstrap
            ? QStringLiteral("数据层：NeedsAdminBootstrap。W04 将接入管理员初始化与正常登录。")
            : QStringLiteral("数据层：Ready。认证业务尚待 W04 实现。"));
    layout->addWidget(state);
    auto* split = new QSplitter(this);
    auto* navigation = new QListWidget(split);
    navigation->setObjectName("roleNavigation");
    navigation->setMaximumWidth(180);
    auto* pages = new QStackedWidget(split);
    pages->setObjectName("rolePages");
    const QVector<Role> roles{Role::Customer, Role::Merchant, Role::Rider, Role::Admin};
    const QStringList scopes{
        QStringLiteral("W05：店铺、菜品、购物车；W06：本人订单与确认收货。"),
        QStringLiteral("W04：商家与店铺注册；W05：菜品管理；W06：接单、拒单与出餐。"),
        QStringLiteral("W06：授权配送池、认领、标记送达。送达后仍等待用户确认。"),
        QStringLiteral("W07：账号管理与基础统计；W08：手动备份、恢复。")};
    for (qsizetype i = 0; i < roles.size(); ++i) {
        navigation->addItem(roleLabel(roles.at(i)));
        auto* page = new QWidget(pages);
        auto* body = new QVBoxLayout(page);
        auto* title = new QLabel(roleLabel(roles.at(i)) + QStringLiteral("模块"), page);
        title->setObjectName("pageTitle");
        body->addWidget(title);
        auto* scope = new QLabel(scopes.at(i), page);
        scope->setWordWrap(true);
        body->addWidget(scope);
        auto* boundary = new QLabel(QStringLiteral("页面 → Service 权限校验 → DataStore 候选事务 → JsonRepository\n"
                                                  "授权 DTO → TableModel → ProxyModel → QTableView + Delegate"), page);
        body->addWidget(boundary);
        if (roles.at(i) != Role::Admin) {
            auto* search = new QLineEdit(page);
            search->setPlaceholderText(QStringLiteral("订单号筛选（仅影响视图，不触发保存）"));
            body->addWidget(search);
            auto* proxy = new OrderFilterProxyModel(page);
            proxy->setSourceModel(m_orders);
            connect(search, &QLineEdit::textChanged, proxy, [proxy](const QString& text) {
                OrderFilter filter; filter.keyword = text; proxy->setFilter(filter);
            });
            auto* table = new QTableView(page);
            table->setModel(proxy);
            table->setItemDelegateForColumn(OrderTableModel::Total, new MoneyDelegate(table));
            table->setItemDelegateForColumn(OrderTableModel::Status, new OrderStatusDelegate(table));
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSortingEnabled(true);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            body->addWidget(table, 1);
            body->addWidget(new QLabel(QStringLiteral("尚未登录：不加载任何订单数据。"), page));
        } else {
            body->addWidget(new QLabel(QStringLiteral("账号管理尚未实现。管理员不会通过普通注册入口创建。"), page));
            body->addStretch();
        }
        pages->addWidget(page);
    }
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);
    connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    navigation->setCurrentRow(0);
    // A Session change always clears old projections before querying anew.
    auto reload = [this, &context] {
        m_orders->replaceProjection({});
        const auto result = context.orderQuery().visibleOrders();
        if (result.ok()) m_orders->replaceProjection(result.value());
    };
    connect(&context.session(), &SessionContext::changed, this, reload);
    connect(&context.store(), &DataStore::committed, this, [reload](qint64) { reload(); });
    reload();
    statusBar()->showMessage(QStringLiteral("数据目录：") + context.paths().directory);
}

MainWindow::~MainWindow()
{
    delete ui;
}
