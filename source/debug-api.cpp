#include "debug-api.hpp"
#include "io-broadcast.hpp"

// from 'imgui_main.cpp':
extern bool stdout_echo_toggle;
void run_gui(std::vector<Halide::Func> funcs, Halide::Buffer<> output_buff);

struct UI
{
    bool running = false;
    void run(std::vector<Halide::Func> funcs, Halide::Buffer<> output)
    {
        running = true;
        // redirect stdout to a log file, effectivelly silencing the console output:
        const char* logfile = "data/output/log-halide-visdbg.txt";
        fprintf(stdout, ">> Redirecting 'stdout' to log file '%s'\n", logfile);
        FILE* log = fopen(logfile, "w");
        assert(log);
        Broadcaster iobc = redirect_broadcast(stdout);
        iobc.AddEcho(&stdout_echo_toggle);
        iobc.AddFile(log);
        
        run_gui(funcs, output);
        running = false;
        
        iobc.Terminate();
        fprintf(stdout, "<< Done with 'stdout' redirection\n");
        
        fclose(log);
    }
    
    
};

    
void DebugFunc::realize(Halide::Pipeline::RealizationArg outputs, const Halide::Target &target, const Halide::ParamMap & param_map)
{
    if(outputs.size() == 1)
    {
        halide_buffer_t * buf = outputs.buf;
        Halide::Buffer<> buffer (*buf);
        this->output = buffer;
    }
    else
    {
        this->output = outputs.buffer_list->at(0);
    }
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    
    ui.run(funcs, this->output);
    while(ui.running){
        //gui running
    }
    this->f.realize(std::move(outputs), target, param_map);
    
}

//TODO(Emily): other realize calls
Halide::Realization DebugFunc::realize(std::vector<int32_t> sizes, const Halide::Target &target,
                    const Halide::ParamMap &param_map)
{
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    Halide::Realization outputs = this->f.realize(sizes, target, param_map);
    this->output = std::move(outputs[0]);
    ui.run(funcs, this->output);
    while(ui.running)
    {
        //gui running
    }
    return outputs;
}

Halide::Realization DebugFunc::realize(int x_size, int y_size, int z_size, int w_size, const Halide::Target &target,
                    const Halide::ParamMap &param_map)
{
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    Halide::Realization outputs = this->f.realize(x_size, y_size, z_size, w_size, target, param_map);
    this->output = std::move(outputs[0]);
    ui.run(funcs, this->output);
    while(ui.running)
    {
        //gui running
    }
    return outputs;
}

Halide::Realization DebugFunc::realize(int x_size, int y_size, int z_size, const Halide::Target &target,
                    const Halide::ParamMap &param_map)
{
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    Halide::Realization outputs = this->f.realize(x_size, y_size, z_size, target, param_map);
    this->output = std::move(outputs[0]);
    ui.run(funcs, this->output);
    while(ui.running)
    {
        //gui running
    }
    return outputs;
}

Halide::Realization DebugFunc::realize(int x_size, int y_size, const Halide::Target &target,
                    const Halide::ParamMap &param_map)
{
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    Halide::Realization outputs = this->f.realize(x_size, y_size, target, param_map);
    this->output = std::move(outputs[0]);
    ui.run(funcs, this->output);
    while(ui.running)
    {
        //gui running
    }
    return outputs;
}

Halide::Realization DebugFunc::realize(Halide::Buffer<> input, int x_size, const Halide::Target &target,
                    const Halide::ParamMap &param_map)
{
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    Halide::Realization outputs = this->f.realize(x_size, target, param_map);
    this->output = std::move(outputs[0]);
    ui.run(funcs, this->output);
    while(ui.running)
    {
        //gui running
    }
    return outputs;
}

Halide::Realization DebugFunc::realize(const Halide::Target &target,
                    const Halide::ParamMap &param_map)
{
    
    UI ui;
    std::vector<Halide::Func> funcs;
    funcs.push_back(this->f);
    Halide::Realization outputs = this->f.realize(target, param_map);
    this->output = std::move(outputs[0]);
    ui.run(funcs, this->output);
    while(ui.running)
    {
        //gui running
    }
    return outputs;
}

DebugFunc debug(Halide::Func f)
{
    DebugFunc debug_f;
    debug_f.f = f;
    return debug_f;
}

