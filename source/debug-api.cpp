#include "utils.h"
using namespace Halide;

struct DebugFunc
{
    Func f;
    void debug(Func f)
    {
        this->f = f;
    }
    
    void realize() //args, ...
    {
        /*
         UI ui;
         while(ui.running(f, args, ...))
            ;
         return this->f.realize(args, ...)
         */
    }
};

