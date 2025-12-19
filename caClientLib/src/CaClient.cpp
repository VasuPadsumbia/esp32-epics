#include "caClientLib/CaClient.h"

namespace caClientLib {

CaClient::CaClient() : ctx_()
{
}

std::string CaClient::getString(const std::string &pvName, double timeoutSec)
{
    CaChannel ch(pvName, timeoutSec);
    return ch.getString(timeoutSec);
}

void CaClient::putString(const std::string &pvName, const std::string &value, double timeoutSec)
{
    CaChannel ch(pvName, timeoutSec);
    ch.putString(value, timeoutSec);
}

std::unique_ptr<CaMonitor> CaClient::monitorStringTime(
    const std::string &pvName,
    double timeoutSec,
    IMonitorHandler &handler)
{
    return std::unique_ptr<CaMonitor>(new CaMonitor(pvName, timeoutSec, handler));
}

} // namespace caClientLib
