#pragma once
#include "data/apppaths.h"
#include "data/jsonrepository.h"
#include "services/authservice.h"
#include "services/catalogservice.h"
#include "services/orderservice.h"
#include "services/orderqueryservice.h"
#include "services/adminservice.h"
#include "services/statisticsservice.h"
#include <QLockFile>
namespace takeout {
// Composition root: lifetime order is repository -> store -> session -> services.
class AppContext final {
public:
    explicit AppContext(AppPaths paths);
    Result<StartupState> initialize();
    const AppPaths& paths() const { return m_paths; }
    AuthService& auth() { return m_auth; }
    CatalogService& catalog() { return m_catalog; }
    OrderQueryService& orderQuery() { return m_orderQuery; }
    SessionContext& session() { return m_session; }
    DataStore& store() { return m_store; }
private:
    AppPaths m_paths;
    QLockFile m_lock;
    JsonRepository m_repository;
    DataStore m_store;
    SessionContext m_session;
    AuthService m_auth;
    CatalogService m_catalog;
    OrderService m_orders;
    OrderQueryService m_orderQuery;
    AdminService m_admin;
    StatisticsService m_statistics;
};
} // namespace takeout
