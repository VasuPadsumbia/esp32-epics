# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/vasu-hdd/.espressif/v5.5.3/esp-idf/components/bootloader/subproject"
  "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader"
  "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix"
  "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix/tmp"
  "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix/src"
  "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/vasu-hdd/ESP-EPICS/epics_esp32_project/firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
