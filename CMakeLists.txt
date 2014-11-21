cmake_minimum_required(VERSION 2.8)

project(SydneyGPU)

list( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# glfw library
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
add_subdirectory(${CMAKE_SOURCE_DIR}/dep/glfw)

# compiler flags
if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
elseif(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# global include directories
set(COMMON_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/dep/glfw/include
  ${CMAKE_SOURCE_DIR}/dep/glm
  ${CMAKE_SOURCE_DIR}/dep/stb
  ${CMAKE_SOURCE_DIR}/inc
)

# set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# I want to avoid additional sub-directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# samples
add_subdirectory(${CMAKE_SOURCE_DIR}/projects/glexample)
add_subdirectory(${CMAKE_SOURCE_DIR}/projects/clflock)
add_subdirectory(${CMAKE_SOURCE_DIR}/projects/clmarchingcubes)