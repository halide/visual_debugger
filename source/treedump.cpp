#ifdef  _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #endif//_CRT_SECURE_NO_WARNINGS
#endif//_MSC_VER

#ifdef  _MSC_VER
    #include <direct.h>
    #define mkdir(path, dontcare) _mkdir(path)
    #define S_IRWXU 0000700
    #define S_IRWXG 0000070
    #define S_IRWXO 0000007
#else
    #include <sys/stat.h>
#endif//_MSC_VER



// //// Utilities /////////////////////////////////////////////////////////////
#ifndef XSTR
#define XSTR(x) #x
#endif//XSTR

#ifndef STR
#define STR(x) XSTR(x)
#endif//XSTR

// auxiliary function to manipulate strings directly in stack memory
#define xsprintf(var, size, ...) char var [size]; sprintf(var, __VA_ARGS__)
///////////////////////////////////////////////////////////// Utilities //// //


//#include "utils.h"

#include "varmap.cpp"




// //// stb image I/O /////////////////////////////////////////////////////////
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb/stb_image.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third-party/stb/stb_image_write.h"

#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../third-party/stb/stb_image_resize.h"
///////////////////////////////////////////////////////// stb image I/O //// //

#include "HalideImageIO.h"

using namespace Halide;

template<typename T>
Runtime::Buffer<T> Crop(Buffer<T>& image, int hBorder, int vBorder)
{
#if !CLAMP_TO_BORDER_EDGE
    const int width  = image.width();
    const int height = image.height();
    auto image_cropped = *image.get();
    image_cropped.crop(0, hBorder, width  - 2 * hBorder);
    image_cropped.crop(1, vBorder, height - 2 * vBorder);
#endif
    return(image_cropped);
}

template<typename T>
Buffer<T>& Uncrop(Buffer<T>& image)
{
    assert(false && "Halide buffer uncropping is dangerous and should not be used!");
    return(image);

    // deprecated/legacy code to follow:
    const int width   = image.width();
    const int height  = image.height();
    const int hBorder = image.dim(0).min();
    const int vBorder = image.dim(1).min();
    image.crop(0, 0, width  + 2 * hBorder);
    image.crop(1, 0, height + 2 * vBorder);
    return(image);
}



Type type__of(Func f)
{
    // assuming 'f' is not a tuple and only resolves to a single value:
    Expr expr = f.value();
    Type type = expr.type();
    return(type);
}

Type promote(Type type)
{
    if (type.is_float())
    {
        return(type);
    }

    auto more_bits = type.bits() * 2;
    auto promoted = Int(more_bits);
    return(promoted);
}

Func promote(Func f)
{
    auto ftype = type__of(f);
    auto gtype = promote(ftype);
    Func g { f.name() };
    auto domain = f.args();
    g(domain) = cast(gtype, f(domain));
    return(g);
}

Func as_weights(Func f)
{
    auto ftype = type__of(f);
    if (ftype.is_float())
        return(f);

    auto domain = f.args();
    Expr w = cast(Float(32), f(domain));
    w /= cast(Float(32), ftype.max());
    Func g { "w_" + f.name() };
    g(domain) = w;
    return(g);
}

Var x { "x" };
Var y { "y" };
Var c { "c" };

Func Sobel(Func input)
{
    Var x, y, c;

    Type base_type = type__of(input);
    Type high_type = promote(base_type);

    Func I { "input" };
    I(x, y, c) = cast(high_type, input(x, y, c));

    /*
        *     -1    0   +1         1    0   +1
        *   +--------------+    +--------------+
        * -1| -1 |  0 |  1 |  -1| -1 | -2 | -1 |
        *   +--------------+    +--------------+
        *  0| -2 |  0 |  2 |   0|  0 |  0 |  0 |
        *   +--------------+    +--------------+
        * +1| -1 |  0 |  1 |  +1|  1 |  2 |  1 |
        *   +--------------+    +--------------+
        *        sobel_x             sobel_y
        */
    Func sobel_x ("sobel_x");
    sobel_x(x, y, c) = I(x+1, y-1, c)
                     - I(x-1, y-1, c)
                     + 2 * (I(x+1, y, c) - I(x-1, y, c))
                     + I(x+1, y+1, c)
                     - I(x-1, y+1, c);

    Func sobel_y ("sobel_y");
    sobel_y(x, y, c) = I(x-1, y+1, c)
                     - I(x-1, y-1, c)
                     + 2 * (I(x, y+1, c) - I(x, y-1, c))
                     + I(x+1, y+1, c)
                     - I(x+1, y-1, c);

    Expr sobel_xy = sobel_x(x, y, c) * sobel_x(x, y, c)
                  + sobel_y(x, y, c) * sobel_y(x, y, c);

    sobel_xy = sqrt(sobel_xy);
    sobel_xy = cast(high_type, sobel_xy);
    sobel_xy = clamp(sobel_xy, 0, base_type.max());

    Func output { "Sobel" };
    output(x, y, c) = cast(base_type, sobel_xy);;
    return output;
}

Func Blur(Func input)
{
    Var x, y, c;

    Type base_type = type__of(input);
    Type high_type = promote(base_type);

    Func I { "input" };
    I(x, y, c) = cast(high_type, input(x, y, c));

    /*
     * +--------------+
     * |  0 |  1 |  0 |
     * +--------------+
     * |  1 |  4 |  1 |
     * +--------------+
     * |  0 |  1 |  0 |
     * +--------------+
     */
    Expr blur = I(x, y - 1, c)
              + I(x - 1, y, c)
              + 4 * I(x, y, c)
              + I(x + 1, y, c)
              + I(x, y + 1, c);
    blur /= 8;

    Func output { "Blur" };
    output(x, y, c) = cast(base_type, blur);
    return(output);
}

using namespace Halide::Internal;


struct IRDump : public Halide::Internal::IRVisitor
{
    //vars and method to help create expr_node tree
    std::vector<expr_node *> parents; //keep track of depth/parents
    expr_node * root = new expr_node();
    void add_expr_node(expr_node * temp)
    {
        if(root->name.empty() && root->children.empty())
        {
            root = temp;
            parents.push_back(root);
        }
        else
        {
            parents.back()->children.push_back(temp);
            parents.push_back(temp);
        }
    }
    
    void copy(expr_node * op){
        op->modify = op->original;
    }
    
    std::string indent;
    void add_indent()
    {
        indent.push_back(' ');
        indent.push_back(' ');
    }
    void remove_indent()
    {
        indent.pop_back();
        indent.pop_back();
    }

    int id = 0;

    #define EMIT(format, ...)           \
    {                                   \
        ++id;                           \
        printf("[%5d]%s" format "\n",   \
               id, indent.c_str(),      \
               ##__VA_ARGS__);          \
    }                                   \

    #define INDENTED(format, ...)       \
    {                                   \
        printf("       %s" format,      \
               indent.c_str(),          \
               ##__VA_ARGS__);          \
    }                                   \

    #define SELECT(op)  \
    if (select(op))     \
        return;         \

    

    std::vector<Expr> new_args;
    
    bool selecting = false;
    expr_node * selected;
    
    bool select(expr_node * op)
    {
        if(op == NULL) //Note(Emily): for stmts
        {
            return false;
        }
        if(id == 1503)
        {
            selecting = true;
            selected = op;
            selected->modify = selected->original;
            INDENTED("====== vvvvvv [ SELECTED SUB-EXPRESSION ] vvvvvv ======\n");
            selected->modify.accept(this);
            INDENTED("====== ^^^^^^ [ SELECTED SUB-EXPRESSION ] ^^^^^^ ======\n");
            selecting = false;
            return(true);
        }
        return(false);
    }

    std::string print_type(Type type)
    {
        std::stringstream ss;
        switch (type.code())
        {
            case halide_type_uint :
                ss << "u";
            case halide_type_int :
                ss << "int";
                break;
            case halide_type_float :
                ss << "float";
                break;
            case halide_type_handle :
                ss << "handle";
                break;
        }
        ss << type.bits();
        ss << "[";
        ss << type.lanes();
        ss << "]";
        return(ss.str());
    }

    void visit(const Cast* op)
    {
        expr_node * cast_op = new expr_node();
        cast_op->name = print_type(op->type).c_str();
        cast_op->original = Expr(op);
        add_expr_node(cast_op);
        EMIT("Cast: %s", print_type(op->type).c_str());
        SELECT(cast_op);
        if(selecting)
        {
            copy(cast_op);
        }
        add_indent();
        op->value->accept(this);
        parents.pop_back();
        remove_indent();
    }

    void visit(const Min* op)
    {
        expr_node * min_op = new expr_node();
        min_op->name = "Min";
        min_op->original = Expr(op);
        add_expr_node(min_op);
        EMIT("Min");
        SELECT(min_op);
        if(selecting)
        {
            copy(min_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }

    void visit(const Max* op)
    {
        expr_node * max_op = new expr_node();
        max_op->name = "Max";
        max_op->original = Expr(op);
        add_expr_node(max_op);
        EMIT("Max");
        SELECT(max_op);
        if(selecting)
        {
            copy(max_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const EQ* op)
    {
        expr_node * eq_op = new expr_node();
        eq_op->name = "EQ";
        eq_op->original = Expr(op);
        add_expr_node(eq_op);
        EMIT("EQ");
        SELECT(eq_op);
        if(selecting)
        {
            copy(eq_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const NE* op)
    {
        expr_node * ne_op = new expr_node();
        ne_op->name = "NE";
        ne_op->original = Expr(op);
        add_expr_node(ne_op);
        EMIT("NE");
        SELECT(ne_op);
        if(selecting)
        {
            copy(ne_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const LT* op)
    {
        expr_node * lt_op = new expr_node();
        lt_op->name = "LT";
        lt_op->original = Expr(op);
        add_expr_node(lt_op);
        EMIT("LT");
        SELECT(lt_op);
        if(selecting)
        {
            copy(lt_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const LE* op)
    {
        expr_node * le_op = new expr_node();
        le_op->name = "LE";
        le_op->original = Expr(op);
        add_expr_node(le_op);
        EMIT("LE");
        SELECT(le_op);
        if(selecting)
        {
            copy(le_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const GT* op)
    {
        expr_node * gt_op = new expr_node();
        gt_op->name = "GT";
        gt_op->original = Expr(op);
        add_expr_node(gt_op);
        EMIT("GT");
        SELECT(gt_op);
        if(selecting)
        {
            copy(gt_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const GE* op)
    {
        expr_node * ge_op = new expr_node();
        ge_op->name = "GE";
        ge_op->original = Expr(op);
        add_expr_node(ge_op);
        EMIT("GE");
        SELECT(ge_op);
        if(selecting)
        {
            copy(ge_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }

    void visit(const And* op)
    {
        expr_node * and_op = new expr_node();
        and_op->name = "And";
        and_op->original = Expr(op);
        add_expr_node(and_op);
        EMIT("And");
        SELECT(and_op);
        if(selecting)
        {
            copy(and_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Or* op)
    {
        expr_node * or_op = new expr_node();
        or_op->name = "Or";
        or_op->original = Expr(op);
        add_expr_node(or_op);
        EMIT("Or");
        SELECT(or_op);
        if(selecting)
        {
            copy(or_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Not* op)
    {
        expr_node * not_op = new expr_node();
        not_op->name = "Not";
        not_op->original = Expr(op);
        add_expr_node(not_op);
        EMIT("Not");
        SELECT(not_op);
        if(selecting)
        {
            copy(not_op);
        }
        add_indent();
        op->a->accept(this);
        parents.pop_back();
        remove_indent();
    }

    std::string print_var_type(const Variable* op)
    {
        std::string category;

        if (op->param.defined())
        {
            assert(category.empty());
            category = "parameter";
        }

        if (op->image.defined())
        {
            assert(category.empty());
            category = "image";
        }

        if (op->reduction_domain.defined())
        {
            assert(category.empty());
            category = "rdom";
        }

        if (category.empty())
        {
            category = "let";
        }

        return(category);
    }

    void visit(const Variable* op)
    {
        expr_node * var_op = new expr_node();
        var_op->name = "Variable: " + op->name;
        var_op->original = Expr(op);
        EMIT("Variable: %s | %s", op->name.c_str(), print_var_type(op).c_str());
        SELECT(NULL);
        if(selecting)
        {
            copy(var_op);
        }
        // terminal node!
    }
    
    void visit(const FloatImm* op)
    {
        expr_node * floatimm_op = new expr_node();
        floatimm_op->name = "FloatImm: " + std::to_string(op->value);
        floatimm_op->original = Expr(op);
        add_expr_node(floatimm_op);
        parents.pop_back();
        EMIT("FloatImm: %f", op->value);
        SELECT(floatimm_op);
        if(selecting)
        {
            copy(floatimm_op);
        }
        // terminal node!
    }
    
    void visit(const IntImm* op)
    {
        expr_node * intimm_op = new expr_node();
        intimm_op->name = "IntImm: " + std::to_string(op->value);
        intimm_op->original = Expr(op);
        add_expr_node(intimm_op);
        EMIT("IntImm: %lli", op->value);
        SELECT(intimm_op);
        if(selecting)
        {
            copy(intimm_op);
        }
        parents.pop_back();
        // terminal node!
    }
    
    void visit(const UIntImm* op)
    {
        expr_node * uintimm_op = new expr_node();
        uintimm_op->name = "UIntImm: " + std::to_string(op->value);
        uintimm_op->original = Expr(op);
        add_expr_node(uintimm_op);
        parents.pop_back();
        EMIT("UIntImm: %llu", op->value);
        SELECT(uintimm_op);
        if(selecting)
        {
            copy(uintimm_op);
        }
        // terminal node!
    }

    void visit(const StringImm* op)
    {
        expr_node * stringimm_op = new expr_node();
        stringimm_op->name = "StringImm: " + op->value;
        stringimm_op->original = Expr(op);
        add_expr_node(stringimm_op);
        parents.pop_back();
        EMIT("StringImm: %s", op->value.c_str());
        SELECT(stringimm_op);
        if(selecting)
        {
            copy(stringimm_op);
        }
        // terminal node!
    }
    
    void visit(const Add* op)
    {
        expr_node * add_op = new expr_node();
        add_op->name = "Add";
        add_op->original = Expr(op);
        add_expr_node(add_op);
        EMIT("Add");
        SELECT(add_op);
        if(selecting)
        {
            copy(add_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Sub* op)
    {
        expr_node * sub_op = new expr_node();
        sub_op->name = "Sub";
        sub_op->original = Expr(op);
        add_expr_node(sub_op);
        EMIT("Sub");
        SELECT(sub_op);
        if(selecting)
        {
            copy(sub_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }

    void visit(const Mul* op)
    {
        expr_node * mul_op = new expr_node();
        mul_op->name = "Mul";
        mul_op->original = Expr(op);
        add_expr_node(mul_op);
        EMIT("Mul");
        SELECT(mul_op);
        if(selecting)
        {
            copy(mul_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Div* op)
    {
        expr_node * div_op = new expr_node();
        div_op->name = "Div";
        div_op->original = Expr(op);
        add_expr_node(div_op);
        EMIT("Div");
        SELECT(div_op);
        if(selecting)
        {
            copy(div_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Mod* op)
    {
        expr_node * mod_op = new expr_node();
        mod_op->name = "Mod";
        mod_op->original = Expr(op);
        add_expr_node(mod_op);
        EMIT("Mod");
        SELECT(mod_op);
        if(selecting)
        {
            copy(mod_op);
        }
        add_indent();
        op->a->accept(this);
        op->b->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Select* op)
    {
        expr_node * select_op = new expr_node();
        select_op->name = "Select";
        select_op->original = Expr(op);
        add_expr_node(select_op);
        EMIT("Select");
        SELECT(select_op);
        if(selecting)
        {
            copy(select_op);
        }
        add_indent();
        op->condition->accept(this);
        op->true_value->accept(this);
        op->false_value->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Load* op)
    {
        expr_node * load_op = new expr_node();
        load_op->name = "Load: " + op->name;
        load_op->original = Expr(op);
        add_expr_node(load_op);
        EMIT("Load: %s", op->name.c_str());
        SELECT(load_op);
        if(selecting)
        {
            copy(load_op);
        }
        add_indent();
        op->index->accept(this);
        op->predicate->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Ramp* op)
    {
        expr_node * ramp_op = new expr_node();
        ramp_op->name = "Ramp";
        ramp_op->original = Expr(op);
        add_expr_node(ramp_op);
        EMIT("Ramp");
        SELECT(ramp_op);
        if(selecting)
        {
            copy(ramp_op);
        }
        add_indent();
        op->base->accept(this);
        op->stride->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Broadcast* op)
    {
        expr_node * broadcast_op = new expr_node();
        broadcast_op->name = "Broadcast";
        broadcast_op->original = Expr(op);
        add_expr_node(broadcast_op);
        EMIT("Broadcast");
        SELECT(broadcast_op);
        if(selecting)
        {
            copy(broadcast_op);
        }
        add_indent();
        op->value->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Let* op)
    {
        expr_node * let_op = new expr_node();
        let_op->name = "Let: " + op->name;
        let_op->original = Expr(op);
        add_expr_node(let_op);
        EMIT("Let: %s", op->name.c_str());
        SELECT(let_op);
        if(selecting)
        {
            copy(let_op);
        }
        //TODO(Emily): differentiate between definition/dependent expression in expr_node?
        add_indent();
        INDENTED("<definition>\n");
        add_indent();
        op->value->accept(this);  // the definition
        remove_indent();

        INDENTED("<dependent expression>\n");
        add_indent();
        op->body->accept(this);   // the expression that depends on the let definition
        remove_indent();
        
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const LetStmt* op)
    {
        expr_node * letstmt_op = new expr_node();
        letstmt_op->name = "LetStmt: " + op->name;
        //letstmt_op->original = Expr(op);
        add_expr_node(letstmt_op);
        EMIT("LetStmt: %s", op->name.c_str());
        SELECT(NULL);

        add_indent();
        //TODO(Emily): differentiate between definition/dependent expression in expr_node?
        INDENTED("<definition>\n");
        add_indent();
        op->value->accept(this);
        remove_indent();

        INDENTED("<dependent expression>\n");
        add_indent();
        op->body->accept(this);
        remove_indent();
        
        parents.pop_back();
        remove_indent();
    }

    void visit(const AssertStmt* op)
    {
        expr_node * assertstmt_op = new expr_node();
        assertstmt_op->name = "AssertStmt";
        //assertstmt_op->original = Expr(op);
        add_expr_node(assertstmt_op);
        EMIT("AssertStmt");
        SELECT(NULL);
        add_indent();
        // TODO(marcos): transform the assert condition into something that can
        // be visualized (as a binary true/false) mask
        op->condition->accept(this);
        op->message->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const ProducerConsumer* op)
    {
        expr_node * pc_op = new expr_node();
        pc_op->name = "ProduceConsumer: " + op->name;
        //pc_op->original = Expr(op);
        add_expr_node(pc_op);
        EMIT("ProduceConsumer: %s", op->name.c_str());
        SELECT(NULL);
        add_indent();
        op->body->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const For* op){
        expr_node * for_op = new expr_node();
        for_op->name = "For: " + op->name;
        //for_op->original = Expr(op);
        add_expr_node(for_op);
        EMIT("For: %s", op->name.c_str());
        SELECT(NULL);
        add_indent();
        op->min->accept(this);
        op->extent->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Store* op){
        expr_node * store_op = new expr_node();
        store_op->name = "Store: " + op->name;
        //store_op->original = Expr(op);
        add_expr_node(store_op);
        EMIT("Store: %s", op->name.c_str());
        SELECT(NULL);
        add_indent();
        op->predicate->accept(this);
        op->value->accept(this);
        op->index->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Provide* op){
        expr_node * provide_op = new expr_node();
        provide_op->name = "Provide: " + op->name;
        //provide_op->original = Expr(op);
        add_expr_node(provide_op);
        EMIT("Provide: %s", op->name.c_str());
        SELECT(NULL);
        parents.pop_back();
        //has vector of values and args - need to loop through and visit these
    }
    
    void visit(const Allocate* op){
        expr_node * allocate_op = new expr_node();
        allocate_op->name = "Allocate: " + op->name;
        //allocate_op->original = Expr(op);
        add_expr_node(allocate_op);
        EMIT("Allocate: %s", op->name.c_str());
        SELECT(NULL);
        add_indent();
        //vector of extents
        op->condition->accept(this);
        for(auto& extent : op->extents){
            extent->accept(this);
        }
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Free* op){
        expr_node * free_op = new expr_node();
        free_op->name = "Free: " + op->name;
        //free_op->original = Expr(op);
        add_expr_node(free_op);
        parents.pop_back();
        EMIT("Free: %s", op->name.c_str());
        SELECT(NULL);
        //terminal
    }
    
    void visit(const Realize* op){
        expr_node * realize_op = new expr_node();
        realize_op->name = "Realize: " + op->name;
        //realize_op->original = Expr(op);
        add_expr_node(realize_op);
        EMIT("Realize: %s", op->name.c_str());
        SELECT(NULL);
        add_indent();
        //vector of types + other elements (memory_type, Region bounds, body (stmt))
        for(auto& type : op->types){
            //what do we want to do with the types?
        }
        op->condition->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Block* op){
        expr_node * block_op = new expr_node();
        block_op->name = "Block";
        //block_op->original = Expr(op);
        add_expr_node(block_op);
        EMIT("Block");
        SELECT(NULL);
        add_indent();
        //these are stmts
        op->first->accept(this);
        op->rest->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const IfThenElse* op){
        expr_node * ite_op = new expr_node();
        ite_op->name = "IfThenElse";
        //ite_op->original = Expr(op);
        add_expr_node(ite_op);
        EMIT("IfThenElse");
        SELECT(NULL);
        add_indent();
        //cases are stmts and else case can be empty
        op->condition->accept(this);
        op->then_case->accept(this);
        op->else_case->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Evaluate* op){
        expr_node * eval_op = new expr_node();
        eval_op->name = "Evaluate";
        //eval_op->original = Expr(op);
        add_expr_node(eval_op);
        EMIT("Evaluate");
        SELECT(NULL);
        add_indent();
        //returns stmt
        op->value->accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Shuffle* op){
        expr_node * shuffle_op = new expr_node();
        shuffle_op->name = "Shuffle";
        shuffle_op->original = Expr(op);
        add_expr_node(shuffle_op);
        EMIT("Shuffle");
        SELECT(shuffle_op);
        if(selecting)
        {
            copy(shuffle_op);
        }
        add_indent();
        //vector of indices (ints) and vectors (exprs)
        for(auto& vecs : op->vectors){
            vecs->accept(this);
        }
        parents.pop_back();
        remove_indent();
    }
    
    void visit(const Prefetch* op){
        expr_node * prefetch_op = new expr_node();
        prefetch_op->name = "Prefetch: " + op->name;
        //prefetch_op->original = Expr(op);
        add_expr_node(prefetch_op);
        EMIT("Prefetch: %s", op->name.c_str());
        SELECT(NULL);
        add_indent();
        //vector of types + Region element (bounds)
        parents.pop_back();
        remove_indent();
    }

    std::string print_call_type(Call::CallType call_type)
    {
        std::string category;

        #define CALLTYPE_CASE(TYPE) \
        case Call::CallType::TYPE : \
            category = #TYPE;       \
            break;                  \

        switch (call_type)
        {
            CALLTYPE_CASE(Halide)
            CALLTYPE_CASE(Image)
            CALLTYPE_CASE(Intrinsic)
            CALLTYPE_CASE(Extern)
            CALLTYPE_CASE(ExternCPlusPlus)
            CALLTYPE_CASE(PureIntrinsic)
            CALLTYPE_CASE(PureExtern)
            default :
                category = "UNKNOWN";
                break;
        }

        #undef CALLTYPE_CASE

        return(category);
    }
    
    
    void visit(const Call* op)
    {
        expr_node * call_op = new expr_node();
        call_op->name = "Call: " + op->name + " | " + print_call_type(op->call_type);
        call_op->original = Expr(op);
        add_expr_node(call_op);
        EMIT("Call: %s | %s", op->name.c_str(), print_call_type(op->call_type).c_str());
        SELECT(call_op);
        if(new_args.empty())
        {
            new_args = op->args;
        }
        

        if (id == 11)
        {
            int a = 0;
        }

        add_indent();
        INDENTED("<arguments>\n");
        
        for (auto& arg : op->args)
        {
            add_indent();
            arg.accept(this);
            remove_indent();
        }
        remove_indent();

        const IRNode* rhs = nullptr;
        switch (op->call_type)
        {
            case Call::CallType::Halide :
            {
                assert(op->func.defined());     // paranoid check...
                auto inner_func = Function(op->func);
                if(selecting){
                    call_op->modify = Call::make(inner_func, new_args);
                }
                add_indent();
                INDENTED("<definition>\n");
                add_indent();
                this->visit(inner_func);
                remove_indent();
                parents.pop_back();
                remove_indent();
                break;
            }
            case Call::CallType::Intrinsic :
            case Call::CallType::Extern :
            case Call::CallType::ExternCPlusPlus :
            case Call::CallType::PureIntrinsic :
            case Call::CallType::Image :
            case Call::CallType::PureExtern :
            default :
                parents.pop_back();
                break;
        }
    }

    // NOTE(marcos): not really a part of IRVisitor:
    void visit(Function f)
    {
        expr_node * function_op = new expr_node();
        function_op->name = "Function: " + f.name();
        function_op->original = Expr(f.definition().values()[0]);
        add_expr_node(function_op);
        EMIT("Function: %s", f.name().c_str());
        if(new_args.empty())
        {
            new_args = f.definition().args();
        }
        if(selecting){
            function_op->modify = Call::make(f, new_args);
        }
        auto def = f.definition();
        //def.accept(this);
        add_indent();
        INDENTED("<arguments>\n");
        
        for (auto& arg : def.args())
        {
            add_indent();
            arg.accept(this);
            remove_indent();
        }
        INDENTED("<definition>\n");
        Expr rhs = def.values()[0];
        add_indent();
        rhs->accept(this);
        remove_indent();
        parents.pop_back();
        remove_indent();
    }

    void visit(Func f)
    {
        expr_node * func_op = new expr_node();
        func_op->name = "Func: " + f.name();
        func_op->original = Expr(f.function().definition().values()[0]);
        add_expr_node(func_op);
        EMIT("Func: %s", f.name().c_str());
        add_indent();
        INDENTED("<arguments>\n");
        
        for (Expr arg : f.args())
        {
            arg.accept(this);
        }
        
        remove_indent();

        add_indent();
        INDENTED("<definition>\n");
        f.function().definition().values()[0].accept(this);
        parents.pop_back();
        remove_indent();
    }
    
    void visit(Expr e)
    {
        e.accept(this);
    }

    #undef EMIT
};

struct IRNodePrinter
{
    static std::string print(Type type)
    {
        std::stringstream ss;
        switch (type.code())
        {
        case halide_type_uint:
            ss << "u";
        case halide_type_int:
            ss << "int";
            break;
        case halide_type_float:
            ss << "float";
            break;
        case halide_type_handle:
            ss << "handle";
            break;
        }
        ss << type.bits();
        ss << "[";
        ss << type.lanes();
        ss << "]";
        return ss.str();
    }

    static std::string print(const Cast* op)
    {
        std::stringstream ss;
        ss << "Cast : " << print(op->type);
        return ss.str();
    }

    static std::string print(const Min* op)
    {
        std::stringstream ss;
        ss << "Min";
        return ss.str();
    }

    static std::string print(const Max* op)
    {
        std::stringstream ss;
        ss << "Max";
        return ss.str();
    }
    
    static std::string print(const EQ* op)
    {
        std::stringstream ss;
        ss << "EQ (equals)";
        return ss.str();
    }
    
    static std::string print(const NE* op)
    {
        std::stringstream ss;
        ss << "NE (not-equal)";
        return ss.str();
    }
    
    static std::string print(const LT* op)
    {
        std::stringstream ss;
        ss << "LT (less than)";
        return ss.str();
    }
    
    static std::string print(const LE* op)
    {
        std::stringstream ss;
        ss << "LE (less-or-equal than)";
        return ss.str();
    }
    
    static std::string print(const GT* op)
    {
        std::stringstream ss;
        ss << "GT (greater than)";
        return ss.str();
    }
    
    static std::string print(const GE* op)
    {
        std::stringstream ss;
        ss << "GE (greater-or-equal than)";
        return ss.str();
    }

    static std::string print(const And* op)
    {
        std::stringstream ss;
        ss << "And (logical boolean)";
        return ss.str();
    }
    
    static std::string print(const Or* op)
    {
        std::stringstream ss;
        ss << "Or (logical boolean)";
        return ss.str();
    }
    
    static std::string print(const Not* op)
    {
        std::stringstream ss;
        ss << "Not (logical boolean)";
        return ss.str();
    }

    static std::string print_var_type(const Variable* op)
    {
        std::string category;

        if (op->param.defined())
        {
            assert(category.empty());
            category = "parameter";
        }

        if (op->image.defined())
        {
            assert(category.empty());
            category = "image";
        }

        if (op->reduction_domain.defined())
        {
            assert(category.empty());
            category = "rdom";
        }

        if (category.empty())
        {
            category = "Var-or-Let ?";
        }

        return(category);
    }

    static std::string print(const Variable* op)
    {
        std::stringstream ss;
        ss << "Variable : " << op->name << " (" << print_var_type(op) << ")";
        return ss.str();
    }
    
    static std::string print(const FloatImm* op)
    {
        std::stringstream ss;
        ss << "FloatImm : " << op->value;
        return ss.str();
    }
    
    static std::string print(const IntImm* op)
    {
        std::stringstream ss;
        ss << "IntImm : " << op->value;
        return ss.str();
    }
    
    static std::string print(const UIntImm* op)
    {
        std::stringstream ss;
        ss << "UIntImm : " << op->value;
        return ss.str();
    }

    static std::string print(const StringImm* op)
    {
        std::stringstream ss;
        ss << "StringImm : " << op->value;
        return ss.str();
    }
    
    static std::string print(const Add* op)
    {
        std::stringstream ss;
        ss << "Add";
        return ss.str();
    }
    
    static std::string print(const Sub* op)
    {
        std::stringstream ss;
        ss << "Sub";
        return ss.str();
    }

    static std::string print(const Mul* op)
    {
        std::stringstream ss;
        ss << "Mul";
        return ss.str();
    }
    
    static std::string print(const Div* op)
    {
        std::stringstream ss;
        ss << "Div";
        return ss.str();
    }
    
    static std::string print(const Mod* op)
    {
        std::stringstream ss;
        ss << "Mod";
        return ss.str();
    }
    
    static std::string print(const Select* op)
    {
        std::stringstream ss;
        ss << "Select";
        return ss.str();
    }
    
    static std::string print(const Load* op)
    {
        std::stringstream ss;
        ss << "Load : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Ramp* op)
    {
        std::stringstream ss;
        ss << "Ramp";
        return ss.str();
    }
    
    static std::string print(const Broadcast* op)
    {
        std::stringstream ss;
        ss << "Broadcast";
        return ss.str();
    }
    
    static std::string print(const Let* op)
    {
        std::stringstream ss;
        ss << "Let : " << op->name;
        return ss.str();
    }
    
    static std::string print(const LetStmt* op)
    {
        std::stringstream ss;
        ss << "LetStmt : " << op->name;
        return ss.str();
    }

    static std::string print(const AssertStmt* op)
    {
        std::stringstream ss;
        ss << "AssertStmt";
        return ss.str();
    }
    
    static std::string print(const ProducerConsumer* op)
    {
        std::stringstream ss;
        ss << "LetStmt : " << op->name;
        return ss.str();
    }
    
    static std::string print(const For* op)
    {
        std::stringstream ss;
        ss << "For : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Store* op)
    {
        std::stringstream ss;
        ss << "Store : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Provide* op)
    {
        std::stringstream ss;
        ss << "Provide : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Allocate* op)
    {
        std::stringstream ss;
        ss << "Allocate : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Free* op)
    {
        std::stringstream ss;
        ss << "Free : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Realize* op)
    {
        std::stringstream ss;
        ss << "Realize : " << op->name;
        return ss.str();
    }
    
    static std::string print(const Block* op)
    {
        std::stringstream ss;
        ss << "Block";
        return ss.str();
    }
    
    static std::string print(const IfThenElse* op)
    {
        std::stringstream ss;
        ss << "IfThenElse";
        return ss.str();
    }
    
    static std::string print(const Evaluate* op)
    {
        std::stringstream ss;
        ss << "Evaluate";
        return ss.str();
    }
    
    static std::string print(const Shuffle* op)
    {
        std::stringstream ss;
        ss << "Shuffle";
        return ss.str();
    }
    
    static std::string print(const Prefetch* op)
    {
        std::stringstream ss;
        ss << "Prefetch : " << op->name;
        return ss.str();
    }

    static std::string print(Call::CallType call_type)
    {
        std::string category;

        #define CALLTYPE_CASE(TYPE) \
        case Call::CallType::TYPE : \
            category = #TYPE;       \
            break;                  \

        switch (call_type)
        {
            CALLTYPE_CASE(Halide)
            CALLTYPE_CASE(Image)
            CALLTYPE_CASE(Intrinsic)
            CALLTYPE_CASE(Extern)
            CALLTYPE_CASE(ExternCPlusPlus)
            CALLTYPE_CASE(PureIntrinsic)
            CALLTYPE_CASE(PureExtern)
            default :
                category = "UNKNOWN";
                break;
        }

        #undef CALLTYPE_CASE

        return(category);
    }
    
    
    static std::string print(const Call* op)
    {
        std::stringstream ss;
        ss << "Call : " << print(op->call_type);
        return ss.str();
    }

    // NOTE(marcos): not really a part of IRVisitor:
    static std::string print(Function f)
    {
        std::stringstream ss;
        ss << "Function : " << f.name();
        return ss.str();
    }

    static std::string print(Func f)
    {
        std::stringstream ss;
        ss << "Func : " << f.name();
        return ss.str();
    }
    
    //static std::string print(Expr e)
    //{
    //    e.accept(this);
    //}
};

struct IRDump2 : public Halide::Internal::IRVisitor
{
    const int indent_length = 3;
    std::string indent;

    void add_indent()
    {
        for (int i = 0; i < indent_length; ++i)
        {
            indent.push_back(' ');
        }
    }
    void remove_indent()
    {
        for (int i = 0; i < indent_length; ++i)
        {
            indent.pop_back();
        }
    }

    int id = 0;

    int assign_id()
    {
        return ++id;
    }

    #define indented_printf(format, ...) \
        printf("        %s " format, indent.c_str(), ##__VA_ARGS__);

    template<typename T>
    void dump_head(T op)
    {
        assign_id();
        printf("[%5d] %s %s\n", id, indent.c_str(),
                                IRNodePrinter::print(op).c_str());
    }

    template<typename T>
    void dump_guts(const T* op)
    {
        add_indent();
            IRVisitor::visit(op);
        remove_indent();
    }

    template<typename T>
    void dump(const T* op)
    {
        dump_head(op);
        dump_guts(op);
    }

    void visit(const Cast* op)
    {
        dump(op);
    }

    void visit(const Min* op)
    {
        dump(op);
    }

    void visit(const Max* op)
    {
        dump(op);
    }
    
    void visit(const EQ* op)
    {
        dump(op);
    }
    
    void visit(const NE* op)
    {
        dump(op);
    }
    
    void visit(const LT* op)
    {
        dump(op);
    }
    
    void visit(const LE* op)
    {
        dump(op);
    }
    
    void visit(const GT* op)
    {
        dump(op);
    }
    
    void visit(const GE* op)
    {
        dump(op);
    }

    void visit(const And* op)
    {
        dump(op);
    }
    
    void visit(const Or* op)
    {
        dump(op);
    }
    
    void visit(const Not* op)
    {
        dump(op);
    }

    void visit(const Variable* op)
    {
        dump(op);
    }
    
    void visit(const FloatImm* op)
    {
        dump(op);
    }
    
    void visit(const IntImm* op)
    {
        dump(op);
    }
    
    void visit(const UIntImm* op)
    {
        dump(op);
    }

    void visit(const StringImm* op)
    {
        dump(op);
    }
    
    void visit(const Add* op)
    {
        dump(op);
    }
    
    void visit(const Sub* op)
    {
        dump(op);
    }

    void visit(const Mul* op)
    {
        dump(op);
    }
    
    void visit(const Div* op)
    {
        dump(op);
    }
    
    void visit(const Mod* op)
    {
        dump(op);
    }
    
    void visit(const Select* op)
    {
        dump(op);
    }
    
    void visit(const Load* op)
    {
        dump(op);
    }
    
    void visit(const Ramp* op)
    {
        dump(op);
    }
    
    void visit(const Broadcast* op)
    {
        dump(op);
    }
    
    void visit(const Let* op)
    {
        dump(op);
    }
    
    void visit(const LetStmt* op)
    {
        dump(op);
    }

    void visit(const AssertStmt* op)
    {
        dump(op);
    }
    
    void visit(const ProducerConsumer* op)
    {
        dump(op);
    }
    
    void visit(const For* op)
    {
        dump(op);
    }
    
    void visit(const Store* op)
    {
        dump(op);
    }
    
    void visit(const Provide* op)
    {
        dump(op);
    }
    
    void visit(const Allocate* op)
    {
        dump(op);
    }
    
    void visit(const Free* op)
    {
        dump(op);
    }
    
    void visit(const Realize* op)
    {
        dump(op);
    }
    
    void visit(const Block* op)
    {
        dump(op);
    }
    
    void visit(const IfThenElse* op)
    {
        dump(op);
    }
    
    void visit(const Evaluate* op)
    {
        dump(op);
    }
    
    void visit(const Shuffle* op)
    {
        dump(op);
    }
    
    void visit(const Prefetch* op)
    {
        dump(op);
    }

    void dump_guts(const Call* op)
    {
        add_indent();
            indented_printf("<arguments>\n");
            for (auto& arg : op->args)
            {
                add_indent();
                    arg.accept(this);
                remove_indent();
            }
        remove_indent();
        add_indent();
            indented_printf("<callable>\n");
            add_indent();
                switch (op->call_type)
                {
                    case Call::CallType::Halide :
                    {
                        assert(op->func.defined());     // paranoid check...
                        auto inner_func = Function(op->func);
                        visit(inner_func);
                        break;
                    }
                    case Call::CallType::Intrinsic :
                    case Call::CallType::Extern :
                    case Call::CallType::ExternCPlusPlus :
                    case Call::CallType::PureIntrinsic :
                    case Call::CallType::Image :
                    case Call::CallType::PureExtern :
                        indented_printf("<terminal definition: %s>\n", IRNodePrinter::print(op->call_type).c_str());
                        break;
                    default :
                        indented_printf("<UNKNOWN>\n");
                        break;
                }
            remove_indent();
        remove_indent();
    }

    void visit(const Call* op)
    {
        dump(op);
    }

    // The following are convenience functions (not really a part of IRVisitor)
    void visit(Function f)
    {
        dump_head(f);
        add_indent();
            //f.accept(this);
            // NOTE(marcos): the default Function visitor is bit wacky, visitng
            // [predicate, body, args, schedule, specializaitons] in this order
            // which is confusing since we are mostly interested in [args, body]
            // for the time being, so we can roll our own visiting pattern:
            auto definition = f.definition();
            indented_printf("<arguments>\n");
            add_indent();
                for (auto& arg : definition.args())
                {
                    arg.accept(this);
                }
            remove_indent();
            int value_idx = 0;
            for (auto& expr : definition.values())
            {
                indented_printf("<value %d>\n", value_idx++);
                add_indent();
                    expr.accept(this);
                remove_indent();
            }
        remove_indent();
    }

    void visit(Func f)
    {
        dump_head(f);
        add_indent();
            visit(f.function());
        remove_indent();
    }
    
    void visit(Expr e)
    {
        e.accept(this);
    }

    #undef indented_printf
};


Func transform(Func f)
{
    auto domain = f.args();
    Func g { "t_" + f.name() };

    IRDump dump;
    dump.visit(f);
    //f.function().accept(&dump);
    
    Expr selected = dump.selected->modify;
    if (!selected.defined())
        selected = dump.selected->original;
    if(!selected.defined())
        selected = f.value();

    g(domain) = cast(type__of(f), selected);
    return(g);
}


void display_map(Func f)
{
    IRVarMap test;
    test.visit(f);
    std::map<std::string, Halide::Expr> map = test.var_map;
    for(auto name : map)
    {
        printf("%s\n", name.first.c_str());
        IRDump dump;
        dump.visit(name.second);
    }
}

//NOTE(Emily): helper function to print out expr_node tree
void display_node(expr_node * parent)
{
    printf("%s\n",parent->name.c_str());
    for(int i = 0; i < parent->children.size(); i++)
    {
        display_node(parent->children[i]);
    }
}

expr_node * get_tree(Func f)
{
    //testing that expr_node tree was correctly built
    IRDump dump;
    dump.visit(f);
    expr_node * tree = dump.root;
    //printf("displaying nodes in tree: \n");
    //display_node(tree);
    return tree;
    
}
#define PROFILE(...)            \
[&]()                           \
{                               \
    typedef std::chrono::high_resolution_clock clock_t; \
    auto ini = clock_t::now();  \
    __VA_ARGS__;                \
    auto end = clock_t::now();  \
    auto eps = std::chrono::duration<double>(end - ini).count();    \
    return(eps);                \
}()

#define PROFILE_P(label, ...) printf(#label "> %fs\n", PROFILE(__VA_ARGS__))

// utility function to deep copy a Func (but it does not recursive!)
Func clone(Func f)
{
    // f.function().deep_copy() only duplicates the first level of definitions
    // in f, so we need to recursively visit CallType::Halide sub-expressions
    // and duplicate them as well:
    struct FuncCloner : public Halide::Internal::IRMutator2
    {
        static Func duplicate(Func f)
        {
            Func g;
            std::map<FunctionPtr, FunctionPtr> env;
            f.function().deep_copy("deep_copy_of_" + f.name(), g.function().get_contents(), env);
            return g;
        }
        virtual Expr visit(const Call* op) override
        {
            Expr expr = IRMutator2::visit(op);
            op = expr.as<Call>();
            if (op->func.defined())
            {
                // FunctionPtr -> Function -> Func
                Func f = Func(Function(op->func));
                Func g = duplicate(f);
                g.function().mutate(this);
                expr = Call::make(g.function(), op->args, op->value_index);
            }
            return expr;
        }
    };

    Func g = FuncCloner::duplicate(f);
    FuncCloner cloner;
    g.function().mutate(&cloner);
    return g;
}

// utility function to mutate a Func (not part of IRMutator2 interface)
template<typename Mutator>
Func mutate(Func f)
{
    Mutator mutator;

    //Func g ("HalideVisDbg");
    //auto domain = f.args();
    //g(domain) = f(domain);  // identity. forwarding

    Func g = clone(f);
    g.function().mutate(&mutator);
    return g;
}

struct DebuggerSelector : public Halide::Internal::IRMutator2
{
    int traversal_id = 0;
    int target_id = 42;
    Expr selected;

    // convenience method (not really a part of IRMutator2)
    template<typename T>
    Expr mutate_and_select(const T* op)
    {
        const int id = ++traversal_id;      // generate unique id
        Expr expr = IRMutator2::visit(op);  // visit/mutate children
        if (id == target_id)
        {
            assert(!selected.defined());
            selected = expr;
        }
        // propagate selection upwards in the traversal
        if (selected.defined())
        {
            expr = selected;
        }
        return expr;
    }

    // convenience method (not really a part of IRMutator2)
    template<typename PatchFn>
    Expr apply_patch(Expr expr, PatchFn patch)
    {
        // paranoid checks...
        assert( expr.defined() );
        assert( selected.defined() );
        assert( selected.same_as(expr) );
        // apply patch:
        Expr patched_expr = patch(expr);
        selected = patched_expr;
        return patched_expr;
    }

    virtual Expr visit(const Add* op) override
    {
        return mutate_and_select(op);
        //Expr keep = mutate_and_select(op);
        //op = keep.as<Add>();
        //Expr replaced = Div::make(op->a, op->b);
        //return replaced;
    }

    virtual Expr visit(const Mul* op) override
    {
        return mutate_and_select(op);
        //Expr keep = mutate_and_select(op);
        //op = keep.as<Mul>();
        //Expr replaced = Div::make(op->b, op->a);
        //return replaced;
    }
    
    virtual Expr visit(const Div* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Mod* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Sub* op) override
    {
        return mutate_and_select(op);
    }

    virtual Expr visit(const Min* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Max* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const EQ* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const NE* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const LT* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const LE* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const GT* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const GE* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const And* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Or* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Not* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const UIntImm* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const IntImm* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const FloatImm* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const StringImm* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Variable* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Cast* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Select* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Load* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Ramp* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Broadcast* op) override
    {
        return mutate_and_select(op);
    }
    
    virtual Expr visit(const Shuffle* op) override
    {
        return mutate_and_select(op);
    }
    
    //NOTE(Emily): Disabling Stmt nodes for now - it doesn't make sense to visualize them, so we are not assigning IDs to them
    /*
    template<typename T>
    Stmt mutate_and_select_stmt(const T*& op)
    {
        const int id = ++traversal_id;      // generate unique id
        Stmt stmt = IRMutator2::visit(op);  // visit/mutate children
        return stmt;
    }
    
    virtual Stmt visit(const LetStmt* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const AssertStmt* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const ProducerConsumer* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const For* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Store* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Provide* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Allocate* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Free* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Prefetch* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Block* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const IfThenElse* op) override
    {
        return mutate_and_select_stmt(op);
    }
    
    virtual Stmt visit(const Evaluate* op) override
    {
        return mutate_and_select_stmt(op);
    }
    */
    
    virtual Expr visit(const Let* op) override
    {
        Expr expr = mutate_and_select(op);
        if (!selected.defined())
        {
            return expr;
        }

        // patching...
        Expr patched_expr = apply_patch(expr, [&](Expr original_expr)
        {
            Expr new_let_body = original_expr;
            Expr new_let_expr = Let::make(op->name, op->value, new_let_body);
            return new_let_expr;
        });
        return patched_expr;
    }

    virtual Expr visit(const Call* op) override
    {
        // 'mutate_and_select()' won't recurse into the definition of a Func
        // (when 'op->call_type == CallType::Halide' or 'op->func.defined()')
        Expr expr = mutate_and_select(op);

        // I guess at this point the only way of something getting selected is
        // if we allow for Func argument/parameters to get selected, or if the
        // selected expression happens to be this Call.
        // TODO(marcos): how should we patch the Call if we select an argument?
        assert(!selected.defined());

        if (!op->func.defined())
        {
            return expr;
        }

        Function(op->func).mutate(this);

        if (!selected.defined())
        {
            return expr;
        }

        // patching...
        assert(op->func.defined());     // we're in a CallType::Halide node
        assert(selected.defined());     // and something has been selected
        Expr patched_expr = apply_patch(selected, [&](Expr original_expr)
        {
            Func g;
            std::vector<Var> domain;
            for (auto& var_name : Function(op->func).args())
            {
                domain.emplace_back(var_name);
            }
            g(domain) = selected;
            Expr new_call_expr = Call::make(g.function(), op->args, op->value_index);
            return new_call_expr;
        });
        return patched_expr;
    }
};

#include <unordered_map>

struct ExampleMutator : public Halide::Internal::IRMutator2
{
    using IRMutator2::visit;
    using IRMutator2::mutate;

    std::vector<const Let*>  lets;
    std::vector<const Call*> calls;

    virtual Expr visit(const Add* op) override
    {
        // replace addition by subtraction:
        Expr expr = mutate_inner(op);
        Expr replaced = Sub::make(op->a, op->b);
        return replaced;
    }

    virtual Expr visit(const Mul* op) override
    {
        // replace multiplication by subtraction:
        Expr expr = mutate_inner(op);
        Expr replaced = Sub::make(op->a, op->b);
        return replaced;
    }

    virtual Expr visit(const Variable* var) override
    {
        //pr replaced = Variable::make();
        return IRMutator2::visit(var);
    }

    virtual Expr visit(const Let* op) override
    {
        lets.push_back(op);
        Expr expr = IRMutator2::visit(op);
        lets.pop_back();
        return expr;
    }

    virtual Stmt visit(const LetStmt* op) override
    {
        return IRMutator2::visit(op);
    }

    virtual Expr visit(const Call* op) override
    {
        calls.push_back(op);

        Expr expr = mutate_inner(op);
        if (op->func.defined())
        {
            Function(op->func).mutate(this);
            expr = Call::make(Function(op->func), op->args, op->value_index);
        }

        calls.pop_back();

        return expr;
    }

    // convenience method (not really a part of IRMutator2)
    template<typename T>
    Expr mutate_inner(const T*& op)
    {
        // use the default implementation of IRMutator2 to visit (and mutate)
        // the op's children/inner expression/nodes:
        Expr expr = IRMutator2::visit(op);
        op = expr.as<T>();
        return expr;
    }
};



expr_node * tree_from_func()
{
    xsprintf(input_filename, 128, "data/pencils.jpg");
    Halide::Buffer<uint8_t> input_full = LoadImage(input_filename);
    if (!input_full.defined())
        return  NULL;

    Func input = Identity(input_full, "image");
    Func sobel = Sobel(input);
    Func blur  = Blur(input);
    Func soft  = Sobel(blur);
    Expr final = as_weights(soft)(x, y, c)
               + as_weights(sobel)(x, y, c);
    final *= blur(x, y, c);
    final += input(x, y, c);
    final = clamp(final, 0, 255);

    Func output { "output" };
    //output(x, y, c) = cast(type__of(input), final);
    
    {
    Func f{ "f" };
    {
    Var x, y, c;
    f(x,y, c) = 10 * input(x, 0, c + 1);
    }

    {
    Var x, y, c;
    output(x, y, c) = f(x + 3, y,c);
    }
    }

    Func h = mutate<ExampleMutator>(output);
    Func m = mutate<DebuggerSelector>(output);
    IRDump().visit(h);
    IRDump().visit(m);
    IRDump2().visit(output);

    //checking expr_node tree
    //expr_node * full_tree = get_tree(output);
    //output = transform(output);
    //display_map(output);
    
    return NULL; //TEMPORARY - returning before realizing image
    
    Halide::Buffer<uint8_t> output_buffer = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved(input_full.width(), input_full.height(), input_full.channels());
    output.output_buffer()
            .dim(0).set_stride( output_buffer.dim(0).stride() )
            .dim(1).set_stride( output_buffer.dim(1).stride() )
            .dim(2).set_stride( output_buffer.dim(2).stride() );
    auto output_cropped = Crop(output_buffer, 2, 2);
    Target target = get_host_target();
    //target.set_feature(Target::CUDA);
    //output.gpu_tile(...)

    typedef std::chrono::high_resolution_clock clock_t;

    PROFILE_P(
        "compile_jit",
        output.compile_jit(target);
    );
    //output.print_loop_nest();
    output.compile_to_lowered_stmt("data/output/output.html", {}, HTML, target);

    PROFILE_P(
        "realize",
        output.realize(output_cropped);
    );
    

    mkdir("data/output", S_IRWXU | S_IRWXG | S_IRWXO);
    xsprintf(output_filename, 128, "data/output/output-%s.png", target.to_string().c_str());
    if (!SaveImage(output_filename, output_buffer))
        return NULL;

    //return full_tree;
}
