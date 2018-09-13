# halide_visualdbg
A visual debugger for image processing kernels using Halide

![example screenshot of debugger](https://git.corp.adobe.com/slomp/halide_visualdbg/blob/furst/bug-fixes/vis_debug_screenshot.png)

## Usage:
The visual debugger can be added to an existing halide program. Just add `#include "include/debug-api.h"` to your file and call the debugger on your `Func`:
```
Func f = ...
debug(func).realize(output_buffer);
```
Using the debug API will essentially set a breakpoint in your program before the realize call that will open the visual debugger. Once the debugger window is closed, 
program execution will continue and your `Func` will be realized.

## Amalgamation:
We provide an amalgamation of the debugger for you to use in your existing projects. To use the debugger in your own projects, you will need to add the following
files to your build:
- `src/amalg/amalg.cpp` 
- `src/amalg/amalg.c` if on unix/windows
- `src/amalg/amalg.m` if on MacOS  

## Third Party dependencies:
The visual debugger has the following dependencies:
- OpenGL
- GLFW
- Dear ImGui
- stb

We assume that you already have Halide and its dependencies installed.

## Demo program:
We provide a demo program to show how to use the visual debugger. It can be found in the `demo` directory.

## Directory structure:

- `build`:
- `scripts`:
- `third-party`: contains dependencies (code, libraries, etc) from external sources; if a dependency stems from a git repository, it is managed here through [git subrepo](https://github.com/ingydotnet/git-subrepo).
