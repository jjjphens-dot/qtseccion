#pragma once
#include "entities.h"
#include "requests.h"
#include "result.h"

namespace takeout::Credentials {
inline constexpr auto Algorithm = "PBKDF2-HMAC-SHA256";
inline constexpr int Iterations = 600000;
inline constexpr int SaltBytes = 16;
inline constexpr int HashBytes = 32;

Result<Account> createAccount(const StoreSnapshot &snapshot,
                              const RegisterRequest &request);
// Prepares validated account fields and a fresh salt without doing the
// expensive digest. The returned Account is not persistable until finalized.
Result<Account> prepareAccount(const StoreSnapshot &snapshot,
                               const RegisterRequest &request);
Result<Account> finalizeAccount(Account account, const QByteArray &passwordHash);
// Pure CPU work used by the UI worker. It does not access application state.
Result<QByteArray> derivePbkdf2(const QByteArray &passwordUtf8,
                                const QByteArray &salt,
                                int iterations = Iterations);
Result<void> validateStored(const Account &account);
bool verifyPassword(const Account &account, const QString &password);
// Compares all positions up to the longer input without ordinary early exit.
// This is a fixed-work digest comparison helper, not an absolute timing proof.
bool constantTimeEqual(const QByteArray &left, const QByteArray &right);
} // namespace takeout::Credentials
