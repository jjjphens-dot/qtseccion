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
Result<void> validateStored(const Account &account);
bool verifyPassword(const Account &account, const QString &password);
// Compares all positions up to the longer input without ordinary early exit.
// This is a fixed-work digest comparison helper, not an absolute timing proof.
bool constantTimeEqual(const QByteArray &left, const QByteArray &right);
} // namespace takeout::Credentials
