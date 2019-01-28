//
// Licensed under the MIT License. See LICENSE.TXT file in the project root for full license information.
//

#include "../halide-image-io.cpp"
#include "../imgui_main.cpp"
#include "../treedump.cpp"
#include "../system.cpp"
#include "../debug-api.cpp"

#include "../../third-party/imgui/imgui.h"
#include "../../third-party/imgui/imgui.cpp"
#include "../../third-party/imgui/imgui_draw.cpp"

#include "../../third-party/glfw/include/GLFW/glfw3.h"
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include "../../third-party/glfw/include/GLFW/glfw3native.h"   // for glfwGetWin32Window
#endif
#include "../../third-party/imgui/examples/imgui_impl_glfw.cpp"
#include "../../third-party/imgui/examples/imgui_impl_opengl2.cpp"

#include "../../third-party/imguifilesystem/imguifilesystem.cpp"
