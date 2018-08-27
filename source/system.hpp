#ifndef SYSTEM_INTROSPECTION_H
#define SYSTEM_INTROSPECTION_H

#include <Halide.h>

struct SystemInfo
{
    Halide::Target host;

    bool cuda;
    bool opencl;
    bool metal;
    bool d3d12;

    SystemInfo();

    bool Supports(Halide::Target::Feature feature) const;

    bool Supports(Halide::Target::Arch architecture, int bits=0) const;
};

#endif//SYSTEM_INTROSPECTION_H
