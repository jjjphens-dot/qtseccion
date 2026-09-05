#pragma once
#include "repository.h"
#include <QObject>
namespace takeout {
class ServiceBase;
class DataStore final : public QObject {
    Q_OBJECT
public:
    explicit DataStore(Repository& repository, QObject* parent = nullptr);
    Result<StartupState> initialize();
    bool isInitialized() const { return m_initialized; }
    // Copy-on-write value snapshot; no mutable references escape this owner.
    StoreSnapshot snapshot() const { return m_snapshot; }
signals:
    void committed(qint64 revision);
private:
    friend class ServiceBase;
    Result<void> commitCandidate(StoreSnapshot candidate);
    Repository& m_repository;
    StoreSnapshot m_snapshot;
    bool m_initialized = false;
};
} // namespace takeout
