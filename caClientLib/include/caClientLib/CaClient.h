#ifndef CACL_CA_CLIENT_H
#define CACL_CA_CLIENT_H

#include "caClientLib/CaChannel.h"
#include "caClientLib/CaContext.h"
#include "caClientLib/CaMonitor.h"

#include <memory>
#include <string>

namespace caClientLib {

class CaClient {
public:
    CaClient();

    std::string getString(const std::string &pvName, double timeoutSec);
    void putString(const std::string &pvName, const std::string &value, double timeoutSec);

    std::unique_ptr<CaMonitor> monitorStringTime(
        const std::string &pvName,
        double timeoutSec,
        IMonitorHandler &handler);

    void pendEvent(double seconds) { ctx_.pendEvent(seconds); }

private:
    CaContext ctx_;
};

} // namespace caClientLib

#endif
