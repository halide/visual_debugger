#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#ifdef _WIN32
    // to accommodate for OpenGL user callbacks (like GLDEBUGPROCARB), glfw3.h
    // needs to #define APIENTRY if it has not been defined yet; it's safer to
    // just include the Windows header prior to including the glfw3 header
    #define NOMINMAX
    #include <Windows.h>
#endif//_WIN32

// include our own local copy of imconfig.h, should we need to customize it
#include "imconfig.h"
// we also need to define this to prevent imgui.h form including the stock
// version of imconfig.h
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <limits>

#include <thread>
#include <condition_variable>

namespace ImGui
{
    // non-public, internal imgui routine; handy, and has been there forever...
    void ColorTooltip(const char* text, const float* col, ImGuiColorEditFlags flags);
}

#include <stdio.h>
#include <Halide.h>
#include <GLFW/glfw3.h>

#include "system.hpp"
#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "HalideImageIO.h"

#include "../third-party/imguifilesystem/imguifilesystem.h"

using namespace Halide;

bool stdout_echo_toggle (false);
bool save_images(false);

int view_transform_value(1);
int min_val(0), max_val(0);
int rgba_select(-1);

std::thread t1;



//NOTE(Emily): vars related to saving images
Halide::Buffer<> output, orig_output;
std::string fname = "";

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void ToggleButton(const char* str_id, bool* v)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    float height = ImGui::GetFrameHeight();
    float width = height * 1.55f;
    float radius = height * 0.50f;
    
    if (ImGui::InvisibleButton(str_id, ImVec2(width, height)))
        *v = !*v;
    ImU32 col_bg;
    if (ImGui::IsItemHovered())
        col_bg = *v ? IM_COL32(145+20, 211, 68+20, 255) : IM_COL32(218-20, 218-20, 218-20, 255);
    else
        col_bg = *v ? IM_COL32(145, 211, 68, 255) : IM_COL32(218, 218, 218, 255);
    
    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(*v ? (p.x + width - radius) : (p.x + radius), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));
}

std::string sanitize(std::string input)
{
    std::string illegalChars = "\\/:?\"<>| ";
    for(auto i = input.begin(); i < input.end(); i++)
    {
        bool found = illegalChars.find(*i) != std::string::npos;
        if(found)
        {
            *i = '-';
        }
    }
    return input;
}

void default_output_name(std::string name, int id)
{
    assert(output.defined());
    if(output.type().is_float())
    {
        fname = "data/output/" + sanitize(name) + "_" + std::to_string(id) + ".pfm";
    }
    else if(output.type().is_uint() && output.type().bits() == 8)
    {
        fname = "data/output/" + sanitize(name) + "_" + std::to_string(id) + ".png";
    }
    else
    {
        output = output.as<float>();
        fname = "data/output/" + sanitize(name) + "_" +std::to_string(id) + ".pfm";
    }
}

void default_output_name_no_dirs(std::string name, int id)
{
    assert(output.defined());
    if(output.type().is_float())
    {
        fname = sanitize(name) + "_" + std::to_string(id) + ".pfm";
    }
    else if(output.type().is_uint() && output.type().bits() == 8)
    {
        fname = sanitize(name) + "_" + std::to_string(id) + ".png";
    }
    else
    {
        output = output.as<float>();
        fname = sanitize(name) + "_" +std::to_string(id) + ".pfm";
    }
}

int id_expr_debugging = -1;
Halide::Type selected_type;

// from 'treedump.cpp':
void select_and_visualize(Func f, int id, Halide::Type& type, Halide::Buffer<>& output, std::string target_features, int view_transform_value = 0, int min = 0, int max = 0, int channels = -1);
void halide_worker_proc(void(*notify_result)());
extern std::mutex result_lock;
extern std::vector<Result> result_queue;

bool process_result(Result& result)
{
    std::unique_lock<std::mutex> l2(result_lock);
    if (result_queue.empty())
    {
        return false;
    }

    result = std::move(result_queue.back());
    result_queue.pop_back();
    
    return true;
}

void refresh_texture(GLuint idMyTexture, Halide::Buffer<>& output)
{
    auto dims = output.dimensions();
    // only 2D or 3D images for now (no 1D LUTs or nD entities)
    assert(dims >= 2);
    assert(dims <= 3);

    int width    = output.width();      // same as output.dim(0).extent()
    int height   = output.height();     // same as output.dim(1).extent()
    int channels = output.channels();   // same as output.dim(2).extent() when 'dims >= 2', or 1 otherwise

    // only monochrome or rgb for now
    assert(channels == 1 || channels == 3);

    bool is_monochrome = (channels == 1);

    GLenum internal_format (GL_INVALID_ENUM),
           external_format (GL_INVALID_ENUM),
           external_type   (GL_INVALID_ENUM);

    external_format = (is_monochrome) ? GL_RED
                                      : GL_RGB;

    #ifndef GL_RGBA32F
    #define GL_RGBA32F 0x8814
    #endif//GL_RGBA32F

    Type type = output.type();
    bool is_float = type.is_float();
    auto bits = type.bits();
    switch(bits)
    {
        case 8:
            external_type   = GL_UNSIGNED_BYTE;
            internal_format = GL_RGBA8;
            break;
        case 16:
            external_type   = GL_UNSIGNED_SHORT;
            internal_format = GL_RGBA16;                                    // maybe force GL_RGBA8 fow now?
            break;
        case 32:
            external_type   = (is_float) ? GL_FLOAT   : GL_UNSIGNED_INT;
            internal_format = (is_float) ? GL_RGBA32F : GL_RGBA16;          // maybe force GL_RGBA8 for now?
            break;
        default:
            assert(false);
            break;
            
    }

    glBindTexture(GL_TEXTURE_2D, idMyTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, external_format, external_type, output.data());
        // TODO(marcos): generate mip-maps to improve visualization on zoom-out
        // auto glGenerateMipMap = (void(*)(GLenum))glfwGetProcAddress("glGenerateMipMap");
        //glGenerateMipMap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

template<typename T, int K>
void query_pixel(Halide::Runtime::Buffer<T, K>& image, int x, int y, float& r, float& g, float& b)
{
    r = (float)image(x, y, 0);
    g = (float)image(x, y, 1);
    b = (float)image(x, y, 2);
}

void query_pixel(Halide::Buffer<>& buffer, int x, int y, float& r, float& g, float& b)
{
    r = g = b = 0.0f;
    
    if (!buffer.defined())
        return;

    if ((x < 0) || (x >= buffer.width()))
        return;
    if ((y < 0) || (y >= buffer.height()))
        return;

    switch (orig_output.type().code())
    {
        case halide_type_int :
            switch (orig_output.type().bits())
            {
                case 8 :
                {
                    Halide::Runtime::Buffer<int8_t> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                case 16 :
                {
                    Halide::Runtime::Buffer<int16_t> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                case 32 :
                {
                    Halide::Runtime::Buffer<int32_t> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            break;
        case halide_type_uint :
            switch (orig_output.type().bits())
            {
                case 8 :
                {
                    Halide::Runtime::Buffer<uint8_t> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                case 16 :
                {
                    Halide::Runtime::Buffer<uint16_t> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                case 32 :
                {
                    Halide::Runtime::Buffer<uint32_t> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            break;
        case halide_type_float :
            switch (orig_output.type().bits())
            {
                case 32 :
                {
                    Halide::Runtime::Buffer<float> image = *buffer.get();
                    query_pixel(image, x, y, r, g, b);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            break;
        default :
            assert(false);
            break;
    }
}

void display_node(expr_node* node, GLuint idMyTexture, Func f, std::string& selected_name, Profiling& times, const std::string& target_features)
{
    const int id = node->node_id;
    const char* label = node->name.c_str();
    const bool selected = (id_expr_debugging == id);
    const bool terminal = node->children.empty();
    const bool viewable = (id != 0);   // <- whether or not this expr_node can be visualized

    if (selected)
    {
        const ImU32 LimeGreen = 0xFF00CF40;
        ImGui::PushStyleColor(ImGuiCol_Text,   LimeGreen);
        ImGui::PushStyleColor(ImGuiCol_Button, LimeGreen);
    }

    bool clicked = false;
    if (viewable)
    {
        ImGui::PushID(id);
        clicked = ImGui::SmallButton(" ");
        ImGui::PopID();
        ImGui::SameLine();
    }

    bool open = (terminal) ? ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Leaf)
                           : ImGui::TreeNode(label);

    if (selected)
    {
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }

    if (clicked)
    {
        select_and_visualize(f, id, selected_type, output, target_features, view_transform_value, min_val, max_val, rgba_select);

        if(save_images)
        {
            assert(output.defined());
            //NOTE(Emily): need to create default filename for all images
            //we want to decide file extension based on data type

            if(fname == "") default_output_name(f.name(), id);
            
            if(!SaveImage(fname.c_str(), output))
                fprintf(stderr, "Error saving image\n");
            
            fname = ""; //NOTE(Emily): done saving so want to reset fname
        }
        id_expr_debugging = id;
        selected_name = label;
    }

    if (!open)
    {
        return;
    }

    for(auto& child : node->children)
    {
        display_node(child, idMyTexture, f, selected_name, times, target_features);
    }

    // NOTE(marcos): TreePop() must be called only when TreeNode*() returns true
    ImGui::TreePop();
}

std::string type_to_string(Halide::Type type)
{
    std::stringstream ss;
    switch (type.code())
    {
        case halide_type_uint:
            ss << "u";
        case halide_type_int:
            ss << "int";
            break;
        case halide_type_float:
            ss << "float";
            break;
        case halide_type_handle:
            ss << "handle";
            break;
    }
    ss << type.bits();
    ss << "[";
    ss << type.lanes();
    ss << "]";
    return ss.str();
    return ss.str();
}

void render_gui(GLFWwindow* window)
{
    assert(glfwGetCurrentContext() == window);

    ImGui::Render();

    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void glfw_on_window_resized(GLFWwindow* window, int width, int height)
{
    assert(glfwGetCurrentContext() == window);
    glViewport(0, 0, width, height);

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    render_gui(window);
}

bool OptionalCheckbox(const char* label, bool* v, bool enabled=true)
{
    if (enabled)
    {
        return ImGui::Checkbox(label, v);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    bool dummy = false;
    bool result = ImGui::Checkbox(label, &dummy);
    ImGui::PopStyleVar();
    return result;
}

bool OptionalRadioButton(const char* label, int* v, int v_button, bool enabled=true)
{
    if (enabled)
    {
        return ImGui::RadioButton(label, v, v_button);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    bool result = ImGui::RadioButton(label, false);
    ImGui::PopStyleVar();
    return result;
}

ImVec2 calculate_range()
{
    ImVec2 range;
    switch (selected_type.code())
    {
        case halide_type_int :
            switch (selected_type.bits())
        {
            case 8 :
            {
                range = { float(std::numeric_limits<int8_t>::min()),
                          float(std::numeric_limits<int8_t>::max()) };
                break;
            }
            case 16 :
            {
                range = { float(std::numeric_limits<int16_t>::min()),
                          float(std::numeric_limits<int16_t>::max()) };
                break;
            }
            case 32 :
            {
                range = { float(std::numeric_limits<int32_t>::min()),
                          float(std::numeric_limits<int32_t>::max()) };
                break;
            }
            default:
                assert(false);
                break;
        }
            break;
        case halide_type_uint :
            switch (selected_type.bits())
        {
            case 1 : //boolean expression - cast to 8 bit
            case 8 :
            {
                range = { float(std::numeric_limits<uint8_t>::min()),
                          float(std::numeric_limits<uint8_t>::max()) };
                break;
            }
            case 16 :
            {
                range = { float(std::numeric_limits<uint16_t>::min()),
                          float(std::numeric_limits<uint16_t>::max()) };
                break;
            }
            case 32 :
            {
                range = { float(std::numeric_limits<uint32_t>::min()),
                          float(std::numeric_limits<uint32_t>::max()) };
                break;
            }
            default:
                assert(false);
                break;
        }
            break;
        case halide_type_float :
            switch (selected_type.bits())
        {
            case 32 :
            {
                range = { std::numeric_limits<float_t>::min(),
                          std::numeric_limits<float_t>::max() };
                break;
            }
            default:
                assert(false);
                break;
        }
            break;
        default :
            assert(false);
            break;
    }
    return range;
}

int calculate_speed()
{
    int speed;
    switch (selected_type.bits()) {
        case 1: //boolean expression - cast to 8 bit
        case 8:
            speed = 1;
            break;
        case 16:
            speed = 1000;
            break;
        case 32:
            speed = 5000000;
            break;
        default:
            assert(false);
            break;
    }
    return speed;
}

ImVec2 ImageViewer(ImTextureID texture, const ImVec2& texture_size, float& zoom, const ImVec2& canvas_size)
{
    auto& io = ImGui::GetIO();

    //NOTE(Emily): in order to get horizontal scrolling needed to add other parameters (from example online)
    ImGuiWindowFlags ScrollFlags =  ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove;
    ScrollFlags |= (io.KeyCtrl) ? ImGuiWindowFlags_NoScrollWithMouse : 0;
    ImGui::BeginChild(" ", canvas_size, false, ScrollFlags);
        // screen coords of the current drawing "tip" (cursor) location
        // (which is where the child image control will start rendering)
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImGui::Image(texture , ImVec2(zoom*texture_size.x, zoom*texture_size.y));
    
        bool hovering = ImGui::IsItemHovered();
    
        if (hovering && ImGui::IsMouseDragging(0))
        {
            ImVec2 drag = ImGui::GetMouseDragDelta(0);
            ImGui::SetScrollX(ImGui::GetScrollX() - drag.x);
            ImGui::SetScrollY(ImGui::GetScrollY() - drag.y);
            ImGui::ResetMouseDragDelta(0);
        }
    
        if (hovering && io.KeyCtrl && (io.MouseWheel != 0.0f))
        {
            const float scale = 0.0618f;
            float factor = 1.0f + (io.MouseWheel * scale);
            // 1. zoom around top-left
            //float dx = 0.0f;
            //float dy = 0.0f;
            // 2. zoom around center
            //float ds_x = scale * 0.5;
            //float ds_y = scale * 0.5;
            //float dx = io.MouseWheel*size.x*ds_x;
            //float dy = io.MouseWheel*size.y*ds_y;
            // 2. zoom off-center (around mouse pointer location)
            ImVec2  mouse_pos = ImGui::GetMousePos();
            ImVec2  hover_pos = mouse_pos;
            hover_pos.x -= cursor_pos.x;
            hover_pos.y -= cursor_pos.y;
            hover_pos.x -= ImGui::GetScrollX();
            hover_pos.y -= ImGui::GetScrollY();
            float ds_x = scale * (hover_pos.x / canvas_size.x);
            float ds_y = scale * (hover_pos.y / canvas_size.y);
            float dx = io.MouseWheel * canvas_size.x * ds_x;
            float dy = io.MouseWheel * canvas_size.y * ds_y;
            zoom *= factor;
            ImGui::SetScrollX(ImGui::GetScrollX() * factor + dx);
            ImGui::SetScrollY(ImGui::GetScrollY() * factor + dy);
        }
    ImGui::EndChild();

    ImVec2 pixel_coord = { -1.0f, -1.0f };
    if (hovering)
    {
        ImVec2  mouse_pos = ImGui::GetMousePos();
        ImVec2  hover_pos = mouse_pos;
        hover_pos.x -= cursor_pos.x;
        hover_pos.y -= cursor_pos.y;
        pixel_coord.x = (hover_pos.x / zoom);
        pixel_coord.y = (hover_pos.y / zoom);
    }
    
    return pixel_coord;
}

void run_gui(std::vector<Func> funcs, std::vector<Buffer<>> funcs_outputs)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return;
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Halide Visual Debugger", NULL, NULL);
    assert(window);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable vsync to reduce input response lag

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= 0; // this no-op is here just to suppress unused variable warnings

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
    glfwSetFramebufferSizeCallback(window, glfw_on_window_resized);

    // Setup style
    ImGui::StyleColorsClassic();

    bool show_image = true;
    bool show_expr_tree = true;
    bool show_func_select = true;
    bool show_target_select = true;
    bool show_stdout_box = true;
    bool show_save_image = true;
    bool show_range_normalize = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    std::string selected_name = "No node selected, displaying output";

    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

    GLuint idMyTexture = 0;
    glGenTextures(1, &idMyTexture);
    assert(idMyTexture != 0);

    bool linear_filter = false;
    glBindTexture(GL_TEXTURE_2D, idMyTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear_filter ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);

    // TODO(marcos): maybe we should make 'select_and_visualize' return the
    // corresponding expr_tree; this will spare us of a visitor step.
    //expr_tree tree = get_tree(funcs[0]);
    expr_tree tree;
    std::string target_features;

    //NOTE(Emily): call to update buffer to display output of function
    Profiling times = { };
    int cpu_value(0), gpu_value(0), func_value(0);
    
    int range_value(2);

    //target flag bools (need to be outside of loop to maintain state)
    bool sse41(false), avx(false), avx2(false), avx512(false), fma(false), fma4(false), f16c(false);
    bool neon(false);
    bool debug_runtime(false), no_asserts(false), no_bounds_query(false);
    
    bool range_select(false), all_channels(true);

    SystemInfo sys;

    //NOTE(Emily): temporary to explore demo window
    bool open_demo(false);

    Func selected;
    
    std::thread halide_worker (halide_worker_proc, [](){ glfwPostEmptyEvent(); });

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Start the ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if(show_stdout_box)
        {
            bool * no_close = NULL;
            ImGui::Begin("Output", no_close);
            ImGui::Checkbox("Show stdout in terminal", &stdout_echo_toggle);
            ImGui::End();
        }
        
        if(true)
        {
            ImGui::Begin("display demo window");
            ImGui::Checkbox("open", &open_demo);
            if(open_demo)
            {
                ImGui::ShowDemoWindow();
            }
            ImGui::End();
        }
        
        if(show_save_image)
        {
            bool * no_close = NULL;
            ImGui::Begin("Save images to disk upon selection: ", no_close);
            ToggleButton("Save all images", &save_images);
            ImGui::SameLine();
            ImGui::Text("Save all displayed images");
            
            ImGui::End();
        }

        std::string target_features_before = target_features;

        if(show_target_select)
        {
            bool * no_close = NULL;
            
            ImGui::Begin("Select compilation target: ", no_close);
            
            ImGui::Text("CPU: ");

            ImGui::RadioButton("host", &cpu_value, 0);

            OptionalRadioButton("x86",    &cpu_value, 1, sys.Supports(Target::Arch::X86, 32));
            ImGui::SameLine();
            OptionalRadioButton("x86_64", &cpu_value, 2, sys.Supports(Target::Arch::X86, 64));

            if(cpu_value == 1 || cpu_value == 2)
            {
                OptionalCheckbox("sse41", &sse41, sys.Supports(Target::SSE41));
                ImGui::SameLine();
                OptionalCheckbox("avx", &avx, sys.Supports(Target::AVX));
                ImGui::SameLine();
                OptionalCheckbox("avx2", &avx2, sys.Supports(Target::AVX2));
                ImGui::SameLine();
                OptionalCheckbox("avx512", &avx512, sys.Supports(Target::AVX512));

                // FMA is actually orthogonal to AVX (and is even orthogonal to AVX2!)
                OptionalCheckbox("fma", &fma, sys.Supports(Target::FMA));
                ImGui::SameLine();
                // FMA4 is AMD-only; Intel adopted FMA3 (which Halide does not yet support)
                OptionalCheckbox("fma4", &fma4, sys.Supports(Target::FMA4));

                OptionalCheckbox("f16c", &f16c, sys.Supports(Target::F16C));
            }

            OptionalRadioButton("ARM", &cpu_value, 3, sys.Supports(Target::Arch::ARM));
            if(cpu_value == 3)
            {
                ImGui::Checkbox("NEON", &neon);
                target_features += (neon) ? "-neon" : "-no_neon";
            }

            ImGui::Text("GPU: ");
            
            OptionalRadioButton("none",        &gpu_value, 0);
            OptionalRadioButton("Metal",       &gpu_value, 1, sys.metal);
            OptionalRadioButton("CUDA",        &gpu_value, 2, sys.cuda);
            OptionalRadioButton("OpenCL",      &gpu_value, 3, sys.opencl);
            OptionalRadioButton("Direct3D 12", &gpu_value, 4, sys.d3d12);

            ImGui::Text("Halide: ");
            ImGui::Checkbox("Debug Runtime", &debug_runtime);
            ImGui::Checkbox("No Asserts", &no_asserts);
            ImGui::Checkbox("No Bounds Query", &no_bounds_query);

            //ImGui::Text("%s", target_features.c_str());

            ImGui::End();

            if (cpu_value == 0) target_features = "host";
            if (cpu_value == 1) target_features = "x86";
            if (cpu_value == 2) target_features = "x86_64";
            if (cpu_value == 3) target_features = "armv7s";

            target_features += (sse41)  ? "-sse41"  : "" ;
            target_features += (avx)    ? "-avx"    : "" ;
            target_features += (avx2)   ? "-avx2"   : "" ;
            target_features += (avx512) ? "-avx512" : "" ;
            target_features += (fma)    ? "-fma"    : "" ;
            target_features += (fma4)   ? "-fma4"   : "" ;
            target_features += (f16c)   ? "-f16c"   : "" ;

            if (gpu_value == 1) target_features += "-metal";
            if (gpu_value == 2) target_features += "-cuda";
            if (gpu_value == 3) target_features += "-opencl";
            if (gpu_value == 4) target_features += "-d3d12compute";

            target_features += (debug_runtime)   ? "-debug"           : "" ;
            target_features += (no_asserts)      ? "-no_asserts"      : "" ;
            target_features += (no_bounds_query) ? "-no_bounds_query" : "" ;
        }

        bool target_changed = (target_features_before != target_features);
        target_features_before = target_features;

        bool target_selected = !target_features.empty();

        if(show_func_select)
        {
            bool * no_close = NULL;
            ImGui::Begin("Select Func to visualize: ", no_close);
            int id = 0;
            for(auto func : funcs)
            {
                int func_value_before = func_value;
                ImGui::RadioButton(func.name().c_str(), &func_value, id);
                bool func_changed = (func_value_before != func_value);
                bool changed = func_changed || !selected.defined() || target_changed;
                if(func_value == id && changed)
                {
                    int show_id = id_expr_debugging;
                    if (func_changed)
                    {
                        tree.root = nullptr;
                        id_expr_debugging = -1;
                        show_id = 0;
                        selected = Func();
                        output = Buffer<>();
                    }
                    if (!output.defined())
                    {
                        output = funcs_outputs[id];
                    }
                    if (!selected.defined())
                    {
                        selected = func;
                    }
                    if (tree.root == nullptr)
                    {
                        tree = get_tree(func);
                    }
                    select_and_visualize(func, id_expr_debugging, selected_type, output, target_features, view_transform_value, min_val, max_val, rgba_select);
                }
                id++;
            }
            ImGui::End();
        }

        bool func_selected = selected.defined();

        // NOTE(Emily): main expression tree window
        if(show_expr_tree)
        {
            
            bool * no_close = NULL;
            ImGui::Begin("Expression Tree", no_close, ImGuiWindowFlags_HorizontalScrollbar);
            //Note(Emily): call recursive method to display tree
            if(func_selected && target_selected)
            {
                display_node(tree.root, idMyTexture, selected, selected_name, times, target_features);
            }
            ImGui::End();
            
        }

        
        if (show_image)
        {
            bool * no_close = NULL;
            
            int width    = output.width();
            int height   = output.height();
            int channels = output.channels();

            std::string info = std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(channels)
                             + " | " + type_to_string(selected_type)
                             + " | " + std::to_string(times.run_time) + "s"
                             + " | (up: " + std::to_string(times.upl_time) + "s)"
                             + " | (jit: " + std::to_string(times.jit_time) + "s)"
                             + "###ImageDisplay";   // <- ImGui ID control (###): we want this window to be the same, regardless of its title
            
            
            ImGui::Begin(info.c_str() , no_close);

            static float zoom = 1.0f;
            ImGui::SliderFloat("Image Zoom", &zoom, 0, 10, "%.001f");

            ImGui::SameLine();

            if (ImGui::Checkbox("filter", &linear_filter))
            {
                GLenum filter = (linear_filter) ? GL_LINEAR : GL_NEAREST;
                glBindTexture(GL_TEXTURE_2D, idMyTexture);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            
            
            ImGui::SameLine();
            
            bool show_fs_dialogue = ImGui::Button("Save Image");
            default_output_name_no_dirs(selected_name, id_expr_debugging);             //update fname to be default string
            const char * default_dir = "./data/output";                                //default directory to open
            static ImGuiFs::Dialog dlg;                                                // one per dialog (and must be static)
            const char* chosenPath = dlg.saveFileDialog(show_fs_dialogue, default_dir, fname.c_str());  // see other dialog types and the full list of arguments for advanced usage
            if (strlen(chosenPath)>0) {
                ImGui::Text("Chosen file: \"%s\"",dlg.getChosenPath());
                if(!SaveImage(dlg.getChosenPath(), output))
                    fprintf(stderr, "Error saving image\n");
                chosenPath = NULL;
            }
            
            if(show_range_normalize)
            {
                
                bool changed = false;
                ImVec2 range = calculate_range();
                int speed = calculate_speed();
                
                changed = ImGui::DragIntRange2("Pixel Range", &min_val, &max_val, (float)speed, (int)range.x, (int)range.y, "Min: %d", "Max: %d");
                ImGui::SameLine();
                if(ImGui::Button("Reset"))
                {
                    min_val = 0;
                    max_val = 0;
                    changed = true;
                    view_transform_value = 1;
                }
                
                int previous = range_value; //NOTE(Emily): if we switch between range normalize/clamp we want to force refresh
                
                ImGui::SameLine();
                ImGui::RadioButton("range normalize", &range_value, 2);
                ImGui::SameLine();
                ImGui::RadioButton("range clamp", &range_value, 3);
                
                
                
                // must be set to it back false when 'min_val' and 'max_val' are both zero
                range_select = (min_val != 0 || max_val != 0);
                if (range_select)
                {
                    // prevent division by zero:
                    max_val = (min_val == max_val) ? max_val + 1
                                                   : max_val;
                    view_transform_value = range_value; //NOTE(Emily): set transform view to type of range transform
                }
                if(changed && !range_select)
                {
                    view_transform_value = 1; //NOTE(Emily): switch back to default handling of overflow values
                }
                if (changed || (previous != range_value))
                    selected = Func();  // will force a refresh
            }
            
            //NOTE(Emily): slider to view specific color channels
            {
                ImGui::Checkbox("View all channels", &all_channels);
                if(all_channels)
                {
                    rgba_select = -1;
                    selected = Func(); //force a refresh
                }
                if(!all_channels)
                {
                    int previous = rgba_select;
                    bool changed = false;
                    ImGui::SameLine();
                    int num_channels = output.channels();
                    changed = ImGui::SliderInt("RGBA Select", &rgba_select, 0, (num_channels - 1));
                    if(!changed && previous == -1)
                    {
                        rgba_select = 0;
                        changed = true;
                    }
                    if(changed)
                        selected = Func(); //force a refresh
                }
            }
            
            
            // save some space to draw the hovered pixel value below the image:
            ImVec2 canvas_size = ImGui::GetContentRegionAvail();
            canvas_size.y -= ImGui::GetFrameHeightWithSpacing();

            ImVec2 pixel_coord = ImageViewer((ImTextureID)(uintptr_t)idMyTexture, ImVec2((float)width, (float)height), zoom, canvas_size);
            bool hovering = (pixel_coord.x >= 0.0f) && (pixel_coord.y >= 0.0f);

            if (hovering)
            {
                int x = (int)pixel_coord.x;
                int y = (int)pixel_coord.y;
                float rgb [3];
                query_pixel(orig_output, x, y, rgb[0], rgb[1], rgb[2]);
                ImGui::ColorEdit3("hovered pixel", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip);
                ImGui::SameLine();
                ImGui::Text("(x=%d, y=%d) = [r=%f, g=%f, b=%f]", x, y, rgb[0], rgb[1], rgb[2]);
                if (io.KeyShift)
                {
                    ImGui::ColorTooltip("", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                }
            }
            else
            {
                ImGui::Text("");
            }
            
            
            static int pick_x = 0;
            static int pick_y = 0;
            if (hovering && (io.MouseDown[1] || (io.KeyCtrl && io.MouseDown[0])))
            {
                pick_x = (int)pixel_coord.x;
                pick_y = (int)pixel_coord.y;
            }
            {
                float rgb [3];
                query_pixel(orig_output, pick_x, pick_y, rgb[0], rgb[1], rgb[2]);
                ImGui::SameLine(ImGui::GetWindowWidth() - 600);
                ImGui::ColorEdit3("picked pixel", rgb, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip);
                ImGui::SameLine();
                ImGui::Text("(x=%d, y=%d) = [r=%f, g=%f, b=%f]", pick_x, pick_y, rgb[0], rgb[1], rgb[2]);
            }
             

            ImGui::End();
        }

        // Rendering
        render_gui(window);

        // NOTE(marcos): moving event handling to the end of the main loop so
        // that we don't "lose" the very first frame and are left staring at a
        // blank screen until the mouse starts hovering the window:
        // (alternatively, we can use 'glfwPostEmptyEvent()' to force an event
        //  artificially into the event queue)

        //glfwPollEvents();     // <- this is power hungry!
        glfwWaitEvents();       // <- this is a more CPU/power/battery friendly choice
        
        Result result;
        while (process_result(result))
        {
            times = result.times;
            output = std::move(result.output);

            if(view_transform_value == 1)
                orig_output = std::move(output); //save original output vals for pixel picking
            refresh_texture(idMyTexture, output);
        }
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // TODO(marcos): issue a "faux" work to wake up and teardown the worker thread
    //halide_worker.join(); //TODO(Emily): do we want this here?
    // Yes, ideally the thread must be joined here, but until the faux work is
    // implemented, it is easier to leave the trhread dangling around
    halide_worker.detach(); // <- remove this once worker termination is implemented

    glfwDestroyWindow(window);
    glfwTerminate();

}


