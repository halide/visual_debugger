#include "utils.h"
#include "io-broadcast.hpp"

using namespace Halide;

// from 'imgui_main.cpp':
extern bool stdout_echo_toggle;
void run_gui(std::vector<Func> funcs, Halide::Buffer<uint8_t>& input_full);

struct UI
{
    bool running = false;
    void run(std::vector<Func> funcs, Halide::Buffer<uint8_t> input)
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
        
        run_gui(funcs, input);
        running = false;
        
        iobc.Terminate();
        fprintf(stdout, "<< Done with 'stdout' redirection\n");
        
        fclose(log);
    }
    
    
};

struct DebugFunc
{
    Func f;
    
    void realize(Halide::Buffer<uint8_t> input, Halide::Runtime::Buffer<> output, const Target &target = Target(), const ParamMap & param_map = ParamMap::empty_map()) //args, ...
    {
        
        UI ui;
        std::vector<Func> funcs;
        funcs.push_back(f);
        ui.run(funcs, input);
        while(ui.running){
            //do something
        }
        this->f.realize(output, target, param_map); //TODO(Emily): realize should take a Pipeline::RealizationArg
        
    }
};

// TODO(Emily): need to implement
DebugFunc debug(Func f);
