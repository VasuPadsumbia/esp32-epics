#include "caClientLib/CaStatus.h"

#include <iostream>
#include <stdexcept>

namespace caClientLib {

void CaStatus::requireOk(int status, const char *what)
{
    if (status == ECA_NORMAL) {
        return;
    }
    std::string msg = what ? what : "CA error";
    msg += ": ";
    msg += ca_message(status);
    throw std::runtime_error(msg);
}

} // namespace caClientLib
