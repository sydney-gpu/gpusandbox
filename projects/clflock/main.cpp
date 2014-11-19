#include "utilities.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>

#if defined(__APPLE__) || defined(MACOSX)
	#include <OpenCL/cl.h>
	#include <OpenCL/cl_gl_ext.h>
	#include <OpenGL/OpenGL.h>
#elif defined(WIN32)
	#include <CL/cl.h>
	#include <CL/cl_gl_ext.h>
	#include <windows.h>
#else
	#include <GL/glx.h>
	#include <GL/gl.h>
	#include <CL/cl.h>
	#include <CL/cl_gl.h>
#endif

// helper macros for checking for opencl errors
#define CL_CHECK(str, result) if (result != CL_SUCCESS) { printf("Error: %s - %i\n", #str, result); }

// on windows / linux we need to get access to the extension function handle to determine which
// device is sharable as the opencl / opengl context
#if !defined(__APPLE__) && !defined(MACOSX)
typedef CL_API_ENTRY cl_int(CL_API_CALL *clGetGLContextInfoKHRfunc)(const cl_context_properties*, cl_gl_context_info, size_t, void*, size_t*);
#endif

GLFWwindow* createGLWindow(int width, int height, const char* title, bool fullscreen);

struct Params
{
	float neighbourRadiusSqr;

	float maxSteeringForce;
	float maxBoidSpeed;

	float wanderRadius;
	float wanderJitter;
	float wanderDistance;
	float wanderWeight;

	float separationWeight;
	float cohesionWeight;
	float alignmentWeight;

	unsigned int boidCount;
};

//////////////////////////////////////////////////////////////////////////
int main(int a_iArgc, char* a_aszArgv[])
{
	glm::vec3 simulationArea(200);
	Params params = { 
		20*20, // neighbourhood radius^2
		15, // max sterring force
		50, // max velocity
		8.0f,  // wander radius
		0.3f, // wander jitter
		10, // wander offset distance
		10, // wander weight
		1.5f, // separation weight
		1, // cohesion weight
		2, // alignment weight
		0 };

	GLFWwindow* window = createWindow(1280, 720, "Flocking");
	if (window == nullptr)
		exit(EXIT_FAILURE);

	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);

	// shader
	char* vsSource = STRINGIFY(#version 330\n
		layout(location = 0) in vec4 Position;
		layout(location = 1) in vec4 Colour;
		out vec4 C;
		uniform mat4 pvm;
		void main() {
			gl_Position = pvm * Position;
			C = vec4(min(1,Colour.w / 500.f),1,0,1);
		});
	char* fsSource = STRINGIFY(#version 330\n
		in vec4 C;
		out vec4 Colour;
		void main() {
			Colour = C;
		});

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vs, 1, &vsSource, 0);
	glShaderSource(fs, 1, &fsSource, 0);

	glCompileShader(vs);
	glCompileShader(fs);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glUseProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	// hand-coded crappy box around the flock
	glm::vec4 lines[] = {
		glm::vec4(0, 0, 0, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, 0, 0, 1), glm::vec4(1),
		glm::vec4(0, 0, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, 0, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(0, simulationArea.y, 0, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, simulationArea.y, 0, 1), glm::vec4(1),
		glm::vec4(0, simulationArea.y, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, simulationArea.y, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(0, 0, 0, 1), glm::vec4(1),
		glm::vec4(0, simulationArea.y, 0, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, 0, 0, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, simulationArea.y, 0, 1), glm::vec4(1),
		glm::vec4(0, 0, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(0, simulationArea.y, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, 0, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, simulationArea.y, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(0, 0, 0, 1), glm::vec4(1),
		glm::vec4(0, 0, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(0, simulationArea.y, 0, 1), glm::vec4(1),
		glm::vec4(0, simulationArea.y, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, 0, 0, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, 0, simulationArea.z, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, simulationArea.y, 0, 1), glm::vec4(1),
		glm::vec4(simulationArea.x, simulationArea.y, simulationArea.z, 1), glm::vec4(1)
	};

	GLuint boxVAO, boxVBO;
	glGenBuffers(1, &boxVBO);
	glBindBuffer(GL_ARRAY_BUFFER, boxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 48, lines, GL_STATIC_DRAW);

	glGenVertexArrays(1, &boxVAO);
	glBindVertexArray(boxVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, ((char*)0) + sizeof(glm::vec4));
	glBindVertexArray(0);

	cl_uint boidCount = 1 << 16;
	params.boidCount = boidCount / 8;
	printf("Boids: %i\n", params.boidCount);

	glm::vec4* positions = new glm::vec4[boidCount];
	glm::vec4* velocities = new glm::vec4[boidCount]; // will use W as neighbour counts
	glm::vec4* wanderTargets = new glm::vec4[boidCount]; 
	for (cl_uint i = 0; i < boidCount; ++i)
	{		
		positions[i] = glm::vec4(glm::linearRand(simulationArea * -0.5f, simulationArea * 0.5f), 1);
		velocities[i] = glm::vec4(glm::linearRand(simulationArea * -0.5f, simulationArea * 0.5f) * params.maxBoidSpeed, 0);
		wanderTargets[i] = glm::vec4(glm::linearRand(simulationArea * -0.5f, simulationArea * 0.5f) * params.maxBoidSpeed, 1);
	}

	GLuint boidVAO, boidPositionVBO, boidVelocityVBO;
	glGenBuffers(1, &boidPositionVBO);
	glBindBuffer(GL_ARRAY_BUFFER, boidPositionVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * boidCount, positions, GL_STATIC_DRAW);
	glGenBuffers(1, &boidVelocityVBO);
	glBindBuffer(GL_ARRAY_BUFFER, boidVelocityVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * boidCount, velocities, GL_STATIC_DRAW);

	glGenVertexArrays(1, &boidVAO);
	glBindVertexArray(boidVAO);
	glBindBuffer(GL_ARRAY_BUFFER, boidPositionVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
	glBindBuffer(GL_ARRAY_BUFFER, boidVelocityVBO);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), 0);
	glBindVertexArray(0);

	glm::mat4 perspectiveTransform = glm::perspective(glm::radians(90.0f), 16 / 9.f, 0.1f, 2000.f);

	//////////////////////////////////////////////////////////////////////////
	// opencl setup
	cl_uint numPlatforms = 0;
	cl_uint numDevices = 0;

	// query platform count
	cl_int result = clGetPlatformIDs(0, nullptr, &numPlatforms);
	printf("Platforms: %i\n", numPlatforms);
	CL_CHECK(clGetPlatformIDs, result);

	// get platforms, though we will only use the first
	std::vector<cl_platform_id> platforms(numPlatforms);
	result = clGetPlatformIDs(numPlatforms, platforms.data(), 0);
	CL_CHECK(clGetPlatformIDs, result);

	// query device count (default should be GPU-based devices)
	result = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_DEFAULT, 0, nullptr, &numDevices);
	CL_CHECK(clGetDeviceIDs, result);
	printf("Devices: %i\n", numDevices);

	// get the devices
	std::vector<cl_device_id> devices(numDevices);
	result = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_DEFAULT, numDevices, devices.data(), 0);
	CL_CHECK(clGetDeviceIDs, result);

	// create appropriate context properties depending on platform
#if defined(__APPLE__) || defined(MACOSX)
	CGLContextObj kCGLContext = CGLGetCurrentContext();
	CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);

	cl_context_properties contextProperties[] = {
		CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
		(cl_context_properties)kCGLShareGroup,
		CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
		0
	};
#elif defined(WIN32)
	cl_context_properties contextProperties[] = {
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
		0
	};
#else
	cl_context_properties contextProperties[] =
	{
		CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(),
		CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0],
		0
	};
#endif

	// create the context based on available devices and context properties
	cl_context context = clCreateContext(contextProperties, numDevices, devices.data(), 0, 0, &result);
	CL_CHECK(clCreateContext, result);

	// handle to the device that is sharing the opencl / opengl context
	cl_device_id cl_gl_device;

	// query which device is the current opengl device
#if defined(__APPLE__) || defined(MACOSX)
	result = clGetGLContextInfoAPPLE(context, kCGLContext, CL_CGL_DEVICE_FOR_CURRENT_VIRTUAL_SCREEN_APPLE, sizeof(cl_device_id), &cl_gl_device, nullptr);
	CL_CHECK(clGetGLContextInfoAPPLE, result);
#else    
	clGetGLContextInfoKHRfunc myclGetGLContextInfoKHRf = (clGetGLContextInfoKHRfunc)clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
	result = myclGetGLContextInfoKHRf(contextProperties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &cl_gl_device, nullptr);
	CL_CHECK(clGetGLContextInfoKHR, result);
#endif

	// create a command queue for the device so that we can fire off opencl calls
	cl_command_queue queue = clCreateCommandQueue(context, cl_gl_device, 0, &result);
	CL_CHECK(clCreateCommandQueue, result);

	// load kernel code
	size_t size = 0;
	char* kernelSource = readFileContents(
		"/Users/AIE/Development/GitHub/gpusandbox/bin/kernels/cl/flock.cl", &size);

	// build program for the selected device and context
	cl_program clprogram = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, &size, &result);
	delete[] kernelSource;
	CL_CHECK(clCreateProgramWithSource, result);
	result = clBuildProgram(clprogram, 1, &cl_gl_device, 0, 0, 0);
	if (result != CL_SUCCESS)
	{
		size_t len = 0;
		clGetProgramBuildInfo(clprogram, cl_gl_device, CL_PROGRAM_BUILD_LOG, 0, 0, &len);
		char* log = new char[len];
		clGetProgramBuildInfo(clprogram, cl_gl_device, CL_PROGRAM_BUILD_LOG, len, log, 0);
		printf("Kernel error:\n%s\n", log);
		delete[] log;

		clReleaseProgram(clprogram);
		clReleaseCommandQueue(queue);
		clReleaseContext(context);
		glDeleteBuffers(1, &boxVBO);
		glDeleteVertexArrays(1, &boxVAO);
		glDeleteBuffers(1, &boidPositionVBO);
		glDeleteBuffers(1, &boidVelocityVBO);
		glDeleteVertexArrays(1, &boidVAO);
		glDeleteProgram(program);

		exit(EXIT_FAILURE);
	}

	// extract the kernel
	cl_kernel kernel = clCreateKernel(clprogram, "flocking", &result);
	CL_CHECK(clCreateKernel, result);

	// create opencl memory object links
	cl_mem positionLink = clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, boidPositionVBO, &result);
	CL_CHECK(clCreateFromGLBuffer, result);
	cl_mem velocityLink = clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, boidVelocityVBO, &result);
	CL_CHECK(clCreateFromGLBuffer, result);
	cl_mem wanderLink = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(glm::vec4) * boidCount, wanderTargets, &result);
	CL_CHECK(clCreateBuffer, result);
	cl_mem paramsLink = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Params), &params, &result);
	CL_CHECK(clCreateBuffer, result);
	
	// set the kernel arguments
	float deltaTime = 0.0166666f;	// setting a 1/60fps time step by default
	result = clSetKernelArg(kernel, 0, sizeof(cl_mem), &positionLink);
	result |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &velocityLink);
	result |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &wanderLink);
	result |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &paramsLink);
	result |= clSetKernelArg(kernel, 4, sizeof(float), &deltaTime);
	CL_CHECK(clSetKernelArg, result);

	float prevTime = (float)glfwGetTime();

	// loop
	while (!glfwWindowShouldClose(window) &&
		!glfwGetKey(window, GLFW_KEY_ESCAPE))
	{
		float time = (float)glfwGetTime();
		deltaTime = time - prevTime;

		// ensure opengl is complete
		glFinish();

		// uncomment this if you want, but the simulation wouldn't be even
	//	result = clSetKernelArg(kernel, 4, sizeof(float), &deltaTime);
	//	CL_CHECK(clSetKernelArg, result);
		
		// we set up write events in case we use out-of-order computations
		cl_event writeEvents[3] = { 0, 0, 0 };

		result = clEnqueueAcquireGLObjects(queue, 1, &positionLink, 0, 0, &writeEvents[0]);
		CL_CHECK(clEnqueueAcquireGLObjects, result);
		result = clEnqueueAcquireGLObjects(queue, 1, &velocityLink, 0, 0, &writeEvents[1]);
		CL_CHECK(clEnqueueAcquireGLObjects, result);

		// execute the marching cubes kernel
		cl_event processEvent = 0;
		size_t globalWorkSize[] = { params.boidCount };
		size_t localWorkSize[] = { 32 };
		result = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, globalWorkSize, nullptr, 2, writeEvents, &processEvent);
		CL_CHECK(clEnqueueNDRangeKernel, result);

		// release the opengl buffer from opencl so that it can be drawn
		result = clEnqueueReleaseGLObjects(queue, 1, &positionLink, 1, &processEvent, 0);
		CL_CHECK(clEnqueueReleaseGLObjects, result);
		result = clEnqueueReleaseGLObjects(queue, 1, &velocityLink, 1, &processEvent, 0);
		CL_CHECK(clEnqueueReleaseGLObjects, result);

		// wait until opencl has finished before we draw
		clFinish(queue);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// target center of grid and spin the camera
		glm::vec3 target(0);

		float zoom = 5 + /*(cos(time * 0.1f) * 0.25f + 0.25f) */0.5f * simulationArea.z;
		glm::vec3 eye(sin(time*0.25f) * zoom, 0, cos(time*0.25f) * zoom);
		glm::mat4 pv = perspectiveTransform * glm::lookAt(target + eye, target, glm::vec3(0, 1, 0));

		// bind the projection-view-model (pvm) matrix
		glUniformMatrix4fv(glGetUniformLocation(program, "pvm"), 1, GL_FALSE, glm::value_ptr(pv));

		// draw box around grid
		glBindVertexArray(boidVAO);
		glDrawArrays(GL_POINTS, 0, params.boidCount);

		glUniformMatrix4fv(glGetUniformLocation(program, "pvm"), 1, GL_FALSE, glm::value_ptr(pv * glm::translate(simulationArea * -0.5f)));

		// draw box around grid
		glBindVertexArray(boxVAO);
		glDrawArrays(GL_LINES, 0, 48);

		// present
		glfwSwapBuffers(window);
		glfwPollEvents();
		prevTime = time;
	}

	// cleanup cl
	clFinish(queue);
	clReleaseMemObject(positionLink);
	clReleaseMemObject(velocityLink);
	clReleaseMemObject(wanderLink);
	clReleaseMemObject(paramsLink);
	clReleaseKernel(kernel);
	clReleaseProgram(clprogram);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

	delete[] positions;
	delete[] velocities;
	delete[] wanderTargets;
	glDeleteBuffers(1, &boidPositionVBO);
	glDeleteBuffers(1, &boidVelocityVBO);
	glDeleteVertexArrays(1, &boidVAO);
	glDeleteBuffers(1, &boxVBO);
	glDeleteVertexArrays(1, &boxVAO);
	glDeleteProgram(program);

	glfwTerminate();

	return 0;
}