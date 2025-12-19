#include "caClientLib/CaContext.h"
#include "caClientLib/CaStatus.h"

namespace caClientLib {

CaContext::CaContext() : created_(false)
{
    int st = ca_context_create(ca_disable_preemptive_callback);
    CaStatus::requireOk(st, "ca_context_create");
    created_ = true;
}

CaContext::~CaContext()
{
    if (created_) {
        ca_context_destroy();
    }
}

void CaContext::pendEvent(double seconds)
{
    ca_pend_event(seconds);
}

} // namespace caClientLib
