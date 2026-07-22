cmake_minimum_required(VERSION 4.4)
project(
    umkacxx
    VERSION 0.0.0
    DESCRIPTION "Umka CXX Wrapper"
    LANGUAGES C CXX
)
set(CMAKE_EXPERIMENTAL_FIND_CPS_PACKAGES ON)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
include(FetchContent)
include(GNUInstallDirs)
include(CTest)

include(options.cmake)
add_subdirectory(src)
if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
