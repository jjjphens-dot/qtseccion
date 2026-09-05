#pragma once
#include "core/entities.h"
#include <QObject>
namespace takeout {
class AuthService;
struct Session { Id accountId; Role role; QString displayName; };
class SessionContext final : public QObject {
    Q_OBJECT
public:
    explicit SessionContext(QObject* parent = nullptr) : QObject(parent) {}
    std::optional<Session> current() const { return m_current; }
signals:
    void changed();
private:
    friend class AuthService;
    void clear();
    void establish(Session session);
    std::optional<Session> m_current;
};
} // namespace takeout
