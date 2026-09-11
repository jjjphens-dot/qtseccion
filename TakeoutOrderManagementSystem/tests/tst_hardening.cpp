#include <QtTest>

#include "app/appcontext.h"
#include "core/credentials.h"
#include "data/jsoncodec.h"
#include "data/jsonrepository.h"

#include <QFile>
#include <QTemporaryDir>

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
  snapshot.savedAt = at(revision);
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
} // namespace

class HardeningTest final : public QObject {
  Q_OBJECT
private slots:
  void startupRecoveryRequiresVerifiedBackupAndReinitializesStore() {
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

    writeFile(path, "{damaged");
    AppContext context(AppPaths::resolve(dir.path()));
    const auto startup = context.initialize();
    QVERIFY(!startup.ok());
    QCOMPARE(startup.error().code, ErrorCode::RecoveryAvailable);

    const auto recovered = context.recoverFromBackup();
    QVERIFY(recovered.ok());
    QCOMPARE(recovered.value(), StartupState::Ready);
    QCOMPARE(context.store().snapshot().revision, qint64(1));
    QCOMPARE(repository.load().value().revision, qint64(1));
  }

  void adminCanExportImportAndRestoreWithoutAcceptingCorruptData() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
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
    writeFile(exportPath, "not-json");
    const auto rejected = context.admin().importData(exportPath);
    QVERIFY(!rejected.ok());
    QCOMPARE(rejected.error().code, ErrorCode::CorruptData);
    QCOMPARE(context.store().snapshot().revision, revision);

    QVERIFY(context.admin().exportData(exportPath).ok());
    QVERIFY(context.admin().importData(exportPath).ok());
    QVERIFY(context.admin().restoreBackup().ok());
    QVERIFY(context.store().snapshot().revision > revision);
    QVERIFY(!context.store().snapshot().accounts.isEmpty());
  }

  void passwordComparisonUsesFixedLoopAndRejectsLengthMismatch() {
    const QByteArray expected("0123456789abcdef");
    QVERIFY(Credentials::constantTimeEqual(expected, expected));
    QVERIFY(!Credentials::constantTimeEqual(expected, "0123456789abcdee"));
    QVERIFY(!Credentials::constantTimeEqual(expected, "short"));
    QVERIFY(Credentials::constantTimeEqual({}, {}));
    QVERIFY(!Credentials::constantTimeEqual({}, "x"));
  }
};

QTEST_APPLESS_MAIN(HardeningTest)
#include "tst_hardening.moc"
