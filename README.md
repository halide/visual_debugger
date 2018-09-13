# halide_visualdbg
A visual debugger for image processing kernels using Halide

## Usage:
The visual debugger can be added to an existing halide program. Just add `#include "include/debug-api.h"` to your file and call the debugger on your `Func`:
```
Func f = ...
debug(func).realize(output_buffer);
```
Using the debug API will essentially set a breakpoint in your program before the realize call that will open the visual debugger. Once the debugger window is closed, 
program execution will continue and your `Func` will be realized. 

## Amalgamation:

## Third Party dependencies:
The visual debugger has the following dependencies:
- OpenGL
- GLFW
- Dear ImGui
- stb

## Demo program:

## Directory structure:

- `build`:
- `scripts`:
- `third-party`: contains dependencies (code, libraries, etc) from external sources; if a dependency stems from a git repository, it is managed here through [git subrepo](https://github.com/ingydotnet/git-subrepo).
