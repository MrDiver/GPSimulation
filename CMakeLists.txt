cmake_minimum_required(VERSION 3.30)

# ==== Dependencies ==================================================
include(cmake/CPM.cmake)
CPMAddPackage("https://github.com/gabime/spdlog.git@1.15.2")
CPMAddPackage("https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip")
CPMAddPackage("https://github.com/ocornut/imgui.git@1.91.9b")
CPMAddPackage("https://github.com/nothings/stb.git#f056911")
CPMAddPackage("https://github.com/tinyobjloader/tinyobjloader.git@2.0.0rc13")
CPMAddPackage("https://github.com/charles-lunarg/vk-bootstrap.git@1.4.313")
CPMAddPackage("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git@3.2.1")
CPMAddPackage("https://github.com/zeux/volk.git#vulkan-sdk-1.4.309.0")
CPMAddPackage(NAME SDL URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.10/SDL3-3.2.10.zip OPTIONS "SDL_STATIC ON" )

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(DXC_EXECUTABLE NAMES dxc HINTS "$ENV{VULKAN_SDK}/Bin")

if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "DXC not found. Set VULKAN_SDK or install DXC.")
endif()

set(CMAKE_C_STANDARD 20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

project(engine CXX CUDA)
add_executable(${PROJECT_NAME})

add_subdirectory(graphics)

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan spdlog glm SDL3-static tinyobjloader vk-bootstrap VulkanMemoryAllocator volk )
target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${stb_SOURCE_DIR} ${SDL_SOURCE_DIR}/include)

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

# --- HLSL to SPIR-V compile function --------------------------------
function(compile_hlsl_to_spirv shader_name shader_file stage entry)
    get_filename_component(file_we ${shader_file} NAME_WE)
    set(output_spv "${CMAKE_CURRENT_BINARY_DIR}/${file_we}_${stage}.spv")

    add_custom_command(
        OUTPUT ${output_spv}
        COMMAND ${DXC_EXECUTABLE}
                -spirv
                -T ${stage}_6_7
                -E ${entry}
                -fvk-use-dx-layout
                -fspv-target-env=vulkan1.3
                -Fo ${output_spv}
                ${shader_file}
        DEPENDS ${shader_file}
        COMMENT "Compiling ${shader_file} (${stage}) to SPIR-V"
        VERBATIM
    )

    # Group shaders in a common target
    add_custom_target(${shader_name} DEPENDS ${output_spv})
    add_dependencies(${PROJECT_NAME} ${shader_name})
endfunction()

# --- HLSL to SPIR-V compile commands --------------------------------
compile_hlsl_to_spirv("basic_compute" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.hlsl" "cs" "main")
