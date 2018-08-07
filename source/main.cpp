#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

//#include "kawase.cpp"
//#include "treedump.cpp"
#include "imgui_main.cpp"

//#include "io-broadcast.hpp"


Func example_broken(Halide::Buffer<uint8_t> input_full)
{
    
    Var x("x"), y("y"), c("c"), i("i");
    
    Expr clamped_x = clamp(x, 0, input_full.width()-1);
    Expr clamped_y = clamp(y, 0, input_full.height()-1);
    
    Func bounded("bounded");
    bounded(x,y,c) = input_full(clamped_x, clamped_y, c);
    
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

Func example_fixed(Halide::Buffer<uint8_t> input_full)
{
    
    Var x("x"), y("y"), c("c"), i("i");
    
    Expr clamped_x = clamp(x, 0, input_full.width()-1);
    Expr clamped_y = clamp(y, 0, input_full.height()-1);
    
    Func bounded("bounded");
    bounded(x,y,c) = cast<int16_t>(input_full(clamped_x, clamped_y, c));
    
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

extern bool stdout_echo_toggle;    // <- from imgui_main.cpp;

int main()
{
    // redirect stdout to a log file, effectivelly silencing the console output:
    const char* logfile = "data/output/log-halide-visdbg.txt";
    fprintf(stdout, ">> Redirecting 'stdout' to log file '%s'\n", logfile);
    FILE* log = fopen(logfile, "w");
    assert(log);
    Broadcaster iobc = redirect_broadcast(stdout);
    iobc.AddEcho(&stdout_echo_toggle);
    iobc.AddFile(log);

    //NOTE(Emily): define func here instead of in treedump for now
    xsprintf(input_filename, 128, "data/pencils.jpg");
    Halide::Buffer<uint8_t> input_full = LoadImage(input_filename);
    if (!input_full.defined())
        return  NULL;
    
    Func input = Identity(input_full, "image");
    
    Func output { "output" };
    //output(x, y, c) = cast(type__of(input), final);
    
    {
        Func f{ "f" };
        Func g{"g"};
        {
            Var x, y, c;
            f(x,y, c) = 10 * input(x, 0, 2-c);
            g(x,y,c) = input(x, y, c) + 1;
        }
        
        {
            Var x, y, c;
            output(x, y, c) = f(x, y,c) + g(x,y,c);
        }
    }

    Func broken { "broken" };
    broken = example_broken(input_full);
    
    Func fixed { "fixed" };
    fixed = example_fixed(input_full);
    
    std::vector<Func> funcs;
    funcs.push_back(broken);
    funcs.push_back(fixed);
    run_gui(funcs, input_full, iobc);
    
    //run_gui(output, input_full);

    //run_gui(example_broken(), input_full);

    iobc.Terminate();
    fprintf(stdout, "<< Done with 'stdout' redirection\n");

    fclose(log);

    return 0;
}
