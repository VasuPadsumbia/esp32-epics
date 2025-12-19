#!../../bin/linux-x86_64/espCmd

#- You may have to change espCmd to something else
#- everywhere it appears in this file

## Ensure relative includes (envPaths) work even when launched elsewhere
cd "${IOC_BOOT_DIR}"

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/espCmd.dbd"
espCmd_registerRecordDeviceDriver pdbbase

# -- Serial Port Configuration --
drvAsynSerialPortConfigure("vasu-usb","/dev/ttyACM0",0,0,0)

# Serial Port Parameters
asynSetOption("vasu-usb", 0, "baud", "115200")
asynSetOption("vasu-usb", 0, "parity", "none")
asynSetOption("vasu-usb", 0, "stop", "1")
asynSetOption("vasu-usb", 0, "bits", "8")

# Debugging Options
asynSetTraceIOMask("vasu-usb", 0, 2)
asynSetTraceMask("vasu-usb", 0, 9)

#- StreamDevice Configuration -
epicsEnvSet("STREAM_PROTOCOL_PATH","${TOP}/espCmdApp/protocol")

# Enable StreamDevice debug output
epicsEnvSet("STREAM_DEVICE_DEBUG","1")

## Load record instances
dbLoadRecords("${TOP}/espCmdApp/Db/espCmd.db","P=ESP:,PORT=vasu-usb,user=ESP")

cd "${TOP}/espCmdApp/Db"
dbLoadTemplate("gpio.substitutions")
cd "${TOP}"

#cd "${TOP}/iocBoot/${IOC}"
iocInit

## Start any sequence programs
#seq sncxxx,"user=vasu-usb"
