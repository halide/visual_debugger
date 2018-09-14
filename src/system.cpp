//
// Licensed under the MIT License. See LICENSE.TXT file in the project root for full license information.
//

#include "system.hpp"

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

#include <iterator>     // std::size

static
void* load_library(const char* filename)
{
    void* handle = nullptr;
    #ifdef _WIN32
        HMODULE hLib = LoadLibraryA(filename);
        handle = reinterpret_cast<void*>(hLib);
    #else
        handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    #endif
    return handle;
}

static
void free_library(void* handle)
{
    #ifdef _WIN32
        HMODULE hLib = reinterpret_cast<HMODULE>(handle);
        FreeLibrary(hLib);
    #else
        dlclose(handle);
    #endif
}

template<int count>
void* load_any(const char* (&filenames) [count])
{
    for (auto filename : filenames)
    {
        void* handle = load_library(filename);
        if (handle != nullptr)
        {
            return handle;
        }
    }
    return nullptr;
}

static
bool check_halide_features(const char* features)
{
    std::string target_string = "host-";
    target_string += features;
    bool valid = Halide::Target::validate_target_string(target_string);
    return valid;
}

static
bool check_CUDA()
{
    if (!check_halide_features("cuda"))
        return false;

    const char* libs [] =
    {
        #ifdef _WIN32
            "nvcuda.dll",
        #else
            "libcuda.so",
            "libcuda.dylib",
            "/Library/Frameworks/CUDA.framework/CUDA",
        #endif
    };
    static void* lib_handle = load_any(libs);
    bool has = (lib_handle != nullptr);
    // NOTE(marcos): keeping the handle open to prevent external deletion
    //free_library(lib_handle);
    return has;
}

static
bool check_OpenCL()
{
    if (!check_halide_features("opencl"))
        return false;

    const char* libs [] =
    {
        #ifdef _WIN32
            "opencl.dll",
        #else
            "libOpenCL.so",
            "/System/Library/Frameworks/OpenCL.framework/OpenCL",
        #endif
    };
    static void* lib_handle = load_any(libs);
    bool has = (lib_handle != nullptr);
    // NOTE(marcos): keeping the handle open to prevent external deletion
    //free_library(lib_handle);
    return has;
}

static
bool check_Metal()
{
    if (!check_halide_features("metal"))
        return false;

    const char* libs [] =
    {
        #ifdef _WIN32
            ""
        #else
            "/System/Library/Frameworks/Metal.framework/Versions/Current/Metal",
        #endif
    };
    static void* lib_handle = load_any(libs);
    bool has = (lib_handle != nullptr);
    // NOTE(marcos): keeping the handle open to prevent external deletion
    //free_library(lib_handle);
    return has;
}

static
bool check_D3D12()
{
    if (!check_halide_features("d3d12compute"))
        return false;

    const char* libs [] =
    {
        #ifdef _WIN32
            "d3d12.dll",
        #else
            ""
        #endif
    };
    static void* lib_handle = load_any(libs);
    bool has = (lib_handle != nullptr);
    // NOTE(marcos): keeping the handle open to prevent external deletion
    //free_library(lib_handle);
    return has;
}

SystemInfo::SystemInfo()
{
    this->host = Halide::get_host_target();

    cuda   = check_CUDA();
    opencl = check_OpenCL();
    metal  = check_Metal();
    d3d12  = check_D3D12();
}

bool SystemInfo::Supports(Halide::Target::Feature feature) const
{
    return host.has_feature(feature);
}

bool SystemInfo::Supports(Halide::Target::Arch architecture, int bits) const
{
    bool support = (host.arch == architecture);
    if (bits != 0)
    {
        support &= (host.bits == bits);
    }
    return support;
}
