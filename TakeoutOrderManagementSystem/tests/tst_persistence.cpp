#include "data/jsoncodec.h"
#include "data/jsonrepository.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace takeout;
namespace {
const Id customerId = "11111111-1111-4111-8111-111111111111";
const Id merchantId = "22222222-2222-4222-8222-222222222222";
const Id riderId = "33333333-3333-4333-8333-333333333333";
const Id adminId = "44444444-4444-4444-8444-444444444444";
const Id shopId = "55555555-5555-4555-8555-555555555555";
const Id dishId = "66666666-6666-4666-8666-666666666666";
const Id orderId = "77777777-7777-4777-8777-777777777777";
QDateTime at(int seconds = 0) {
  return QDateTime::fromString("2026-09-01T10:00:00.000Z", Qt::ISODateWithMs)
      .addSecs(seconds);
}
Account account(const Id &id, const QString &login, const QString &name,
                Role role) {
  Account v;
  v.id = id;
  v.loginName = login;
  v.displayName = name;
  v.role = role;
  v.passwordSalt = QByteArray("salt-") + login.toUtf8();
  v.passwordHash = QByteArray("hash-") + login.toUtf8();
  v.passwordIterations = 210000;
  v.passwordAlgorithm = "PBKDF2-HMAC-SHA256";
  if (role == Role::Customer)
    v.defaultAddress = "用户地址";
  v.createdAt = at();
  return v;
}
StoreSnapshot completeSnapshot() {
  StoreSnapshot s;
  s.revision = 1;
  s.savedAt = at(20);
  s.accounts = {account(customerId, "customer", "顾客甲", Role::Customer),
                account(merchantId, "merchant", "商家甲", Role::Merchant),
                account(riderId, "rider", "骑手甲", Role::Rider),
                account(adminId, "admin", "管理员", Role::Admin)};
  Shop shop;
  shop.id = shopId;
  shop.merchantId = merchantId;
  shop.name = "店铺甲";
  shop.description = "简介";
  shop.address = "店铺地址";
  shop.isOpen = true;
  shop.createdAt = at();
  s.shops = {shop};
  Dish dish;
  dish.id = dishId;
  dish.shopId = shopId;
  dish.name = "菜品甲";
  dish.priceCents = 1234;
  dish.isAvailable = true;
  dish.createdAt = at();
  dish.updatedAt = at();
  s.dishes = {dish};
  Cart cart;
  cart.customerId = customerId;
  cart.shopId = shopId;
  cart.items = {{dishId, 2}};
  s.carts = {cart};
  Order o;
  o.id = orderId;
  o.customerId = customerId;
  o.shopId = shopId;
  o.riderId = riderId;
  o.items = {{dishId, "历史菜品", 1234, 2, 2468}};
  o.status = OrderStatus::Completed;
  o.paymentStatus = PaymentStatus::Paid;
  o.subtotalCents = 2468;
  o.deliveryFeeCents = 500;
  o.totalCents = 2968;
  o.riderIncomeCents = 500;
  o.customerNameSnapshot = "历史顾客";
  o.addressSnapshot = "历史地址";
  o.shopNameSnapshot = "历史店铺";
  o.shopAddressSnapshot = "历史店址";
  o.riderNameSnapshot = "历史骑手";
  o.createdAt = at();
  o.paidAt = at(1);
  o.acceptedAt = at(2);
  o.readyAt = at(3);
  o.claimedAt = at(4);
  o.deliveredAt = at(5);
  o.completedAt = at(6);
  o.updatedAt = at(6);
  o.history = {{OrderAction::CreateOrder,
                {},
                OrderStatus::PendingPayment,
                customerId,
                at(),
                {}},
               {OrderAction::Pay,
                OrderStatus::PendingPayment,
                OrderStatus::PendingAcceptance,
                customerId,
                at(1),
                {}},
               {OrderAction::Accept,
                OrderStatus::PendingAcceptance,
                OrderStatus::Preparing,
                merchantId,
                at(2),
                {}},
               {OrderAction::MarkReady,
                OrderStatus::Preparing,
                OrderStatus::ReadyForDelivery,
                merchantId,
                at(3),
                {}},
               {OrderAction::Claim,
                OrderStatus::ReadyForDelivery,
                OrderStatus::Delivering,
                riderId,
                at(4),
                {}},
               {OrderAction::MarkDelivered,
                OrderStatus::Delivering,
                OrderStatus::Delivering,
                riderId,
                at(5),
                {}},
               {OrderAction::ConfirmReceipt,
                OrderStatus::Delivering,
                OrderStatus::Completed,
                customerId,
                at(6),
                {}}};
  Order cancelled;
  cancelled.id = "88888888-8888-4888-8888-888888888888";
  cancelled.customerId = customerId;
  cancelled.shopId = shopId;
  cancelled.items = {{dishId, "历史菜品", 1234, 1, 1234}};
  cancelled.status = OrderStatus::Cancelled;
  cancelled.paymentStatus = PaymentStatus::Unpaid;
  cancelled.subtotalCents = 1234;
  cancelled.deliveryFeeCents = 500;
  cancelled.totalCents = 1734;
  cancelled.customerNameSnapshot = "历史顾客";
  cancelled.addressSnapshot = "历史地址";
  cancelled.shopNameSnapshot = "历史店铺";
  cancelled.shopAddressSnapshot = "历史店址";
  cancelled.createdAt = at(10);
  cancelled.updatedAt = at(11);
  cancelled.cancelledAt = at(11);
  cancelled.history = {{OrderAction::CreateOrder,
                        {},
                        OrderStatus::PendingPayment,
                        customerId,
                        at(10),
                        {}},
                       {OrderAction::Cancel,
                        OrderStatus::PendingPayment,
                        OrderStatus::Cancelled,
                        customerId,
                        at(11),
                        {}}};
  s.orders = {o, cancelled};
  return s;
}
QByteArray contents(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll();
}
} // namespace

class PersistenceTest final : public QObject {
  Q_OBJECT
private slots:
  void roundTripsEveryEntityAndOptionalField() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    JsonRepository repo(dir.filePath("appdata.json"));
    const auto original = completeSnapshot();
    QVERIFY(repo.save(original).ok());
    const auto loaded = repo.load();
    QVERIFY(loaded.ok());
    QCOMPARE(JsonCodec::encode(loaded.value()), JsonCodec::encode(original));
  }
  void secondSaveAtomicallyPreservesPreviousRevision() {
    QTemporaryDir dir;
    const auto path = dir.filePath("nested/appdata.json");
    JsonRepository repo(path);
    auto first = completeSnapshot();
    QVERIFY(repo.save(first).ok());
    auto second = first;
    second.revision = 2;
    second.savedAt = at(30);
    second.shops[0].description = "新简介";
    QVERIFY(repo.save(second).ok());
    QCOMPARE(repo.load().value().revision, 2);
    const auto backup = repo.loadBackup();
    QVERIFY(backup.ok());
    QCOMPARE(backup.value().revision, 1);
    QCOMPARE(backup.value().shops[0].description, QString("简介"));
  }
  void reportsVerifiedRecoveryWithoutOverwritingPrimary() {
    QTemporaryDir dir;
    const auto path = dir.filePath("appdata.json");
    JsonRepository repo(path);
    auto first = completeSnapshot();
    QVERIFY(repo.save(first).ok());
    auto second = first;
    second.revision = 2;
    second.savedAt = at(30);
    QVERIFY(repo.save(second).ok());
    QFile damaged(path);
    QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(damaged.write("{broken") > 0);
    damaged.close();
    const auto result = repo.load();
    QVERIFY(!result.ok());
    QCOMPARE(result.error().code, ErrorCode::RecoveryAvailable);
    QCOMPARE(contents(path), QByteArray("{broken"));
    QCOMPARE(repo.loadBackup().value().revision, 1);
  }
  void strictSchemaRejectsUnknownEnumsFieldsAndReferences() {
    auto root = JsonCodec::encode(completeSnapshot());
    auto accounts = root["accounts"].toArray();
    auto a = accounts[0].toObject();
    a["role"] = "superuser";
    accounts[0] = a;
    root["accounts"] = accounts;
    QCOMPARE(JsonCodec::decode(root).error().code, ErrorCode::CorruptData);
    root = JsonCodec::encode(completeSnapshot());
    accounts = root["accounts"].toArray();
    a = accounts[0].toObject();
    a["unexpected"] = true;
    accounts[0] = a;
    root["accounts"] = accounts;
    QVERIFY(!JsonCodec::decode(root).ok());
    root = JsonCodec::encode(completeSnapshot());
    auto dishes = root["dishes"].toArray();
    auto d = dishes[0].toObject();
    d["shopId"] = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    dishes[0] = d;
    root["dishes"] = dishes;
    QVERIFY(!JsonCodec::decode(root).ok());
    root = JsonCodec::encode(completeSnapshot());
    root["savedAt"] = "2026-09-01T18:00:20.000+08:00";
    QVERIFY(!JsonCodec::decode(root).ok());
  }
  void unsupportedVersionIsDistinct() {
    auto root = JsonCodec::encode(completeSnapshot());
    root["schemaVersion"] = 2;
    const auto result = JsonCodec::decode(root);
    QVERIFY(!result.ok());
    QCOMPARE(result.error().code, ErrorCode::UnsupportedVersion);
  }
  void futurePrimaryIsNotReplacedByOlderBackup() {
    QTemporaryDir dir;
    const auto path = dir.filePath("appdata.json");
    JsonRepository repo(path);
    auto first = completeSnapshot();
    QVERIFY(repo.save(first).ok());
    first.revision = 2;
    first.savedAt = at(30);
    QVERIFY(repo.save(first).ok());
    auto future = JsonCodec::encode(first);
    future["schemaVersion"] = 2;
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto bytes = QJsonDocument(future).toJson();
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
    file.close();
    const auto result = repo.load();
    QVERIFY(!result.ok());
    QCOMPARE(result.error().code, ErrorCode::UnsupportedVersion);
    QCOMPARE(repo.loadBackup().value().revision, 1);
  }
  void failedValidationDoesNotReplaceExistingFile() {
    QTemporaryDir dir;
    const auto path = dir.filePath("appdata.json");
    JsonRepository repo(path);
    auto valid = completeSnapshot();
    QVERIFY(repo.save(valid).ok());
    const auto before = contents(path);
    valid.orders[0].totalCents++;
    QVERIFY(!repo.save(valid).ok());
    QCOMPARE(contents(path), before);
  }
  void rejectsFilesAboveConfiguredLimit() {
    QTemporaryDir dir;
    const auto path = dir.filePath("appdata.json");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.resize(Limits::MaxFileBytes + 1));
    f.close();
    JsonRepository repo(path);
    const auto result = repo.load();
    QVERIFY(!result.ok());
    QCOMPARE(result.error().code, ErrorCode::CorruptData);
    QCOMPARE(result.error().field, QString("fileSize"));
  }
};
QTEST_APPLESS_MAIN(PersistenceTest)
#include "tst_persistence.moc"
