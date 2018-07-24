//#include "kawase.cpp"
//#include "treedump.cpp"
#include "imgui_main.cpp"

int main()
{
    expr_node * tree = tree_from_func();
    run_gui(tree);
    return 0;
}
