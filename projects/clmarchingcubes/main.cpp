#include "gl_core_4_4.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <GLFW/glfw3.h>
#include <vector>

#if defined(__APPLE__) || defined(MACOSX)
    #include <OpenCL/cl.h>
    #include <OpenCL/cl_gl_ext.h>
	#include <OpenGL/OpenGL.h>
#elif defined(WIN32)
	#include <CL/cl.h>
	#include <CL/cl_gl_ext.h>
	#include <GL/GL.h>
	#include <windows.h>
#else
	#include <GL/glx.h>
	#include <GL/gl.h>
	#include <CL/cl.h>
	#include <CL/cl_gl.h>
#endif

// helper macros for creating a shader and checking for opencl errors
#define STRINGIFY(str) #str
#define CL_CHECK(str, result) if (result != CL_SUCCESS) { printf("Error: %s - %i\n", #str, result); }

// on windows / linux we need to get access to the extension function handle to determine which
// device is sharable as the opencl / opengl context
#if !defined(__APPLE__) && !defined(MACOSX)
	typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetGLContextInfoKHRfunc)(const cl_context_properties*,cl_gl_context_info,size_t,void*,size_t*);
#endif

struct GLData
{
	// shader program
	GLuint	program;

	// marching cube vertex data
	GLuint	blobVAO;
	GLuint	blobVBO;

	// border square vertex data
	GLuint	boxVAO;
	GLuint	boxVBO;
};

struct MCData
{
	size_t		gridSize[3];
	cl_float	threshold;
	cl_uint		maxFaces;
	cl_uint		faceCount;
};

struct CLData
{
	cl_context			context;
	cl_command_queue	queue;
	cl_program			program;
	cl_kernel			kernel;

	cl_mem				vboLink;
	cl_mem				faceCountLink;
	cl_mem				particleLink;
};

// method to initialise all opengl settings and buffers
GLFWwindow* setupGL(int width, int height, const char* tile, 
	GLData& glData, const MCData& mcData);

int main(int argc, char* argv[])
{
	// setup initial data
	MCData mcData = { { 64, 64, 64 }, 0.04f, 250000, 0 };
	GLData glData = { 0 };
	CLData clData;
	const int particleCount = 8;
	glm::vec4 particles[particleCount];

	// window creation and opengl initialisaion
	GLFWwindow* window = setupGL(1280, 720, "OpenCLMC", glData, mcData);
    
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
    clData.context = clCreateContext(contextProperties, numDevices, devices.data(), 0, 0, &result);
	CL_CHECK(clCreateContext, result);

	// handle to the device that is sharing the opencl / opengl context
	cl_device_id cl_gl_device;
    
	// query which device is the current opengl device
#if defined(__APPLE__) || defined(MACOSX)
    result = clGetGLContextInfoAPPLE(clData.context, kCGLContext, CL_CGL_DEVICE_FOR_CURRENT_VIRTUAL_SCREEN_APPLE, sizeof(cl_device_id), &cl_gl_device, nullptr);
    CL_CHECK(clGetGLContextInfoAPPLE, result);
#else    
	clGetGLContextInfoKHRfunc myclGetGLContextInfoKHRf = (clGetGLContextInfoKHRfunc)clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
	result = myclGetGLContextInfoKHRf(contextProperties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &cl_gl_device, nullptr);
	CL_CHECK(clGetGLContextInfoKHR, result);
#endif
    
	// create a command queue for the device so that we can fire off opencl calls
    clData.queue = clCreateCommandQueue(clData.context, cl_gl_device, 0, &result);
    CL_CHECK(clCreateCommandQueue, result);

	// load kernel code
	FILE* file = fopen("/Users/AIE/Development/GitHub/gpusandbox/kernels/cl/marchingcubes.cl", "rb");
	if (file == nullptr)
	{
		printf("Failed to load kernel file mc.cl!\n");

		clReleaseCommandQueue(clData.queue);
		clReleaseContext(clData.context);
		glDeleteBuffers(1, &glData.boxVBO);
		glDeleteVertexArrays(1, &glData.boxVAO);
		glDeleteBuffers(1, &glData.blobVBO);
		glDeleteVertexArrays(1, &glData.blobVAO);
		glDeleteProgram(glData.program);

		exit(EXIT_FAILURE);
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* kernelSource = new char[size];
	fread(kernelSource, sizeof(char), size, file);
	fclose(file);

	// build program for the selected device and context
	clData.program = clCreateProgramWithSource(clData.context, 1, (const char**)&kernelSource, &size, &result);
	delete[] kernelSource;
	CL_CHECK(clCreateProgramWithSource, result);
	result = clBuildProgram(clData.program, 1, &cl_gl_device, 0, 0, 0);
	if (result != CL_SUCCESS)
	{
		size_t len = 0;
		clGetProgramBuildInfo(clData.program, cl_gl_device, CL_PROGRAM_BUILD_LOG, 0, 0, &len);
		char* log = new char[len];
		clGetProgramBuildInfo(clData.program, cl_gl_device, CL_PROGRAM_BUILD_LOG, len, log, 0);
		printf("Kernel error:\n%s\n", log);
		delete[] log;

		clReleaseProgram(clData.program);
		clReleaseCommandQueue(clData.queue);
		clReleaseContext(clData.context);
		glDeleteBuffers(1, &glData.boxVBO);
		glDeleteVertexArrays(1, &glData.boxVAO);
		glDeleteBuffers(1, &glData.blobVBO);
		glDeleteVertexArrays(1, &glData.blobVAO);
		glDeleteProgram(glData.program);

		exit(EXIT_FAILURE);
	}

	// extract the kernel
	clData.kernel = clCreateKernel(clData.program, "kernelMC", &result);
	CL_CHECK(clCreateKernel, result);

	// create opencl memory object links
	clData.vboLink = clCreateFromGLBuffer(clData.context, CL_MEM_WRITE_ONLY, glData.blobVBO, &result);
	CL_CHECK(clCreateFromGLBuffer, result);
	clData.faceCountLink = clCreateBuffer(clData.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(cl_uint), &mcData.faceCount, &result);
	CL_CHECK(clCreateBuffer, result);
	clData.particleLink = clCreateBuffer(clData.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(glm::vec4) * particleCount, particles, &result);
	CL_CHECK(clCreateBuffer, result);

	// set the kernel arguments
	result = clSetKernelArg(clData.kernel, 0, sizeof(cl_int), &mcData.maxFaces);
	result |= clSetKernelArg(clData.kernel, 1, sizeof(cl_mem), &clData.faceCountLink);
	result |= clSetKernelArg(clData.kernel, 2, sizeof(cl_mem), &clData.vboLink);
	result |= clSetKernelArg(clData.kernel, 3, sizeof(cl_float), &mcData.threshold);
	result |= clSetKernelArg(clData.kernel, 4, sizeof(cl_int), &particleCount);
	result |= clSetKernelArg(clData.kernel, 5, sizeof(cl_mem), &clData.particleLink);
	CL_CHECK(clSetKernelArg, result);
	
	// loop
	while (!glfwWindowShouldClose(window) && 
		   !glfwGetKey(window, GLFW_KEY_ESCAPE)) 
	{
		float time = (float)glfwGetTime();

		// our sample volume is made of meta balls (they were placed based on a 128^3 grid)
		// simple animation of the balls for now
		float scale = mcData.gridSize[0] / (float)128;
		particles[0] = glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 0)  * 0.5f;
		particles[1] = glm::vec4(sin(time) * 32, cos(time * 0.5f) * 32, sin(time * 2) * 16, 0) * scale + particles[0];
		particles[2] = glm::vec4(cos(-time * 0.25f) * 8, cos(time * 0.5f), cos(time) * 32, 0) * scale + particles[0];
		particles[3] = glm::vec4(sin(time) * 32, cos(time * 0.5f) * 32, cos(-time * 2) * 16, 0) * scale + particles[0];
		particles[4] = glm::vec4(sin(time) * 16, sin(time * 1.5f) * 16, sin(time * 2) * 32, 0) * scale + particles[0];
		particles[5] = glm::vec4(cos(time * 0.3f) * 32, cos(time * 1.5f) * 32, sin(time * 2) * 32, 0) * scale + particles[0];
		particles[6] = glm::vec4(sin(time) * 16, sin(time * 1.5f) * 16, sin(time * 2) * 32, 0) * scale + particles[0];
		particles[7] = glm::vec4(sin(-time) * 32, sin(time * 1.5f) * 32, cos(time * 4) * 32, 0) * scale + particles[0];

		// ensure opengl is complete
		glFinish();

		// reset marching cube face count
		mcData.faceCount = 0;

		// we set up write events in case we use out-of-order computations
		cl_event writeEvents[3] = { 0, 0, 0 };

		// send data to the device for opencl to use, aquiring the opengl buffer for opencl use
		 result = clEnqueueAcquireGLObjects(clData.queue, 1, &clData.vboLink, 0, 0, &writeEvents[0]);
		CL_CHECK(clEnqueueAcquireGLObjects, result);
		result = clEnqueueWriteBuffer(clData.queue, clData.faceCountLink, CL_FALSE, 0, sizeof(unsigned int), &mcData.faceCount, 0, nullptr, &writeEvents[1]);
		CL_CHECK(clEnqueueWriteBuffer, result);
		result = clEnqueueWriteBuffer(clData.queue, clData.particleLink, CL_FALSE, 0, sizeof(glm::vec4) * particleCount, particles, 0, nullptr, &writeEvents[2]);
		CL_CHECK(clEnqueueWriteBuffer, result);

		// execute the marching cubes kernel
		cl_event processEvent = 0;
		result = clEnqueueNDRangeKernel(clData.queue, clData.kernel, 3, 0, mcData.gridSize, 0, 3, writeEvents, &processEvent);
		CL_CHECK(clEnqueueNDRangeKernel, result);

		// release the opengl buffer from opencl so that it can be drawn
		result = clEnqueueReleaseGLObjects(clData.queue, 1, &clData.vboLink, 1, &processEvent, 0);
		CL_CHECK(clEnqueueReleaseGLObjects, result);

		// read how many triangles to draw
		result = clEnqueueReadBuffer(clData.queue, clData.faceCountLink, CL_FALSE, 0, sizeof(unsigned int), &mcData.faceCount, 1, &processEvent, 0);
		CL_CHECK(clEnqueueReadBuffer, result);

		// wait until opencl has finished before we draw
		clFinish(clData.queue);

		// draw
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// target center of grid and spin the camera
		glm::vec3 target(mcData.gridSize[0] / 2, mcData.gridSize[1] / 2, mcData.gridSize[2] / 2);
		glm::vec3 eye(sin(time) * mcData.gridSize[0], 0, cos(time) * mcData.gridSize[0]);
		glm::mat4 pvm = glm::perspective(glm::radians(90.0f), 16 / 9.f, 0.1f, 2000.f) * glm::lookAt(target + eye, target, glm::vec3(0, 1, 0));

		// bind the projection-view-model (pvm) matrix
		glUniformMatrix4fv(glGetUniformLocation(glData.program, "pvm"), 1, GL_FALSE, glm::value_ptr(pvm));

		// draw marching cube blob
		glBindVertexArray(glData.blobVAO);
		glDrawArrays(GL_TRIANGLES, 0, glm::min(mcData.faceCount, mcData.maxFaces) * 3);
		
		// draw box around grid
		glBindVertexArray(glData.boxVAO);
		glDrawArrays(GL_LINES, 0, 48);

		// present
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// cleanup cl
	clFinish(clData.queue);
	clReleaseMemObject(clData.vboLink);
	clReleaseMemObject(clData.faceCountLink);
	clReleaseKernel(clData.kernel);
	clReleaseProgram(clData.program);
	clReleaseCommandQueue(clData.queue);
	clReleaseContext(clData.context);

	// cleanup gl
	glDeleteBuffers(1, &glData.boxVBO);
	glDeleteVertexArrays(1, &glData.boxVAO);
	glDeleteBuffers(1, &glData.blobVBO);
	glDeleteVertexArrays(1, &glData.blobVAO);
	glDeleteProgram(glData.program);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}

GLFWwindow* setupGL(int width, int height, const char* title, GLData& glData, const MCData& mcData)
{
	// window creation and OpenGL initialisaion
	if (!glfwInit())
	{
		exit(EXIT_FAILURE);
		return nullptr;
	}

	// for now I'm using opengl 4.1 (OSX / Windows)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (window == nullptr)
	{
		printf("Window creation failed\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	// update opengl function pointers using "OpenGL Loader Generator"
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
		return nullptr;
	}

	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);

	// shader
	char* vsSource = STRINGIFY(#version 330\n
		layout(location = 0) in vec4 Position;
		layout(location = 1) in vec4 Normal;
		out vec4 N;
		uniform mat4 pvm;
		void main() {
			gl_Position = pvm * Position;
			N = Normal;
		});
	char* fsSource = STRINGIFY(#version 330\n
		in vec4 N;
		out vec4 Colour;
		void main() {
			float d = dot(normalize(N.xyz), normalize(vec3(1)));
			Colour = vec4(mix(vec3(0, 0, 0.75), vec3(0, 0.75, 1), d), 1);
		});

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vs, 1, &vsSource, 0);
	glShaderSource(fs, 1, &fsSource, 0);

	glCompileShader(vs);
	glCompileShader(fs);

	glData.program = glCreateProgram();
	glAttachShader(glData.program, vs);
	glAttachShader(glData.program, fs);
	glLinkProgram(glData.program);
	glUseProgram(glData.program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	
	// mesh data for marching cube blob
	glGenBuffers(1, &glData.blobVBO);
	glBindBuffer(GL_ARRAY_BUFFER, glData.blobVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * mcData.maxFaces * 3, 0, GL_STATIC_DRAW);

	glGenVertexArrays(1, &glData.blobVAO);
	glBindVertexArray(glData.blobVAO);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(glm::vec4) * 2, ((char*)0) + sizeof(glm::vec4));
	glBindVertexArray(0);

	glBindVertexArray(0);

	// hand-coded crappy box around the marching cube blob
	glm::vec4 lines[] = {
		glm::vec4(0, 0, 0, 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, 0, 1), glm::vec4(1),
		glm::vec4(0, 0, mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], 0, 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], 0, 1), glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(0, 0, 0, 1), glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], 0, 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, 0, 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], 0, 1), glm::vec4(1),
		glm::vec4(0, 0, mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(0, 0, 0, 1), glm::vec4(1),
		glm::vec4(0, 0, mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], 0, 1), glm::vec4(1),
		glm::vec4(0, mcData.gridSize[1], mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, 0, 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], 0, mcData.gridSize[2], 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], 0, 1), glm::vec4(1),
		glm::vec4(mcData.gridSize[0], mcData.gridSize[1], mcData.gridSize[2], 1), glm::vec4(1)
	};

	glGenBuffers(1, &glData.boxVBO);
	glBindBuffer(GL_ARRAY_BUFFER, glData.boxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 48, lines, GL_STATIC_DRAW);

	glGenVertexArrays(1, &glData.boxVAO);
	glBindVertexArray(glData.boxVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(glm::vec4) * 2, ((char*)0) + sizeof(glm::vec4));
	glBindVertexArray(0);

	return window;
}