
find_package(OpenGL REQUIRED)
find_package(OpenCL REQUIRED)

set(MARCHINGCUBES_INC_DIRS
	${COMMON_INCLUDE_DIRS}
	${OPENCL_INCLUDE_DIRS}
)

include_directories(${MARCHINGCUBES_INC_DIRS})

# sample source files
file(GLOB MARCHINGCUBES_SRC_FILES 
  ${CMAKE_SOURCE_DIR}/inc/gl_core_4_4.h
  ${CMAKE_SOURCE_DIR}/src/gl_core_4_4.c
  *.cpp
  *.c
  *.h
  ${CMAKE_SOURCE_DIR}/kernels/cl/marchingcubes.cl
)

add_executable(clmarchingcubes ${MARCHINGCUBES_SRC_FILES})

target_link_libraries(clmarchingcubes glfw ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} ${OPENCL_LIBRARIES})