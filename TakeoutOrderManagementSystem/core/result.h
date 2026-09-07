#pragma once
#include <QString>
#include <optional>
#include <utility>
#include <variant>

namespace takeout {
enum class ErrorCode {
  Validation,
  NotFound,
  Forbidden,
  Conflict,
  InvalidTransition,
  AlreadyProcessed,
  Persistence,
  CorruptData,
  UnsupportedVersion,
  RecoveryAvailable,
  NotImplemented // Architecture milestone: explicit failure, never fake
                 // success.
};
struct Error {
  ErrorCode code;
  QString message;
  QString field;
};

template <typename T> class [[nodiscard]] Result {
public:
  static Result success(T value) { return Result(std::move(value)); }
  static Result failure(Error error) { return Result(std::move(error)); }
  bool ok() const { return std::holds_alternative<T>(m_result); }
  const T &value() const { return std::get<T>(m_result); }
  const Error &error() const { return std::get<Error>(m_result); }

private:
  explicit Result(T value) : m_result(std::move(value)) {}
  explicit Result(Error error) : m_result(std::move(error)) {}
  std::variant<T, Error> m_result;
};

template <> class [[nodiscard]] Result<void> {
public:
  static Result success() { return Result(std::nullopt); }
  static Result failure(Error error) { return Result(std::move(error)); }
  bool ok() const { return !m_error.has_value(); }
  const Error &error() const { return m_error.value(); }

private:
  explicit Result(std::optional<Error> error) : m_error(std::move(error)) {}
  std::optional<Error> m_error;
};

inline Error notImplemented(const QString &operation) {
  return {ErrorCode::NotImplemented,
          QStringLiteral("架构版本：%1 尚未实现").arg(operation),
          {}};
}
} // namespace takeout
