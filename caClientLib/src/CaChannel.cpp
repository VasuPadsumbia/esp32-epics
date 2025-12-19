#include "caClientLib/CaChannel.h"
#include "caClientLib/CaStatus.h"

#include <cstring>

namespace caClientLib {

CaChannel::CaChannel(const std::string &pvName, double timeoutSec)
    : pvName_(pvName), chid_(0)
{
    int st = ca_create_channel(pvName_.c_str(), 0, 0, CA_PRIORITY_DEFAULT, &chid_);
    CaStatus::requireOk(st, "ca_create_channel");

    st = ca_pend_io(timeoutSec);
    CaStatus::requireOk(st, "ca_pend_io (connect)");
}

CaChannel::~CaChannel()
{
    if (chid_) {
        ca_clear_channel(chid_);
    }
}

std::string CaChannel::getString(double timeoutSec) const
{
    dbr_string_t buf;
    std::memset(buf, 0, sizeof(buf));

    int st = ca_get(DBR_STRING, chid_, buf);
    CaStatus::requireOk(st, "ca_get");

    st = ca_pend_io(timeoutSec);
    CaStatus::requireOk(st, "ca_pend_io (get)");

    return std::string(buf);
}

void CaChannel::putString(const std::string &value, double timeoutSec) const
{
    dbr_string_t buf;
    std::memset(buf, 0, sizeof(buf));
    std::strncpy(buf, value.c_str(), sizeof(buf) - 1);

    int st = ca_put(DBR_STRING, chid_, buf);
    CaStatus::requireOk(st, "ca_put");

    st = ca_pend_io(timeoutSec);
    CaStatus::requireOk(st, "ca_pend_io (put)");
}

} // namespace caClientLib
