#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "app/appcontext.h"
#include "core/validation.h"
#include "delegates/moneydelegate.h"
#include "delegates/orderstatusdelegate.h"
#include "dialogs/logindialog.h"
#include "dialogs/registerdialog.h"
#include "models/cartmodel.h"
#include "models/dishmodel.h"
#include "models/orderfilterproxymodel.h"
#include "models/ordertablemodel.h"
#include "models/shopmodel.h"
#include "services/catalogservice.h"
#include "services/orderservice.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QSpinBox>
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
      m_shops(new takeout::ShopModel(this)),
      m_dishes(new takeout::DishModel(this)),
      m_cart(new takeout::CartModel(this)),
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
  setWindowTitle(QStringLiteral("外卖订单管理系统 · W05 目录购物车版 0.6.0"));
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
    if (roles.at(i) == Role::Customer) {
      auto *profile = new QGroupBox(QStringLiteral("我的资料"), page);
      auto *form = new QFormLayout(profile);
      auto *display = new QLineEdit(profile);
      display->setObjectName("customerDisplayName");
      auto *address = new QLineEdit(profile);
      address->setObjectName("customerAddress");
      auto *saveProfile = new QPushButton(QStringLiteral("保存资料"), profile);
      auto *message = new QLabel(profile);
      message->setObjectName("customerBusinessStatus");
      form->addRow(QStringLiteral("显示名"), display);
      form->addRow(QStringLiteral("配送地址"), address);
      form->addRow(saveProfile, message);
      body->addWidget(profile);
      auto *shopTable = new QTableView(page);
      shopTable->setObjectName("customerShopTable");
      shopTable->setModel(m_shops);
      shopTable->setSelectionBehavior(QAbstractItemView::SelectRows);
      shopTable->setMaximumHeight(150);
      shopTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
      body->addWidget(new QLabel(QStringLiteral("营业店铺"), page));
      body->addWidget(shopTable);
      auto *dishTable = new QTableView(page);
      dishTable->setObjectName("customerDishTable");
      dishTable->setModel(m_dishes);
      dishTable->setSelectionBehavior(QAbstractItemView::SelectRows);
      dishTable->setItemDelegateForColumn(DishModel::Price,
                                          new MoneyDelegate(dishTable));
      dishTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
      body->addWidget(new QLabel(QStringLiteral("可购买菜品"), page));
      body->addWidget(dishTable, 1);
      auto *cartControls = new QHBoxLayout;
      auto *quantity = new QSpinBox(page);
      quantity->setObjectName("cartQuantity");
      quantity->setRange(1, Validation::MaxItemQuantity);
      auto *addCart = new QPushButton(QStringLiteral("加入/更新购物车"), page);
      auto *clearCart = new QPushButton(QStringLiteral("清空购物车"), page);
      cartControls->addWidget(new QLabel(QStringLiteral("数量"), page));
      cartControls->addWidget(quantity);
      cartControls->addWidget(addCart);
      cartControls->addWidget(clearCart);
      cartControls->addStretch();
      body->addLayout(cartControls);
      auto *cartTable = new QTableView(page);
      cartTable->setObjectName("customerCartTable");
      cartTable->setModel(m_cart);
      cartTable->setSelectionBehavior(QAbstractItemView::SelectRows);
      cartTable->setItemDelegateForColumn(CartModel::UnitPrice,
                                          new MoneyDelegate(cartTable));
      cartTable->setItemDelegateForColumn(CartModel::LineTotal,
                                          new MoneyDelegate(cartTable));
      cartTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
      body->addWidget(new QLabel(QStringLiteral("当前购物车"), page));
      body->addWidget(cartTable, 1);
      auto setMessage = [message](const Result<void> &result) {
        message->setText(result.ok() ? QStringLiteral("已保存")
                                     : result.error().message);
      };
      connect(saveProfile, &QPushButton::clicked, this,
              [this, display, address, setMessage] {
                setMessage(m_context.catalog().updateProfile(
                    {display->text(), address->text()}));
              });
      connect(addCart, &QPushButton::clicked, this,
              [this, dishTable, quantity, setMessage] {
                const auto index = dishTable->currentIndex();
                if (!index.isValid()) {
                  setMessage(Result<void>::failure(
                      {ErrorCode::Validation, QStringLiteral("请先选择菜品"),
                       "dishId"}));
                  return;
                }
                const auto dishId = index.data(DishModel::IdRole).toString();
                const auto shopId =
                    index.data(DishModel::ShopIdRole).toString();
                if (m_cart->rowCount() > 0 && m_cart->shopId() != shopId) {
                  setMessage(Result<void>::failure(
                      {ErrorCode::Conflict,
                       QStringLiteral("购物车只能选择一家店铺"), "shopId"}));
                  return;
                }
                auto items = m_cart->items();
                bool found = false;
                for (auto &item : items)
                  if (item.dishId == dishId) {
                    item.quantity = quantity->value();
                    found = true;
                  }
                if (!found)
                  items.push_back({dishId, quantity->value()});
                setMessage(m_context.orders().updateCart({shopId, items}));
              });
      connect(clearCart, &QPushButton::clicked, this, [this, setMessage] {
        if (m_cart->rowCount() == 0)
          return;
        setMessage(m_context.orders().updateCart({m_cart->shopId(), {}}));
      });
    } else if (roles.at(i) == Role::Merchant) {
      auto *profile = new QGroupBox(QStringLiteral("商家资料"), page);
      auto *profileForm = new QFormLayout(profile);
      auto *display = new QLineEdit(profile);
      display->setObjectName("merchantDisplayName");
      auto *saveProfile = new QPushButton(QStringLiteral("保存资料"), profile);
      profileForm->addRow(QStringLiteral("显示名"), display);
      profileForm->addRow(saveProfile);
      body->addWidget(profile);
      auto *shop = new QGroupBox(QStringLiteral("店铺资料与营业状态"), page);
      auto *shopForm = new QFormLayout(shop);
      auto *shopName = new QLineEdit(shop);
      shopName->setObjectName("merchantShopName");
      auto *shopAddress = new QLineEdit(shop);
      shopAddress->setObjectName("merchantShopAddress");
      auto *shopDescription = new QLineEdit(shop);
      shopDescription->setObjectName("merchantShopDescription");
      auto *shopOpen = new QCheckBox(QStringLiteral("营业中"), shop);
      shopOpen->setObjectName("merchantShopOpen");
      auto *saveShop = new QPushButton(QStringLiteral("保存店铺"), shop);
      auto *message = new QLabel(shop);
      message->setObjectName("merchantBusinessStatus");
      shopForm->addRow(QStringLiteral("名称"), shopName);
      shopForm->addRow(QStringLiteral("地址"), shopAddress);
      shopForm->addRow(QStringLiteral("简介"), shopDescription);
      shopForm->addRow(shopOpen, saveShop);
      shopForm->addRow(message);
      body->addWidget(shop);
      auto *dishFormBox = new QGroupBox(QStringLiteral("菜品管理"), page);
      auto *dishForm = new QFormLayout(dishFormBox);
      auto *dishName = new QLineEdit(dishFormBox);
      dishName->setObjectName("merchantDishName");
      auto *dishPrice = new QSpinBox(dishFormBox);
      dishPrice->setObjectName("merchantDishPrice");
      dishPrice->setRange(0, Validation::MaxDishPriceCents);
      auto *dishAvailable = new QCheckBox(QStringLiteral("上架"), dishFormBox);
      dishAvailable->setObjectName("merchantDishAvailable");
      auto *createDish =
          new QPushButton(QStringLiteral("新增菜品"), dishFormBox);
      auto *updateDish =
          new QPushButton(QStringLiteral("保存选中菜品"), dishFormBox);
      auto *deleteDish =
          new QPushButton(QStringLiteral("删除选中菜品"), dishFormBox);
      dishForm->addRow(QStringLiteral("名称"), dishName);
      dishForm->addRow(QStringLiteral("价格（分）"), dishPrice);
      dishForm->addRow(dishAvailable);
      auto *dishButtons = new QHBoxLayout;
      dishButtons->addWidget(createDish);
      dishButtons->addWidget(updateDish);
      dishButtons->addWidget(deleteDish);
      dishForm->addRow(dishButtons);
      body->addWidget(dishFormBox);
      auto *dishTable = new QTableView(page);
      dishTable->setObjectName("merchantDishTable");
      dishTable->setModel(m_dishes);
      dishTable->setSelectionBehavior(QAbstractItemView::SelectRows);
      dishTable->setItemDelegateForColumn(DishModel::Price,
                                          new MoneyDelegate(dishTable));
      dishTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
      body->addWidget(dishTable, 1);
      auto setMessage = [message](const Result<void> &result) {
        message->setText(result.ok() ? QStringLiteral("已保存")
                                     : result.error().message);
      };
      connect(saveProfile, &QPushButton::clicked, this,
              [this, display, setMessage] {
                setMessage(
                    m_context.catalog().updateProfile({display->text(), {}}));
              });
      connect(
          saveShop, &QPushButton::clicked, this,
          [this, shopName, shopAddress, shopDescription, shopOpen, setMessage] {
            setMessage(m_context.catalog().updateShop(
                {shopName->text(), shopDescription->text(), shopAddress->text(),
                 shopOpen->isChecked()}));
          });
      connect(dishTable, &QTableView::clicked, this,
              [dishName, dishPrice, dishAvailable](const QModelIndex &index) {
                const auto *dish =
                    static_cast<const DishModel *>(index.model())->rowAt(index.row());
                if (!dish)
                  return;
                dishName->setText(dish->name);
                dishPrice->setValue(int(dish->priceCents));
                dishAvailable->setChecked(dish->isAvailable);
              });
      connect(createDish, &QPushButton::clicked, this,
              [this, dishName, dishPrice, dishAvailable, setMessage] {
                const auto result = m_context.catalog().createDish(
                    {dishName->text(), dishPrice->value(),
                     dishAvailable->isChecked()});
                if (result.ok())
                  setMessage(Result<void>::success());
                else
                  setMessage(Result<void>::failure(result.error()));
              });
      connect(
          updateDish, &QPushButton::clicked, this,
          [this, dishTable, dishName, dishPrice, dishAvailable, setMessage] {
            const auto index = dishTable->currentIndex();
            if (!index.isValid()) {
              setMessage(Result<void>::failure({ErrorCode::Validation,
                                                QStringLiteral("请先选择菜品"),
                                                "dishId"}));
              return;
            }
            setMessage(m_context.catalog().updateDish(
                index.data(DishModel::IdRole).toString(),
                {dishName->text(), dishPrice->value(),
                 dishAvailable->isChecked()}));
          });
      connect(deleteDish, &QPushButton::clicked, this,
              [this, dishTable, setMessage] {
                const auto index = dishTable->currentIndex();
                if (!index.isValid()) {
                  setMessage(Result<void>::failure(
                      {ErrorCode::Validation, QStringLiteral("请先选择菜品"),
                       "dishId"}));
                  return;
                }
                setMessage(m_context.catalog().deleteDish(
                    index.data(DishModel::IdRole).toString()));
              });
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
    refreshBusinessModels();
    refreshAuthenticationUi();
  };
  connect(&context.session(), &SessionContext::changed, this, reload);
  connect(&context.store(), &DataStore::committed, this,
          [reload](qint64) { reload(); });
  reload();
  statusBar()->showMessage(QStringLiteral("数据目录：") +
                           context.paths().directory);
}

void MainWindow::refreshBusinessModels() {
  using namespace takeout;
  const auto session = m_context.session().current();
  const auto snapshot = m_context.store().snapshot();
  m_shops->replaceProjection({});
  m_dishes->replaceProjection({});
  m_cart->replaceProjection({}, {});
  if (!session)
    return;
  const Account *current = nullptr;
  for (const auto &account : snapshot.accounts)
    if (account.id == session->accountId) {
      current = &account;
      break;
    }
  if (!current || current->isDeleted)
    return;
  auto setText = [this](const char *name, const QString &value) {
    if (auto *edit = m_pages->findChild<QLineEdit *>(name))
      edit->setText(value);
  };
  if (session->role == Role::Customer) {
    setText("customerDisplayName", current->displayName);
    setText("customerAddress", current->defaultAddress);
    QSet<Id> openShops;
    QVector<ShopRow> shops;
    for (const auto &shop : snapshot.shops) {
      bool merchantActive = false;
      for (const auto &account : snapshot.accounts)
        if (account.id == shop.merchantId && account.role == Role::Merchant &&
            !account.isDeleted)
          merchantActive = true;
      if (!shop.isOpen || !merchantActive)
        continue;
      openShops.insert(shop.id);
      shops.push_back(
          {shop.id, shop.name, shop.description, shop.address, shop.isOpen});
    }
    QVector<DishRow> dishes;
    for (const auto &dish : snapshot.dishes)
      if (openShops.contains(dish.shopId) && !dish.isDeleted &&
          dish.isAvailable)
        dishes.push_back({dish.id, dish.shopId, dish.name, dish.priceCents,
                          dish.isAvailable, dish.isDeleted});
    m_shops->replaceProjection(std::move(shops));
    m_dishes->replaceProjection(std::move(dishes));
    for (const auto &cart : snapshot.carts)
      if (cart.customerId == current->id) {
        QVector<CartRow> rows;
        for (const auto &item : cart.items)
          for (const auto &dish : snapshot.dishes)
            if (dish.id == item.dishId) {
              rows.push_back({dish.id, dish.shopId, dish.name, dish.priceCents,
                              item.quantity, dish.priceCents * item.quantity});
              break;
            }
        m_cart->replaceProjection(cart.shopId, std::move(rows));
        break;
      }
  } else if (session->role == Role::Merchant) {
    setText("merchantDisplayName", current->displayName);
    for (const auto &shop : snapshot.shops)
      if (shop.merchantId == current->id) {
        setText("merchantShopName", shop.name);
        setText("merchantShopAddress", shop.address);
        setText("merchantShopDescription", shop.description);
        if (auto *open = m_pages->findChild<QCheckBox *>("merchantShopOpen"))
          open->setChecked(shop.isOpen);
        QVector<DishRow> dishes;
        for (const auto &dish : snapshot.dishes)
          if (dish.shopId == shop.id)
            dishes.push_back({dish.id, dish.shopId, dish.name, dish.priceCents,
                              dish.isAvailable, dish.isDeleted});
        m_dishes->replaceProjection(std::move(dishes));
        break;
      }
  }
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
