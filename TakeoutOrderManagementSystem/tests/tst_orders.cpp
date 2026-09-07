#include "core/orderpolicy.h"
#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "services/authservice.h"
#include "services/catalogservice.h"
#include "services/orderqueryservice.h"
#include "services/orderservice.h"
#include <QTemporaryDir>
#include <QtTest>

using namespace takeout;

namespace {
void must(const Result<void> &result, const char *file, int line) {
  if (!result.ok())
    QTest::qFail(qPrintable(result.error().message), file, line);
}
Id mustId(const Result<Id> &result, const char *file, int line) {
  if (!result.ok()) {
    QTest::qFail(qPrintable(result.error().message), file, line);
    return {};
  }
  return result.value();
}
#define MUST(result) must((result), __FILE__, __LINE__)
#define MUST_ID(result) mustId((result), __FILE__, __LINE__)
} // namespace

class OrdersTest final : public QObject {
  Q_OBJECT
private slots:
  void fullLifecycleAndRoleScopedQueries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("appdata.json");
    Id completedId;
    Id cancelledId;
    {
      JsonRepository repository(path);
      DataStore store(repository);
      SessionContext session;
      AuthService auth(store, session);
      CatalogService catalog(store, session);
      OrderService orders(store, session);
      OrderQueryService query(store, session);
      QVERIFY(store.initialize().ok());
      MUST(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}));
      MUST(auth.registerAccount(
          {"customer", "Customer!1", "顾客甲", "收货地址", Role::Customer}));
      MUST(auth.registerAccount(
          {"rider", "Rider!123", "骑手甲", {}, Role::Rider}));
      MUST(catalog.createMerchantWithShop(
          {"merchant", "Merchant!1", "商家甲", "店铺甲", "简介", "店铺地址"}));
      MUST(auth.login("merchant", "Merchant!1", Role::Merchant));
      MUST(catalog.updateShop({"店铺甲", "简介", "店铺地址", true}));
      const auto dishId = MUST_ID(catalog.createDish({"面条", 1200, true}));
      MUST(auth.logout());

      MUST(auth.login("customer", "Customer!1", Role::Customer));
      const auto shopId = store.snapshot().shops.first().id;
      MUST(orders.updateCart({shopId, {{dishId, 2}}}));
      completedId = MUST_ID(orders.createOrder({}));
      QCOMPARE(store.snapshot().carts.size(), 0);
      QCOMPARE(store.snapshot().orders.first().totalCents, Money(2900));
      QCOMPARE(query.visibleOrders().value().size(), 1);
      const auto customerDetail = query.orderDetail(completedId);
      QVERIFY(customerDetail.ok());
      QCOMPARE(customerDetail.value().address, QString("收货地址"));
      const auto invalidActionRevision = store.snapshot().revision;
      QVERIFY(!orders.execute(completedId, static_cast<OrderAction>(99)).ok());
      QCOMPARE(store.snapshot().revision, invalidActionRevision);
      QVERIFY(!orders.execute(completedId, OrderAction::CreateOrder).ok());
      QCOMPARE(store.snapshot().revision, invalidActionRevision);
      MUST(auth.logout());
      MUST(auth.login("merchant", "Merchant!1", Role::Merchant));
      QVERIFY(query.visibleOrders().value().isEmpty());
      QVERIFY(!query.orderDetail(completedId).ok());
      MUST(auth.logout());
      MUST(auth.login("rider", "Rider!123", Role::Rider));
      QVERIFY(query.visibleOrders().value().isEmpty());
      QVERIFY(!query.orderDetail(completedId).ok());
      MUST(auth.logout());
      MUST(auth.login("customer", "Customer!1", Role::Customer));
      MUST(orders.execute(completedId, OrderAction::Pay));
      QCOMPARE(store.snapshot().orders.first().status,
               OrderStatus::PendingAcceptance);
      QVERIFY(!orders.execute(completedId, OrderAction::Pay).ok());
      MUST(auth.logout());

      MUST(auth.login("rider", "Rider!123", Role::Rider));
      QCOMPARE(query.visibleOrders().value().size(), 1);
      const auto pendingDetail = query.orderDetail(completedId);
      QVERIFY(pendingDetail.ok());
      QVERIFY(pendingDetail.value().customerName.isEmpty());
      QVERIFY(pendingDetail.value().address.isEmpty());
      QVERIFY(pendingDetail.value().customerId.isEmpty());
      QVERIFY(pendingDetail.value().history.isEmpty());
      MUST(auth.logout());
      MUST(auth.login("merchant", "Merchant!1", Role::Merchant));
      QCOMPARE(query.visibleOrders().value().size(), 1);
      MUST(orders.execute(completedId, OrderAction::Accept));
      MUST(auth.logout());
      MUST(auth.login("rider", "Rider!123", Role::Rider));
      QCOMPARE(query.visibleOrders().value().size(), 1);
      QVERIFY(query.orderDetail(completedId).ok());
      QVERIFY(query.orderDetail(completedId).value().history.isEmpty());
      MUST(auth.logout());
      MUST(auth.login("merchant", "Merchant!1", Role::Merchant));
      MUST(orders.execute(completedId, OrderAction::MarkReady));
      QVERIFY(!orders.execute(completedId, OrderAction::Reject, "缺货").ok());
      MUST(auth.logout());

      MUST(auth.login("rider", "Rider!123", Role::Rider));
      const auto ready = query.visibleOrders();
      QVERIFY(ready.ok());
      QCOMPARE(ready.value().size(), 1);
      const auto beforeClaim = query.orderDetail(completedId);
      QVERIFY(beforeClaim.ok());
      QVERIFY(beforeClaim.value().address.isEmpty());
      QVERIFY(beforeClaim.value().customerId.isEmpty());
      QVERIFY(beforeClaim.value().history.isEmpty());
      MUST(orders.execute(completedId, OrderAction::Claim));
      const auto afterClaim = query.orderDetail(completedId);
      QVERIFY(afterClaim.ok());
      QCOMPARE(afterClaim.value().customerName, QString("顾客甲"));
      QCOMPARE(afterClaim.value().address, QString("收货地址"));
      MUST(orders.execute(completedId, OrderAction::MarkDelivered));
      QVERIFY(!orders.execute(completedId, OrderAction::MarkDelivered).ok());
      MUST(auth.logout());

      MUST(auth.login("customer", "Customer!1", Role::Customer));
      MUST(orders.execute(completedId, OrderAction::ConfirmReceipt));
      QVERIFY(!orders.execute(completedId, OrderAction::ConfirmReceipt).ok());
      QCOMPARE(store.snapshot().orders.first().status, OrderStatus::Completed);
      QCOMPARE(store.snapshot().orders.first().riderIncomeCents, Money(500));

      MUST(orders.updateCart({shopId, {{dishId, 1}}}));
      cancelledId = MUST_ID(orders.createOrder({"顾客甲", "临时地址"}));
      MUST(orders.execute(cancelledId, OrderAction::Cancel, "  客户取消  "));
      QCOMPARE(store.snapshot().orders.size(), 2);
      QCOMPARE(store.snapshot().orders.last().status, OrderStatus::Cancelled);
      QCOMPARE(store.snapshot().orders.last().cancelReason, QString("客户取消"));
      QCOMPARE(store.snapshot().orders.last().history.last().reason,
               QString("客户取消"));
      QVERIFY(OrderPolicy::validateAll(store.snapshot()).ok());
      OrderFilter invalid;
      invalid.from = QDateTime();
      QVERIFY(!query.visibleOrders(invalid).ok());
      invalid = {};
      invalid.until = QDateTime();
      QVERIFY(!query.visibleOrders(invalid).ok());
      invalid = {};
      invalid.from = QDateTime::currentDateTimeUtc();
      invalid.until = invalid.from;
      const auto invalidRange = query.visibleOrders(invalid);
      QVERIFY(!invalidRange.ok());
      QCOMPARE(invalidRange.error().code, ErrorCode::Validation);
      QCOMPARE(query.visibleOrders().value().size(), 2);
    }

    JsonRepository repository(path);
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    OrderQueryService query(store, session);
    QVERIFY(store.initialize().ok());
    MUST(auth.login("customer", "Customer!1", Role::Customer));
    const auto orders = query.visibleOrders();
    QVERIFY(orders.ok());
    QCOMPARE(orders.value().size(), 2);
    QVERIFY(query.orderDetail(completedId).ok());
    QCOMPARE(query.orderDetail(cancelledId).value().status,
             OrderStatus::Cancelled);
  }

  void invalidOwnershipAndPaymentSnapshotDoNotCommit() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    JsonRepository repository(directory.filePath("appdata.json"));
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    CatalogService catalog(store, session);
    OrderService orders(store, session);
    QVERIFY(store.initialize().ok());
    MUST(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}));
    MUST(auth.registerAccount(
        {"customer", "Customer!1", "顾客甲", "地址甲", Role::Customer}));
    MUST(catalog.createMerchantWithShop(
        {"merchant", "Merchant!1", "商家甲", "店铺甲", "简介", "店铺地址"}));
    MUST(auth.login("merchant", "Merchant!1", Role::Merchant));
    MUST(catalog.updateShop({"店铺甲", "简介", "店铺地址", true}));
    const auto dishId = MUST_ID(catalog.createDish({"面条", 1200, true}));
    const auto shopId = store.snapshot().shops.first().id;
    MUST(auth.logout());
    MUST(auth.login("customer", "Customer!1", Role::Customer));
    MUST(orders.updateCart({shopId, {{dishId, 1}}}));
    const auto orderId = MUST_ID(orders.createOrder({}));
    MUST(auth.logout());
    MUST(auth.login("merchant", "Merchant!1", Role::Merchant));
    MUST(catalog.updateDish(dishId, {"面条", 1300, true}));
    MUST(auth.logout());
    MUST(auth.login("customer", "Customer!1", Role::Customer));
    const auto paymentRevision = store.snapshot().revision;
    QVERIFY(!orders.execute(orderId, OrderAction::Pay).ok());
    QCOMPARE(store.snapshot().orders.first().status,
             OrderStatus::PendingPayment);
    QCOMPARE(store.snapshot().orders.first().paymentStatus,
             PaymentStatus::Unpaid);
    QCOMPARE(store.snapshot().revision, paymentRevision);
  }
};

QTEST_APPLESS_MAIN(OrdersTest)
#include "tst_orders.moc"
