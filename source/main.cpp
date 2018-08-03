#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

//#include "kawase.cpp"
//#include "treedump.cpp"
#include "imgui_main.cpp"

#include "io-redirect.hpp"

Func example_broken(){
    xsprintf(input_filename, 128, "data/pencils.jpg");
    Halide::Buffer<uint8_t> input_full = LoadImage(input_filename);
    assert(input_full.defined());
    
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
    
    Func output("output");
    output(x,y,c) = cast<uint8_t>(lut(cast<uint8_t>(blend(x,y,c))));
    
    return output;
}

int main()
{
    // redirect stdout to a log file, effectivelly silencing the console output:
    const char* logfile = "data/output/log-halide-visdbg.txt";
    fprintf(stderr, "redirecting 'stdout' to log file '%s'\n", logfile);
    FILE* log = fopen(logfile, "w");
    assert(log);
    redirect_permanently(stdout, log);

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
    
    run_gui(output, input_full);

    //run_gui(example_broken(), input_full);

    fclose(log);

    return 0;
}
