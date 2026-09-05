#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableView>
#include "app/appcontext.h"
#include "mainwindow.h"
#include "models/ordertablemodel.h"
#include "models/orderfilterproxymodel.h"
#include "delegates/moneydelegate.h"
#include "delegates/orderstatusdelegate.h"
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
        QVERIFY(!bootstrap.ok()); QCOMPARE(bootstrap.error().code, ErrorCode::NotImplemented);
        QVERIFY(store.snapshot().accounts.isEmpty());
        RegisterRequest registration; registration.role = Role::Admin;
        const auto rejected = auth.registerAccount(registration);
        QVERIFY(!rejected.ok()); QCOMPARE(rejected.error().code, ErrorCode::Forbidden);
        QVERIFY(!auth.login("admin", "Demo1234!", Role::Admin).ok());
        QVERIFY(!session.current());
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
    void emptyJsonRoundTripAndNonemptyRefusal() {
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
        StoreSnapshot nonempty = empty; nonempty.accounts.push_back(Account{});
        auto save = repository.save(nonempty);
        QVERIFY(!save.ok()); QCOMPARE(save.error().code, ErrorCode::NotImplemented);
        QVERIFY(before.open(QIODevice::ReadOnly)); QCOMPARE(before.readAll(), bytes); before.close();
        auto altered = bytes; altered.replace("\"accounts\": [", "\"accounts\": [{\"id\":\"existing\"}");
        writeFile(path, altered);
        auto loaded = repository.load();
        QVERIFY(!loaded.ok()); QCOMPARE(loaded.error().code, ErrorCode::NotImplemented);
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
    void shellNavigationDoesNotAuthenticate() {
        QTemporaryDir temp;
        AppContext context(AppPaths::resolve(temp.path()));
        auto startup = context.initialize(); QVERIFY(startup.ok());
        MainWindow window(context, startup); window.show();
        auto* navigation = window.findChild<QListWidget*>("roleNavigation");
        auto* pages = window.findChild<QStackedWidget*>("rolePages");
        QVERIFY(navigation); QVERIFY(pages); QCOMPARE(pages->count(), 4);
        for (int i=0; i<4; ++i) {
            navigation->setCurrentRow(i); QCOMPARE(pages->currentIndex(), i);
            QVERIFY(!context.session().current());
        }
        const auto tables = window.findChildren<QTableView*>();
        QCOMPARE(tables.size(), 3);
        for (auto* table : tables) {
            QCOMPARE(table->model()->rowCount(), 0);
            QVERIFY(dynamic_cast<MoneyDelegate*>(table->itemDelegateForColumn(OrderTableModel::Total)));
            QVERIFY(dynamic_cast<OrderStatusDelegate*>(table->itemDelegateForColumn(OrderTableModel::Status)));
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
