#!../../bin/linux-x86_64/esp32

< envPaths
< env_project.cmd

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/esp32.dbd"
esp32_registerRecordDeviceDriver pdbbase

## Search path for protocol files
epicsEnvSet("STREAM_PROTOCOL_PATH", "${TOP}/db")

## Configure Serial Port
drvAsynSerialPortConfigure("ESP32_SERIAL", "${SERIAL_PORT}", 0, 0, 0)
asynSetOption("ESP32_SERIAL", -1, "baud", "115200")
asynSetOption("ESP32_SERIAL", -1, "bits", "8")
asynSetOption("ESP32_SERIAL", -1, "parity", "none")
asynSetOption("ESP32_SERIAL", -1, "stop", "1")

## Configure WiFi Port (As documented)
drvAsynIPPortConfigure("ESP32_WIFI", "${ESP32_IP}:${TCP_PORT}", 0, 0, 0)

## Load Global Records
# Using Serial as primary for this test run, but WiFi is now defined
dbLoadRecords("${TOP}/db/esp32.db", "PORT=ESP32_SERIAL")

## Load Per-Pin Records for all standard GPIOs
## This matches the "version 12" approach of per-pin instantiation
# Grouping some common pins
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=2")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=4")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=12")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=13")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=14")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=15")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=16")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=17")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=18")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=19")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=21")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=22")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=23")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=25")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=26")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=27")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=32")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=33")

# Inputs only
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=34")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=35")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=36")
dbLoadRecords("${TOP}/db/esp32_pin.db", "PORT=ESP32_SERIAL,PIN=39")

cd "${TOP}/iocBoot/${IOC}"
iocInit
