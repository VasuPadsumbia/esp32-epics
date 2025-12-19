cmake_minimum_required(VERSION 3.16)

# Root-level ESP-IDF project wrapper.
# This allows configuring from the repository root (e.g. VS Code tasks that run
# `cmake -S ${workspaceFolder}`) while keeping the actual app sources in `esp32/`.

if(DEFINED IDF_PATH AND NOT "${IDF_PATH}" STREQUAL "")
  # Allow passing -DIDF_PATH=/path/to/esp-idf without requiring the environment
  # to be pre-configured.
  set(ENV{IDF_PATH} "${IDF_PATH}")
endif()

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
  # Best-effort auto-detect for common ESP-IDF install locations.
  if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    file(GLOB _idf_candidates
      LIST_DIRECTORIES true
      "$ENV{HOME}/esp/esp-idf"
      "$ENV{HOME}/esp/*/esp-idf"
      "$ENV{HOME}/esp/v*/esp-idf"
    )
    if(_idf_candidates)
      list(SORT _idf_candidates ORDER DESCENDING)
      foreach(_cand IN LISTS _idf_candidates)
        if(EXISTS "${_cand}/tools/cmake/project.cmake")
          set(ENV{IDF_PATH} "${_cand}")
          break()
        endif()
      endforeach()
    endif()
  endif()
endif()

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
  message(FATAL_ERROR "IDF_PATH is not set and ESP-IDF could not be auto-detected. Either open an ESP-IDF environment (source export.sh) or pass -DIDF_PATH=/path/to/esp-idf when configuring.")
endif()

# Treat `esp32/` as an extra component directory. The component there provides
# the application's entry point (app_main) via epics_esp32.c.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/esp32")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ioc_esp32)
