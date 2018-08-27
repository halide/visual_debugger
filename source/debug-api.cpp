#include "utils.h"
using namespace Halide;

struct DebugFunc
{
    Func f;
    
    void realize(Halide::Runtime::Buffer<> output, const Target &target = Target(), const ParamMap & param_map = ParamMap::empty_map()) //args, ...
    {
        /*
         UI ui;
         while(ui.running(f, args, ...))
            ;
         return this->f.realize(args, ...)
         */
        this->f.realize(output, target, param_map); //TODO(Emily): realize should take a Pipeline::RealizationArg
        
    }
};

// TODO(Emily): need to implement
DebugFunc debug(Func f);
