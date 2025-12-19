#ifndef CACL_CA_CHANNEL_H
#define CACL_CA_CHANNEL_H

#include <cadef.h>

#include <string>

namespace caClientLib {

class CaChannel {
public:
    CaChannel(const std::string &pvName, double timeoutSec);
    ~CaChannel();

    CaChannel(const CaChannel &) = delete;
    CaChannel &operator=(const CaChannel &) = delete;

    const std::string &pvName() const { return pvName_; }
    chid chidHandle() const { return chid_; }

    std::string getString(double timeoutSec) const;
    void putString(const std::string &value, double timeoutSec) const;

private:
    std::string pvName_;
    chid chid_;
};

} // namespace caClientLib

#endif
