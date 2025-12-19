#include "caClientLib/CaClient.h"

#include <epicsTime.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string prefix = "ESP:";
    double timeoutSec = 2.0;
    double monitorDurationSec = 0.0; // 0 = run forever unless count set
    int monitorCount = 0;            // 0 = unlimited unless duration set
};

static void printUsage(const char *argv0)
{
    const char *prog = argv0;
    if (argv0) {
        const char *slash = std::strrchr(argv0, '/');
        if (slash && *(slash + 1) != '\0') {
            prog = slash + 1;
        }
    }

    std::cerr
        << "Usage:\n"
        << "  " << prog << " [--prefix PFX] [--timeout SEC] get <pv>\n"
        << "  " << prog << " [--prefix PFX] [--timeout SEC] put <pv> <value>\n"
        << "  " << prog << " [--prefix PFX] [--timeout SEC] monitor <pv> [--duration SEC] [--count N]\n\n"
        << "Examples (your StreamDevice IOC PVs):\n"
        << "  " << prog << " get led\n"
        << "  " << prog << " put led 1\n"
        << "  " << prog << " get ai0:mean\n"
        << "  " << prog << " monitor ai0:mean --duration 5\n";
}

static std::string fullPvName(const Options &opt, const std::string &pv)
{
    if (opt.prefix.empty()) {
        return pv;
    }
    if (pv.rfind(opt.prefix, 0) == 0) {
        return pv;
    }
    return opt.prefix + pv;
}

static bool parseInt(const std::string &s, int &out)
{
    char *end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

static bool parseDouble(const std::string &s, double &out)
{
    char *end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (!end || *end != '\0') {
        return false;
    }
    out = v;
    return true;
}

static void printTimeStamp(const epicsTimeStamp &ts)
{
    char buf[64];
    if (epicsTimeToStrftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.%06f", &ts) == 0) {
        std::cout << "(time?)";
        return;
    }
    std::cout << buf;
}

class PrintHandler final : public caClientLib::IMonitorHandler {
public:
    void onUpdate(const caClientLib::MonitorUpdate &u) override
    {
        printTimeStamp(u.ts);
        std::cout << " " << u.pvName << " = " << u.value
                  << " (stat=" << u.alarmStatus << ", sevr=" << u.alarmSeverity << ")\n";
        ++seen_;
    }

    int seen() const { return seen_; }

private:
    int seen_ = 0;
};

static void cmdGet(const Options &opt, const std::string &pvArg)
{
    caClientLib::CaClient client;
    const std::string pv = fullPvName(opt, pvArg);
    std::cout << pv << " = " << client.getString(pv, opt.timeoutSec) << "\n";
}

static void cmdPut(const Options &opt, const std::string &pvArg, const std::string &value)
{
    caClientLib::CaClient client;
    const std::string pv = fullPvName(opt, pvArg);
    client.putString(pv, value, opt.timeoutSec);
}

static void cmdMonitor(const Options &opt, const std::string &pvArg)
{
    caClientLib::CaClient client;
    const std::string pv = fullPvName(opt, pvArg);

    PrintHandler handler;
    std::unique_ptr<caClientLib::CaMonitor> mon = client.monitorStringTime(pv, opt.timeoutSec, handler);

    epicsTimeStamp start{};
    epicsTimeGetCurrent(&start);

    while (true) {
        client.pendEvent(0.1);

        if (opt.monitorCount > 0 && handler.seen() >= opt.monitorCount) {
            break;
        }

        if (opt.monitorDurationSec > 0.0) {
            epicsTimeStamp now{};
            epicsTimeGetCurrent(&now);
            const double elapsed = epicsTimeDiffInSeconds(&now, &start);
            if (elapsed >= opt.monitorDurationSec) {
                break;
            }
        }
    }
}

static int run(int argc, char **argv)
{
    if (argc >= 2) {
        const std::string firstArg(argv[1]);
        if (firstArg == "--help" || firstArg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    Options opt;

    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        args.emplace_back(argv[i]);
    }

    // Parse global options
    size_t idx = 0;
    while (idx < args.size()) {
        if (args[idx] == "--prefix" && idx + 1 < args.size()) {
            opt.prefix = args[idx + 1];
            idx += 2;
            continue;
        }
        if (args[idx] == "--timeout" && idx + 1 < args.size()) {
            double t;
            if (!parseDouble(args[idx + 1], t) || t <= 0.0) {
                std::cerr << "Invalid --timeout\n";
                return 2;
            }
            opt.timeoutSec = t;
            idx += 2;
            continue;
        }
        break;
    }

    if (idx >= args.size()) {
        printUsage(argv[0]);
        return 2;
    }

    const std::string cmd = args[idx++];

    try {
        if (cmd == "get") {
            if (idx + 1 != args.size()) {
                printUsage(argv[0]);
                return 2;
            }
            cmdGet(opt, args[idx]);
            return 0;
        }

        if (cmd == "put") {
            if (idx + 2 != args.size()) {
                printUsage(argv[0]);
                return 2;
            }
            cmdPut(opt, args[idx], args[idx + 1]);
            return 0;
        }

        if (cmd == "monitor") {
            if (idx >= args.size()) {
                printUsage(argv[0]);
                return 2;
            }

            const std::string pv = args[idx++];

            while (idx < args.size()) {
                if (args[idx] == "--duration" && idx + 1 < args.size()) {
                    double d;
                    if (!parseDouble(args[idx + 1], d) || d <= 0.0) {
                        std::cerr << "Invalid --duration\n";
                        return 2;
                    }
                    opt.monitorDurationSec = d;
                    idx += 2;
                    continue;
                }
                if (args[idx] == "--count" && idx + 1 < args.size()) {
                    int c;
                    if (!parseInt(args[idx + 1], c) || c <= 0) {
                        std::cerr << "Invalid --count\n";
                        return 2;
                    }
                    opt.monitorCount = c;
                    idx += 2;
                    continue;
                }

                std::cerr << "Unknown option: " << args[idx] << "\n";
                return 2;
            }

            cmdMonitor(opt, pv);
            return 0;
        }

        printUsage(argv[0]);
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}

} // namespace

int main(int argc, char **argv)
{
    return run(argc, argv);
}
