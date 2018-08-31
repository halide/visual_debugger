#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#include "debug-api.hpp"
#include "io-broadcast.hpp"

using namespace Halide;

// from 'imgui_main.cpp':
extern bool stdout_echo_toggle;
void run_gui(std::vector<Func> funcs, std::vector<Buffer<>> funcs_outputs);

struct UI
{
    void run(std::vector<Func> funcs, std::vector<Buffer<>> funcs_outputs)
    {
        // redirect stdout to a log file, effectivelly silencing the console output:
        const char* logfile = "data/output/log-halide-visdbg.txt";
        fprintf(stdout, ">> Redirecting 'stdout' to log file '%s'\n", logfile);
        FILE* log = fopen(logfile, "w");
        assert(log);
        Broadcaster iobc = redirect_broadcast(stdout);
        iobc.AddEcho(&stdout_echo_toggle);
        iobc.AddFile(log);
        
        run_gui(funcs, funcs_outputs);
        
        iobc.Terminate();
        fprintf(stdout, "<< Done with 'stdout' redirection\n");
        
        fclose(log);
    }
};

namespace Halide
{

void DebugFunc::realize(Pipeline::RealizationArg outputs, const Target &target,
                        const ParamMap &param_map)
{
    Buffer<> output;
    if(outputs.size() == 1)
    {
        halide_buffer_t * buf = outputs.buf;
        Buffer<> buffer (*buf);
        output = buffer;
    }
    else
    {
        output = outputs.buffer_list->at(0);
    }
    UI ui;
    std::vector<Func> funcs;
    funcs.push_back(this->f);
        
    ui.run(funcs, { output });

    this->f.realize(std::move(outputs), target, param_map);
        
}
    
Realization DebugFunc::realize(std::vector<int32_t> sizes, const Target &target,
                               const ParamMap &param_map)
{
    // TODO(marcos): maybe this should be the core implementation, instead of
    // the one with 'Pipeline::RealizationArg' above...
    Realization outputs = this->f.realize(sizes, target, param_map);
    realize(outputs, target, param_map);
    return outputs;
}

Realization DebugFunc::realize(int x_size, int y_size, int z_size, int w_size, const Target &target,
                               const ParamMap &param_map)
{
    return realize({ x_size, y_size, z_size, w_size }, target, param_map);;
}

Realization DebugFunc::realize(int x_size, int y_size, int z_size, const Target &target,
                               const ParamMap &param_map)
{
    return realize({ x_size, y_size, z_size }, target, param_map);
}

Realization DebugFunc::realize(int x_size, int y_size, const Target &target,
                               const ParamMap &param_map)
{
    return realize({ x_size, y_size }, target, param_map);
}

Realization DebugFunc::realize(Halide::Buffer<> input, int x_size, const Target &target,
                               const ParamMap &param_map)
{
    return realize(std::vector<int>{x_size}, target, param_map);
}

Realization DebugFunc::realize(const Target &target,
                               const ParamMap &param_map)
{
    return realize(std::vector<int>{}, target, param_map);
}

DebugFunc debug(Func f)
{
    DebugFunc debug_f;
    debug_f.f = f;
    return debug_f;
}



ReplayableFunc::ReplayableFunc(Func f)
: outputs(nullptr)
{
    this->f = f;
}

ReplayableFunc&& ReplayableFunc::realize(Pipeline::RealizationArg outputs, const Target &target, const ParamMap &param_map)
{
    new (&(this->outputs)) Pipeline::RealizationArg(std::move(outputs));
    this->target = target;
    new (&(this->param_map)) ParamMap(std::move(param_map));
    return std::move(*this);
}

void replay(std::vector<ReplayableFunc> &rpfuncs)
{
    std::vector<Func> funcs;
    std::vector<Buffer<>> funcs_outputs;
    for (auto& rpf : rpfuncs)
    {
        Func& f = rpf.f;
        auto& outputs = rpf.outputs;
        Buffer<> output;
        if (outputs.size() == 1)
        {
            halide_buffer_t * buf = outputs.buf;
            Buffer<> buffer(*buf);
            output = buffer;
        }
        else
        {
            output = outputs.buffer_list->at(0);
        }
        funcs.emplace_back(f);
        funcs_outputs.emplace_back(output);
    }

    UI ui;
    ui.run(funcs, funcs_outputs);
}

}
