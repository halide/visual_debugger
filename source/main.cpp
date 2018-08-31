#include "HalideImageIO.h"
#include "HalideIdentity.h"

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

Func simple_realize_x_y_example()
{
    Func gradient;
    Var x, y;
    Expr e = x + y;
    gradient(x,y) = e;
    
    return gradient;
}



int main()
{
    //NOTE(Emily): define func here
    xsprintf(input_filename, 128, "data/pencils.jpg");
    Halide::Buffer<uint8_t> input_full = LoadImage(input_filename);
    if (!input_full.defined())
        return -1;

    Func broken = example_broken(input_full);
    Func fixed = example_fixed(input_full);

    //NOTE(Emily): setting up output buffer to use with realize in debug
    Halide::Buffer<> modified_output_buffer;
    modified_output_buffer = Halide::Buffer<uint8_t>::make_interleaved(input_full.width(), input_full.height(), input_full.channels());
    Target host_target = get_host_target();
    broken.output_buffer()
    .dim(0).set_stride( modified_output_buffer.dim(0).stride() )
    .dim(1).set_stride( modified_output_buffer.dim(1).stride() )
    .dim(2).set_stride( modified_output_buffer.dim(2).stride() );

    std::vector<ReplayableFunc> funcs;
    funcs.emplace_back( ReplayableFunc(broken).realize(modified_output_buffer) );
    funcs.emplace_back( ReplayableFunc(fixed).realize(modified_output_buffer) );
    funcs.emplace_back( ReplayableFunc(example_scoped(input_full)).realize(modified_output_buffer) );
    funcs.emplace_back( ReplayableFunc(example_tuple()).realize(modified_output_buffer) );
    funcs.emplace_back( ReplayableFunc(example_another_tuple(broken, fixed)).realize(modified_output_buffer) );
    funcs.emplace_back( ReplayableFunc(update_example()).realize(modified_output_buffer) );
    funcs.emplace_back( ReplayableFunc(update_tuple_example()).realize(modified_output_buffer) );
    
    debug(broken).realize(modified_output_buffer, host_target);
    
    Func simple_other_realize = simple_realize_x_y_example();
    //debug(simple_other_realize).realize(800, 600, host_target); //NOTE(Emily): right now we need to pass in an input buffer although it isn't used. should handle this use case

    return 0;
}
