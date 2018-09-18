//
// Licensed under the MIT License. See LICENSE.TXT file in the project root for full license information.
//

#ifndef __APPLE__
    #error 'glfw-amalg.m' is only used by MacOS builds.
#endif

#define _GLFW_COCOA

// MacOS:
#ifdef _GLFW_COCOA
    #include "../../glfw/src/cocoa_init.m"
    #include "../../glfw/src/cocoa_joystick.m"
    #include "../../glfw/src/cocoa_monitor.m"
    #include "../../glfw/src/cocoa_window.m"
    #include "../../glfw/src/nsgl_context.m"
#endif//_GLFW_COCOA
