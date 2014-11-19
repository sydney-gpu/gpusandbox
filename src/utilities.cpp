#include "utilities.h"

char* readFileContents(const char* path, size_t* size)
{
	char* buffer = nullptr;

	FILE* file = fopen(path, "rb");
	if (file != nullptr)
	{
		fseek(file, 0, SEEK_END);
		size_t s = ftell(file);
		fseek(file, 0, SEEK_SET);
		buffer = new char[s];
		fread(buffer, sizeof(char), s, file);
		fclose(file);

		if (size != nullptr)
			*size = s;
	}
	else
	{
		printf("Failed to read file '%s'\n", path);

	}

	return buffer;
}

GLFWwindow* createWindow(int width, int height, const char* title, 
	bool fullscreen, int glMajor, int glMinor)
{
	// window creation and OpenGL initialisaion
	if (!glfwInit())
	{
		printf("Window creation failed\n");
		return nullptr;
	}

	// for now I'm using opengl 4.1 (OSX / Windows)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glMinor);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	GLFWwindow* window = glfwCreateWindow(width, height, title, 
		fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
	if (window == nullptr)
	{
		printf("Window creation failed\n");
		glfwTerminate();
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	// update opengl function pointers using "OpenGL Loader Generator"
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
	{
		printf("Window creation failed\n");
		glfwTerminate();
		return nullptr;
	}

	// output active GL version
	printf("GL %i.%i\n", ogl_GetMajorVersion(), ogl_GetMinorVersion());

	glfwSetWindowSizeCallback(window, [](GLFWwindow*, int w, int h){ glViewport(0,0,w,h); });

	return window;
}