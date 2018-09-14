//
// Licensed under the MIT License. See LICENSE.TXT file in the project root for full license information.
//

#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#include "halide-image-io.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third-party/stb/stb_image.h"
#include "../third-party/stb/stb_image_write.h"

Halide::Buffer<> LoadImage(const char* filename)
{
    // load image from disk:
    int imgWidth(0), imgHeight(0), imgChannels(0);
    auto imgPixels = stbi_load(filename, &imgWidth, &imgHeight, &imgChannels, 3);
    if (nullptr == imgPixels)
    {
        return Halide::Buffer<>();
    }
    imgChannels = 3;

    // prepare a Buffer from a Runtime::Buffer using interleaved pixel format:
    Halide::Buffer<uint8_t> image = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved((uint8_t*)imgPixels, imgWidth, imgHeight, imgChannels);
    return(image);
}

const bool SaveImage(const char* filename, Halide::Buffer<>& image)
{
    auto imgPixels   = image.data();
    auto imgWidth    = image.width();
    auto imgHeight   = image.height();
    auto imgChannels = image.channels();

    auto type = image.type();

    // find image extension and save it to disk:
    int ret = 0;
    auto ext = strrchr(filename, '.');
    if (strstr(ext, ".jpg") || strstr(ext, ".jpeg"))
    {
        const int jpeg_quality = 95;  // from 0 (worst) to 100 (best)
        ret = stbi_write_jpg(filename, imgWidth, imgHeight, imgChannels, (stbi_uc*)imgPixels, jpeg_quality);
    }
    if (strstr(ext, ".png"))
    {
      int stride = imgChannels*imgWidth;
      ret = stbi_write_png(filename, imgWidth, imgHeight, imgChannels, (stbi_uc*)imgPixels, stride);
    }
    if (strstr(ext, ".bmp"))
    {
        ret = stbi_write_bmp(filename, imgWidth, imgHeight, imgChannels, (stbi_uc*)imgPixels);
    }
    if (strstr(ext, ".tga"))
    {
        ret = stbi_write_tga(filename, imgWidth, imgHeight, imgChannels, (stbi_uc*)imgPixels);
    }
    if (strstr(ext, ".hdr"))
    {
        assert(type.is_float());
        assert(type.bits() == 32);
        ret = stbi_write_hdr(filename, imgWidth, imgHeight, imgChannels, (float*)imgPixels);
    }
    if(strstr(ext, ".pfm"))
    {
        assert(type.is_float());
        assert(type.bits() == 32);
        ret = stbi_write_pfm(filename, imgWidth, imgHeight, imgChannels, (float*)imgPixels);
    }

    bool saved = (ret != 0);
    return saved;
}
