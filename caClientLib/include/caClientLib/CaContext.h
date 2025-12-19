#ifndef CACL_CA_CONTEXT_H
#define CACL_CA_CONTEXT_H

#include <cadef.h>

namespace caClientLib {

class CaContext {
public:
    CaContext();
    ~CaContext();

    CaContext(const CaContext &) = delete;
    CaContext &operator=(const CaContext &) = delete;

    void pendEvent(double seconds);

private:
    bool created_;
};

} // namespace caClientLib

#endif
