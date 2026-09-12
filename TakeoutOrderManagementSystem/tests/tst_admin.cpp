#include <QtTest>

#include "core/credentials.h"
#include "core/orderpolicy.h"
#include "data/datastore.h"
#include "services/adminservice.h"
#include "services/authservice.h"
#include "services/sessioncontext.h"
#include "services/statisticsservice.h"

using namespace takeout;

namespace {

class MemoryRepository final : public Repository {
public:
  StoreSnapshot disk;
  int writes = 0;
  Result<StoreSnapshot> load() const override {
    return Result<StoreSnapshot>::success(disk);
  }
  Result<void> save(const StoreSnapshot &snapshot) override {
    ++writes;
    disk = snapshot;
    return Result<void>::success();
  }
};

Account addAccount(StoreSnapshot &snapshot, const QString &login,
                   const QString &password, const QString &name, Role role,
                   const QString &address = {}) {
  auto result = Credentials::createAccount(
      snapshot, {login, password, name, address, role});
  Q_ASSERT(result.ok());
  snapshot.accounts.push_back(result.value());
  return result.value();
}

struct Fixture {
  StoreSnapshot snapshot;
  Account admin, customer, idleCustomer, merchant, rider;
  Shop shop;
  Id dishId = QStringLiteral("66666666-6666-4666-8666-666666666666");
};

Fixture makeFixture() {
  Fixture fixture;
  fixture.admin = addAccount(fixture.snapshot, "admin", "Admin!234",
                             "管理员", Role::Admin);
  fixture.customer = addAccount(fixture.snapshot, "customer", "Customer!1",
                                "顾客", Role::Customer, "顾客地址");
  fixture.idleCustomer = addAccount(fixture.snapshot, "idle", "Customer!2",
                                    "空闲顾客", Role::Customer, "空闲地址");
  fixture.merchant = addAccount(fixture.snapshot, "merchant", "Merchant!1",
                                "商家", Role::Merchant);
  fixture.rider = addAccount(fixture.snapshot, "rider", "Rider!234", "骑手",
                             Role::Rider);
  fixture.shop = {QStringLiteral("55555555-5555-4555-8555-555555555555"),
                  fixture.merchant.id, QStringLiteral("店铺甲"),
                  QStringLiteral("简介"), QStringLiteral("店铺地址"), true,
                  fixture.merchant.createdAt};
  fixture.snapshot.shops.push_back(fixture.shop);
  fixture.snapshot.dishes.push_back(
      {fixture.dishId, fixture.shop.id, QStringLiteral("菜品"), 1000, true,
       false, fixture.shop.createdAt, fixture.shop.createdAt});
  fixture.snapshot.carts.push_back(
      {fixture.idleCustomer.id, fixture.shop.id, {{fixture.dishId, 1}}});
  return fixture;
}

Order completedOrder(const Fixture &fixture, const Id &id, Money subtotal,
                     const QDateTime &at) {
  Order order;
  order.id = id;
  order.customerId = fixture.customer.id;
  order.shopId = fixture.shop.id;
  order.riderId = fixture.rider.id;
  order.status = OrderStatus::Completed;
  order.paymentStatus = PaymentStatus::Paid;
  const int quantity = int(subtotal / 1000);
  order.items = {{fixture.dishId, QStringLiteral("菜品"), 1000, quantity,
                  subtotal}};
  order.subtotalCents = subtotal;
  order.deliveryFeeCents = 500;
  order.totalCents = subtotal + 500;
  order.riderIncomeCents = 500;
  order.customerNameSnapshot = fixture.customer.displayName;
  order.addressSnapshot = fixture.customer.defaultAddress;
  order.shopNameSnapshot = fixture.shop.name;
  order.shopAddressSnapshot = fixture.shop.address;
  order.riderNameSnapshot = fixture.rider.displayName;
  order.createdAt = at;
  order.paidAt = at.addSecs(1);
  order.acceptedAt = at.addSecs(2);
  order.readyAt = at.addSecs(3);
  order.claimedAt = at.addSecs(4);
  order.deliveredAt = at.addSecs(5);
  order.completedAt = at.addSecs(6);
  order.updatedAt = *order.completedAt;
  order.history = {
      {OrderAction::CreateOrder, {}, OrderStatus::PendingPayment,
       fixture.customer.id, order.createdAt, {}},
      {OrderAction::Pay, OrderStatus::PendingPayment,
       OrderStatus::PendingAcceptance, fixture.customer.id, *order.paidAt, {}},
      {OrderAction::Accept, OrderStatus::PendingAcceptance,
       OrderStatus::Preparing, fixture.merchant.id, *order.acceptedAt, {}},
      {OrderAction::MarkReady, OrderStatus::Preparing,
       OrderStatus::ReadyForDelivery, fixture.merchant.id, *order.readyAt, {}},
      {OrderAction::Claim, OrderStatus::ReadyForDelivery,
       OrderStatus::Delivering, fixture.rider.id, *order.claimedAt, {}},
      {OrderAction::MarkDelivered, OrderStatus::Delivering,
       OrderStatus::Delivering, fixture.rider.id, *order.deliveredAt, {}},
      {OrderAction::ConfirmReceipt, OrderStatus::Delivering,
       OrderStatus::Completed, fixture.customer.id, *order.completedAt, {}}};
  return order;
}

} // namespace

class AdminTest final : public QObject {
  Q_OBJECT
private slots:
  void accountDeletionChecksPermissionsBoundariesAndCart() {
    auto fixture = makeFixture();
    MemoryRepository repository;
    repository.disk = fixture.snapshot;
    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext session;
    AuthService auth(store, session);
    AdminService admin(store, session);
    QVERIFY(auth.login("admin", "Admin!234", Role::Admin).ok());
    const auto listed = admin.listAccounts();
    QVERIFY(listed.ok());
    QCOMPARE(listed.value().size(), 4);
    for (const auto &row : listed.value())
      QVERIFY(row.role != Role::Admin);
    const auto revision = store.snapshot().revision;
    QVERIFY(admin.deleteAccount(fixture.admin.id).error().code ==
            ErrorCode::Forbidden);
    QCOMPARE(store.snapshot().revision, revision);
    QVERIFY(admin.deleteAccount(fixture.idleCustomer.id).ok());
    QCOMPARE(store.snapshot().revision, revision + 1);
    QVERIFY(store.snapshot().carts.isEmpty());
    for (const auto &account : store.snapshot().accounts)
      if (account.id == fixture.idleCustomer.id)
        QVERIFY(account.isDeleted);
    QVERIFY(auth.logout().ok());
    QVERIFY(!auth.login("idle", "Customer!2", Role::Customer).ok());
  }

  void statisticsUseRoleSpecificCompletedAmountsAndDateRange() {
    auto fixture = makeFixture();
    const auto first = QDateTime::fromString("2026-09-01T10:00:00.000Z",
                                             Qt::ISODateWithMs);
    fixture.snapshot.orders.push_back(completedOrder(
        fixture, QStringLiteral("77777777-7777-4777-8777-777777777777"),
        1000, first));
    fixture.snapshot.orders.push_back(completedOrder(
        fixture, QStringLiteral("88888888-8888-4888-8888-888888888888"),
        2000, first.addSecs(1)));
    QVERIFY(OrderPolicy::validateAll(fixture.snapshot).ok());
    MemoryRepository repository;
    repository.disk = fixture.snapshot;
    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext session;
    AuthService auth(store, session);
    StatisticsService statistics(store, session);
    const DateRange range{first.addSecs(-1), first.addSecs(10)};

    QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
    auto result = statistics.roleSummary(range);
    QVERIFY(result.ok());
    QCOMPARE(result.value().completedCount, qint64(2));
    QCOMPARE(result.value().totalCents, qint64(3000));
    QVERIFY(auth.logout().ok());

    QVERIFY(auth.login("rider", "Rider!234", Role::Rider).ok());
    result = statistics.roleSummary(range);
    QVERIFY(result.ok());
    QCOMPARE(result.value().totalCents, qint64(1000));
    QVERIFY(auth.logout().ok());

    QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
    result = statistics.roleSummary(range);
    QVERIFY(result.ok());
    QCOMPARE(result.value().totalCents, qint64(4000));
    QCOMPARE(statistics.adminSummary(range).error().code, ErrorCode::Forbidden);
    QVERIFY(auth.logout().ok());

    QVERIFY(auth.login("admin", "Admin!234", Role::Admin).ok());
    result = statistics.roleSummary(range);
    QVERIFY(result.ok());
    QCOMPARE(result.value().totalCents, qint64(4000));
    const auto adminResult = statistics.adminSummary(range);
    QVERIFY(adminResult.ok());
    QCOMPARE(adminResult.value().activeAccountCount, qint64(5));
    QCOMPARE(adminResult.value().validShopCount, qint64(1));
    QVERIFY(!statistics.roleSummary({QDateTime(), first}).ok());
    QVERIFY(!statistics.adminSummary({first, first}).ok());
  }

  void nonAdminCannotUseAccountManagement() {
    auto fixture = makeFixture();
    MemoryRepository repository;
    repository.disk = fixture.snapshot;
    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext session;
    AuthService auth(store, session);
    AdminService admin(store, session);
    QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
    QCOMPARE(admin.listAccounts().error().code, ErrorCode::Forbidden);
    QCOMPARE(admin.deleteAccount(fixture.idleCustomer.id).error().code,
             ErrorCode::Forbidden);
  }

  void merchantDeletionUnpublishesAndCleansEveryCart() {
    auto fixture = makeFixture();
    MemoryRepository repository;
    repository.disk = fixture.snapshot;
    DataStore store(repository);
    QVERIFY(store.initialize().ok());
    SessionContext session;
    AuthService auth(store, session);
    AdminService admin(store, session);
    QVERIFY(auth.login("admin", "Admin!234", Role::Admin).ok());
    QVERIFY(admin.deleteAccount(fixture.merchant.id).ok());
    QCOMPARE(store.snapshot().carts.size(), 0);
    QCOMPARE(store.snapshot().shops.first().isOpen, false);
    QCOMPARE(store.snapshot().dishes.first().isAvailable, false);
    QVERIFY(!admin.deleteAccount(fixture.merchant.id).ok());
  }
};

QTEST_APPLESS_MAIN(AdminTest)
#include "tst_admin.moc"
