#include "sessioncontext.h"
namespace takeout {
void SessionContext::clear() { m_current.reset(); emit changed(); }
void SessionContext::establish(Session session) { m_current = std::move(session); emit changed(); }
void SessionContext::updateDisplayName(const QString &displayName) {
    if (!m_current || m_current->displayName == displayName)
        return;
    m_current->displayName = displayName;
    emit changed();
}
} // namespace takeout
