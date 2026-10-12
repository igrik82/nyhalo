# The following four lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
#set(CMAKE_CXX_STANDARD 20)
# set(EXTRA_COMPONENT_DIRS main build)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)


project(PIR_CAM VERSION ${PROJECT_VER} LANGUAGES C CXX ASM)
