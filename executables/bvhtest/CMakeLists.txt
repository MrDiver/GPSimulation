project(bvhtest VERSION 0.1 LANGUAGES CXX CUDA)

CPMAddPackage(NAME RAYLIB URL "https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip" OPTIONS "GRAPHICS GRAPHICS_API_OPENGL_43")
set_target_properties(raylib
	PROPERTIES
	FOLDER "External")       # put them all under one folder

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/bvhtest.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog raylib_static)
target_precompile_headers(${PROJECT_NAME} PRIVATE <vector> <string> <iostream> <fstream> <sstream> <map> <set> <unordered_map> <unordered_set> <algorithm> <cmath> <limits> <spdlog/spdlog.h> <raylib.h>)

# Add target to copy shaders folder
# to the output directory after build

add_custom_target(copy_shaders ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/shaders
	$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
	COMMENT "Copying shaders folder to output directory"
)

add_custom_target(copy_models ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/models
	$<TARGET_FILE_DIR:${PROJECT_NAME}>/models
	COMMENT "Copying models folder to output directory"
)

add_dependencies(${PROJECT_NAME} copy_shaders)
add_dependencies(${PROJECT_NAME} copy_models)
