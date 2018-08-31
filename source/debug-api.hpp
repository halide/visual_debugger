#ifndef HALIDE_VISDBG_DEBUG_API_H
#define HALIDE_VISDBG_DEBUG_API_H

#include <Halide.h>

namespace Halide
{

struct DebugFunc
{
    Func f;
    Buffer<> output;

    void realize(Pipeline::RealizationArg outputs, const Target &target = Target(),
                 const ParamMap & param_map = ParamMap::empty_map());

    Realization realize(std::vector<int32_t> sizes, const Target &target = Target(),
                        const ParamMap &param_map = ParamMap::empty_map());

    Realization realize(int x_size, int y_size, int z_size, int w_size, const Target &target = Target(),
                        const ParamMap &param_map = ParamMap::empty_map());

    Realization realize(int x_size, int y_size, int z_size, const Target &target = Target(),
                        const ParamMap &param_map = ParamMap::empty_map());

    Realization realize(int x_size, int y_size, const Target &target = Target(),
                        const ParamMap &param_map = ParamMap::empty_map());

    Realization realize(Buffer<> input, int x_size, const Target &target = Target(),
                        const ParamMap &param_map = ParamMap::empty_map());

    Realization realize(const Target &target = Target(),
                        const ParamMap &param_map = ParamMap::empty_map());
};

DebugFunc debug(Func f);

}

#endif//HALIDE_VISDBG_DEBUG_API_H