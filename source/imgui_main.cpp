// ImGui - standalone example application for GLFW + OpenGL2, using legacy fixed pipeline
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_glfw_opengl2/ folder**
// See imgui_impl_glfw.cpp for details.

// include our own local copy of imconfig.h, should we need to customize it
#include "imconfig.h"
// we also need to define this to prevent imgui.h form including the stock
// version of imconfig.h
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

#include "treedump.cpp"

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

struct rgba {
    float r, g, b, a;
};


//NOTE(Emily): method to generate example tree using struc
expr_node * generate_example_tree(){
    expr_node * root = new expr_node();
    root->name = "Func: output";
    expr_node * temp = new expr_node();
    temp->name = "<arguments>";
    root->children.push_back(temp);
    temp->name = "Variable: v60 | let";
    root->children[0]->children.push_back(temp);
    temp->name = "Variable: v61 | let";
    root->children[0]->children.push_back(temp);
    temp->name = "Variable: v62 | let";
    root->children[0]->children.push_back(temp);
    temp->name = "<definition>";
    root->children.push_back(temp);
    temp->name = "Function: f";
    root->children[1]->children.push_back(temp);
    temp->name = "<arguments>";
    root->children[1]->children[0]->children.push_back(temp);
    temp->name = "Variable: v54 | let";
    root->children[1]->children[0]->children[0]->children.push_back(temp);
    temp->name = "Variable: v55 | let";
    root->children[1]->children[0]->children[0]->children.push_back(temp);
    temp->name = "Variable: v56 | let";
    root->children[1]->children[0]->children[0]->children.push_back(temp);
    temp->name = "<definition>";
    root->children[1]->children[0]->children.push_back(temp);
    temp->name = "Mul";
    root->children[1]->children[0]->children[1]->children.push_back(temp);
    temp->name = "UIntImm: 10";
    root->children[1]->children[0]->children[1]->children[0]->children.push_back(temp);
    temp->name = "Call: image | Halide";
    root->children[1]->children[0]->children[1]->children[0]->children.push_back(temp);
    temp->name = "...";
    root->children[1]->children[0]->children[1]->children[0]->children[1]->children.push_back(temp);
    return root;
}

void set_color(std::vector<rgba>& pixels){
    float R = rand()/(float)RAND_MAX;
    float G = rand()/(float)RAND_MAX;
    float B = rand()/(float)RAND_MAX;
    for(auto& p : pixels){
        p.r = R;
        p.g = G;
        p.b = B;
        p.a = 1;
    }
}

void update_buffer(GLuint idMyTexture, std::vector<rgba> pixels, int width, int height){
    glBindTexture(GL_TEXTURE_2D, idMyTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
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

void display_node(expr_node * parent, GLuint idMyTexture, int width, int height, Func f, Halide::Buffer<uint8_t> input_full, std::string& selected_name, Profiling& times)
{
    if(ImGui::TreeNode(parent->name.c_str()))
    {
        if(parent->node_id != 0)
        {
            static int clicked = 0;
            if (ImGui::Button("View Result of Expr"))
                clicked++;
            if (clicked & 1)
            {
                selected_name = parent->name;
                times = select_and_visualize(f, parent->node_id, input_full, idMyTexture);
                
            }
            clicked = 0;
        }
        if(!parent->children.empty())
        {
            for(int i = 0; i < parent->children.size(); i++)
            {
                display_node(parent->children[i], idMyTexture, width, height, f, input_full, selected_name, times);
            }
        }
        ImGui::TreePop();
    }
}

void run_gui(expr_node * tree, Func f, Halide::Buffer<uint8_t> input_full)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return;
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Halide Visual Debugger", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= 0; // this no-op is here just to suppress unused variable warnings
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Setup style
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsClassic();
    

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them. 
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple. 
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    bool show_another_window = true;
    bool show_image = true;
    bool show_expr_tree = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    
    std::string selected_name = "No node selected, displaying output";
    
    int width = input_full.width();
    int height = input_full.height();
    int channels = input_full.channels();
    
    GLuint idMyTexture = 0;
    glGenTextures(1, &idMyTexture);
    assert(idMyTexture != 0);
    
    glBindTexture(GL_TEXTURE_2D, idMyTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    //NOTE(Emily): call to update buffer to display output of function
    Profiling times = select_and_visualize(f, 0, input_full, idMyTexture);

    
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Start the ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // NOTE(Emily): main expression tree window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets automatically appears in a window called "Debug".
        if(show_expr_tree)
        {
            bool * no_close = NULL;
            //ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
            //ImGui::SetNextWindowSize(ImVec2(500,600));
            ImGui::Begin("Expression Tree", no_close, ImGuiWindowFlags_HorizontalScrollbar);
            //Note(Emily): call recursive method to display tree
            display_node(tree, idMyTexture, width, height, f, input_full, selected_name, times);
            ImGui::End();
            
        }
        
        
        //NOTE(Emily): Window to show image info
        if (show_another_window)
        {
            
            bool * no_close = NULL;
            //ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
            //ImGui::SetNextWindowSize(ImVec2(300,100));
            ImGui::Begin("Image Information", no_close);
            ImGui::Text("Information about the currently displayed image: ");
            std::string size_info = "width: " + std::to_string(width) + " height: " + std::to_string(height) + " channels: " + std::to_string(channels);
            ImGui::Text("%s", size_info.c_str());
            ImGui::Text("Currently Selected Expr: %s", selected_name.c_str());
            ImGui::Text("Time for JIT compilation: %f", times.jit_time);
            ImGui::Text("Time to realize: %f", times.run_time);
            ImGui::End();
        }
        
        if (show_image)
        {
            bool * no_close = NULL;
            
            //ImGui::SetNextWindowPos(ImVec2(650, 200), ImGuiCond_FirstUseEver);
            //ImGui::SetNextWindowSize(ImVec2(500,500));
            ImGui::Begin("Image", no_close, ImGuiWindowFlags_HorizontalScrollbar);
            
            static float zoom = 1.0f;
            ImGui::SliderFloat("Image Zoom", &zoom, 0, 10, "%.001f");
            
            ImGui::Image((void *) (uintptr_t) idMyTexture , ImVec2(width*zoom, height*zoom));
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);

        // NOTE(marcos): moving event handling to the end of the main loop so
        // that we don't "lose" the very first frame and are left staring at a
        // blank screen until the mouse starts hovering the window:

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        //glfwPollEvents();     // <- this is power hungry!
        glfwWaitEvents();       // <- this is a more CPU/power/battery friendly choice
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

}
