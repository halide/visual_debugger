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

#include "utils.h"
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



Expr eval(Func f, int tuple_idx=0, int updef_idx=-1)
{
    if (!f.defined())
    {
        return Expr();
    }

    if (updef_idx == -1)
    {
        // by default, choose the very last update definition (if any)
        updef_idx = f.num_update_definitions();
    }

    Tuple values { Expr() };
    if (updef_idx == 0)
    {
        // pure definition
        values = f.values();
    }
    else
    {
        --updef_idx;
        assert(updef_idx >= 0);
        assert(updef_idx < f.num_update_definitions());
        values = f.update_values(updef_idx);
    }

    assert(tuple_idx < values.size());
    Expr value = values[tuple_idx];
    return value;
}

struct def
{
    Func _f;
    Func g;
    def(Func f) : _f(f), g("def-" + f.name()) { }
    operator Func() { return g; }
    Func operator = (Expr value)
    {
        dom(g, _f) = value;
        return g;
    }
    static FuncRef dom(Func g, Func f)
    {
        assert(!g.defined());
        assert( f.defined());
        auto domain = f.args();
        return g(domain);
    }
};

Func wrap(Func f)
{
    if (!f.defined())
    {
        return Func();
    }
    
    Func g = def(f) = eval(f);
    return g;
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
        ss << "Call : " << op->name << " | " << print(op->call_type) << " | " << "[" << op->value_index << "]";
        return ss.str();
    }

    // NOTE(marcos): not really a part of IRVisitor:
    static std::string print(Function f)
    {
        std::stringstream ss;
        ss << "Function : " << f.name() << " | ";
        if (f.is_pure())
        {
            ss << "pure";
        }
        else
        {
            ss << f.updates().size() << " updates";
        }
        ss << " | ";
        ss << f.outputs();  // num_outputs; Tuple cardinality
        ss << " values";
        return ss.str();
    }

    static std::string print(Func f)
    {
        std::stringstream ss;
        ss << "Func : " << f.name();
        return ss.str();
    }

    // NOTE(marcos): we'll need an auxiliary class that inherits from IRVisitor
    // to implement this...
    //static std::string print(Expr e)
    //{
    //    IRNodePrinterVisitor vis;
    //    e.accept(vis);
    //    return vis.str;
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
        node_op->node_id = id;
        //node_op->original = op;
        tree.enter(node_op);
        
        dump_head(op, id);
        Expr expr = dump_guts(op);
        //node_op->modified = expr;

        if (id == target_id)
        {
            assert(id > 0);
            assert(!selected.defined());
            selected = expr;
        }
        
        //NOTE(Emily): we need to pop back the parents after visiting children
        tree.leave(node_op);
        return expr;
    }

    #define default_expr_visitor(IRNodeType) \
        virtual Expr visit(const IRNodeType* op) override { return mutate_and_select(op); }

    default_expr_visitor(Add)
    default_expr_visitor(Mul)
    default_expr_visitor(Div)
    default_expr_visitor(Mod)
    default_expr_visitor(Sub)
    default_expr_visitor(Min)
    default_expr_visitor(Max)
    default_expr_visitor(EQ)
    default_expr_visitor(NE)
    default_expr_visitor(LT)
    default_expr_visitor(LE)
    default_expr_visitor(GT)
    default_expr_visitor(GE)
    default_expr_visitor(And)
    default_expr_visitor(Or)
    default_expr_visitor(Not)
    default_expr_visitor(UIntImm)
    default_expr_visitor(IntImm)
    default_expr_visitor(FloatImm)
    default_expr_visitor(StringImm)
    default_expr_visitor(Variable)
    default_expr_visitor(Cast)
    default_expr_visitor(Select)
    default_expr_visitor(Load)
    default_expr_visitor(Ramp)
    default_expr_visitor(Broadcast)
    default_expr_visitor(Shuffle)

    #undef default_expr_visitor

    //NOTE(Emily): Disabling Stmt nodes for now - it doesn't make sense to visualize them, so we are not assigning IDs to them
    template<typename T>
    Stmt mutate_and_select_stmt(const T* op)
    {
        dump_head(op);
        add_indent();
            Stmt stmt = IRMutator2::visit(op);  // visit/mutate children
        remove_indent();
        return stmt;
    }

    #define default_stmt_visitor(IRNodeType) \
        virtual Stmt visit(const IRNodeType* op) override { return mutate_and_select_stmt(op); }

    default_stmt_visitor(LetStmt)
    default_stmt_visitor(AssertStmt)
    default_stmt_visitor(ProducerConsumer)
    default_stmt_visitor(For)
    default_stmt_visitor(Store)
    default_stmt_visitor(Provide)
    default_stmt_visitor(Allocate)
    default_stmt_visitor(Free)
    default_stmt_visitor(Prefetch)
    default_stmt_visitor(Block)
    default_stmt_visitor(IfThenElse)
    default_stmt_visitor(Evaluate)

    #undef default_stmt_visitor

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

    expr_node* add_spacer_node(std::string label)
    {
        expr_node* node_op = tree.new_expr_node();
        node_op->name = label;
        tree.enter(node_op);
        return node_op;
    }
    
    void leave_spacer_node(expr_node* spacer)
    {
        tree.leave(spacer);
    }

    bool arg_selected = false;

    Expr dump_guts(const Call* op)
    {
        bool had_selection = selected.defined();
        
        //NOTE(Emily): adding separation node here
        expr_node* arg_spacer = add_spacer_node("<arguments>");

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
        
        leave_spacer_node(arg_spacer);

        add_indent();
            indented_printf("<callable>\n");
            add_indent();
                switch (op->call_type)
                {
                    case Call::CallType::Halide :
                    {
                        //NOTE(Emily): adding separation node here
                        expr_node* spacer = add_spacer_node("<callable>");
                        assert(op->func.defined());     // paranoid check...
                        auto inner_func = Function(op->func);
                        //visit(inner_func);
                        // TODO(marcos): OK, I know the 'value_index', but how
                        // am I supposed to know if it is referring to the pure
                        // definition or to some update definition?!
                        visit(inner_func, op->value_index);
                        leave_spacer_node(spacer);
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

        return Expr(op);
    }

    virtual Expr visit(const Call* op) override
    {
        // it's possible that something up the expression tree has been selected
        // already, but we still want to visit the entire tree for visualization
        // purposes
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
            // NOTE(marcos): the 'g.definition() = def' below is a major hack,
            // but it's there because all other options failed...
            ReductionDomain rdom;
            Definition def (args, { selected }, rdom, true);
            Function g (op->name);
            g.define(domain, { cast(selected.type(), 0) });
            g.definition() = def;
            Expr new_call_expr = Call::make(g, op->args, op->value_index);

            return new_call_expr;
        });

        return expr;
    }

    // The following are convenience functions (not really a part of IRVisitor)
    void visit(Function f, int value_idx=-1)
    {
        dump_head(f);
        
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

        add_indent();
            // NOTE(marcos): the default Function visitor is bit wacky, visitng
            // [predicate, body, args, schedule, specializaitons] in this order
            // which is confusing since we are mostly interested in [args, body]
            // for the time being, so we can roll our own visiting pattern:
            indented_printf("<arguments>\n");
            expr_node * arg_spacer = add_spacer_node("<arguments>");
            add_indent();
                for (auto& arg : definition.args())
                {
                    IRMutator2::mutate(arg);
                }
            remove_indent();
            leave_spacer_node(arg_spacer);

            if (value_idx != -1)
            {
                assert(value_idx < f.values().size());
                Expr pure_value = f.values()[value_idx];
                indented_printf("<value %d>\n", value_idx);
                expr_node * spacer = add_spacer_node("<value " + std::to_string(value_idx) + ">");
                add_indent();
                    IRMutator2::mutate(pure_value);
                remove_indent();
                leave_spacer_node(spacer);
                // TODO(marcos): the code below may loop in infinite recursion;
                // still not sure as to which definition (pure or updates) the
                // value index from a Call node refers to...
                /*
                if (f.has_update_definition())
                {
                    expr_node * update_spacer = add_spacer_node("<update definitions>");
                    for (auto& update_def : f.updates())
                    {
                        assert(value_idx < update_def.values().size());
                        Expr update_value = update_def.values()[value_idx];
                        indented_printf("<value %d>\n", value_idx);
                        expr_node * spacer = add_spacer_node("<value>");
                        add_indent();
                            IRMutator2::mutate(update_value);
                        remove_indent();
                    }
                    leave_spacer_node(update_spacer);
                }
                */
            }
            else
            {
                // show everything:
                expr_node * spacer = add_spacer_node("<values>");
                int value_idx = 0;
                for (auto& expr : definition.values())
                {
                    indented_printf("<value %d>\n", value_idx++);
                    add_indent();
                        IRMutator2::mutate(expr);
                    remove_indent();
                }
                leave_spacer_node(spacer);
        
                if(f.has_update_definition())
                {
                    expr_node * update_spacer = add_spacer_node("<update definitions>");
                    int num_updates = (int)f.updates().size();
                    for(int i = 0; i < num_updates; i++)
                    {
                        auto update_def = f.update(i);
                        value_idx = 0;
                        expr_node * spacer  = add_spacer_node("<update " + std::to_string(i) + ">");
                        expr_node * spacer2 = add_spacer_node("<values>");
                        for(auto& expr : update_def.values())
                        {
                            indented_printf("<value %d>\n", value_idx++);
                            add_indent();
                                IRMutator2::mutate(expr);
                            remove_indent();
                        }
                        leave_spacer_node(spacer2);
                        leave_spacer_node(spacer);
                    }
                    leave_spacer_node(update_spacer);
                }
            }
        
        //leave_spacer_node(spacer);
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
        // NOTE(marcos): skipping the original expression in the expr_tree
        // because 'f' might emcapsulate a Tuple of values, and a Tuple is
        // not an Expr in Halide.
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
        if(!selected.defined())
        {
            // note that Func f could be encapsulating a Tuple of values, or even
            // consist of update definitions (which themselves could be Tuples)
            // ideally, if no sub-expression has been selected, it should default
            // to selecting the first tuple value of the last update definition
            selected = eval(f);
        }
        assert(selected.defined());
        Func g = def(f) = selected;
        return g;
    }

    void visit(const Expr expr)
    {
        IRMutator2::mutate(expr);
    }

    #undef indented_printf
};

struct IRDump : public DebuggerSelector { };

Func transform(Func f, int id=0, int value_idx=0)
{
    Func g = DebuggerSelector(id).mutate(f);

    Expr transformed_expr = eval(g, value_idx);
    if (!transformed_expr.defined())
    {
        transformed_expr = 0;
    }

    Func h = def(g) = transformed_expr;
    return h;
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
    IRDump dump;
    dump.mutate(f);
    return std::move(dump.tree);
}

struct FindInputBuffers : public Halide::Internal::IRVisitor
{
    std::vector< Buffer<> > buffers;

    virtual void visit(const Call* op) override
    {
        if (op->call_type == Call::CallType::Image)
        {
            // TODO(marcos): the same buffer is likely to be accessed multiple
            // times by an expression... right now, this visitor will collect
            // the same buffer multiple times -- not really a problem, but it
            // would be nice to track uniqueness with an additional set<name>.
            assert(op->image.defined());
            buffers.emplace_back(op->image);
        }
        IRVisitor::visit(op);
        if (op->call_type == Call::CallType::Halide)
        {
            assert(op->func.defined());
            auto definition = Function(op->func).definition();
            for (auto& expr : definition.values())
            {
                expr.accept(this);
            }
        }
    }

    std::vector< Buffer<> > visit(Expr expr)
    {
        expr.accept(this);
        return std::move(buffers);
    }

    std::vector< Buffer<> > visit(Func f)
    {
        buffers = visit(eval(f));
        return std::move(buffers);
    }
};

Profiling select_and_visualize(Func f, int id, Halide::Type& type, Halide::Buffer<>& output, std::string target_features, int view_transform_value = 0, int min = 0, int max = 0)
{
    Func m = transform(f, id);
    auto input_buffers = FindInputBuffers().visit(m);

    // preserve type prior to any data type view transforms:
    type = eval(m).type();

    // Apply data type view transforms here (type casts, range normalization, etc)
    // https://git.corp.adobe.com/slomp/halide_visualdbg/issues/30
    switch (view_transform_value)
    {
        case 0 : // do nothing (full bit-range normalization)
            break;
        case 1 : // overflow (force-cast to 8bit)
        {
            if (!type.is_float() && type.bits() > 8)
            {
                m = def(m) = cast<uint8_t>(eval(m));
            }
            break;
        }
        case 2 : // range-normalize
        {
            m = def(m) = ( cast<float>(eval(m)) - cast<float>(min) )
                       / ( cast<float>(max)     - cast<float>(min) );
            break;
        }
        case 3 : // range-clamp
        {
            Expr lower = Halide::max(Expr(0),   Expr(min));
            Expr upper = Halide::min(Expr(255), Expr(max));
            Expr constrained = clamp( eval(m), cast(type, lower), cast(type, upper) );
            m = def(m) = cast<uint8_t>(constrained);
            break;
        }
        case 4 : // automatic min-max range detection (based on buffer content)
                 // Halide inline reductions might be useful here:
                 // http://halide-lang.org/docs/namespace_halide.html#a9d7999c3871839488df9d591b3f55adf
            assert(false);
            break;
        default:
            break;
    }
    
    Type t = eval(m).type();
    bool is_float = t.is_float();

    Halide::Buffer<uint8_t> output_buffer = Halide::Runtime::Buffer<uint8_t, 3>::make_interleaved(output.width(), output.height(), output.channels());
    auto output_cropped = Crop(output_buffer, 2, 2);

    auto dims = output_buffer.dimensions();
    // only 2D or 3D images for now (no 1D LUTs or nD entities)
    assert(dims >= 2);
    assert(dims <= 3);

    auto bits = t.bits(); //size of type

    int width    = output_buffer.width();       // same as output.dim(0).extent()
    int height   = output_buffer.height();      // same as output.dim(1).extent()
    int channels = output_buffer.channels();    // same as output.dim(2).extent() when 'dims >= 2', or 1 otherwise

    // only monochrome or rgb for now
    assert(channels == 1 || channels == 3);

    bool is_monochrome = (channels == 1);

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

    Target host_target = get_host_target();
    Target::OS os      = host_target.os;
    Target::Arch arch  = host_target.arch;
    int arch_bits      = host_target.bits;

    Target base_target (os, arch, arch_bits);
    std::string target_string = base_target.to_string();
    target_features.erase(0, target_features.find("-"));    // erase the cpu feature, since base_target already has the correct one
    if (!target_features.empty())
    {
        target_features.erase(0, target_features.find("-")+1);
        target_string.append("-");
        target_string.append(target_features);
    }

    Target target;
    if (Target::validate_target_string(target_string))
    {
        target = Target(target_string);
    }
    else
    {
        fprintf(stderr, "WARNING: Invalid Halide Target : %s\n", target_string.c_str());
        fprintf(stderr, "         will revert to the default host target.\n");
        target = host_target;
    }
    target_string = target.to_string();
    printf("Halide Target : %s\n", target_string.c_str());
    fflush(stdout);

    bool gpu = target.has_gpu_feature();

    if (gpu)
    {
        m = wrap(m);                        // <- add one level of indirection to isolate the schedule below
        Var x = m.args()[0];
        Var y = m.args()[1];
        Var tx, ty;
        m.gpu_tile(x, y, tx, ty, 8, 8, Halide::TailStrategy::GuardWithIf);
        for (auto buffer : input_buffers)
        {
            //buffer.device_free();         // <- not necessary: device_deallocate() will call it anyway if necessary
            buffer.device_deallocate();     // <- to prevent "halide_copy_to_device does not support switching interfaces"
            buffer.set_host_dirty();        // <- mandatory! otherwise data won't be re-uploaded even though a new device interface is evetually created! (Halide bug?)
        }
    }
    else
    {
        // TODO(marcos): we might need some here vectorize() for CPU too...
        //m.vectorize();
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

    Profiling times = { };

    times.jit_time =
        PROFILE(
            m.compile_jit(target);
        );

    times.upl_time =
        PROFILE(
            if (!gpu)
                return 0.0;
            for (auto buffer : input_buffers)
            {
                buffer.copy_to_device(target);
            }
        );

    times.run_time =
        PROFILE(
            m.realize(modified_output_buffer, target);
        );

    modified_output_buffer.copy_to_host();

    output = std::move(modified_output_buffer);

    return times;
}
