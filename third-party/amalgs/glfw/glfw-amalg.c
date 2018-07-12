#ifdef _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#undef _GLFW_BUILD_DLL
#undef GLFW_DLL

#ifdef _WIN32
    #define _GLFW_WIN32
#elif defined(__APPLE__)
    #define _GLFW_COCOA
    #error for MacOS builds, use 'glfw-amalg.m' instead.
#else
    #define _GLFX_UNIX
    #define _GLFW_X11
    #define _GLFW_WAYLAND
    #define _GLFW_MIR
    #define _GLFW_OSMESA
#endif

#ifdef _GLFX_UNIX
    #include "../../GLFW/src/glx_context.c"
    #include "../../GLFW/src/linux_joystick.c"
    #include "../../GLFW/src/posix_thread.c"
    #include "../../GLFW/src/posix_time.c"
    #include "../../GLFW/src/xkb_unicode.c"

    #ifdef _GLFW_X11
        #include "../../GLFW/src/x11_init.c"
        #include "../../GLFW/src/x11_monitor.c"
        #include "../../GLFW/src/x11_window.c"
    #endif//_GLFW_X11

    #ifdef _GLFW_WAYLAND
        #include "../../GLFW/src/wl_init.c"
        #include "../../GLFW/src/wl_monitor.c"
        #include "../../GLFW/src/wl_window.c"
    #endif//_GLFW_WAYLAND

    #ifdef _GLFW_MIR
        #include "../../GLFW/src/mir_init.c"
        #include "../../GLFW/src/mir_monitor.c"
        #include "../../GLFW/src/mir_window.c"
    #endif//_GLFW_MIR

    #ifdef _GLFW_OSMESA
        #include "../../GLFW/src/null_init.c"
        #include "../../GLFW/src/null_joystick.c"
        #include "../../GLFW/src/null_monitor.c"
        #include "../../GLFW/src/null_window.c"
    #endif//_GLFW_OSMESA

#endif//_GLFX_UNIX

// Windows:
#ifdef _GLFW_WIN32
    #include "../../glfw/src/wgl_context.c"
    #include "../../glfw/src/win32_init.c"
    #include "../../glfw/src/win32_joystick.c"
    #include "../../glfw/src/win32_monitor.c"
    #include "../../glfw/src/win32_thread.c"
    #include "../../glfw/src/win32_time.c"
    #include "../../glfw/src/win32_window.c"
#endif//_GLFW_WIN32

// MacOS: use 'glfw-amalg.m' instead!
#ifdef _GLFW_COCOA
    #error for MacOS builds, use 'glfw-amalg.m' instead.
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
