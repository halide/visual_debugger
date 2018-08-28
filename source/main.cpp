#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#include <Halide.h>
#include "HalideIdentity.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "HalideImageIO.h"

#include "io-broadcast.hpp"
#include "debug-api.hpp"

using namespace Halide;

Func example_broken(Buffer<> image)
{
    Var x("x"), y("y"), c("c"), i("i");
    
    Expr clamped_x = clamp(x, 0, image.width()-1);
    Expr clamped_y = clamp(y, 0, image.height()-1);

    Func input = Identity(image, "image");

    Func bounded("bounded");
    bounded(x,y,c) = input(clamped_x, clamped_y, c);
    
    Func sharpen("sharpen");
    sharpen(x,y,c) = 2 * bounded(x,y,c) - (bounded(x-1, y, c) + bounded(x, y-1, c) + bounded(x+1, y, c) + bounded(x, y+1, c))/4;
    
    Func gradient("gradient");
    gradient(x,y,c) = (x + y)/1024.0f;
    
    Func blend("blend");
    blend(x,y,c) = sharpen(x,y,c) * gradient(x,y,c);
    
    Func lut("lut");
    lut(i) = pow((i/255.0f), 1.2f) * 255.0f;
    
    Func output("broken output");
    output(x,y,c) = cast<uint8_t>(lut(cast<uint8_t>(blend(x,y,c))));
    
    return output;
}

Func example_fixed(Buffer<> image)
{
    Var x("x"), y("y"), c("c"), i("i");
    
    Expr clamped_x = clamp(x, 0, image.width()-1);
    Expr clamped_y = clamp(y, 0, image.height()-1);

    Func input = Identity(image, "image");

    Func bounded("bounded");
    bounded(x,y,c) = cast<int16_t>(input(clamped_x, clamped_y, c));
    
    Func sharpen("sharpen");
    sharpen(x,y,c) = cast<uint8_t>(clamp(2 * bounded(x,y,c) - (bounded(x-1, y, c) + bounded(x, y-1, c) + bounded(x+1, y, c) + bounded(x, y+1, c))/4, 0, 255));
    
    Func gradient("gradient");
    gradient(x,y,c) = clamp((x + y)/1024.0f, 0, 255);
    
    Func blend("blend");
    blend(x,y,c) = clamp(sharpen(x,y,c) * gradient(x,y,c), 0, 255);
    
    Func lut("lut");
    lut(i) = pow((i/255.0f), 1.2f) * 255.0f;
    
    Func output("fixed output");
    output(x,y,c) = cast<uint8_t>(lut(cast<uint8_t>(blend(x,y,c))));
    
    return output;
}

Func example_scoped(Buffer<> image)
{
    Func input = Identity(image, "image");

    Func f ("f");
    {
        Var x, y, c;
        f(x,y, c) = 10 * input(x, 0, 2-c);
    }

    Func g ("g");
    {
        Var x, y, c;
        g(x,y,c) = input(x, y, c) + 1;
    }

    Func h ("h");
    {
        Var x, y, c;
        h(x, y, c) = f(x, y, c) + g(x, y, c);
    }

    return h;
}

Func example_tuple()
{
    // TODO(marcos): if we were to write 'multi_valued_2(x, y) = ...' the tool
    // would crash in 'select_and_visualize()' because the output realization
    // buffer is formatted according to 'input_full' dimensions, which may be
    // a color image... ideally, the output realization buffers must be passed
    // in order to get the correct layout and format for the visualization.
    Var x, y, c;
    Func multi_valued_2 ("example_tuple");
    multi_valued_2(x, y, c) = { x + y, sin(x*y) };
    return multi_valued_2;
}

Func example_another_tuple(Func broken, Func fixed)
{
    Var x, y, c;
    Func test_tuple ("test");
    test_tuple(x, y, c) = Tuple(broken(x, y, c), fixed(x, y, c));
    return test_tuple;
}

Func update_example()
{
    Var x, y, c;
    Func updated ("update def example");
    updated(x,y,c) = x + y;
    updated(x,y,0) = x + 100;
    updated(x,y,c) = updated(x,y,c) + 20;
    return updated;
}

Func update_tuple_example()
{
    Var x, y, c;
    Func updated ("update def tuple example");
    updated(x,y,c) = {x + y, sin(x*y)};
    updated(x,y,0) = {x + 100, x + 0.0f};
    return updated;
}


// from 'imgui_main.cpp':
extern bool stdout_echo_toggle;
void run_gui(std::vector<Func> funcs, Halide::Buffer<uint8_t>& input_full); 

int main()
{

    //NOTE(Emily): define func here
    xsprintf(input_filename, 128, "data/pencils.jpg");
    Halide::Buffer<uint8_t> input_full = LoadImage(input_filename);
    if (!input_full.defined())
        return -1;

    Func broken = example_broken(input_full);

    Func fixed = example_fixed(input_full);

    std::vector<Func> funcs;
    funcs.push_back(broken);
    funcs.push_back(fixed);
    funcs.push_back(example_scoped(input_full));
    funcs.push_back(example_tuple());
    funcs.push_back(example_another_tuple(broken, fixed));
    funcs.push_back(update_example());
    funcs.push_back(update_tuple_example());
    
    
    //NOTE(Emily): setting up output buffer to realize in debug
    //Probably want to instead figure out stride/size etc. in debug.realize
    Halide::Runtime::Buffer<> modified_output_buffer;
    modified_output_buffer = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved(input_full.width(), input_full.height(), input_full.channels());
    Target host_target = get_host_target();
    broken.output_buffer()
    .dim(0).set_stride( modified_output_buffer.dim(0).stride() )
    .dim(1).set_stride( modified_output_buffer.dim(1).stride() )
    .dim(2).set_stride( modified_output_buffer.dim(2).stride() );
    
    debug(broken).realize(input_full, modified_output_buffer, host_target);

    return 0;
}
