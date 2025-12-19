#ifndef CACL_CA_STATUS_H
#define CACL_CA_STATUS_H

#include <cadef.h>

namespace caClientLib {

class CaStatus {
public:
    static void requireOk(int status, const char *what);
};

} // namespace caClientLib

#endif
