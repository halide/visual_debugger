#include "utils.h"
#include <map>

using namespace Halide::Internal;

struct IRVarMap : public Halide::Internal::IRVisitor
{
    std::map<std::string, Expr> var_map;
    std::vector<Expr> temp;
    bool adding = false;
    bool simplifying = false;
    
    Expr simplify(Expr e)
    {
        simplifying = true;
        e.accept(this);
        simplifying = false;
        return e;
    }
    
    void update(Expr * op, Expr def)
    {
        //need to change: op = def
    }
    
    void visit(const Variable* op)
    {
        std::string n = op->name;
        if(adding && !simplifying)
        {
            auto curr_expr = temp.front();
            auto simple_expr = simplify(curr_expr);
            var_map[n] = simple_expr;
            temp.erase(temp.begin());
        }
        if(simplifying)
        {
            if(var_map.count(n) == 0)
            {
                simplifying = false; //not found in map so stop simplifying and add it
            }
            else //need to simplify
            {
                Expr def = var_map[n];
                if(def.same_as(op)){ //might be a base definition
                    printf("found base definition\n");
                    simplifying = false;
                }
                else{
                    update((Expr *)op, def);
                    def->accept(this);
                }
            }
        }
        // terminal node!
    }
    
    void visit(const Call* op)
    {
        //definition of function args (move this into the switch statement?)
        for (auto& arg : op->args)
        {
            temp.push_back(arg);
        }
        
        switch (op->call_type)
        {
            case Call::CallType::Halide :
            {
                assert(op->func.defined());     // paranoid check...
                auto inner_func = Function(op->func);
                this->visit(inner_func);
                break;
            }
            case Call::CallType::Intrinsic :
            case Call::CallType::Extern :
            case Call::CallType::ExternCPlusPlus :
            case Call::CallType::PureIntrinsic :
            case Call::CallType::Image :
            case Call::CallType::PureExtern :
            default :
                break;
        }
    }
    
    void visit(Function f)
    {
        auto def = f.definition();
        adding = true;
        for (auto& arg : def.args())
        {
            arg.accept(this);
        }
        adding = false;
        temp.clear();
        Expr rhs = def.values()[0];
        rhs->accept(this);
    }
    
    void visit(Func f)
    {
        adding = true;
        for (Expr arg : f.args())
        {
            temp.push_back(arg);
            arg.accept(this);
        }
        adding = false;
        temp.clear();
        
        
        f.function().definition().values()[0].accept(this);
    }
    
    
    
};
