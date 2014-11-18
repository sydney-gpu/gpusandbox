gpu-sandbox
=======

A collection of random GPU projects utilising a mix of CUDA, OpenCL, OpenGL and DirectX.

**CMake**:

  The C/C++ code uses cmake to create the project files / makefile for your desired platform. Be sure to install cmake and either use the cmake GUI program to generate the files, or use the command-line:
  
    *cmake .*

**OpenCL**:

  On Windows and Linux you will need to download an OpenCL library from your device manufacturer, either AMD, Nvidia or Intel.
  
**CUDA**:

  Requires Nvidia hardware. Download the CUDA SDK from Nvidia's website.
  
**OpenGL**:

  Current examples are written for OpenCL 4.1 (OSX supported). Most modern GPU hardware supports GL4.3 which allows for COmpute shaders. Future example projects will include 4.3 and 4.4 samples to demonstrate Compute.
  
**DirectX**:

  Windows only.
  
**Future Plans**

  This repo will eventually contain examples of various GPU-related techniques including computation and rendering, written in various languages including C, C++, C# and Python.
  WebGL may also be included for some samples.
