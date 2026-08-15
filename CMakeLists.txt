cmake_minimum_required(VERSION 3.22)

project(Abracapture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Dependencies.cmake)

add_executable(abracapture
    src/app/main.cpp
)

target_link_libraries(abracapture
    PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        PkgConfig::PIPEWIRE
        PkgConfig::SNDFILE
        PkgConfig::FLAC
)
