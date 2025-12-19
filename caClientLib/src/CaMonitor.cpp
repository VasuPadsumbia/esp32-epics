#include "caClientLib/CaMonitor.h"
#include "caClientLib/CaStatus.h"

#include <cstring>

namespace caClientLib {

CaMonitor::CaMonitor(const std::string &pvName, double timeoutSec, IMonitorHandler &handler)
    : pvName_(pvName), chid_(0), evid_(0), handler_(&handler)
{
    int st = ca_create_channel(pvName_.c_str(), 0, 0, CA_PRIORITY_DEFAULT, &chid_);
    CaStatus::requireOk(st, "ca_create_channel");

    st = ca_pend_io(timeoutSec);
    CaStatus::requireOk(st, "ca_pend_io (connect)");

    st = ca_create_subscription(
        DBR_TIME_STRING,
        0,
        chid_,
        DBE_VALUE | DBE_ALARM,
        &CaMonitor::callback,
        this,
        &evid_);
    CaStatus::requireOk(st, "ca_create_subscription");
}

CaMonitor::~CaMonitor()
{
    if (evid_) {
        ca_clear_subscription(evid_);
    }
    if (chid_) {
        ca_clear_channel(chid_);
    }
}

void CaMonitor::callback(struct event_handler_args args)
{
    if (args.status != ECA_NORMAL || args.dbr == 0) {
        return;
    }

    CaMonitor *self = static_cast<CaMonitor *>(args.usr);
    if (!self || !self->handler_) {
        return;
    }

    const dbr_time_string *v = static_cast<const dbr_time_string *>(args.dbr);

    MonitorUpdate u;
    u.pvName = self->pvName_;
    u.value = std::string(v->value);
    u.alarmStatus = v->status;
    u.alarmSeverity = v->severity;
    u.ts.secPastEpoch = v->stamp.secPastEpoch;
    u.ts.nsec = v->stamp.nsec;

    self->handler_->onUpdate(u);
}

} // namespace caClientLib
