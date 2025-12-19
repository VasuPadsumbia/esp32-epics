TOP = ../..
include $(TOP)/configure/CONFIG

# Avoid races like: `make -j clean install` deleting O.* while compiling.
.NOTPARALLEL: clean install

# Host-side Channel Access helper library
LIBRARY_HOST = caClientLib

caClientLib_SRCS += CaStatus.cpp
caClientLib_SRCS += CaContext.cpp
caClientLib_SRCS += CaChannel.cpp
caClientLib_SRCS += CaMonitor.cpp
caClientLib_SRCS += CaClient.cpp

caClientLib_LIBS += ca
caClientLib_LIBS += Com

USR_INCLUDES += -I$(TOP)/caClientLib/include

include $(TOP)/configure/RULES
