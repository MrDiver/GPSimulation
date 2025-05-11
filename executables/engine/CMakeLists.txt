project(engine VERSION 0.1 LANGUAGES CXX CUDA)

CPMAddPackage("https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip")
CPMAddPackage("https://github.com/ocornut/imgui.git@1.91.9b")
CPMAddPackage("https://github.com/nothings/stb.git#f056911")
CPMAddPackage("https://github.com/tinyobjloader/tinyobjloader.git@2.0.0rc13")
CPMAddPackage("https://github.com/charles-lunarg/vk-bootstrap.git@1.4.313")
CPMAddPackage("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git@3.2.1")
CPMAddPackage("https://github.com/zeux/volk.git#vulkan-sdk-1.4.309.0")
CPMAddPackage(NAME SDL URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.10/SDL3-3.2.10.zip OPTIONS "SDL_STATIC ON" )

set_target_properties(glm SDL3_test SDL3-shared SDL3-static tinyobjloader SDL_uclibc vk-bootstrap volk glfw
    PROPERTIES
    FOLDER "External")       # put them all under one folder

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE vkengine)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
