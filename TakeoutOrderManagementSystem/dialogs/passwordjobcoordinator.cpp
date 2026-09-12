#include "passwordjobcoordinator.h"
#include "core/credentials.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

namespace takeout {
PasswordJobCoordinator::PasswordJobCoordinator(QObject *parent)
    : QObject(parent) {}

quint64 PasswordJobCoordinator::start(QByteArray passwordUtf8,
                                      QByteArray salt, int iterations,
                                      Completion completion) {
  const auto generation = ++m_generation;
  auto *watcher = new QFutureWatcher<Result<QByteArray>>(this);
  connect(watcher, &QFutureWatcher<Result<QByteArray>>::finished, this,
          [this, watcher, generation,
           completion = std::move(completion)]() mutable {
            const auto result = watcher->result();
            watcher->deleteLater();
            if (generation != m_generation)
              return;
            completion(result);
          });
  watcher->setFuture(QtConcurrent::run(
      [passwordUtf8 = std::move(passwordUtf8), salt = std::move(salt),
       iterations]() mutable {
        const auto result =
            Credentials::derivePbkdf2(passwordUtf8, salt, iterations);
        passwordUtf8.fill('\0');
        salt.fill('\0');
        return result;
      }));
  return generation;
}

void PasswordJobCoordinator::invalidate() { ++m_generation; }
} // namespace takeout
