#ifndef HALIDE_VISDBG_IMAGE_IO_H
#define HALIDE_VISDBG_IMAGE_IO_H

#include <Halide.h>

Halide::Buffer<> LoadImage(const char* filename);

const bool SaveImage(const char* filename, Halide::Buffer<>& image);

#endif//HALIDE_VISDBG_IMAGE_IO_H
