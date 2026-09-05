#include "sessioncontext.h"
namespace takeout {
void SessionContext::clear() { m_current.reset(); emit changed(); }
void SessionContext::establish(Session session) { m_current = std::move(session); emit changed(); }
} // namespace takeout
