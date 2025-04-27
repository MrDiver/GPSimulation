project("graphics")

target_sources(${CMAKE_PROJECT_NAME} 
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

target_precompile_headers(${CMAKE_PROJECT_NAME} PUBLIC header/vk_types.h)