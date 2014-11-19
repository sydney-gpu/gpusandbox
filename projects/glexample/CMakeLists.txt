
find_package(OpenGL REQUIRED)

set(GLEXAMPLE_INC_DIRS
	${COMMON_INCLUDE_DIRS}
)

include_directories(${GLEXAMPLE_INC_DIRS})

# sample source files
file(GLOB GLEXAMPLE_SRC_FILES 
  ${CMAKE_SOURCE_DIR}/inc/gl_core_4_4.h
  ${CMAKE_SOURCE_DIR}/src/gl_core_4_4.c
  *.cpp
  *.c
  *.h
)

add_executable(glexample ${GLEXAMPLE_SRC_FILES})

target_link_libraries(glexample glfw ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})