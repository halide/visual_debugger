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

#define GL_RGBA32F 0x8814

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
        ss << "Call : " << op->name << " | " << print(op->call_type);
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



struct DebuggerSelector : public Halide::Internal::IRMutator2
{
    int traversal_id = 0;
    const int target_id = 1801;
    Expr selected;

    DebuggerSelector(int _target_id = 0)
    : target_id(_target_id) { }

    // -----------------------
    const int indent_length = 2;
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
    // -----------------------
    int assign_id()
    {
        return ++traversal_id;
    }
    // -----------------------
    #define indented_printf(format, ...) \
        printf("        %s " format, indent.c_str(), ##__VA_ARGS__);
    // -----------------------
    template<typename T>
    void dump_head(T op, int id=-1)
    {
        if (id == target_id)
        {
            indented_printf("vvvvvvvvvv SELECTED vvvvvvvvvv\n");
        }

        (id > 0) ? printf("[%5d]", id)
                 : printf("       ");
        printf(" %s %s\n", indent.c_str(),
                           IRNodePrinter::print(op).c_str());

        if (id == target_id)
        {
            indented_printf("^^^^^^^^^^ SELECTED ^^^^^^^^^^\n");
        }
    }

    template<typename T>
    Expr dump_guts(const T* op)
    {
        add_indent();
            Expr expr = IRMutator2::visit(op);
        remove_indent();
        return expr;
    }

    template<typename T>
    void dump(const T* op)
    {
        dump_head(op);
        dump_guts(op);
    }
    // -----------------------
    //vars and method to help create expr_node tree
    expr_tree tree;

    // -----------------------


    // convenience method (not really a part of IRMutator2)
    template<typename T>
    Expr mutate_and_select(const T* op)
    {
        const int id = assign_id();
        
        //NOTE(Emily): This won't display modified tree because of
        // building the tree recursively (need to add node before visiting children)
        expr_node* node_op = tree.new_expr_node();
        node_op->name = IRNodePrinter::print(op);
        node_op->original = op;
        node_op->node_id = id;
        tree.enter(node_op);
        
        dump_head(op, id);
        Expr expr = dump_guts(op);
    
        node_op->modify = expr;

        //const int id = assign_id();         // generate unique id
        //Expr expr = IRMutator2::visit(op);  // visit/mutate children
        if (id == target_id)
        {
            assert(id > 0);
            assert(!selected.defined());
            selected = expr;
        }
        // propagate selection upwards in the traversal
        //if (selected.defined())
        //{
        //    expr = selected;
        //}
        
        //NOTE(Emily): we need to pop back the parents after visiting children
        tree.leave(node_op);
        return expr;
    }
    
    void add_spacer_node(std::string label)
    {
        expr_node* node_op = tree.new_expr_node();
        node_op->name = label;
        tree.enter(node_op);
        tree.leave(node_op);
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
    template<typename T>
    Stmt mutate_and_select_stmt(const T* op)
    {
        //const int id = ++traversal_id;    // generate unique id
        dump_head(op);
        add_indent();
            Stmt stmt = IRMutator2::visit(op);  // visit/mutate children
        remove_indent();
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
    
    virtual Expr visit(const Let* op) override
    {
        if (selected.defined())
        {
            return mutate_and_select(op);
        }

        Expr expr = mutate_and_select(op);

        // has this very own Let node expression been selected?
        if (selected.same_as(Expr(op)))
        {
            return expr;
        }
        if (!selected.defined())
        {
            return expr;
        }

        // patching...
        Expr patched_expr = apply_patch(selected, [&](Expr original_expr)
        {
            Expr new_let_body = original_expr;
            Expr new_let_expr = Let::make(op->name, op->value, new_let_body);
            return new_let_expr;
        });
        return patched_expr;
    }

    bool arg_selected = false;

    Expr dump_guts(const Call* op)
    {
        bool had_selection = selected.defined();
        
        //NOTE(Emily): adding separation node here
        add_spacer_node("<arguments>");

        add_indent();
            indented_printf("<arguments>\n");
            for (auto& arg : op->args)
            {
                add_indent();
                    IRMutator2::mutate(arg);
                remove_indent();
            }
        remove_indent();

        bool has_selection = selected.defined();
        
        //NOTE(Emily): adding separation node here
        add_spacer_node("<callable>");

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

        arg_selected = (!had_selection && has_selection);
        if (arg_selected)
        {
            int a = 0;
        }

        return Expr(op);
    }

    virtual Expr visit(const Call* op) override
    {
        // it's possible that something up the expression tree has been selected
        // already, but we still want to visit the entire tree for visualization
        // purposes
        //assert(!selected.defined());
        if (selected.defined())
        {
            return mutate_and_select(op);
        }

        Expr expr = mutate_and_select(op);

        // if selected expression is an argument, skip call!
        if (arg_selected)
        {
            // keep 'selected' as is, unaltered.
            arg_selected = false;
            return expr;
        }

        // has this very own Call node expression been selected?
        if (selected.same_as(Expr(op)))
        {
            return expr;
        }
        if (!selected.defined())
        {
            return expr;
        }

        // terminal Call node, nothing to patch
        if (!op->func.defined())
        {
            return expr;
        }

        // patching...
        assert(op->func.defined());     // paranoid checks...
        assert(op->call_type == Call::CallType::Halide);
        Expr patched_expr = apply_patch(selected, [&](Expr original_expr)
        {
            auto& domain = Function(op->func).args();
            std::vector<Expr> args;
            for (auto& var_name : domain)
            {
                args.emplace_back( Var(var_name) );
            }
            ReductionDomain rdom;
            Definition def (args, { selected }, rdom, true);
            Function g (op->name);
            g.define(domain, { cast(selected.type(), 0) });
            g.definition() = def;
            Expr new_call_expr = Call::make(g, op->args, op->value_index);

            /*
            Function g (op->name);
            auto& domain = Function(op->func).args();
            g.define(domain, { selected });
            Expr new_call_expr = Call::make(g, op->args, op->value_index);
            */

            /*
            Func g;
            add_indent();
            indented_printf("g -- %s\n", g.name().c_str());
            std::vector<Var> domain;
            for (auto& var_name : Function(op->func).args())
            {
                indented_printf("p -- %s\n", var_name.c_str());
                domain.emplace_back(var_name);
            }
            remove_indent();
            g(domain) = selected;
            Expr new_call_expr = Call::make(g.function(), op->args, op->value_index);
            */

            return new_call_expr;
        });

        return expr;
    }

    void visit(const Expr expr)
    {
        IRMutator2::mutate(expr);
    }

    // The following are convenience functions (not really a part of IRVisitor)
    void visit(Function f)
    {
        dump_head(f);
        
        // NOTE(emily): need to add the root Func as root of expr_node tree
        expr_node* node_op = tree.new_expr_node();
        node_op->name = IRNodePrinter::print(f);
        tree.enter(node_op);

        auto definition = f.definition();
        if (!definition.defined())
        {
            add_indent();
                indented_printf("<UNDEFINED>\n");
            remove_indent();
            return;
        }
        
        //NOTE(Emily): adding separation node here
        add_spacer_node("<arguments>");

        add_indent();
            //f.accept(this);
            // NOTE(marcos): the default Function visitor is bit wacky, visitng
            // [predicate, body, args, schedule, specializaitons] in this order
            // which is confusing since we are mostly interested in [args, body]
            // for the time being, so we can roll our own visiting pattern:
            indented_printf("<arguments>\n");
            add_indent();
                for (auto& arg : definition.args())
                {
                    IRMutator2::mutate(arg);
                }
            remove_indent();
        
            //NOTE(Emily): adding separation node here
            add_spacer_node("<value>");
        
            int value_idx = 0;
            for (auto& expr : definition.values())
            {
                indented_printf("<value %d>\n", value_idx++);
                add_indent();
                    IRMutator2::mutate(expr);
                remove_indent();
            }
        remove_indent();

        tree.leave(node_op);
    }

    void visit(Func f)
    {
        indented_printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
        dump_head(f);
        
        // NOTE(emily): need to add the root Func as root of expr_node tree
        expr_node* node_op = tree.new_expr_node();
        node_op->name = IRNodePrinter::print(f);
        Expr expr = f(f.args());
        node_op->original = expr;
        tree.enter(node_op);
        
        add_indent();
            visit(f.function());
        remove_indent();
        
        tree.leave(node_op);
        indented_printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    }

    Func mutate(Func f)
    {
        visit(f);
        Func g;
        if(!selected.defined()){
            return f;
        }
        DebuggerSelector().visit(selected);
        auto domain = f.args();
        g(domain) = selected;
        return g;
    }

    #undef indented_printf
};

struct IRDump3 : public DebuggerSelector { };

Func transform(Func f)
{
    Func g = DebuggerSelector(
        //26      //Let
        //27      //Call, immediatelly inside a Let
        //823     //Let
        //1556    //Let
        //1653    //Call, immediatelly inside a Let
        //1801    //Let
    ).mutate(f);

    auto domain = f.args();
    Expr transformed_expr = 0;
    if (g.defined())
    {
        domain = g.args();
        transformed_expr = g(domain);
    }

    Func h;
    h(domain) = cast(type__of(f), transformed_expr);
    return h;
}

void display_map(Func f)
{
    IRVarMap test;
    test.visit(f);
    std::map<std::string, Halide::Expr> map = test.var_map;
    for(auto name : map)
    {
        printf("%s\n", name.first.c_str());
        IRDump3 dump;
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

expr_tree get_tree(Func f)
{
    //testing that expr_node tree was correctly built
    IRDump3 dump;
    dump.mutate(f);
    //printf("displaying nodes in tree: \n");
    //display_node(tree);
    return std::move(dump.tree);
    
}

void select_and_visualize(Func f, int id, Halide::Buffer<uint8_t> input_full, GLuint idMyTexture)
{
    //id = 24;
    Func m = DebuggerSelector(id).mutate(f);
    //printf("SELECTION RESULT BELOW:");
    //IRDump3().visit(m);
    //printf("SELECTION RESULT ABOVE:");
    
    auto domain = f.args();
    Expr transformed_expr = 0;
    if (m.defined())
    {
        domain = m.args();
        transformed_expr = m(domain);
    }
    
    
    
    Halide::Buffer<uint8_t> output_buffer = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved(input_full.width(), input_full.height(), input_full.channels());
    auto output_cropped = Crop(output_buffer, 2, 2);
    Target target = get_host_target();
    //target.set_feature(Target::CUDA);
    //output.gpu_tile(...)
    
    typedef std::chrono::high_resolution_clock clock_t;
    
    Type t = transformed_expr.type();
    bool is_float = t.is_float();
    
    auto dims = output_buffer.dimensions();
    // only 2D or 3D images for now (no 1D LUTs or nD entities)
    assert(dims >= 2);
    assert(dims <= 3);

    int channels = 0;
    if(dims == 2)
    {
        channels = 1;
    }
    else if(dims == 3)
    {
        channels = output_buffer.dim(2).extent();
    }
    // only monochrome or rgb for now
    assert(channels == 1 || channels == 3);

    bool is_monochrome = (channels == 1);
    
    GLenum internal_format(GL_INVALID_ENUM), external_format(GL_INVALID_ENUM), external_type(GL_INVALID_ENUM);
    external_format = (is_monochrome) ? GL_RED
                                      : GL_RGB;
    
    auto bits = t.bits(); //size of type
    switch(bits)
    {
        case 8:
            external_type   = GL_UNSIGNED_BYTE;
            internal_format = GL_RGBA8;
            break;
        case 16:
            external_type   = GL_UNSIGNED_SHORT;
            internal_format = GL_RGBA16;                                    // maybe force GL_RGBA8 fow now?
            break;
        case 32:
            external_type   = (is_float) ? GL_FLOAT   : GL_UNSIGNED_INT;
            internal_format = (is_float) ? GL_RGBA32F : GL_RGBA16;          // maybe force GL_RGBA8 for now?
            break;
        default:
            assert(false);
            break;
            
    }
    
    int width  = output_buffer.dim(0).extent();
    int height = output_buffer.dim(1).extent();
    
    Halide::Runtime::Buffer<> modified_output_buffer;

    switch(t.code())
    {
        case halide_type_int:
        {
            assert(!is_float);
            switch(bits)
            {
                case 8:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<int8_t, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<int8_t, 3>::make_interleaved(width, height, channels);
                    break;
                }
                case 16:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<int16_t, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<int16_t, 3>::make_interleaved(width, height, channels);
                    break;
                }
                case 32:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<int32_t, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<int32_t, 3>::make_interleaved(width, height, channels);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            break;
        }
        case halide_type_uint:
        {
            assert(!is_float);
            switch(bits)
            {
                case 8:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<uint8_t, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved(width, height, channels);
                    break;
                }
                case 16:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<uint16_t, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<uint16_t, 3>::make_interleaved(width, height, channels);
                    break;
                }
                case 32:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<uint32_t, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<uint32_t, 3>::make_interleaved(width, height, channels);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            break;
        }
        case halide_type_float:
        {
            assert(is_float);
            switch(bits)
            {
                case 32:
                {
                    if (is_monochrome)
                        modified_output_buffer = Halide::Runtime::Buffer<float, 2>::make_interleaved(width, height, 1);
                    else
                        modified_output_buffer = Halide::Runtime::Buffer<float, 3>::make_interleaved(width, height, channels);
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            break;
        }
        case halide_type_handle:
        default:
            break;
    }

    if(is_monochrome)
    {
        m.output_buffer()
            .dim(0).set_stride( modified_output_buffer.dim(0).stride() )
            .dim(1).set_stride( modified_output_buffer.dim(1).stride() );
    }
    else
    {
        m.output_buffer()
            .dim(0).set_stride( modified_output_buffer.dim(0).stride() )
            .dim(1).set_stride( modified_output_buffer.dim(1).stride() )
            .dim(2).set_stride( modified_output_buffer.dim(2).stride() );
    }

    PROFILE_P(
                "compile_jit",
                m.compile_jit(target);
                );
                    
    PROFILE_P(
                "realize",
                m.realize(modified_output_buffer);
                );

    glBindTexture(GL_TEXTURE_2D, idMyTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, external_format, external_type, modified_output_buffer.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    
    /*
    Func h;
    h(domain) = cast(type__of(f), transformed_expr);

    h.output_buffer()
        .dim(0).set_stride( output_buffer.dim(0).stride() )
        .dim(1).set_stride( output_buffer.dim(1).stride() )
        .dim(2).set_stride( output_buffer.dim(2).stride() );

    PROFILE_P(
              "compile_jit",
              h.compile_jit(target);
              );
    
    PROFILE_P(
              "realize",
              h.realize(output_cropped);
              );
    
    mkdir("data/output", S_IRWXU | S_IRWXG | S_IRWXO);
    xsprintf(output_filename, 128, "data/output/output-%s-%d.png", target.to_string().c_str(), id);
    if (!SaveImage(output_filename, output_buffer))
        printf("Error saving image\n");
    */
}

expr_tree tree_from_func(Func output, Halide::Buffer<uint8_t> input_full)
{
    /*xsprintf(input_filename, 128, "data/pencils.jpg");
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
    */
    
    //IRDump3().visit(output);

    Func out = transform(output);
    //IRDump3().visit(out);

    //IRDump3().visit(output);

    //checking expr_node tree
    expr_tree full_tree = get_tree(output);
    //output = transform(output);
    //display_map(output);
    
    //return NULL; //TEMPORARY - returning before realizing image
    
    Halide::Buffer<uint8_t> output_buffer = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved(input_full.width(), input_full.height(), input_full.channels());
    out.output_buffer()
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
    
    /* NEED TO CAST FUNC TO UINT8_T
    auto domain = m.args();
    Expr body = m.function().extern_definition_proxy_expr();
    Func out;
    out(domain) = cast(type__of(output), body);
    */
    
    
    
    PROFILE_P(
        "realize",
        out.realize(output_cropped);
    );
    

    mkdir("data/output", S_IRWXU | S_IRWXG | S_IRWXO);
    xsprintf(output_filename, 128, "data/output/output-%s.png", target.to_string().c_str());
    if (!SaveImage(output_filename, output_buffer))
        return expr_tree{};

    return std::move(full_tree);
}
