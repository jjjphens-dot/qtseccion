#include <QtTest>
#include "core/validation.h"
#include "core/orderpolicy.h"

using namespace takeout;
namespace {
const Id customerId = "11111111-1111-4111-8111-111111111111";
const Id merchantId = "22222222-2222-4222-8222-222222222222";
const Id riderId = "33333333-3333-4333-8333-333333333333";
const Id adminId = "44444444-4444-4444-8444-444444444444";
const Id shopId = "55555555-5555-4555-8555-555555555555";
const Id dishId = "66666666-6666-4666-8666-666666666666";
const Id orderId = "77777777-7777-4777-8777-777777777777";

Account account(const Id& id, const QString& login, const QString& name, Role role) {
    Account value; value.id=id; value.loginName=login; value.displayName=name; value.role=role;
    value.passwordSalt=QByteArray(16,'s'); value.passwordHash=QByteArray(32,'h');
    value.passwordIterations=600000; value.passwordAlgorithm="PBKDF2-HMAC-SHA256";
    if (role == Role::Customer) value.defaultAddress = "用户地址";
    value.createdAt=QDateTime::fromString("2026-09-01T00:00:00.000Z", Qt::ISODateWithMs);
    return value;
}
StoreSnapshot baseSnapshot() {
    StoreSnapshot result;
    result.accounts = {account(customerId,"customer","顾客甲",Role::Customer),
        account(merchantId,"merchant","商家甲",Role::Merchant),
        account(riderId,"rider","骑手甲",Role::Rider), account(adminId,"admin","管理员",Role::Admin)};
    Shop shop; shop.id=shopId; shop.merchantId=merchantId; shop.name="店铺甲"; shop.address="店铺地址"; shop.isOpen=true; shop.createdAt=result.accounts[0].createdAt;
    result.shops.push_back(shop);
    Dish dish; dish.id=dishId; dish.shopId=shopId; dish.name="菜品甲"; dish.priceCents=1234; dish.isAvailable=true; dish.createdAt=shop.createdAt; dish.updatedAt=shop.createdAt;
    result.dishes.push_back(dish);
    return result;
}
Order completedOrder() {
    const auto base=QDateTime::fromString("2026-09-01T10:00:00.000Z",Qt::ISODateWithMs);
    Order value; value.id=orderId; value.customerId=customerId; value.shopId=shopId; value.riderId=riderId;
    value.items={{dishId,"菜品旧名",1234,2,2468}}; value.status=OrderStatus::Completed; value.paymentStatus=PaymentStatus::Paid;
    value.subtotalCents=2468; value.deliveryFeeCents=500; value.totalCents=2968; value.riderIncomeCents=500;
    value.customerNameSnapshot="旧顾客名"; value.addressSnapshot="旧地址"; value.shopNameSnapshot="旧店名"; value.shopAddressSnapshot="旧店地址"; value.riderNameSnapshot="旧骑手名";
    value.createdAt=base; value.paidAt=base.addSecs(1); value.acceptedAt=base.addSecs(2); value.readyAt=base.addSecs(3);
    value.claimedAt=base.addSecs(4); value.deliveredAt=base.addSecs(5); value.completedAt=base.addSecs(6); value.updatedAt=*value.completedAt;
    value.history={{OrderAction::CreateOrder,{},OrderStatus::PendingPayment,customerId,base,{}},
      {OrderAction::Pay,OrderStatus::PendingPayment,OrderStatus::PendingAcceptance,customerId,*value.paidAt,{}},
      {OrderAction::Accept,OrderStatus::PendingAcceptance,OrderStatus::Preparing,merchantId,*value.acceptedAt,{}},
      {OrderAction::MarkReady,OrderStatus::Preparing,OrderStatus::ReadyForDelivery,merchantId,*value.readyAt,{}},
      {OrderAction::Claim,OrderStatus::ReadyForDelivery,OrderStatus::Delivering,riderId,*value.claimedAt,{}},
      {OrderAction::MarkDelivered,OrderStatus::Delivering,OrderStatus::Delivering,riderId,*value.deliveredAt,{}},
      {OrderAction::ConfirmReceipt,OrderStatus::Delivering,OrderStatus::Completed,customerId,*value.completedAt,{}}};
    return value;
}
}

class RulesTest final : public QObject {
    Q_OBJECT
private slots:
    void loginAndPasswordBoundaries() {
        QVERIFY(!Validation::loginName("ab").ok()); QVERIFY(Validation::loginName("Abc_123").ok()); QVERIFY(!Validation::loginName(QString(33,'a')).ok());
        QVERIFY(!Validation::password("1234567").ok()); QVERIFY(Validation::password("1234567!").ok());
        QVERIFY(Validation::password(QString(128,'!')).ok()); QVERIFY(!Validation::password(QString(129,'!')).ok());
        QVERIFY(!Validation::password("password with space").ok()); QVERIFY(!Validation::password(QStringLiteral("密码abcdefgh")).ok());
        QCOMPARE(Validation::normalizeLoginName(" USER_1 "),QString("user_1"));
    }
    void textAndNumericBoundaries() {
        QVERIFY(!Validation::displayName("  ").ok()); QVERIFY(Validation::displayName(QString(40,QChar(u'名'))).ok()); QVERIFY(!Validation::displayName(QString(41,QChar(u'名'))).ok());
        QVERIFY(Validation::address(QString(200,QChar(u'址'))).ok()); QVERIFY(!Validation::address(QString(201,QChar(u'址'))).ok());
        QVERIFY(Validation::dishPrice(0).ok()); QVERIFY(Validation::dishPrice(1'000'000).ok()); QVERIFY(!Validation::dishPrice(-1).ok()); QVERIFY(!Validation::dishPrice(1'000'001).ok());
        QVERIFY(Validation::quantity(1).ok()); QVERIFY(Validation::quantity(99).ok()); QVERIFY(!Validation::quantity(0).ok()); QVERIFY(!Validation::quantity(100).ok());
        QVERIFY(!Validation::reason(" ",true).ok()); QVERIFY(Validation::reason(" ",false).ok());
    }
    void unicodeAndUuidRules() {
        QCOMPARE(Validation::normalizeNamedEntity(QStringLiteral(" Ａ餐厅 ")),QStringLiteral("ａ餐厅"));
        QVERIFY(Validation::uuid(orderId,"id").ok()); QVERIFY(!Validation::uuid("{77777777-7777-4777-8777-777777777777}","id").ok());
        QVERIFY(!Validation::uuid("77777777-7777-4777-8777-77777777777A","id").ok());
    }
    void completedTimelineIsValidAndSnapshotsStayHistorical() {
        auto snapshot=baseSnapshot(); snapshot.orders.push_back(completedOrder());
        QVERIFY(OrderPolicy::validateAll(snapshot).ok());
        snapshot.accounts[0].displayName="现顾客名"; snapshot.accounts[2].displayName="现骑手名";
        snapshot.shops[0].name="现店名"; snapshot.dishes[0].name="现菜品名"; snapshot.dishes[0].isAvailable=false; snapshot.dishes[0].isDeleted=true;
        QVERIFY(OrderPolicy::validateAll(snapshot).ok());
    }
    void rejectsStatePaymentAndTimestampContradictions() {
        auto snapshot=baseSnapshot(); auto order=completedOrder();
        order.paymentStatus=PaymentStatus::Refunded; snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.completedAt=order.deliveredAt->addSecs(-1); snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.claimedAt.reset(); snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.cancelledAt=order.completedAt; snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.riderIncomeCents=1000; snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
    }
    void rejectsHistoryTampering() {
        auto snapshot=baseSnapshot(); auto order=completedOrder();
        order.history[4].actorId=customerId; snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.history[5].toStatus=OrderStatus::Completed; snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.history[3].at=order.history[2].at.addSecs(-1); snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.history.removeAt(3); snapshot.orders={order}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
    }
    void cancellationsFollowTwoLegalPaths() {
        auto snapshot=baseSnapshot(); auto order=completedOrder(); const auto base=order.createdAt;
        order.status=OrderStatus::Cancelled; order.paymentStatus=PaymentStatus::Unpaid; order.riderId.reset(); order.riderNameSnapshot.reset();
        order.paidAt.reset(); order.acceptedAt.reset(); order.readyAt.reset(); order.claimedAt.reset(); order.deliveredAt.reset(); order.completedAt.reset(); order.cancelledAt=base.addSecs(1); order.updatedAt=*order.cancelledAt; order.riderIncomeCents=0; order.cancelReason="";
        order.history={order.history[0],{OrderAction::Cancel,OrderStatus::PendingPayment,OrderStatus::Cancelled,customerId,*order.cancelledAt,{}}};
        snapshot.orders={order}; QVERIFY(OrderPolicy::validateAll(snapshot).ok());
        order=completedOrder(); order.status=OrderStatus::Cancelled; order.paymentStatus=PaymentStatus::Refunded; order.riderId.reset(); order.riderNameSnapshot.reset();
        order.acceptedAt.reset(); order.readyAt.reset(); order.claimedAt.reset(); order.deliveredAt.reset(); order.completedAt.reset(); order.cancelledAt=base.addSecs(2); order.updatedAt=*order.cancelledAt; order.riderIncomeCents=0; order.cancelReason="缺货";
        order.history={order.history[0],order.history[1],{OrderAction::Reject,OrderStatus::PendingAcceptance,OrderStatus::Cancelled,merchantId,*order.cancelledAt,"缺货"}};
        snapshot.orders={order}; QVERIFY(OrderPolicy::validateAll(snapshot).ok());
        snapshot.orders[0].history[2].reason=""; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
    }
    void globalReferencesAndUniqueness() {
        auto snapshot=baseSnapshot(); QVERIFY(OrderPolicy::validateAll(snapshot).ok());
        snapshot.accounts[1].loginName="CUSTOMER"; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        snapshot=baseSnapshot(); auto duplicate=snapshot.dishes[0]; duplicate.id="88888888-8888-4888-8888-888888888888"; duplicate.name="菜品甲 "; snapshot.dishes.push_back(duplicate); QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        snapshot=baseSnapshot(); snapshot.shops[0].merchantId=customerId; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        snapshot=baseSnapshot(); snapshot.shops.clear(); snapshot.dishes.clear(); QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        snapshot=baseSnapshot(); const Id otherMerchantId="aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"; snapshot.accounts.push_back(account(otherMerchantId,"merchant2","商家乙",Role::Merchant)); auto otherShop=snapshot.shops[0]; otherShop.id="99999999-9999-4999-8999-999999999999"; otherShop.merchantId=otherMerchantId; snapshot.shops.push_back(otherShop); snapshot.dishes[0].shopId=otherShop.id; snapshot.orders={completedOrder()}; QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        snapshot=baseSnapshot(); snapshot.accounts[0].defaultAddress.clear(); QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
        snapshot=baseSnapshot(); snapshot.orders.resize(Limits::MaxOrders+1); QVERIFY(!OrderPolicy::validateAll(snapshot).ok());
    }
    void transitionMatrixKeepsSevenStates() {
        QVERIFY(OrderPolicy::canTransition(OrderStatus::PendingPayment,OrderAction::Pay));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::PendingPayment,OrderAction::Cancel));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::PendingAcceptance,OrderAction::Accept));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::PendingAcceptance,OrderAction::Reject));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::Preparing,OrderAction::MarkReady));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::ReadyForDelivery,OrderAction::Claim));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::Delivering,OrderAction::MarkDelivered));
        QVERIFY(OrderPolicy::canTransition(OrderStatus::Delivering,OrderAction::ConfirmReceipt));
        QVERIFY(!OrderPolicy::canTransition(OrderStatus::Completed,OrderAction::ConfirmReceipt));
        QCOMPARE(int(OrderStatus::Completed)+1,7);
    }
};
QTEST_APPLESS_MAIN(RulesTest)
#include "tst_rules.moc"
