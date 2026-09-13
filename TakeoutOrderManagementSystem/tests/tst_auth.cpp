#include "core/credentials.h"
#include "core/orderpolicy.h"
#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "services/authservice.h"
#include "services/catalogservice.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace takeout;
namespace {
class MemoryRepository final : public Repository {
public:
  StoreSnapshot disk;
  bool failSave = false;
  int writes = 0;
  Result<StoreSnapshot> load() const override {
    return Result<StoreSnapshot>::success(disk);
  }
  Result<void> save(const StoreSnapshot &snapshot) override {
    ++writes;
    if (failSave)
      return Result<void>::failure(
          {ErrorCode::Persistence, QStringLiteral("injected"), {}});
    disk = snapshot;
    return Result<void>::success();
  }
};
} // namespace

class AuthTest final : public QObject {
  Q_OBJECT
private slots:
  void bootstrapIsAtomicAndPersistsOnlyDerivedCredentials() {
    MemoryRepository repository;
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    QCOMPARE(store.initialize().value(), StartupState::NeedsAdminBootstrap);
    QVERIFY(!auth.bootstrapAdmin({"admin", "short", "管理员"}).ok());
    QCOMPARE(repository.writes, 0);
    repository.failSave = true;
    const AdminBootstrapRequest request{"Root_Admin", "Admin!234",
                                        "初始管理员"};
    QVERIFY(!auth.bootstrapAdmin(request).ok());
    QCOMPARE(repository.writes, 1);
    QVERIFY(store.snapshot().isEmpty());
    QVERIFY(!session.current());
    repository.failSave = false;
    QVERIFY(auth.bootstrapAdmin(request).ok());
    QCOMPARE(repository.writes, 2);
    QCOMPARE(store.snapshot().accounts.size(), 1);
    const auto account = store.snapshot().accounts.first();
    QCOMPARE(account.loginName, QString("root_admin"));
    QCOMPARE(account.role, Role::Admin);
    QCOMPARE(account.passwordSalt.size(), Credentials::SaltBytes);
    QCOMPARE(account.passwordHash.size(), Credentials::HashBytes);
    QCOMPARE(account.passwordIterations, Credentials::Iterations);
    QCOMPARE(account.passwordAlgorithm,
             QString::fromLatin1(Credentials::Algorithm));
    QVERIFY(!account.passwordHash.contains(request.password.toUtf8()));
    QVERIFY(!auth.bootstrapAdmin(request).ok());
    QCOMPARE(repository.writes, 2);
  }

  void registrationBoundariesAndMerchantShopAreSingleTransactions() {
    MemoryRepository repository;
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    CatalogService catalog(store, session);
    QVERIFY(store.initialize().ok());
    QVERIFY(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}).ok());
    QCOMPARE(repository.writes, 1);
    RegisterRequest forbidden{
        "merchant_direct", "Merchant!1", "商家", {}, Role::Merchant};
    QVERIFY(!auth.registerAccount(forbidden).ok());
    forbidden.role = Role::Admin;
    QVERIFY(!auth.registerAccount(forbidden).ok());
    QCOMPARE(repository.writes, 1);
    RegisterRequest customer{" Customer_1 ", "Customer!1", " 顾客甲 ",
                             " 地址甲 ", Role::Customer};
    QVERIFY(auth.registerAccount(customer).ok());
    QCOMPARE(repository.writes, 2);
    QCOMPARE(store.snapshot().accounts.last().loginName, QString("customer_1"));
    QCOMPARE(store.snapshot().accounts.last().defaultAddress,
             QString("地址甲"));
    auto duplicate = customer;
    duplicate.loginName = "CUSTOMER_1";
    QVERIFY(!auth.registerAccount(duplicate).ok());
    QCOMPARE(repository.writes, 2);
    RegisterRequest rider{"rider_1", "Rider!234", "骑手甲", "不应保存",
                          Role::Rider};
    QVERIFY(auth.registerAccount(rider).ok());
    QCOMPARE(store.snapshot().accounts.last().defaultAddress, QString());
    QCOMPARE(repository.writes, 3);
    MerchantRegistration invalid{"merchant_1", "Merchant!1", "商家甲",
                                 " ",          "简介",       "店铺地址"};
    QVERIFY(!catalog.createMerchantWithShop(invalid).ok());
    QCOMPARE(repository.writes, 3);
    MerchantRegistration merchant{"merchant_1", "Merchant!1", "商家甲",
                                  " 店铺甲 ",   " 简介 ",     " 店铺地址 "};
    QVERIFY(catalog.createMerchantWithShop(merchant).ok());
    QCOMPARE(repository.writes, 4);
    QCOMPARE(store.snapshot().shops.size(), 1);
    const auto shop = store.snapshot().shops.first();
    const auto merchantAccount = store.snapshot().accounts.last();
    QCOMPARE(merchantAccount.role, Role::Merchant);
    QCOMPARE(shop.merchantId, merchantAccount.id);
    QCOMPARE(shop.name, QString("店铺甲"));
    QCOMPARE(shop.address, QString("店铺地址"));
    QVERIFY(!shop.isOpen);
    QVERIFY(OrderPolicy::validateAll(store.snapshot()).ok());
  }

  void loginEnforcesPasswordRoleDeletionAndSessionLifecycle() {
    MemoryRepository repository;
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    CatalogService catalog(store, session);
    QVERIFY(store.initialize().ok());
    QVERIFY(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}).ok());
    QVERIFY(auth.registerAccount(
                    {"customer", "Customer!1", "顾客", "地址", Role::Customer})
                .ok());
    QVERIFY(
        auth.registerAccount({"rider", "Rider!234", "骑手", {}, Role::Rider})
            .ok());
    QVERIFY(catalog
                .createMerchantWithShop(
                    {"merchant", "Merchant!1", "商家", "店铺", {}, "店址"})
                .ok());
    QSignalSpy changes(&session, &SessionContext::changed);
    QVERIFY(!auth.login("customer", "wrong-password", Role::Customer).ok());
    QVERIFY(!session.current());
    QVERIFY(!auth.login("customer", "Customer!1", Role::Merchant).ok());
    QVERIFY(auth.login(" CUSTOMER ", "Customer!1", Role::Customer).ok());
    QCOMPARE(session.current()->role, Role::Customer);
    QVERIFY(!auth.login("admin", "Admin!234", Role::Admin).ok());
    QVERIFY(auth.logout().ok());
    QVERIFY(!session.current());
    QVERIFY(auth.login("merchant", "Merchant!1", Role::Merchant).ok());
    QVERIFY(auth.logout().ok());
    QVERIFY(auth.login("rider", "Rider!234", Role::Rider).ok());
    QVERIFY(auth.logout().ok());
    QVERIFY(auth.login("admin", "Admin!234", Role::Admin).ok());
    QVERIFY(auth.logout().ok());
    QCOMPARE(changes.count(), 8);
    auto deleted = repository.disk;
    for (auto &account : deleted.accounts)
      if (account.loginName == "customer")
        account.isDeleted = true;
    MemoryRepository deletedRepository;
    deletedRepository.disk = deleted;
    DataStore deletedStore(deletedRepository);
    SessionContext deletedSession;
    AuthService deletedAuth(deletedStore, deletedSession);
    QVERIFY(deletedStore.initialize().ok());
    QVERIFY(!deletedAuth.login("customer", "Customer!1", Role::Customer).ok());
    QVERIFY(!deletedSession.current());
  }

  void restartAuthenticatesPersistedDerivedAccount() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("appdata.json");
    {
      JsonRepository repository(path);
      DataStore store(repository);
      SessionContext session;
      AuthService auth(store, session);
      QCOMPARE(store.initialize().value(), StartupState::NeedsAdminBootstrap);
      QVERIFY(auth.bootstrapAdmin({"admin", "Admin!234", "管理员"}).ok());
    }
    JsonRepository repository(path);
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    QCOMPARE(store.initialize().value(), StartupState::Ready);
    QVERIFY(auth.login("admin", "Admin!234", Role::Admin).ok());
    QCOMPARE(session.current()->displayName, QString("管理员"));
  }

  void invalidStoredCredentialSaltIsRejected() {
    Account account;
    account.passwordAlgorithm = QString::fromLatin1(Credentials::Algorithm);
    account.passwordIterations = Credentials::Iterations;
    account.passwordSalt = QByteArray(Credentials::SaltBytes - 1, 's');
    account.passwordHash = QByteArray(Credentials::HashBytes, 'h');
    QVERIFY(!Credentials::validateStored(account).ok());
    account.passwordSalt = QByteArray(Credentials::SaltBytes + 1, 's');
    QVERIFY(!Credentials::validateStored(account).ok());
    account.passwordSalt = QByteArray(Credentials::SaltBytes, 's');
    account.passwordHash = QByteArray(Credentials::HashBytes - 1, 'h');
    QVERIFY(!Credentials::validateStored(account).ok());
  }

  void asyncPasswordFlowsRevalidateSnapshotBeforeCommit() {
    MemoryRepository repository;
    DataStore store(repository);
    SessionContext session;
    AuthService auth(store, session);
    QVERIFY(store.initialize().ok());

    const auto bootstrap = auth.beginBootstrap(
        {"admin", "Admin!234", "管理员"});
    QVERIFY(bootstrap.ok());
    const auto bootstrapHash = Credentials::derivePbkdf2(
        bootstrap.value().passwordUtf8,
        bootstrap.value().account.passwordSalt,
        bootstrap.value().account.passwordIterations);
    QVERIFY(bootstrapHash.ok());
    QVERIFY(auth.completeBootstrap(bootstrap.value(), bootstrapHash.value())
                .ok());

    const RegisterRequest customer{"async_customer", "Customer!1", "顾客",
                                  "地址", Role::Customer};
    const auto draft = auth.beginAccountRegistration(customer);
    QVERIFY(draft.ok());
    QVERIFY(auth.registerAccount(
                         {"revision_bump", "Rider!234", "骑手", {},
                          Role::Rider})
                .ok());
    const auto customerHash = Credentials::derivePbkdf2(
        draft.value().passwordUtf8, draft.value().account.passwordSalt,
        draft.value().account.passwordIterations);
    QVERIFY(customerHash.ok());
    QCOMPARE(auth.completeAccountRegistration(draft.value(),
                                              customerHash.value())
                  .error()
                  .code,
             ErrorCode::Conflict);

    const auto fresh = auth.beginAccountRegistration(customer);
    QVERIFY(fresh.ok());
    const auto freshHash = Credentials::derivePbkdf2(
        fresh.value().passwordUtf8, fresh.value().account.passwordSalt,
        fresh.value().account.passwordIterations);
    QVERIFY(freshHash.ok());
    QVERIFY(auth.completeAccountRegistration(fresh.value(), freshHash.value())
                .ok());

    const auto login =
        auth.beginLogin("async_customer", Role::Customer);
    QVERIFY(login.ok());
    QVERIFY(auth.registerAccount(
                         {"revision_bump_2", "Rider!345", "骑手二", {},
                          Role::Rider})
                .ok());
    const auto loginHash = Credentials::derivePbkdf2(
        "Customer!1", login.value().passwordSalt,
        login.value().passwordIterations);
    QVERIFY(loginHash.ok());
    QCOMPARE(auth.completeLogin(login.value(), loginHash.value())
                  .error()
                  .code,
             ErrorCode::Conflict);
    QVERIFY(!session.current());

    const auto freshLogin =
        auth.beginLogin("async_customer", Role::Customer);
    QVERIFY(freshLogin.ok());
    const auto freshLoginHash = Credentials::derivePbkdf2(
        "Customer!1", freshLogin.value().passwordSalt,
        freshLogin.value().passwordIterations);
    QVERIFY(freshLoginHash.ok());
    QVERIFY(auth.completeLogin(freshLogin.value(), freshLoginHash.value())
                .ok());
    QVERIFY(session.current());
  }
};
QTEST_APPLESS_MAIN(AuthTest)
#include "tst_auth.moc"
