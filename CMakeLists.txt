# Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
# wechat:angaio/13707128443

#-------------------------------------------------------------------------------
# IrIsp — top-level build (distribution layout).
#
#   IrIspDemoApp   the Qt user interface (source, built here)
#   IrIspCore      the enhancement algorithm  — shipped PREBUILT (DLL + import
#   IrMp4Rec       8-bit MP4 recorder          lib + one public header each);
#   IrRaw16Rec     16-bit FFV1/MKV recorder    their sources are NOT distributed.
#
# The three libraries are consumed as imported targets: you only compile the
# application against their public headers and link their import libraries in
# bin/. At run time the matching DLLs (already in bin/) sit next to the exe.
#
# Point CMake at your Qt and OpenCV installs, e.g.
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
#         -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64 ^
#         -DOpenCV_DIR=C:/opencv/build
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

project(IrIsp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Build lands in bin/ so the app finds the prebuilt DLLs next to it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV 4 REQUIRED)
message(STATUS "Qt     : ${Qt5Core_VERSION}")
message(STATUS "OpenCV : ${OpenCV_VERSION}")

# Our sources are UTF-8; tell MSVC so it does not mis-read bytes ending in 0x5C.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---- prebuilt core libraries (headers + import lib + DLL shipped in the repo)
foreach(lib IrIspCore IrMp4Rec IrRaw16Rec)
    add_library(${lib} SHARED IMPORTED)
    set_target_properties(${lib} PROPERTIES
        IMPORTED_IMPLIB   "${CMAKE_SOURCE_DIR}/bin/${lib}.lib"
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/bin/${lib}.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/${lib}/include")
endforeach()

add_subdirectory(IrIspDemoApp)
