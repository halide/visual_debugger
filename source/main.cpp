//#include "kawase.cpp"
//#include "treedump.cpp"
#include "imgui_main.cpp"

int main()
{
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
    
    expr_node * tree = tree_from_func(output, input_full);
    run_gui(tree, output, input_full);
    return 0;
}
