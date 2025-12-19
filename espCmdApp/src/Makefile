TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = espCmd
# espCmd.dbd will be created and installed
DBD += espCmd.dbd

# espCmd.dbd will be made up from these files:
espCmd_DBD += base.dbd
espCmd_DBD += calcSupport.dbd
espCmd_DBD += asyn.dbd
espCmd_DBD += drvAsynSerialPort.dbd
espCmd_DBD += stream.dbd

# Include dbd files from all support applications:
#espCmd_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#espCmd_LIBS += xxx

# espCmd_registerRecordDeviceDriver.cpp derives from espCmd.dbd
espCmd_SRCS += espCmd_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
espCmd_SRCS_DEFAULT += espCmdMain.cpp
espCmd_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#espCmd_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
espCmd_LIBS += $(EPICS_BASE_IOC_LIBS)
espCmd_LIBS += calc
espCmd_LIBS += asyn
espCmd_LIBS += stream

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

