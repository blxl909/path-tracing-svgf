# cmake version
cmake_minimum_required(VERSION 3.1)

# c++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()
if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
	set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

# project
project(path_tracer_svgf)

configure_file(lib/root_directory.h.in lib/root_directory.h)

# target
aux_source_directory("./lib" CPP_FILES)
aux_source_directory("./imgui" IMGUI_CPP_FILES)
add_executable(path_tracer_svgf main.cpp ${CPP_FILES} ${IMGUI_CPP_FILES})
target_include_directories(path_tracer_svgf PUBLIC ${PROJECT_SOURCE_DIR}/Utils)
target_include_directories(path_tracer_svgf PUBLIC ${PROJECT_SOURCE_DIR}/imgui)
target_include_directories(path_tracer_svgf PUBLIC ${PROJECT_SOURCE_DIR}/lib)



#list(APPEND CMAKE_PREFIX_PATH "D:/vcpkg/packages/glad_x64-windows")
#list(APPEND CMAKE_PREFIX_PATH "D:/vcpkg/packages/glfw3_x64-windows")
#list(APPEND CMAKE_PREFIX_PATH "D:/vcpkg/packages/glm_x64-windows")
#if your cmake can not find packages below automatically, you can set your path just like the example above

# find the required packages
find_package(GLM REQUIRED)
find_package(GLFW3 REQUIRED)
find_package(GLAD REQUIRED)


# link
target_link_libraries(path_tracer_svgf PRIVATE ${GLEW_LIBRARIES})
target_link_libraries(path_tracer_svgf PRIVATE ${GLAD_LIBRARIES})
