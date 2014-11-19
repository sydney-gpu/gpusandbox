// a blank OpenGL window example

#include <gl_core_4_4.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <stdio.h>

int main(int argc, char* argv[])
{	
	// window creation and OpenGL initialisaion
	if (!glfwInit())
	{
		printf("GLFW initialisation failed\n");
		exit(EXIT_FAILURE);
	}

	// specify preffered GL version, enable the core profile only with forward compatibility
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// GL added a debug callback to eliminate the need to query for errors
	// note: we haven't set the callback so we aren't using it yet!
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	GLFWwindow* window = glfwCreateWindow(800, 600, "Example OpenGL Window", nullptr, nullptr);
	if (window == nullptr)
	{
		printf("Window creation failed\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// set the window as the current GL context for rendering in to
	glfwMakeContextCurrent(window);

	// update opengl function pointers using "OpenGL Loader Generator"
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
	{
		printf("GL function mapping failed\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// output active GL version
	printf("GL %i.%i\n", ogl_GetMajorVersion(), ogl_GetMinorVersion());

	// set the window resize callback to reset our viewport size (using a lambda)
	glfwSetWindowSizeCallback(window, 
		[](GLFWwindow*, int w, int h){ glViewport(0,0,w,h); });

	// set the colour the window is cleared to
	glClearColor(0,0.5f,1,1);

	// enable depth testing
	glEnable(GL_DEPTH_TEST);

	// loop while window is open and escape hasn't been pressed
	while (!glfwWindowShouldClose(window) && 
		   !glfwGetKey(window, GLFW_KEY_ESCAPE)) 
	{
		// grab the time since the application started (in seconds)
		float time = (float)glfwGetTime();

		// clear the back-buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// present the back-buffer
		glfwSwapBuffers(window);

		// poll the window for events (i.e. input, window resize)
		glfwPollEvents();
	}

	// cleanup our window
	glfwTerminate();

	exit(EXIT_SUCCESS);
}