#include "utils.h"

struct DebugFunc
{
    Halide::Func f;
    Halide::Buffer<> output;
    
    void realize(Halide::Pipeline::RealizationArg outputs, const Halide::Target &target = Halide::Target(), const Halide::ParamMap & param_map = Halide::ParamMap::empty_map());
    
    Halide::Realization realize(std::vector<int32_t> sizes, const Halide::Target &target = Halide::Target(),
                        const Halide::ParamMap &param_map = Halide::ParamMap::empty_map());
    
    Halide::Realization realize(int x_size, int y_size, int z_size, int w_size, const Halide::Target &target = Halide::Target(),
                        const Halide::ParamMap &param_map = Halide::ParamMap::empty_map());
    
    Halide::Realization realize(int x_size, int y_size, int z_size, const Halide::Target &target = Halide::Target(),
                        const Halide::ParamMap &param_map = Halide::ParamMap::empty_map());
    
    Halide::Realization realize(int x_size, int y_size, const Halide::Target &target = Halide::Target(),
                        const Halide::ParamMap &param_map = Halide::ParamMap::empty_map());
    
    Halide::Realization realize(Halide::Buffer<> input, int x_size, const Halide::Target &target = Halide::Target(),
                        const Halide::ParamMap &param_map = Halide::ParamMap::empty_map());
    
    Halide::Realization realize(const Halide::Target &target = Halide::Target(),
                        const Halide::ParamMap &param_map = Halide::ParamMap::empty_map());
};

DebugFunc debug(Halide::Func f);
