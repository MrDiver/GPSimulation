cmake_minimum_required(VERSION 3.30)

# ==== Dependencies ==================================================
include(cmake/CPM.cmake)
CPMAddPackage("https://github.com/gabime/spdlog.git@1.15.2")

set_target_properties(spdlog 
    PROPERTIES
    FOLDER "External")       # put them all under one folder

find_package(Vulkan REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


include(cmake/Configuration.cmake)
project(generalengine)

add_subdirectory(graphics)

option(BUILD_ENGINE "Build the engine" OFF)
option(BUILD_BVH "Build the BVH test" ON)

if(BUILD_ENGINE)
    add_subdirectory(executables/engine)
endif()

if(BUILD_BVH)
    add_subdirectory(executables/bvhtest)
endif()

