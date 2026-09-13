#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QTabWidget>
#include <QTimer>
#include "app/appcontext.h"
#include "app/theme.h"
#include "mainwindow.h"
#include "models/ordertablemodel.h"
#include "models/orderfilterproxymodel.h"
#include "models/accountmodel.h"
#include "delegates/moneydelegate.h"
#include "delegates/orderstatusdelegate.h"
#include "dialogs/logindialog.h"
#include "dialogs/passwordjobcoordinator.h"
#include "dialogs/registerdialog.h"
#include <limits>

using namespace takeout;
namespace {
class MemoryRepository final : public Repository {
public:
    StoreSnapshot disk;
    bool failSave = false;
    int writes = 0;
    Result<StoreSnapshot> load() const override { return Result<StoreSnapshot>::success(disk); }
    Result<void> save(const StoreSnapshot& next) override {
        ++writes;
        if (failSave) return Result<void>::failure({ErrorCode::Persistence, "injected", {}});
        disk = next;
        return Result<void>::success();
    }
};
class TestService final : public ServiceBase {
public:
    using ServiceBase::ServiceBase;
    Result<void> submit(StoreSnapshot candidate) { return commit(std::move(candidate)); }
};
void writeFile(const QString& path, const QByteArray& content) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), content.size());
}
}
class ArchitectureTest final : public QObject {
    Q_OBJECT
private slots:
    void resultContracts() {
        const auto value = Result<int>::success(42);
        QVERIFY(value.ok()); QCOMPARE(value.value(), 42);
        QVERIFY(Result<void>::success().ok());
        const auto error = Result<void>::failure({ErrorCode::Validation, "invalid", "price"});
        QVERIFY(!error.ok()); QCOMPARE(error.error().field, QString("price"));
    }
    void statusMatrix() {
        const OrderStatus states[] = {OrderStatus::PendingPayment, OrderStatus::Cancelled,
            OrderStatus::PendingAcceptance, OrderStatus::Preparing, OrderStatus::ReadyForDelivery,
            OrderStatus::Delivering, OrderStatus::Completed};
        const PaymentStatus payments[] = {PaymentStatus::Unpaid, PaymentStatus::Paid, PaymentStatus::Refunded};
        const bool expected[7][3] = {{true,false,false},{true,false,true},{false,true,false},
            {false,true,false},{false,true,false},{false,true,false},{false,true,false}};
        for (int i=0; i<7; ++i) for (int j=0; j<3; ++j)
            QCOMPARE(isLegalCombination(states[i], payments[j]), expected[i][j]);
    }
    void persistencePublishesOnlyAfterSuccess() {
        MemoryRepository repository;
        DataStore store(repository);
        SessionContext session;
        TestService service(store, session);
        QVERIFY(store.initialize().ok());
        QSignalSpy changes(&store, &DataStore::committed);
        auto candidate = store.snapshot();
        repository.failSave = true;
        QVERIFY(!service.submit(candidate).ok());
        QCOMPARE(store.snapshot().revision, 0);
        QCOMPARE(changes.count(), 0);
        repository.failSave = false;
        QVERIFY(service.submit(candidate).ok());
        QCOMPARE(store.snapshot().revision, 1);
        QCOMPARE(repository.disk.revision, 1);
        QCOMPARE(changes.count(), 1);
        QVERIFY(!service.submit(candidate).ok()); // Stale candidate must not overwrite revision 1.
        QCOMPARE(repository.writes, 2);
        auto detached = store.snapshot();
        detached.revision = 500;
        QCOMPARE(store.snapshot().revision, 1);
    }
    void bootstrapSeparation() {
        MemoryRepository repository;
        DataStore store(repository);
        const auto start = store.initialize();
        QVERIFY(start.ok()); QCOMPARE(start.value(), StartupState::NeedsAdminBootstrap);
        QCOMPARE(repository.writes, 0);
        SessionContext session;
        AuthService auth(store, session);
        const auto bootstrap = auth.bootstrapAdmin({"admin", "Demo1234!", "Administrator"});
        QVERIFY(bootstrap.ok()); QCOMPARE(repository.writes, 1);
        QCOMPARE(store.snapshot().accounts.size(), 1); QVERIFY(!session.current());
        RegisterRequest registration; registration.role = Role::Admin;
        const auto rejected = auth.registerAccount(registration);
        QVERIFY(!rejected.ok()); QCOMPARE(rejected.error().code, ErrorCode::Forbidden);
        QVERIFY(auth.login("admin", "Demo1234!", Role::Admin).ok());
        QVERIFY(session.current()); QCOMPARE(session.current()->role, Role::Admin);
        QVERIFY(auth.logout().ok()); QVERIFY(!session.current());
    }
    void existingAdminRejectsBootstrap() {
        MemoryRepository repository;
        Account admin; admin.id = "admin-fixture"; admin.role = Role::Admin;
        repository.disk.accounts.push_back(admin);
        DataStore store(repository);
        const auto start = store.initialize();
        QVERIFY(start.ok()); QCOMPARE(start.value(), StartupState::Ready);
        SessionContext session; AuthService auth(store, session);
        auto attempt = auth.bootstrapAdmin({});
        QVERIFY(!attempt.ok()); QCOMPARE(attempt.error().code, ErrorCode::Forbidden);
        QCOMPARE(repository.writes, 0);
    }
    void unauthenticatedServicesFailClosed() {
        MemoryRepository repository; DataStore store(repository);
        QVERIFY(store.initialize().ok());
        SessionContext session;
        CatalogService catalog(store, session);
        OrderService orders(store, session);
        OrderQueryService query(store, session);
        AdminService admin(store, session);
        StatisticsService stats(store, session);
        QVERIFY(!catalog.createMerchantWithShop({}).ok());
        QCOMPARE(catalog.deleteDish("other-id").error().code, ErrorCode::Forbidden);
        QVERIFY(!orders.updateCart({}).ok());
        QVERIFY(!orders.createOrder({}).ok());
        QCOMPARE(orders.execute("other-id", OrderAction::ConfirmReceipt).error().code, ErrorCode::Forbidden);
        QVERIFY(!query.visibleOrders().ok());
        QVERIFY(!admin.deleteAccount("other-id").ok());
        QVERIFY(!stats.roleSummary({}).ok());
        QCOMPARE(stats.adminSummary({}).error().code, ErrorCode::Forbidden);
        QCOMPARE(repository.writes, 0);
    }
    void emptyAndNonemptyJsonRoundTrip() {
        QTemporaryDir temp;
        const auto path = temp.filePath("appdata.json");
        JsonRepository repository(path);
        QVERIFY(repository.load().ok());
        QVERIFY(!QFile::exists(path)); // Reading first-run state does not create credentials or files.
        StoreSnapshot empty; empty.savedAt = QDateTime::currentDateTimeUtc();
        QVERIFY(repository.save(empty).ok());
        auto restored = repository.load();
        QVERIFY(restored.ok()); QVERIFY(restored.value().isEmpty());
        QCOMPARE(restored.value().savedAt, empty.savedAt);
        QFile before(path); QVERIFY(before.open(QIODevice::ReadOnly));
        const auto bytes = before.readAll(); before.close();
        StoreSnapshot nonempty = empty; nonempty.revision = 1;
        Account admin; admin.id="11111111-1111-4111-8111-111111111111";
        admin.loginName="admin"; admin.displayName="管理员"; admin.role=Role::Admin;
        admin.passwordSalt=QByteArray(16,'s'); admin.passwordHash=QByteArray(32,'h');
        admin.passwordIterations=600000; admin.passwordAlgorithm="PBKDF2-HMAC-SHA256";
        admin.createdAt=empty.savedAt; nonempty.accounts.push_back(admin);
        auto save = repository.save(nonempty);
        QVERIFY(save.ok()); QVERIFY(repository.load().ok());
        QFile current(path); QVERIFY(current.open(QIODevice::ReadOnly)); auto altered=current.readAll(); current.close();
        altered.replace("\"accounts\": [", "\"accounts\": [{\"id\":\"existing\"}");
        writeFile(path, altered);
        auto loaded = repository.load();
        QVERIFY(!loaded.ok()); QCOMPARE(loaded.error().code, ErrorCode::RecoveryAvailable);
        QVERIFY(!repository.save(empty).ok());
        QVERIFY(before.open(QIODevice::ReadOnly)); QCOMPARE(before.readAll(), altered);
    }
    void corruptAndBackupAreNotEmptyDatabases() {
        QTemporaryDir temp; const auto path = temp.filePath("appdata.json");
        JsonRepository repository(path);
        writeFile(path, "{}");
        QCOMPARE(repository.load().error().code, ErrorCode::CorruptData);
        StoreSnapshot empty; empty.savedAt = QDateTime::currentDateTimeUtc();
        QVERIFY(!repository.save(empty).ok());
        QVERIFY(QFile::remove(path));
        writeFile(path + ".bak", "{}");
        QVERIFY(!repository.load().ok());
        QVERIFY(!QFile::exists(path));
    }
    void projectionSortingAndIdentity() {
        OrderTableModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto now = QDateTime::currentDateTimeUtc();
        model.replaceProjection({{"id-100", "A", OrderStatus::Completed, 10000, now},
            {"id-2", "B", OrderStatus::Delivering, 200, now},
            {"id-10", "C", OrderStatus::Cancelled, 1000, now}});
        OrderFilterProxyModel proxy; proxy.setSourceModel(&model);
        proxy.sort(OrderTableModel::Total);
        QCOMPARE(proxy.index(0, 0).data(OrderTableModel::IdRole).toString(), QString("id-2"));
        QCOMPARE(proxy.index(1, 0).data(OrderTableModel::IdRole).toString(), QString("id-10"));
        OrderFilter filter; filter.status = OrderStatus::Completed; proxy.setFilter(filter);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.mapToSource(proxy.index(0, 0)).data(OrderTableModel::IdRole).toString(), QString("id-100"));
        filter = {}; filter.keyword = "B"; proxy.setFilter(filter);
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, 0).data(OrderTableModel::IdRole).toString(), QString("id-2"));
        filter = {}; filter.until = now; proxy.setFilter(filter); QCOMPARE(proxy.rowCount(), 0);
        model.replaceProjection({}); QCOMPARE(model.rowCount(), 0);
    }
    void accountProjectionExcludesCredentials() {
        AccountModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto now = QDateTime::currentDateTimeUtc();
        model.replaceProjection({{"id-a", "alice", "Alice", Role::Customer, false, now},
                                 {"id-b", "bob", "Bob", Role::Merchant, true, now}});
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.index(0, AccountModel::LoginName).data().toString(), QString("alice"));
        QCOMPARE(model.index(0, AccountModel::RoleColumn).data().toString(), QString("普通用户"));
        QCOMPARE(model.index(1, AccountModel::Status).data().toString(), QString("已删除"));
        QCOMPARE(model.index(1, 0).data(AccountModel::IdRole).toString(), QString("id-b"));
    }
    void moneyUsesIntegerCents() {
        MoneyDelegate delegate;
        QCOMPARE(delegate.displayText(QVariant::fromValue(qint64(2971)), QLocale()), QStringLiteral("￥29.71"));
        QCOMPARE(delegate.displayText(QVariant::fromValue(std::numeric_limits<qint64>::min()), QLocale()),
                 QStringLiteral("-￥92233720368547758.08"));
    }
    void authenticationDialogsExposeOnlyLegalRegistrationRoles() {
        LoginDialog login;
        auto* loginRole=login.findChild<QComboBox*>("loginRole");
        auto* password=login.findChild<QLineEdit*>("loginPassword");
        auto* registration=login.findChild<QPushButton*>("openRegistration");
        QVERIFY(loginRole); QVERIFY(password); QVERIFY(registration);
        QCOMPARE(password->echoMode(),QLineEdit::Password);
        password->setText("temporary");
        login.clearPassword();
        QVERIFY(password->text().isEmpty());
        QCOMPARE(loginRole->count(),4);
        loginRole->setCurrentIndex(3); QVERIFY(!registration->isEnabled());
        RegisterDialog normal(RegisterDialog::Mode::RegisterAccount);
        auto* role=normal.findChild<QComboBox*>("registrationRole");
        auto* address=normal.findChild<QLineEdit*>("registrationAddress");
        auto* shopName=normal.findChild<QLineEdit*>("registrationShopName");
        auto* registrationPassword=normal.findChild<QLineEdit*>("registrationPassword");
        auto* passwordConfirm=normal.findChild<QLineEdit*>("registrationPasswordConfirm");
        QVERIFY(role); QVERIFY(address); QVERIFY(shopName); QVERIFY(registrationPassword);
        QVERIFY(passwordConfirm); QCOMPARE(role->count(),3);
        registrationPassword->setText("temporary");
        passwordConfirm->setText("temporary");
        normal.clearPassword();
        QVERIFY(registrationPassword->text().isEmpty());
        QVERIFY(passwordConfirm->text().isEmpty());
        role->setCurrentIndex(1); QVERIFY(!address->isHidden()); QVERIFY(!shopName->isHidden());
        role->setCurrentIndex(2); QVERIFY(address->isHidden()); QVERIFY(shopName->isHidden());
        for(int i=0;i<role->count();++i) QVERIFY(Role(role->itemData(i).toInt())!=Role::Admin);
        RegisterDialog bootstrap(RegisterDialog::Mode::BootstrapAdmin);
        auto* bootstrapRole=bootstrap.findChild<QComboBox*>("registrationRole");
        QVERIFY(bootstrapRole); QCOMPARE(bootstrapRole->count(),1);
        QCOMPARE(Role(bootstrapRole->currentData().toInt()),Role::Admin);
        QVERIFY(!bootstrapRole->isEnabled());
    }
    void passwordDigestRunsAsynchronouslyAndStaleResultIsDropped() {
        PasswordJobCoordinator coordinator;
        bool completed = false;
        QByteArray digest;
        coordinator.start("Async!234", QByteArray(16, 's'),
                          Credentials::Iterations,
                          [&](Result<QByteArray> result) {
                              completed = true;
                              if (result.ok())
                                  digest = result.value();
                          });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 10000);
        QCOMPARE(digest.size(), Credentials::HashBytes);

        bool staleCalled = false;
        coordinator.start("ignored", QByteArray(1, 'x'), 1,
                          [&](Result<QByteArray>) { staleCalled = true; });
        coordinator.invalidate();
        QTest::qWait(100);
        QVERIFY(!staleCalled);
    }
    void passwordDigestKeepsGuiThreadResponsive() {
        // Baseline: the same derivation executed on this thread. If the
        // coordinator ran PBKDF2 on the GUI thread, the heartbeat below could
        // not tick for about this long.
        QElapsedTimer direct;
        direct.start();
        const auto directResult =
            Credentials::derivePbkdf2("Gui!2345", QByteArray(16, 's'),
                                      Credentials::Iterations);
        const auto digestMs = direct.elapsed();
        QVERIFY(directResult.ok());
        if (digestMs < 100)
            QSKIP("PBKDF2 too fast on this machine to separate the two cases");

        PasswordJobCoordinator coordinator;
        qint64 maxGapMs = 0;
        bool completed = false;
        QElapsedTimer gap;
        QTimer heartbeat;
        heartbeat.setInterval(5);
        connect(&heartbeat, &QTimer::timeout,
                [&gap, &maxGapMs] { maxGapMs = qMax(maxGapMs, gap.restart()); });
        gap.start();
        heartbeat.start();
        coordinator.start("Gui!2345", QByteArray(16, 's'),
                          Credentials::Iterations,
                          [&](Result<QByteArray>) { completed = true; });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 20000);
        heartbeat.stop();

        qInfo().noquote()
            << QStringLiteral("GUI-RESPONSIVE digestMs=%1 maxEventLoopGapMs=%2")
                   .arg(digestMs)
                   .arg(maxGapMs);
        // The GUI thread stalled for well under one whole derivation.
        QVERIFY2(maxGapMs < digestMs / 2,
                 qPrintable(QStringLiteral("maxGapMs=%1 digestMs=%2")
                                .arg(maxGapMs)
                                .arg(digestMs)));
    }
    void shellNavigationDoesNotAuthenticate() {
        applyApplicationTheme(*qApp);
        QCOMPARE(qApp->palette().color(QPalette::WindowText), QColor("#182230"));
        QCOMPARE(qApp->palette().color(QPalette::Base), QColor("#ffffff"));
        QCOMPARE(qApp->palette().color(QPalette::HighlightedText), QColor("#ffffff"));
        QVERIFY(qApp->styleSheet().contains("QTableView::item:selected"));
        QVERIFY(qApp->styleSheet().contains("QPushButton:disabled"));
        QTemporaryDir temp;
        AppContext context(AppPaths::resolve(temp.path()));
        auto startup = context.initialize(); QVERIFY(startup.ok());
        MainWindow window(context, startup); window.show();
        auto* loginButton=window.findChild<QPushButton*>("loginButton");
        auto* registerButton=window.findChild<QPushButton*>("registerButton");
        auto* roleContent=window.findChild<QSplitter*>("roleContent");
        QVERIFY(loginButton); QVERIFY(registerButton); QVERIFY(roleContent);
        QVERIFY(!loginButton->isEnabled()); QVERIFY(registerButton->isEnabled());
        QVERIFY(roleContent->isHidden());
        auto* navigation = window.findChild<QListWidget*>("roleNavigation");
        auto* pages = window.findChild<QStackedWidget*>("rolePages");
        QVERIFY(navigation); QVERIFY(pages); QCOMPARE(pages->count(), 4);
        for (int i=0; i<4; ++i) {
            navigation->setCurrentRow(i); QCOMPARE(pages->currentIndex(), i);
            QVERIFY(!context.session().current());
        }
        const auto tables = window.findChildren<QTableView*>();
        QCOMPARE(tables.size(), 8);
        for (auto* table : tables) {
            QCOMPARE(table->model()->rowCount(), 0);
        }
        navigation->setCurrentRow(0);
        QCoreApplication::processEvents();
        const auto screenshot = qEnvironmentVariable("TAKEOUT_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot));
        AppContext second(AppPaths::resolve(temp.path()));
        QVERIFY(!second.initialize().ok());
        QVERIFY(!QFile::exists(context.paths().dataFile()));
    }
    void merchantPageCommonSizeSmoke() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        AppContext context(AppPaths::resolve(temp.path()));
        QVERIFY(context.initialize().ok());
        QVERIFY(context.auth().bootstrapAdmin(
            {"admin", "Admin!234", QStringLiteral("管理员")}).ok());
        QVERIFY(context.catalog().createMerchantWithShop(
            {"emptymerchant", "Merchant!2", QStringLiteral("空店商家"),
             QStringLiteral("空店铺"), QStringLiteral("暂无菜品"),
             QStringLiteral("测试地址")}).ok());
        QVERIFY(context.catalog().createMerchantWithShop(
            {"merchant", "Merchant!1", QStringLiteral("商家甲"),
             QStringLiteral("测试店铺"),
             QStringLiteral("用于商家页面布局验收的较长店铺简介，内容应保持可读且不能挤压表格。"),
             QStringLiteral("南京市测试区长地址一号楼二单元三层")}).ok());
        QVERIFY(context.auth().login("merchant", "Merchant!1",
                                     Role::Merchant).ok());
        QVERIFY(context.catalog().updateShop(
            {QStringLiteral("测试店铺"),
             QStringLiteral("用于商家页面布局验收的较长店铺简介，内容应保持可读且不能挤压表格。"),
             QStringLiteral("南京市测试区长地址一号楼二单元三层"), true}).ok());
        QVector<Id> dishIds;
        for (int i = 0; i < 6; ++i) {
            const auto dish = context.catalog().createDish(
                {QStringLiteral("测试菜品%1").arg(i + 1), 1000 + i * 100, true});
            QVERIFY(dish.ok());
            dishIds.push_back(dish.value());
        }
        QVERIFY(context.auth().logout().ok());
        QVERIFY(context.auth().registerAccount(
            {"customer", "Customer!1", QStringLiteral("长姓名测试顾客"),
             QStringLiteral("南京市测试区用于检查长地址换行的街道一百二十三号五栋六单元七层"),
             Role::Customer}).ok());
        QVERIFY(context.auth().registerAccount(
            {"rider", "Rider!123", QStringLiteral("骑手甲"), {},
             Role::Rider}).ok());
        QVERIFY(context.auth().login("customer", "Customer!1",
                                     Role::Customer).ok());
        Id shopId;
        for (const auto &shop : context.store().snapshot().shops)
            if (shop.name == QStringLiteral("测试店铺")) {
                shopId = shop.id;
                break;
            }
        QVERIFY(!shopId.isEmpty());
        for (int i = 0; i < 4; ++i) {
            QVERIFY(context.orders().updateCart(
                {shopId, {{dishIds.at(i), i + 1}}}).ok());
            const auto order = context.orders().createOrder({});
            QVERIFY(order.ok());
            QVERIFY(context.orders().execute(order.value(),
                                              OrderAction::Pay).ok());
        }
        QVERIFY(context.auth().logout().ok());
        QVERIFY(context.auth().login("merchant", "Merchant!1",
                                     Role::Merchant).ok());

        MainWindow window(
            context, Result<StartupState>::success(StartupState::Ready));
        auto *navigation = window.findChild<QListWidget *>("roleNavigation");
        auto *sections = window.findChild<QTabWidget *>("merchantSections");
        auto *catalogScroll =
            window.findChild<QScrollArea *>("merchantCatalogScroll");
        auto *display = window.findChild<QLineEdit *>("merchantDisplayName");
        auto *shopName = window.findChild<QLineEdit *>("merchantShopName");
        auto *dishTable = window.findChild<QTableView *>("merchantDishTable");
        auto *orderTable = window.findChild<QTableView *>("merchantOrderTable");
        auto *statistics = window.findChild<QLabel *>("merchantStatistics");
        auto *businessStatus =
            window.findChild<QLabel *>("merchantBusinessStatus");
        auto *orderStatus = window.findChild<QLabel *>("merchantOrderStatus");
        const QStringList actionNames{
            "merchantCreateDish", "merchantUpdateDish", "merchantDeleteDish",
            "merchantAcceptOrder", "merchantRejectOrder",
            "merchantReadyOrder"};
        QVector<QPushButton *> actions;
        for (const auto &name : actionNames)
            actions.push_back(window.findChild<QPushButton *>(name));
        QVERIFY(navigation);
        QVERIFY(sections);
        QVERIFY(catalogScroll);
        QVERIFY(display);
        QVERIFY(shopName);
        QVERIFY(dishTable);
        QVERIFY(orderTable);
        QVERIFY(statistics);
        QVERIFY(businessStatus);
        QVERIFY(orderStatus);
        for (auto *action : actions)
            QVERIFY(action);
        QCOMPARE(sections->count(), 2);
        navigation->setCurrentRow(int(Role::Merchant));
        const auto firstOrder = orderTable->model()->index(0, 0);
        QVERIFY(firstOrder.isValid());
        QVERIFY(QMetaObject::invokeMethod(orderTable, "clicked",
                                          Qt::DirectConnection,
                                          Q_ARG(QModelIndex, firstOrder)));
        businessStatus->setText(QStringLiteral(
            "保存失败：这是一条用于验证长错误信息换行且不撑宽页面的状态消息。"));
        orderStatus->setText(QStringLiteral(
            "操作失败：订单状态已经变化，请刷新后重新选择并提交。"));

        const auto outputDirectory =
            qEnvironmentVariable("MERCHANT_UI_SCREENSHOT_DIR");
        if (!outputDirectory.isEmpty())
            QVERIFY(QDir().mkpath(outputDirectory));
        const QList<QSize> sizes{{960, 640}, {1200, 800}, {1440, 900}};
        for (const auto &size : sizes) {
            window.resize(size);
            window.show();
            QTest::qWait(50);
            sections->setCurrentIndex(0);
            catalogScroll->ensureWidgetVisible(dishTable);
            QCoreApplication::processEvents();
            const QRect dishInViewport(
                dishTable->mapTo(catalogScroll->viewport(), QPoint()),
                dishTable->size());
            QVERIFY(dishTable->width() > 200);
            QVERIFY(dishTable->height() > 100);
            QVERIFY(dishInViewport.intersects(
                catalogScroll->viewport()->rect()));
            for (int i = 0; i < 3; ++i)
                QVERIFY(actions.at(i)->width() > 0);
            if (!outputDirectory.isEmpty()) {
                const auto path = QDir(outputDirectory).filePath(
                    QStringLiteral("merchant-after-%1x%2-catalog.png")
                        .arg(size.width()).arg(size.height()));
                QVERIFY(window.grab().save(path));
            }

            sections->setCurrentIndex(1);
            QCoreApplication::processEvents();
            QVERIFY(orderTable->width() > 200);
            QVERIFY(orderTable->height() > 100);
            QVERIFY(statistics->width() > 0);
            for (int i = 3; i < actions.size(); ++i)
                QVERIFY(actions.at(i)->width() > 0);
            qInfo().noquote()
                << QStringLiteral("MERCHANT-UI %1x%2 dish=%3x%4 order=%5x%6")
                       .arg(size.width()).arg(size.height())
                       .arg(dishTable->width()).arg(dishTable->height())
                       .arg(orderTable->width()).arg(orderTable->height());
            if (!outputDirectory.isEmpty()) {
                const auto path = QDir(outputDirectory).filePath(
                    QStringLiteral("merchant-after-%1x%2-orders.png")
                        .arg(size.width()).arg(size.height()));
                QVERIFY(window.grab().save(path));
            }
        }
        for (int width = 960; width <= 1440; width += 40) {
            const int height = 640 + (width - 960) * 260 / 480;
            window.resize(width, height);
            sections->setCurrentIndex(0);
            catalogScroll->ensureWidgetVisible(dishTable);
            QCoreApplication::processEvents();
            QVERIFY(dishTable->height() > 100);
            sections->setCurrentIndex(1);
            QCoreApplication::processEvents();
            QVERIFY(orderTable->height() > 100);
        }

        if (!outputDirectory.isEmpty()) {
            window.hide();
            const auto captureRole = [&](const char *login,
                                         const char *password, Role role,
                                         const QString &name) {
                QVERIFY(context.auth().logout().ok());
                QVERIFY(context.auth().login(login, password, role).ok());
                MainWindow roleWindow(
                    context,
                    Result<StartupState>::success(StartupState::Ready));
                roleWindow.resize(960, 640);
                roleWindow.show();
                QTest::qWait(50);
                QVERIFY(roleWindow.grab().save(
                    QDir(outputDirectory).filePath(name)));
            };
            captureRole("customer", "Customer!1", Role::Customer,
                        QStringLiteral("quick-customer-960x640.png"));
            captureRole("rider", "Rider!123", Role::Rider,
                        QStringLiteral("quick-rider-960x640.png"));
            captureRole("admin", "Admin!234", Role::Admin,
                        QStringLiteral("quick-admin-960x640.png"));
            captureRole("emptymerchant", "Merchant!2", Role::Merchant,
                        QStringLiteral("merchant-empty-960x640.png"));
        }
    }
};
QTEST_MAIN(ArchitectureTest)
#include "tst_architecture.moc"
