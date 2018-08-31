#ifndef HALIDE_VISDBG_UTILS_H
#define HALIDE_VISDBG_UTILS_H

#include <cstdlib>
#include <string>
#include <vector>
#include <deque>

#include <Halide.h>

struct expr_node {
    std::string name;
    std::vector<expr_node *> children;
    int node_id = 0;
    // NOTE(marcos): the following fields don't seem to be used in practice
    //Halide::Expr original;
    //Halide::Expr modified;
};

struct expr_tree
{
    expr_node* root = nullptr;
    std::deque<expr_node> nodes;
    std::vector<expr_node *> parents; //keep track of depth/parents

    expr_node* new_expr_node()
    {
        nodes.emplace_back();
        auto& node = nodes.back();
        return &node;
    }

    void enter(expr_node* node)
    {
        if(!parents.empty())
        {
            parents.back()->children.push_back(node);
        }
        else
        {
            assert(root == nullptr);
            root = node;
        }
        assert(node->children.empty());
        parents.push_back(node);
    }

    void leave(expr_node* node)
    {
        assert(!parents.empty());
        assert(parents.back() == node);
        parents.pop_back();
    }
};

// NOTE(marcos): implementation in 'treedump.cpp'
expr_tree get_tree(Halide::Func f);



struct Profiling
{
    double jit_time;
    double upl_time;
    double run_time;
};

#endif//HALIDE_VISDBG_UTILS_H
