#pragma once

#include <gl_core_4_4.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

// helper macros
#define STRINGIFY(str) #str

// opens and reads in the contents of a file
// caller takes ownership of returned memory
// if size is not nullptr then it is filled with the size of the return buffer
char* readFileContents(const char* path, size_t* size = nullptr);

// creates a generic glfw window
GLFWwindow* createWindow(int width, int height, const char* title, 
						 bool fullscreen = false, int glMajor = 4, int glMinor = 1);