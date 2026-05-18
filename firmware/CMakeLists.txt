# Top-level project CMake for ESP-IDF build (PlatformIO wraps this)
cmake_minimum_required(VERSION 3.16)

# Component search paths — drivers, tasks, utils live in subdirs
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/drivers
    ${CMAKE_CURRENT_LIST_DIR}/tasks
    ${CMAKE_CURRENT_LIST_DIR}/utils
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(elderly_companion_robot)
