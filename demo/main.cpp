#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#include "../include/debug-api.h"
#include "../src/halide-image-io.h"

using namespace Halide;

// utility to wrap a Buffer into a "safe" Func (bounded domain)
Func SafeIdentity(Buffer<> image, const char* name = nullptr)
{
    Func I = (name) ? Func(name) : Func("safe_" + image.name()) ;
    Var x("x"), y("y"), c("c");

    Expr _x = clamp(x, 0, image.width()    - 1);
    Expr _y = clamp(y, 0, image.height()   - 1);
    Expr _c = clamp(c, 0, image.channels() - 1);

    Expr value;
    const auto dimensions = image.dimensions();
    switch (dimensions)
    {
        case 1 :
            value = image(_x);
            break;
        case 2 :
            value = image(_x, _y);
            break;
        case 3 :
            value = image(_x, _y, _c);
            break;
        default :
        {
            Expr False = Internal::const_false();
            value = require(False, Expr(dimensions), "is an invalid dimension for the Identity(", image.name(), ",", name, ") call.");
            break;
        }
    }

    I(x, y, c) = value;

    return(I);
}

Func add_gpu_schedule(Func f);  // <- quick-hack: defined in imgui_main.cpp

Func example_select()
{
    Var x("x"), y("y");
    Func color_image("select example");
    Var c;
    color_image(x, y, c) = select(c == 0 && x < 400, 255, // Red value
                                  c == 1 && y < 200, 255,  // Green value
                                  0);
    return color_image;
}

Func example_broken(Func input)
{
    Var x("x"), y("y"), c("c"), i("i");
    
    Func bounded("bounded");
    bounded(x,y,c) = input(x, y, c);
    
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

    output = add_gpu_schedule(output);
    
    return output;
}

Func example_fixed(Func input)
{
    Var x("x"), y("y"), c("c"), i("i");
    
    Func bounded("bounded");
    bounded(x,y,c) = cast<int16_t>(input(x, y, c));
    
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

Func example_scoped(Func input)
{
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

Func update_example()
{
    Var x, y, c;
    Func updated ("update def example");
    updated(x,y,c) = x + y;
    updated(x,y,0) = x + 100;
    updated(x,y,c) = updated(x,y,c) + 20;
    return updated;
}

Func select_example2()
{
    Var x, y, c, tx, ty;
    Func selected2 ("select example 2");
    selected2(x,y,c) = select(c == 0, x + 100, x + y);
    selected2 = add_gpu_schedule(selected2);
    return selected2;
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
    Func gradient("gradient example");
    Var x, y;
    Expr e = x + y;
    gradient(x,y) = e;
    
    return gradient;
}



int main()
{
    Halide::Buffer<uint8_t> input_full = LoadImage("data/pencils.jpg");
    if (!input_full.defined())
        return -1;

    Func image = SafeIdentity(input_full, "safe_image");

    Func broken = example_broken(image);
    Func fixed = example_fixed(image);

    //NOTE(Emily): setting up output buffer to use with realize in debug
    Halide::Buffer<> modified_output_buffer;
    modified_output_buffer = Halide::Buffer<uint8_t>::make_interleaved(input_full.width(), input_full.height(), input_full.channels());
    // TODO(marcos): would it make sense to try to automate-away this Func output
    // buffer setup from the user?
    broken.output_buffer()
        .dim(0).set_stride( modified_output_buffer.dim(0).stride() )
        .dim(1).set_stride( modified_output_buffer.dim(1).stride() )
        .dim(2).set_stride( modified_output_buffer.dim(2).stride() );

    std::vector<ReplayableFunc> funcs;
        funcs.emplace_back( ReplayableFunc(broken).realize(modified_output_buffer) );
        funcs.emplace_back( ReplayableFunc(fixed).realize(modified_output_buffer) );
        funcs.emplace_back( ReplayableFunc(example_scoped(image)).realize(modified_output_buffer) );
        funcs.emplace_back( ReplayableFunc(example_tuple()).realize(modified_output_buffer) );
        funcs.emplace_back( ReplayableFunc(update_example()).realize(modified_output_buffer) );
        funcs.emplace_back( ReplayableFunc(update_tuple_example()).realize(modified_output_buffer) );
        funcs.emplace_back( ReplayableFunc(example_select()).realize(modified_output_buffer));
        funcs.emplace_back( ReplayableFunc(select_example2()).realize(modified_output_buffer));
    
    //debug(broken).realize(modified_output_buffer, host_target);
    replay(funcs);

    return 0;
}
