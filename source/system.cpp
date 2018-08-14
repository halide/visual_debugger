#include "system.hpp"

#ifdef _WIN32
    #include <Windows.h>
#else
#endif

#include <iterator>     // std::size

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

void free_library(void* handle)
{
    #ifdef _WIN32
        HMODULE hLib = reinterpret_cast<HMODULE>(handle);
        FreeLibrary(hLib);
    #else
        dlclose(handle);
    #endif
}

void* load_any(const char* filenames [], size_t count)
{
    for (int i = 0; i < count; ++i)
    {
        const char* filename = filenames[i];
        void* handle = load_library(filename);
        if (handle != nullptr)
        {
            return handle;
        }
    }
    return nullptr;
}

bool check_CUDA()
{
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
    static void* lib_handle = load_any(libs, std::size(libs));
    bool has = (lib_handle != nullptr);
    //free_library(lib_handle);
    return has;
}

bool check_OpenCL()
{
    const char* libs [] =
    {
        #ifdef _WIN32
            "opencl.dll",
        #else
            "libOpenCL.so",
            "/System/Library/Frameworks/OpenCL.framework/OpenCL",
        #endif
    };
    static void* lib_handle = load_any(libs, std::size(libs));
    bool has = (lib_handle != nullptr);
    //free_library(lib_handle);
    return has;
}

bool check_Metal()
{
    const char* libs [] =
    {
        #ifdef _WIN32
            nullptr
        #else
            "/System/Library/Frameworks/Metal.framework/Versions/Current/Metal",
        #endif
    };
    static void* lib_handle = load_any(libs, std::size(libs));
    bool has = (lib_handle != nullptr);
    //free_library(lib_handle);
    return has;
}

bool check_D3D12()
{
    const char* libs [] =
    {
        #ifdef _WIN32
            "d3d12.dll",
        #else
            nullptr
        #endif
    };
    static void* lib_handle = load_any(libs, std::size(libs));
    bool has = (lib_handle != nullptr);
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
