
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

function(target_configure target)
    # Check if is target
    if( NOT TARGET ${target})
        message(FATAL_ERROR "Target ${target} not found.")
    endif()
	# ==== 4. Apply per configuration =====================================
	target_compile_options(${target} PRIVATE
		$<$<CONFIG:Debug>:${DEBUG_COMPILE_FLAGS}>
		$<$<CONFIG:RelWithDebInfo>:${DEBUG_COMPILE_FLAGS}>
		$<$<CONFIG:Release>:${RELEASE_HARDEN_FLAGS}>
	)

	target_link_options( ${target} PRIVATE
		$<$<CONFIG:Debug>:${DEBUG_LINK_FLAGS}>
		$<$<CONFIG:RelWithDebInfo>:${DEBUG_LINK_FLAGS}>
		$<$<CONFIG:Release>:${RELEASE_LINK_FLAGS}>
	)
	target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:${DEV_FLAGS}>
						$<$<CONFIG:Release>:${HARDEN_FLAGS}>)

	target_link_options(${target} PRIVATE $<$<CONFIG:Debug>:${DEV_SANITIZERS}>)
endfunction()

# --- HLSL to SPIR-V compile function --------------------------------
function(compile_hlsl_to_spirv target shader_name shader_file stage entry)
    get_filename_component(file_we ${shader_file} NAME_WE)
    set(output_spv "${CMAKE_CURRENT_BINARY_DIR}/shaders/${file_we}_${stage}.spv")
    message(STATUS "Compiling ${shader_file} (${stage}) to SPIR-V: ${output_spv}")

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
    add_dependencies(${target} ${shader_name})
endfunction()

function(compile_glsl_to_spirv target shader_name shader_file stage entry)
    get_filename_component(file_we ${shader_file} NAME_WE)
    set(output_spv "${CMAKE_CURRENT_BINARY_DIR}/shaders/${file_we}_${stage}.spv")
    message(STATUS "Compiling ${shader_file} (${stage}) to SPIR-V: ${output_spv}")

    add_custom_command(
        OUTPUT ${output_spv}
        COMMAND glslangValidator
                -V
                -o ${output_spv}
                ${shader_file}
        DEPENDS ${shader_file}
        COMMENT "Compiling ${shader_file} (${stage}) to SPIR-V"
        VERBATIM
    )

    # Group shaders in a common target
    add_custom_target(${shader_name} DEPENDS ${output_spv})
    add_dependencies(${target} ${shader_name})
    set_target_properties(${shader_name}
        PROPERTIES
        FOLDER "Shaders")       # put them all under one folder
endfunction()
