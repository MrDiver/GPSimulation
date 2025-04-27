cmake_minimum_required(VERSION 3.30)

# ==== Dependencies ==================================================
include(cmake/CPM.cmake)
CPMAddPackage("https://github.com/gabime/spdlog.git@1.15.2")
CPMAddPackage("https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip")
CPMAddPackage("https://github.com/ocornut/imgui.git@1.91.9b")
CPMAddPackage("https://github.com/libsdl-org/SDL.git#release-3.2.10")
CPMAddPackage("https://github.com/nothings/stb.git#f056911")
CPMAddPackage("https://github.com/tinyobjloader/tinyobjloader.git@2.0.0rc13")
CPMAddPackage("https://github.com/charles-lunarg/vk-bootstrap.git@1.4.313")
CPMAddPackage("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git@3.2.1")

set(CMAKE_C_STANDARD 20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

project(engine CXX CUDA)
add_executable(${PROJECT_NAME})

add_subdirectory(graphics)

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog glm SDL3-shared tinyobjloader vk-bootstrap VulkanMemoryAllocator)
target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${stb_SOURCE_DIR})

target_sources(${PROJECT_NAME} PRIVATE src/main.cpp)
# ==== 1. Tweak these lists if you add/remove sanitizers ==============
set(DEBUG_SANITIZERS
    address,undefined,leak,thread,bounds,memory)

set(DEBUG_WARNINGS
    "-Wall -Wextra -Wpedantic -Werror "
    "-Wshadow -Wconversion -Wnull-dereference "
    "-Wpointer-arith -Wunsafe-buffer-usage")

# ==== 2. Compose full flag strings ===================================
set(DEBUG_COMPILE_FLAGS
    "-g -O1 -fno-omit-frame-pointer "
    "-fsanitize=${DEBUG_SANITIZERS} ${DEBUG_WARNINGS}")

set(DEBUG_LINK_FLAGS
    "-fsanitize=${DEBUG_SANITIZERS}")

set(RELEASE_HARDEN_FLAGS
    "-O2 -pipe -ffunction-sections -fdata-sections "
    "-fstack-protector-strong -fstack-clash-protection "
    "-D_FORTIFY_SOURCE=2 -fhardened "
    "-flto -fsanitize=cfi -fvisibility=hidden")

set(RELEASE_LINK_FLAGS
    "-flto -fsanitize=cfi "
    "-fPIE -pie -Wl,-z,relro,-z,now "
    "-Wl,--as-needed")

# ==== 3. Optional pointer-auth & BTI (AArch64 only) ==================
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(RELEASE_HARDEN_FLAGS
        "${RELEASE_HARDEN_FLAGS} -mbranch-protection=pac-ret+bti")  # PAC/BTI
endif()

# ==== 4. Apply per configuration =====================================
add_compile_options(
    $<$<CONFIG:Debug>:${DEBUG_COMPILE_FLAGS}>
    $<$<CONFIG:RelWithDebInfo>:${DEBUG_COMPILE_FLAGS}>
    $<$<CONFIG:Release>:${RELEASE_HARDEN_FLAGS}>
)

add_link_options(
    $<$<CONFIG:Debug>:${DEBUG_LINK_FLAGS}>
    $<$<CONFIG:RelWithDebInfo>:${DEBUG_LINK_FLAGS}>
    $<$<CONFIG:Release>:${RELEASE_LINK_FLAGS}>
)
add_compile_options($<$<CONFIG:Debug>:${DEV_FLAGS}>
                    $<$<CONFIG:Release>:${HARDEN_FLAGS}>)

add_link_options($<$<CONFIG:Debug>:${DEV_SANITIZERS}>)