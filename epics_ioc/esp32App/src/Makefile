TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = esp32
# esp32.dbd will be created and installed
DBD += esp32.dbd

# esp32.dbd will be made up from these files:
esp32_DBD += base.dbd

# Include dbd files from all support applications:
esp32_DBD += asyn.dbd
esp32_DBD += drvAsynSerialPort.dbd
esp32_DBD += drvAsynIPPort.dbd
esp32_DBD += stream.dbd

# Add all the support libraries needed by this IOC
esp32_LIBS += asyn
esp32_LIBS += stream

# esp32_registerRecordDeviceDriver.cpp derives from esp32.dbd
esp32_SRCS += esp32_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
esp32_SRCS_DEFAULT += esp32Main.cpp
esp32_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#esp32_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
esp32_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

