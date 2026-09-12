#pragma once

#include "core/result.h"
#include <QObject>
#include <functional>

namespace takeout {
class PasswordJobCoordinator final : public QObject {
  Q_OBJECT
public:
  using Completion = std::function<void(Result<QByteArray>)>;

  explicit PasswordJobCoordinator(QObject *parent = nullptr);
  quint64 start(QByteArray passwordUtf8, QByteArray salt, int iterations,
                Completion completion);
  void invalidate();

private:
  quint64 m_generation = 0;
};
} // namespace takeout
