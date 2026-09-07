#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include "app/appcontext.h"
#include "app/theme.h"
#include "mainwindow.h"
#include "models/ordertablemodel.h"
#include "models/orderfilterproxymodel.h"
#include "delegates/moneydelegate.h"
#include "delegates/orderstatusdelegate.h"
#include "dialogs/logindialog.h"
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
        QVERIFY(!stats.summary({}).ok());
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
        filter = {}; filter.until = now; proxy.setFilter(filter); QCOMPARE(proxy.rowCount(), 0);
        model.replaceProjection({}); QCOMPARE(model.rowCount(), 0);
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
        QCOMPARE(loginRole->count(),4);
        loginRole->setCurrentIndex(3); QVERIFY(!registration->isEnabled());
        RegisterDialog normal(RegisterDialog::Mode::RegisterAccount);
        auto* role=normal.findChild<QComboBox*>("registrationRole");
        auto* address=normal.findChild<QLineEdit*>("registrationAddress");
        auto* shopName=normal.findChild<QLineEdit*>("registrationShopName");
        QVERIFY(role); QVERIFY(address); QVERIFY(shopName); QCOMPARE(role->count(),3);
        role->setCurrentIndex(1); QVERIFY(!address->isHidden()); QVERIFY(!shopName->isHidden());
        role->setCurrentIndex(2); QVERIFY(address->isHidden()); QVERIFY(shopName->isHidden());
        for(int i=0;i<role->count();++i) QVERIFY(Role(role->itemData(i).toInt())!=Role::Admin);
        RegisterDialog bootstrap(RegisterDialog::Mode::BootstrapAdmin);
        auto* bootstrapRole=bootstrap.findChild<QComboBox*>("registrationRole");
        QVERIFY(bootstrapRole); QCOMPARE(bootstrapRole->count(),1);
        QCOMPARE(Role(bootstrapRole->currentData().toInt()),Role::Admin);
        QVERIFY(!bootstrapRole->isEnabled());
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
        QCOMPARE(tables.size(), 4);
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
};
QTEST_MAIN(ArchitectureTest)
#include "tst_architecture.moc"
