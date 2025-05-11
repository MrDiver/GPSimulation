project("vkengine" CXX)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME} 
    PUBLIC FILE_SET graphics_headers TYPE HEADERS BASE_DIRS header FILES 
    header/camera.h
    header/vk_descriptors.h
    header/vk_engine.h
    header/vk_images.h
    header/vk_initializers.h
    header/vk_loader.h
    header/vk_pipelines.h
    header/vk_types.h 
    PUBLIC
    src/camera.cpp
    src/vk_descriptors.cpp
    src/vk_engine.cpp
    src/vk_images.cpp
    src/vk_initializers.cpp
    src/vk_loader.cpp
    src/vk_pipelines.cpp
    src/vk_types.cpp 
)

target_precompile_headers(${PROJECT_NAME} PUBLIC header/vk_types.h)
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan spdlog glm SDL3-static tinyobjloader vk-bootstrap VulkanMemoryAllocator volk )
target_include_directories(${PROJECT_NAME} PUBLIC ${imgui_SOURCE_DIR} ${stb_SOURCE_DIR} ${SDL_SOURCE_DIR}/include)

compile_hlsl_to_spirv(${PROJECT_NAME} "basic_compute" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/basic.hlsl" "cs" "main")
compile_glsl_to_spirv(${PROJECT_NAME} "gradient_compute" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/gradient.comp" "cs" "main")
