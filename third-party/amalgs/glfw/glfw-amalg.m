#undef _GLFW_BUILD_DLL
#undef GLFW_DLL

#ifndef __APPLE__
    #error 'glfw-amalg.m' is only applicable to MacOS builds; use 'glfw-amalg.c' instead.
#endif

#define _GLFW_COCOA

#ifdef _GLFW_COCOA
    #include "../../glfw/src/cocoa_init.m"
    #include "../../glfw/src/cocoa_joystick.m"
    #include "../../glfw/src/cocoa_monitor.m"
    #include "../../glfw/src/cocoa_time.c"
    #include "../../glfw/src/cocoa_window.m"
    #include "../../glfw/src/nsgl_context.m"
    #include "../../glfw/src/posix_thread.c"
#endif//_GLFW_COCOA

// Core:
#include "../../glfw/src/context.c"
#include "../../glfw/src/init.c"
#include "../../glfw/src/input.c"
#include "../../glfw/src/monitor.c"
#include "../../glfw/src/vulkan.c"
#include "../../glfw/src/window.c"
// are these really core?
#include "../../glfw/src/egl_context.c"
#include "../../glfw/src/osmesa_context.c"
