TOP = ../..
include $(TOP)/configure/CONFIG

# Avoid races like: `make -j clean install` deleting O.* while compiling.
.NOTPARALLEL: clean install

# Build a host-side CLI tool (Channel Access client)
PROD_HOST = caClient
caClient_SRCS += caClientMain.cpp

USR_INCLUDES += -I$(TOP)/caClientLib/include

# Link against our reusable library + EPICS CA
caClient_LIBS += caClientLib
caClient_LIBS += ca
caClient_LIBS += Com

include $(TOP)/configure/RULES
