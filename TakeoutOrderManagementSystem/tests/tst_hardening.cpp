#include <QtTest>

#include "app/appcontext.h"
#include "core/credentials.h"
#include "core/orderpolicy.h"
#include "data/atomicfilewriter.h"
#include "data/datastore.h"
#include "data/jsoncodec.h"
#include "data/jsonrepository.h"
#include "models/ordertablemodel.h"
#include "services/adminservice.h"
#include "services/authservice.h"
#include "services/orderqueryservice.h"
#include "services/sessioncontext.h"
#include "services/statisticsservice.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>

using namespace takeout;

namespace {

QDateTime at(int seconds = 0) {
  return QDateTime::fromString("2026-09-01T10:00:00.000Z",
                               Qt::ISODateWithMs)
      .addSecs(seconds);
}

StoreSnapshot adminSnapshot(qint64 revision = 1) {
  StoreSnapshot snapshot;
  snapshot.revision = revision;
  snapshot.savedAt = at(int(revision));
  Account admin;
  admin.id = "44444444-4444-4444-8444-444444444444";
  admin.loginName = "admin";
  admin.displayName = "管理员";
  admin.role = Role::Admin;
  admin.passwordSalt = QByteArray(16, 's');
  admin.passwordHash = QByteArray(32, 'h');
  admin.passwordIterations = Credentials::Iterations;
  admin.passwordAlgorithm = QString::fromLatin1(Credentials::Algorithm);
  admin.createdAt = at();
  snapshot.accounts.push_back(admin);
  return snapshot;
}

void writeFile(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QCOMPARE(file.write(bytes), bytes.size());
}

void writeSnapshot(const QString &path, const StoreSnapshot &snapshot) {
  writeFile(path, QJsonDocument(JsonCodec::encode(snapshot))
                    .toJson(QJsonDocument::Indented));
}

QStringList corruptCopies(const QString &directory) {
  return QDir(directory).entryList({QStringLiteral("appdata.corrupt.*.json")},
                                   QDir::Files, QDir::Name);
}

class InjectedWriter final : public AtomicFileWriter {
public:
  enum class Failure { None, Backup, Primary };

  explicit InjectedWriter(QString primaryPath)
      : primaryPath(std::move(primaryPath)) {}

  mutable Failure failure = Failure::None;

  Result<void> write(const QString &path,
                     const QByteArray &bytes) const override {
    if ((failure == Failure::Backup && path == primaryPath + ".bak") ||
        (failure == Failure::Primary && path == primaryPath)) {
      return Result<void>::failure(
          {ErrorCode::Persistence, QStringLiteral("injected write failure"),
           path});
    }
    QSaveFileWriter writer;
    return writer.write(path, bytes);
  }

private:
  QString primaryPath;
};

class TestService final : public ServiceBase {
public:
  using ServiceBase::ServiceBase;
  Result<void> submit(StoreSnapshot candidate) {
    return commit(std::move(candidate));
  }
};

Account staticAccount(const QString &id, const QString &login,
                      const QString &displayName, Role role,
                      const QString &address = {}) {
  Account account;
  account.id = id;
  account.loginName = login;
  account.displayName = displayName;
  account.role = role;
  account.defaultAddress = address;
  account.passwordSalt = QByteArray(16, 's');
  account.passwordHash = QByteArray(32, 'h');
  account.passwordIterations = Credentials::Iterations;
  account.passwordAlgorithm = QString::fromLatin1(Credentials::Algorithm);
  account.createdAt = at();
  return account;
}

Order mixedOrder(const Account &customer, const Account &merchant,
                 const Account &rider, const Shop &shop, const Dish &dish,
                 int index, int kind) {
  const auto created = at(index);
  Order order;
  order.id = QStringLiteral("10000000-0000-4000-8000-%1")
                 .arg(index, 12, 16, QLatin1Char('0'));
  order.customerId = customer.id;
  order.shopId = shop.id;
  order.items = {{dish.id, dish.name, dish.priceCents, 1, dish.priceCents}};
  order.subtotalCents = dish.priceCents;
  order.deliveryFeeCents = Limits::DefaultDeliveryFeeCents;
  order.totalCents = order.subtotalCents + order.deliveryFeeCents;
  order.customerNameSnapshot = customer.displayName;
  order.addressSnapshot = customer.defaultAddress;
  order.shopNameSnapshot = shop.name;
  order.shopAddressSnapshot = shop.address;
  order.createdAt = created;
  order.updatedAt = created;
  order.history = {{OrderAction::CreateOrder, {}, OrderStatus::PendingPayment,
                    customer.id, created, {}}};
  auto add = [&order](OrderAction action, OrderStatus from, OrderStatus to,
                      const Id &actor, int seconds,
                      const QString &reason = QString{}) {
    const auto timestamp = order.createdAt.addSecs(seconds);
    order.history.push_back({action, from, to, actor, timestamp, reason});
    order.updatedAt = timestamp;
  };
  if (kind >= 1 && kind <= 5) {
    order.status = OrderStatus::PendingAcceptance;
    order.paymentStatus = PaymentStatus::Paid;
    order.paidAt = created.addSecs(1);
    add(OrderAction::Pay, OrderStatus::PendingPayment,
        OrderStatus::PendingAcceptance, customer.id, 1);
  }
  if (kind >= 2 && kind <= 5) {
    order.status = OrderStatus::Preparing;
    order.acceptedAt = created.addSecs(2);
    add(OrderAction::Accept, OrderStatus::PendingAcceptance,
        OrderStatus::Preparing, merchant.id, 2);
  }
  if (kind >= 3 && kind <= 5) {
    order.status = OrderStatus::ReadyForDelivery;
    order.readyAt = created.addSecs(3);
    add(OrderAction::MarkReady, OrderStatus::Preparing,
        OrderStatus::ReadyForDelivery, merchant.id, 3);
  }
  if (kind >= 4 && kind <= 5) {
    order.status = OrderStatus::Delivering;
    order.riderId = rider.id;
    order.riderNameSnapshot = rider.displayName;
    order.claimedAt = created.addSecs(4);
    add(OrderAction::Claim, OrderStatus::ReadyForDelivery,
        OrderStatus::Delivering, rider.id, 4);
  }
  if (kind == 5) {
    order.status = OrderStatus::Delivering;
    order.deliveredAt = created.addSecs(5);
    add(OrderAction::MarkDelivered, OrderStatus::Delivering,
        OrderStatus::Delivering, rider.id, 5);
    order.status = OrderStatus::Completed;
    order.completedAt = created.addSecs(6);
    order.riderIncomeCents = order.deliveryFeeCents;
    add(OrderAction::ConfirmReceipt, OrderStatus::Delivering,
        OrderStatus::Completed, customer.id, 6);
  }
  if (kind == 6) {
    order.status = OrderStatus::Cancelled;
    order.cancelReason = QStringLiteral("用户取消");
    add(OrderAction::Cancel, OrderStatus::PendingPayment,
        OrderStatus::Cancelled, customer.id, 1, QStringLiteral("用户取消"));
    order.cancelledAt = created.addSecs(1);
  }
  return order;
}

} // namespace

class HardeningTest final : public QObject {
  Q_OBJECT
private slots:
  void startupRecoveryPreservesDamagedPrimary() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    JsonRepository repository(path);
    const auto first = adminSnapshot(1);
    QVERIFY(repository.save(first).ok());
    auto second = first;
    second.revision = 2;
    second.savedAt = at(2);
    QVERIFY(repository.save(second).ok());

    const QByteArray damaged("{damaged");
    writeFile(path, damaged);
    AppContext context(AppPaths::resolve(dir.path()));
    const auto startup = context.initialize();
    QVERIFY(!startup.ok());
    QCOMPARE(startup.error().code, ErrorCode::RecoveryAvailable);

    const auto recovered = context.recoverFromBackup();
    QVERIFY(recovered.ok());
    QCOMPARE(recovered.value(), StartupState::Ready);
    QCOMPARE(context.store().snapshot().revision, qint64(1));
    QCOMPARE(repository.load().value().revision, qint64(1));

    const auto copies = corruptCopies(dir.path());
    QCOMPARE(copies.size(), 1);
    QFile preserved(dir.filePath(copies.first()));
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), damaged);
    QCOMPARE(context.recoverFromBackup().error().code, ErrorCode::Conflict);
  }

  void startupRecoveryRequiresOwningLockAndEligibility() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    JsonRepository repository(path);
    QVERIFY(repository.save(adminSnapshot(1)).ok());
    QVERIFY(repository.save(adminSnapshot(2)).ok());
    const QByteArray damaged("{damaged-by-lock-test");
    writeFile(path, damaged);

    AppContext owner(AppPaths::resolve(dir.path()));
    QCOMPARE(owner.initialize().error().code, ErrorCode::RecoveryAvailable);
    AppContext other(AppPaths::resolve(dir.path()));
    QCOMPARE(other.initialize().error().code, ErrorCode::Conflict);
    QFile before(path);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const auto bytesBefore = before.readAll();
    const auto copiesBefore = corruptCopies(dir.path());
    QCOMPARE(other.recoverFromBackup().error().code, ErrorCode::Conflict);
    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), bytesBefore);
    QCOMPARE(corruptCopies(dir.path()), copiesBefore);
    before.close();
    after.close();
    const auto recovered = owner.recoverFromBackup();
    QVERIFY(recovered.ok());
  }

  void startupRecoveryRejectsUnsupportedPrimaryAsNonRecoverable() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    auto unsupported = adminSnapshot(1);
    unsupported.schemaVersion = 999;
    writeSnapshot(path, unsupported);
    QFile before(path);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const auto bytesBefore = before.readAll();
    AppContext context(AppPaths::resolve(dir.path()));
    QCOMPARE(context.initialize().error().code, ErrorCode::UnsupportedVersion);
    QCOMPARE(context.recoverFromBackup().error().code, ErrorCode::Conflict);
    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), bytesBefore);
    QVERIFY(corruptCopies(dir.path()).isEmpty());
  }

  void startupRecoveryMissingPrimaryDoesNotInventCorruptCopy() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    JsonRepository repository(path);
    QVERIFY(repository.save(adminSnapshot(1)).ok());
    auto second = adminSnapshot(2);
    QVERIFY(repository.save(second).ok());
    QVERIFY(QFile::remove(path));

    AppContext context(AppPaths::resolve(dir.path()));
    QCOMPARE(context.initialize().error().code, ErrorCode::RecoveryAvailable);
    QVERIFY(context.recoverFromBackup().ok());
    QCOMPARE(context.store().snapshot().revision, qint64(1));
    QVERIFY(corruptCopies(dir.path()).isEmpty());
  }

  void startupRecoveryRejectsInvalidOrUnavailableBackups() {
    {
      QTemporaryDir dir;
      QVERIFY(dir.isValid());
      const auto path = dir.filePath("appdata.json");
      writeFile(path, "{damaged");
      AppContext context(AppPaths::resolve(dir.path()));
      QCOMPARE(context.initialize().error().code, ErrorCode::CorruptData);
      QCOMPARE(context.recoverFromBackup().error().code, ErrorCode::Conflict);
    }
    {
      QTemporaryDir dir;
      QVERIFY(dir.isValid());
      const auto path = dir.filePath("appdata.json");
      writeFile(path, "{damaged");
      writeFile(path + ".bak", "{also-damaged");
      AppContext context(AppPaths::resolve(dir.path()));
      QCOMPARE(context.initialize().error().code, ErrorCode::CorruptData);
      QCOMPARE(context.recoverFromBackup().error().code, ErrorCode::Conflict);
    }
    {
      QTemporaryDir dir;
      QVERIFY(dir.isValid());
      const auto path = dir.filePath("appdata.json");
      writeFile(path, "{damaged");
      auto unsupported = adminSnapshot(1);
      unsupported.schemaVersion = 999;
      writeSnapshot(path + ".bak", unsupported);
      AppContext context(AppPaths::resolve(dir.path()));
      QCOMPARE(context.initialize().error().code, ErrorCode::CorruptData);
      QCOMPARE(context.recoverFromBackup().error().code, ErrorCode::Conflict);
    }
  }

  void startupRecoveryNeverOverwritesWhenCorruptCopyFails() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    QVERIFY(QDir().mkpath(path));
    writeSnapshot(path + ".bak", adminSnapshot(1));

    AppContext context(AppPaths::resolve(dir.path()));
    QCOMPARE(context.initialize().error().code, ErrorCode::RecoveryAvailable);
    const auto recovered = context.recoverFromBackup();
    QVERIFY(!recovered.ok());
    QCOMPARE(recovered.error().code, ErrorCode::Persistence);
    QVERIFY(QFileInfo(path).isDir());
  }

  void adminImportRestoreRejectsInvalidDataAndClearsSessionOnSuccess() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir().mkpath(dir.filePath("nested")));
    AppContext context(AppPaths::resolve(dir.path()));
    const auto startup = context.initialize();
    QVERIFY(startup.ok());
    QCOMPARE(startup.value(), StartupState::NeedsAdminBootstrap);
    QVERIFY(context.auth()
                .bootstrapAdmin({"admin", "Admin!234", "管理员"})
                .ok());
    QVERIFY(context.auth().login("admin", "Admin!234", Role::Admin).ok());

    const auto exportPath = dir.filePath("nested/export.json");
    QVERIFY(context.admin().exportData(exportPath).ok());
    JsonRepository repository(context.paths().dataFile());
    const auto exported = repository.loadExternal(exportPath);
    QVERIFY(exported.ok());
    QCOMPARE(exported.value().accounts.size(), 1);

    const auto revision = context.store().snapshot().revision;
    QCOMPARE(context.admin().restoreBackup().error().code, ErrorCode::NotFound);
    QVERIFY(context.session().current());
    writeFile(exportPath, "not-json");
    const auto rejected = context.admin().importData(exportPath);
    QVERIFY(!rejected.ok());
    QCOMPARE(rejected.error().code, ErrorCode::CorruptData);
    QCOMPARE(context.store().snapshot().revision, revision);
    QVERIFY(context.session().current());

    auto missingAdmin = exported.value();
    missingAdmin.accounts.clear();
    writeSnapshot(exportPath, missingAdmin);
    QCOMPARE(context.admin().importData(exportPath).error().code,
             ErrorCode::CorruptData);
    QVERIFY(context.session().current());

    auto deletedCurrentAdmin = exported.value();
    auto secondAdmin = deletedCurrentAdmin.accounts.first();
    secondAdmin.id = "55555555-5555-4555-8555-555555555555";
    secondAdmin.loginName = "admin2";
    secondAdmin.displayName = "管理员二";
    deletedCurrentAdmin.accounts.first().isDeleted = true;
    deletedCurrentAdmin.accounts.push_back(secondAdmin);
    writeSnapshot(exportPath, deletedCurrentAdmin);
    QCOMPARE(context.admin().importData(exportPath).error().code,
             ErrorCode::Forbidden);
    QVERIFY(context.session().current());

    QVERIFY(context.admin().exportData(exportPath).ok());
    QVERIFY(context.admin().importData(exportPath).ok());
    QVERIFY(!context.session().current());
    QVERIFY(context.store().snapshot().revision > revision);

    QVERIFY(context.auth().login("admin", "Admin!234", Role::Admin).ok());
    QVERIFY(context.admin().restoreBackup().ok());
    QVERIFY(!context.session().current());
    QVERIFY(!context.store().snapshot().accounts.isEmpty());
  }

  void exportRejectsNormalizedPrimaryAndBackupPaths() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    JsonRepository repository(path);
    QVERIFY(repository.save(adminSnapshot(1)).ok());
    auto second = adminSnapshot(2);
    QVERIFY(repository.save(second).ok());
    QFile before(path);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const auto bytes = before.readAll();

    const auto parent = QFileInfo(path).absolutePath();
    const QStringList variants = {
        path,
        QDir(parent).filePath(QStringLiteral("./nested/../appdata.json")),
        path + QStringLiteral(".bak"),
    };
#ifdef Q_OS_WIN
    const auto windowsCaseVariant = path.toUpper();
#endif
    for (const auto &variant : variants) {
      const auto result = repository.exportSnapshot(variant, second);
      QCOMPARE(result.error().code, ErrorCode::Conflict);
    }
#ifdef Q_OS_WIN
    QCOMPARE(repository.exportSnapshot(windowsCaseVariant, second).error().code,
             ErrorCode::Conflict);
#endif
    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), bytes);
  }

  void atomicWriterFailureDoesNotPublishPartialState() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    auto writer = std::make_shared<InjectedWriter>(path);
    JsonRepository repository(path, writer);
    QVERIFY(repository.save(adminSnapshot(1)).ok());

    auto second = adminSnapshot(2);
    writer->failure = InjectedWriter::Failure::Backup;
    QVERIFY(!repository.save(second).ok());
    QCOMPARE(repository.load().value().revision, qint64(1));

    writer->failure = InjectedWriter::Failure::Primary;
    QVERIFY(!repository.save(second).ok());
    QCOMPARE(repository.load().value().revision, qint64(1));
    QCOMPARE(repository.loadBackup().value().revision, qint64(1));

    writer->failure = InjectedWriter::Failure::None;
    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext testSession;
    TestService service(store, testSession);
    QSignalSpy committed(&store, &DataStore::committed);
    auto candidate = store.snapshot();
    QFile primaryBefore(path);
    QVERIFY(primaryBefore.open(QIODevice::ReadOnly));
    const auto primaryBytesBefore = primaryBefore.readAll();
    writer->failure = InjectedWriter::Failure::Backup;
    QVERIFY(!service.submit(candidate).ok());
    QCOMPARE(store.snapshot().revision, qint64(1));
    QCOMPARE(committed.count(), 0);
    QFile primaryAfterBackupFailure(path);
    QVERIFY(primaryAfterBackupFailure.open(QIODevice::ReadOnly));
    QCOMPARE(primaryAfterBackupFailure.readAll(), primaryBytesBefore);

    writer->failure = InjectedWriter::Failure::Primary;
    QVERIFY(!service.submit(candidate).ok());
    QCOMPARE(store.snapshot().revision, qint64(1));
    QCOMPARE(committed.count(), 0);
  }

  void passwordComparisonUsesFixedWorkForDifferentLengths() {
    const QByteArray expected("0123456789abcdef");
    QVERIFY(Credentials::constantTimeEqual(expected, expected));
    QVERIFY(!Credentials::constantTimeEqual(expected, "0123456789abcdee"));
    QVERIFY(!Credentials::constantTimeEqual(expected, "short"));
    QVERIFY(Credentials::constantTimeEqual({}, {}));
    QVERIFY(!Credentials::constantTimeEqual({}, "x"));
    QVERIFY(!Credentials::constantTimeEqual(QByteArray(0, '\0'),
                                            QByteArray(256, '\0')));
    QVERIFY(!Credentials::constantTimeEqual(QByteArray(1, '\0'),
                                            QByteArray(257, '\0')));
    QVERIFY(!Credentials::constantTimeEqual(QByteArray(32, '\0'),
                                            QByteArray(32, '\x01')));
  }

  void mixedStateOrdersPerformanceSample() {
    constexpr int orderCount = 1200;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");
    StoreSnapshot snapshot;
    snapshot.revision = 1;
    snapshot.savedAt = at();
    const auto admin = staticAccount(
        "44444444-4444-4444-8444-444444444444", "mixed_admin", "管理员",
        Role::Admin);
    const auto merchant = staticAccount(
        "55555555-5555-4555-8555-555555555555", "mixed_merchant", "商家",
        Role::Merchant);
    const auto rider = staticAccount(
        "66666666-6666-4666-8666-666666666666", "mixed_rider", "骑手",
        Role::Rider);
    snapshot.accounts = {admin, merchant, rider};
    const auto customerResult = Credentials::createAccount(
        snapshot, {"mixed_customer", "Mixed!234", "顾客", "顾客地址",
                   Role::Customer});
    QVERIFY(customerResult.ok());
    const auto customer = customerResult.value();
    snapshot.accounts.push_back(customer);
    const Shop shop{"77777777-7777-4777-8777-777777777777", merchant.id,
                    "混合店铺", "混合态性能样本", "混合地址", true, at()};
    snapshot.shops.push_back(shop);
    const Dish dish{"88888888-8888-4888-8888-888888888888", shop.id,
                    "混合菜品", 100, true, false, at(), at()};
    snapshot.dishes.push_back(dish);
    snapshot.orders.reserve(orderCount);
    int completedCount = 0;
    for (int i = 0; i < orderCount; ++i) {
      const auto kind = i % 7;
      if (kind == 5)
        ++completedCount;
      snapshot.orders.push_back(
          mixedOrder(customer, merchant, rider, shop, dish, i, kind));
    }

    QElapsedTimer timer;
    timer.start();
    const auto encoded = JsonCodec::encode(snapshot);
    const auto encodeMs = timer.elapsed();
    timer.restart();
    const auto decoded = JsonCodec::decode(encoded);
    const auto decodeMs = timer.elapsed();
    QVERIFY(decoded.ok());
    timer.restart();
    QVERIFY(OrderPolicy::validateAll(snapshot).ok());
    const auto validateMs = timer.elapsed();

    JsonRepository repository(path);
    QVERIFY(repository.save(snapshot).ok());
    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext session;
    AuthService auth(store, session);
    QVERIFY(auth.login("mixed_customer", "Mixed!234", Role::Customer).ok());
    OrderQueryService query(store, session);
    OrderTableModel model;
    StatisticsService statistics(store, session);
    timer.restart();
    const auto visible = query.visibleOrders();
    const auto queryMs = timer.elapsed();
    QVERIFY(visible.ok());
    QCOMPARE(visible.value().size(), orderCount);
    timer.restart();
    model.replaceProjection(visible.value());
    const auto modelMs = timer.elapsed();
    QCOMPARE(model.rowCount(), orderCount);
    timer.restart();
    const auto stats = statistics.roleSummary({at(-1), at(orderCount + 10)});
    const auto statsMs = timer.elapsed();
    QVERIFY(stats.ok());
    QCOMPARE(stats.value().completedCount, qint64(completedCount));

    qInfo().noquote()
        << QStringLiteral("MIXED orders=%1 completed=%2 jsonBytes=%3 "
                          "encodeMs=%4 decodeMs=%5 validateMs=%6 queryMs=%7 "
                          "modelMs=%8 statsMs=%9")
               .arg(orderCount)
               .arg(completedCount)
               .arg(QJsonDocument(encoded).toJson(QJsonDocument::Compact).size())
               .arg(encodeMs)
               .arg(decodeMs)
               .arg(validateMs)
               .arg(queryMs)
               .arg(modelMs)
               .arg(statsMs);
  }

  void tenThousandOrdersPerformanceBaseline() {
    constexpr int orderCount = 10000;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("appdata.json");

    StoreSnapshot snapshot;
    snapshot.revision = 1;
    snapshot.savedAt = at();
    const auto admin = staticAccount(
        "44444444-4444-4444-8444-444444444444", "perf_admin", "管理员",
        Role::Admin);
    const auto merchant = staticAccount(
        "55555555-5555-4555-8555-555555555555", "perf_merchant", "商家",
        Role::Merchant);
    snapshot.accounts = {admin, merchant};
    const auto customerResult = Credentials::createAccount(
        snapshot, {"perf_customer", "Perf!234", "顾客", "顾客地址",
                   Role::Customer});
    QVERIFY(customerResult.ok());
    const auto customer = customerResult.value();
    snapshot.accounts.push_back(customer);
    const Shop shop{"66666666-6666-4666-8666-666666666666", merchant.id,
                    "性能店铺", "性能测试", "性能地址", true, at()};
    snapshot.shops.push_back(shop);
    const Dish dish{"77777777-7777-4777-8777-777777777777", shop.id, "性能菜品",
                    100, true, false, at(), at()};
    snapshot.dishes.push_back(dish);
    snapshot.orders.reserve(orderCount);
    for (int i = 0; i < orderCount; ++i) {
      const auto created = at(i);
      Order order;
      order.id = QStringLiteral("10000000-0000-4000-8000-%1")
                     .arg(i, 12, 16, QLatin1Char('0'));
      order.customerId = customer.id;
      order.shopId = shop.id;
      order.items = {{dish.id, dish.name, dish.priceCents, 1, dish.priceCents}};
      order.subtotalCents = dish.priceCents;
      order.deliveryFeeCents = Limits::DefaultDeliveryFeeCents;
      order.totalCents = order.subtotalCents + order.deliveryFeeCents;
      order.customerNameSnapshot = customer.displayName;
      order.addressSnapshot = customer.defaultAddress;
      order.shopNameSnapshot = shop.name;
      order.shopAddressSnapshot = shop.address;
      order.createdAt = created;
      order.updatedAt = created;
      order.history = {{OrderAction::CreateOrder, {},
                        OrderStatus::PendingPayment, customer.id, created, {}}};
      snapshot.orders.push_back(std::move(order));
    }

    QElapsedTimer timer;
    timer.start();
    const auto encoded = JsonCodec::encode(snapshot);
    const auto encodeMs = timer.elapsed();
    timer.restart();
    const auto decoded = JsonCodec::decode(encoded);
    const auto decodeMs = timer.elapsed();
    if (!decoded.ok())
      QFAIL(qPrintable(decoded.error().message));
    timer.restart();
    QVERIFY(OrderPolicy::validateAll(snapshot).ok());
    const auto validateMs = timer.elapsed();

    JsonRepository repository(path);
    timer.restart();
    QVERIFY(repository.save(snapshot).ok());
    const auto saveMs = timer.elapsed();
    timer.restart();
    const auto loaded = repository.load();
    const auto loadMs = timer.elapsed();
    QVERIFY(loaded.ok());
    QCOMPARE(loaded.value().orders.size(), orderCount);

    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext session;
    AuthService auth(store, session);
    QVERIFY(auth.login("perf_customer", "Perf!234", Role::Customer).ok());
    OrderQueryService query(store, session);
    OrderTableModel model;
    StatisticsService statistics(store, session);
    timer.restart();
    const auto visible = query.visibleOrders();
    const auto queryMs = timer.elapsed();
    QVERIFY(visible.ok());
    QCOMPARE(visible.value().size(), orderCount);
    timer.restart();
    model.replaceProjection(visible.value());
    const auto modelMs = timer.elapsed();
    QCOMPARE(model.rowCount(), orderCount);
    timer.restart();
    const auto stats = statistics.roleSummary({at(-1), at(orderCount + 1)});
    const auto statsMs = timer.elapsed();
    QVERIFY(stats.ok());
    QCOMPARE(stats.value().completedCount, qint64(0));

    qInfo().noquote()
        << QStringLiteral("PERF orders=%1 jsonBytes=%2 encodeMs=%3 decodeMs=%4 "
                          "validateMs=%5 saveMs=%6 loadMs=%7 queryMs=%8 "
                          "modelMs=%9 statsMs=%10")
               .arg(orderCount)
               .arg(QJsonDocument(encoded).toJson(QJsonDocument::Compact).size())
               .arg(encodeMs)
               .arg(decodeMs)
               .arg(validateMs)
               .arg(saveMs)
               .arg(loadMs)
               .arg(queryMs)
               .arg(modelMs)
               .arg(statsMs);
  }
};

QTEST_APPLESS_MAIN(HardeningTest)
#include "tst_hardening.moc"
