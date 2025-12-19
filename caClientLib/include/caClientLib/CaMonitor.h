#ifndef CACL_CA_MONITOR_H
#define CACL_CA_MONITOR_H

#include <cadef.h>
#include <epicsTime.h>

#include <string>

namespace caClientLib {

struct MonitorUpdate {
    std::string pvName;
    std::string value;
    short alarmStatus = 0;
    short alarmSeverity = 0;
    epicsTimeStamp ts{};
};

class IMonitorHandler {
public:
    virtual ~IMonitorHandler() = default;
    virtual void onUpdate(const MonitorUpdate &u) = 0;
};

class CaMonitor {
public:
    CaMonitor(const std::string &pvName, double timeoutSec, IMonitorHandler &handler);
    ~CaMonitor();

    CaMonitor(const CaMonitor &) = delete;
    CaMonitor &operator=(const CaMonitor &) = delete;

private:
    static void callback(struct event_handler_args args);

    std::string pvName_;
    chid chid_;
    evid evid_;
    IMonitorHandler *handler_;
};

} // namespace caClientLib

#endif
