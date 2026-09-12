#include "credentials.h"
#include "validation.h"
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QUuid>

namespace takeout::Credentials {
namespace {
QByteArray derive(const QString &password, const QByteArray &salt,
                  int iterations) {
  return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                            password.toUtf8(), salt, iterations,
                                            HashBytes);
}

QByteArray randomSalt() {
  QByteArray result;
  result.reserve(SaltBytes);
  while (result.size() < SaltBytes) {
    const quint32 value = QRandomGenerator::system()->generate();
    for (int shift = 0; shift < 32 && result.size() < SaltBytes; shift += 8)
      result.append(char((value >> shift) & 0xff));
  }
  return result;
}

Result<Account> failure(const Error &error) {
  return Result<Account>::failure(error);
}
} // namespace

Result<QByteArray> derivePbkdf2(const QByteArray &passwordUtf8,
                                const QByteArray &salt, int iterations) {
  if (salt.size() != SaltBytes || iterations <= 0)
    return Result<QByteArray>::failure(
        {ErrorCode::CorruptData, QStringLiteral("密码派生参数无效"),
         QStringLiteral("passwordSalt")});
  return Result<QByteArray>::success(derive(passwordUtf8, salt, iterations));
}

Result<Account> prepareAccount(const StoreSnapshot &snapshot,
                               const RegisterRequest &request) {
  auto checked = Validation::loginName(request.loginName);
  if (!checked.ok())
    return failure(checked.error());
  checked = Validation::password(request.password);
  if (!checked.ok())
    return failure(checked.error());
  checked = Validation::displayName(request.displayName);
  if (!checked.ok())
    return failure(checked.error());
  if (request.role == Role::Customer) {
    checked = Validation::address(request.address);
    if (!checked.ok())
      return failure(checked.error());
  }

  const auto login = Validation::normalizeLoginName(request.loginName);
  for (const auto &account : snapshot.accounts) {
    if (Validation::normalizeLoginName(account.loginName) == login)
      return Result<Account>::failure(
          {ErrorCode::Conflict, QStringLiteral("账号已存在"), "loginName"});
  }

  Account account;
  account.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  account.loginName = login;
  account.displayName =
      request.displayName.normalized(QString::NormalizationForm_C).trimmed();
  account.role = request.role;
  account.defaultAddress =
      request.role == Role::Customer
          ? request.address.normalized(QString::NormalizationForm_C).trimmed()
          : QString{};
  account.passwordSalt = randomSalt();
  account.passwordIterations = Iterations;
  account.passwordAlgorithm = QString::fromLatin1(Algorithm);
  account.createdAt = QDateTime::currentDateTimeUtc();
  return Result<Account>::success(std::move(account));
}

Result<Account> finalizeAccount(Account account,
                                const QByteArray &passwordHash) {
  if (passwordHash.size() != HashBytes)
    return Result<Account>::failure(
        {ErrorCode::CorruptData, QStringLiteral("密码派生值长度无效"),
         QStringLiteral("passwordHash")});
  account.passwordHash = passwordHash;
  return Result<Account>::success(std::move(account));
}

Result<Account> createAccount(const StoreSnapshot &snapshot,
                              const RegisterRequest &request) {
  const auto prepared = prepareAccount(snapshot, request);
  if (!prepared.ok())
    return prepared;
  const auto derived = derivePbkdf2(request.password.toUtf8(),
                                    prepared.value().passwordSalt,
                                    prepared.value().passwordIterations);
  if (!derived.ok())
    return Result<Account>::failure(derived.error());
  return finalizeAccount(prepared.value(), derived.value());
}

Result<void> validateStored(const Account &account) {
  if (account.passwordAlgorithm != QLatin1String(Algorithm) ||
      account.passwordIterations != Iterations ||
      account.passwordSalt.size() != SaltBytes ||
      account.passwordHash.size() != HashBytes)
    return Result<void>::failure({ErrorCode::CorruptData,
                                  QStringLiteral("账号密码凭据无效"),
                                  QStringLiteral("passwordHash")});
  return Result<void>::success();
}

bool verifyPassword(const Account &account, const QString &password) {
  if (!validateStored(account).ok())
    return false;
  const auto derived = derivePbkdf2(password.toUtf8(), account.passwordSalt,
                                    account.passwordIterations);
  return derived.ok() && constantTimeEqual(derived.value(), account.passwordHash);
}

bool constantTimeEqual(const QByteArray &left, const QByteArray &right) {
  const auto size = qMax(left.size(), right.size());
  quint64 difference = static_cast<quint64>(left.size()) ^
                       static_cast<quint64>(right.size());
  for (qsizetype i = 0; i < size; ++i) {
    const auto lhs = i < left.size() ? static_cast<quint8>(left.at(i)) : 0;
    const auto rhs = i < right.size() ? static_cast<quint8>(right.at(i)) : 0;
    difference |= static_cast<quint64>(lhs ^ rhs);
  }
  return difference == 0;
}
} // namespace takeout::Credentials
