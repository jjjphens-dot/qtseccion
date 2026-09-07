#include "core/orderpolicy.h"
#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "models/cartmodel.h"
#include "models/dishmodel.h"
#include "models/shopmodel.h"
#include "services/authservice.h"
#include "services/catalogservice.h"
#include "services/orderservice.h"
#include <QAbstractItemModelTester>
#include <QTemporaryDir>
#include <QtTest>

using namespace takeout;

class CatalogTest final : public QObject {
  Q_OBJECT
private slots:
  void catalogAndCartTransactionsPersistAcrossRestart() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("appdata.json");
    {
      JsonRepository repository(path);
      DataStore store(repository);
      SessionContext session;
      AuthService auth(store, session);
      CatalogService catalog(store, session);
      OrderService orders(store, session);
      QCOMPARE(store.initialize().value(), StartupState::NeedsAdminBootstrap);
      QVERIFY(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}).ok());
      QVERIFY(catalog.createMerchantWithShop(
                            {"merchant", "Merchant!1", "商家甲", "店铺甲",
                             "简介", "店铺地址"})
                  .ok());
      QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
      QCOMPARE(store.snapshot().shops.first().isOpen, false);
      QVERIFY(catalog.updateShop({"店铺甲", "新简介", "新地址", true}).ok());
      QCOMPARE(store.snapshot().shops.first().description, QString("新简介"));
      const auto firstDish = catalog.createDish({" 面条 ", 1234, true});
      QVERIFY(firstDish.ok());
      const auto writesAfterFirst = store.snapshot().revision;
      QVERIFY(!catalog.createDish({"面条", 2000, true}).ok());
      QCOMPARE(store.snapshot().revision, writesAfterFirst);
      const auto secondDish = catalog.createDish({"汤", 500, false});
      QVERIFY(secondDish.ok());
      QVERIFY(catalog.updateDish(firstDish.value(), {"牛肉面", 1500, true}).ok());
      QVERIFY(catalog.deleteDish(secondDish.value()).ok());
      QVERIFY(!catalog.deleteDish(secondDish.value()).ok());
      QVERIFY(auth.logout().ok());
      QVERIFY(auth.registerAccount(
                          {"customer", "Customer!1", "顾客甲", "旧地址",
                           Role::Customer})
                  .ok());
      QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
      QVERIFY(catalog.updateProfile({"顾客乙", "新地址"}).ok());
      QVERIFY(session.current());
      QCOMPARE(session.current()->displayName, QString("顾客乙"));
      QVERIFY(orders.updateCart({store.snapshot().shops.first().id,
                                 {{firstDish.value(), 2}}})
                  .ok());
      QCOMPARE(store.snapshot().carts.size(), 1);
      QCOMPARE(store.snapshot().carts.first().items.first().quantity, 2);
      QVERIFY(!orders.updateCart({store.snapshot().shops.first().id,
                                  {{firstDish.value(), 2},
                                   {firstDish.value(), 3}}})
                  .ok());
      QVERIFY(orders.updateCart({store.snapshot().shops.first().id, {}}).ok());
      QVERIFY(auth.logout().ok());
      QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
      QVERIFY(catalog.updateDish(firstDish.value(), {"牛肉面", 1500, false}).ok());
      QVERIFY(auth.logout().ok());
      QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
      QVERIFY(!orders.updateCart({store.snapshot().shops.first().id,
                                  {{firstDish.value(), 1}}})
                  .ok());
      QVERIFY(orders.updateCart({store.snapshot().shops.first().id, {}}).ok());
      QCOMPARE(store.snapshot().carts.size(), 0);
      QVERIFY(auth.logout().ok());
      QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
      QVERIFY(catalog.updateDish(firstDish.value(), {"牛肉面", 1500, true}).ok());
      QVERIFY(auth.logout().ok());
      QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
      QVERIFY(orders.updateCart({store.snapshot().shops.first().id,
                                 {{firstDish.value(), 3}}})
                  .ok());
      QVERIFY(OrderPolicy::validateAll(store.snapshot()).ok());
    }
    JsonRepository repository(path);
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    QVERIFY(store.initialize().ok());
    QCOMPARE(store.snapshot().accounts.size(), 3);
    QCOMPARE(store.snapshot().shops.size(), 1);
    QCOMPARE(store.snapshot().dishes.size(), 2);
    QCOMPARE(store.snapshot().carts.size(), 1);
    QCOMPARE(store.snapshot().carts.first().items.first().quantity, 3);
    QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
    QCOMPARE(store.snapshot().accounts.at(2).defaultAddress, QString("新地址"));
  }

  void modelsKeepAuthorizedIdsAndResetCorrectly() {
    ShopModel shops;
    QAbstractItemModelTester shopTester(
        &shops, QAbstractItemModelTester::FailureReportingMode::QtTest);
    shops.replaceProjection({{"shop-id", "店铺", "简介", "地址", true}});
    QCOMPARE(shops.rowCount(), 1);
    QCOMPARE(shops.index(0, ShopModel::Name).data(ShopModel::IdRole).toString(),
             QString("shop-id"));
    DishModel dishes;
    QAbstractItemModelTester dishTester(
        &dishes, QAbstractItemModelTester::FailureReportingMode::QtTest);
    dishes.replaceProjection({{"dish-id", "shop-id", "菜品", 1234, true, false}});
    QCOMPARE(dishes.index(0, DishModel::Name).data(DishModel::ShopIdRole).toString(),
             QString("shop-id"));
    CartModel cart;
    QAbstractItemModelTester cartTester(
        &cart, QAbstractItemModelTester::FailureReportingMode::QtTest);
    cart.replaceProjection("shop-id", {{"dish-id", "shop-id", "菜品", 1234, 2, 2468}});
    QCOMPARE(cart.items().first().dishId, QString("dish-id"));
    QCOMPARE(cart.items().first().quantity, 2);
    cart.replaceProjection({}, {});
    QCOMPARE(cart.rowCount(), 0);
  }

  void dishRemovalClearsCartsInSameValidTransaction() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    JsonRepository repository(directory.filePath("appdata.json"));
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    CatalogService catalog(store, session);
    OrderService orders(store, session);
    QVERIFY(store.initialize().ok());
    QVERIFY(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}).ok());
    QVERIFY(catalog.createMerchantWithShop(
                          {"merchant", "Merchant!1", "商家", "店铺", {}, "店址"})
                .ok());
    QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
    QVERIFY(catalog.updateShop({"店铺", {}, "店址", true}).ok());
    const auto dishId = catalog.createDish({"菜", 1200, true});
    QVERIFY(dishId.ok());
    const auto shopId = store.snapshot().shops.first().id;
    QVERIFY(auth.logout().ok());
    QVERIFY(auth.registerAccount(
                          {"customer", "Customer!1", "顾客", "地址", Role::Customer})
                .ok());
    QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
    QVERIFY(orders.updateCart({shopId, {{dishId.value(), 1}}}).ok());
    QCOMPARE(store.snapshot().carts.size(), 1);
    QVERIFY(auth.logout().ok());
    QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
    QVERIFY(catalog.updateDish(dishId.value(), {"菜", 1200, false}).ok());
    QCOMPARE(store.snapshot().carts.size(), 0);
    QVERIFY(OrderPolicy::validateAll(store.snapshot()).ok());

    QVERIFY(catalog.updateDish(dishId.value(), {"菜", 1200, true}).ok());
    QVERIFY(auth.logout().ok());
    QVERIFY(auth.login("customer", "Customer!1", Role::Customer).ok());
    QVERIFY(orders.updateCart({shopId, {{dishId.value(), 1}}}).ok());
    QVERIFY(auth.logout().ok());
    QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
    QVERIFY(catalog.deleteDish(dishId.value()).ok());
    QCOMPARE(store.snapshot().carts.size(), 0);
    QVERIFY(OrderPolicy::validateAll(store.snapshot()).ok());
  }
};

QTEST_APPLESS_MAIN(CatalogTest)
#include "tst_catalog.moc"
