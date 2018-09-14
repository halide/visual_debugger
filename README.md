# Halide Visual Debugger
An interactive visual debugger for image processing kernels using Halide

![example screenshot of debugger](https://git.corp.adobe.com/slomp/halide_visualdbg/blob/furst/bug-fixes/vis_debug_screenshot.png)

## Usage:
The visual debugger can be added to an existing Halide application by including a header file (`include/debug-api.h`) and a couple of source files (see the _Amalgamation_ section below).

Ordinarily, one would execute a Halide pipeline as follows:
```
Func f = ...
f.realize(buffers...);
```

The visual debugger provides a `debug()` routine to a wrap `Func` in order to inspect intermediate computations:
```
#include <debug-api.h>
...
Func f = ...
debug(f).realize(buffers...);
```

In the code above, `debug(f).realize(...)` will act as a _breakpoint_, interrupting the normal execution of the caller, and launching an interactive visual debugger window as shown above. The debugger will not return control to the caller until the debugger window is closed, at which point the host program execution will resume as intended, with `f.realize(...)` getting called.

*Note that the visual debugger requires Halide's JIT (just-in-time) compilation -- AoT (ahead-of-time) compilation is not supported.*

## Amalgamation:
We provide an amalgamation of the debugger for you to use in your existing projects:
To use the debugger in your own projects, you will need to add a couple of source files to your build:
- `src/amalg/amalg.cpp`
- `src/amalg/amalg.c`, if on Linux or Windows
- `src/amalg/amalg.m`, if on MacOS  

## Basic UI controls:
- use the *Expression Tree* panel to browse through sub-expressions
- click on the small square button to the left of a sub-expression to visualize it
- drag the mouse over the image while holding the left mouse button to scroll through the image
- hover the mouse over the image to inspect pixel values (right-click to select a particular pixel)
- hold `Ctrl` while moving the mouse-wheel to zoom-in/out
- hold `Ctrl` while clicking on the Min/Max range to input specific values to them
- The image window title displays: *execution time* | *host -> device buffer upload time* | *jit-compilation time*

## About Third Party dependencies:
Besides Halide, the visual debugger also has the following dependencies:
- OpenGL
- [GLFW](https://www.glfw.org)
- [stb](https://github.com/nothings/stb) (more specifically, `stb`'s image IO routines)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [Filesystem Add-on for Dear ImGui](https://github.com/Flix01/imgui/tree/2015-10-Addons/addons/imguifilesystem)
For convenience, a snapshot of these dependencies is provided in the `third-party` folder.
The source code amalgamation already refers to code contained in this `third-party` folder.

We assume that you already have Halide and its dependencies installed and linked properly against your application.

## Demo program:
We provide a demo program to show how to use the visual debugger. It can be found in the `demo` directory.

The demo also demonstrates how to group multiple `Func`s for debugging through the `replay()` API, also present in `include/debug-api.h`. This allows one to capture the realization context of `Func`s and postpone their actual realization to a later point during the interactive visual debugging session (a list of available `Func`s for replay will be shown in the GUI).
