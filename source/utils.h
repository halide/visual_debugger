#include <cstdlib>
#include <string>
#include <vector>

// //// Halide ////////////////////////////////////////////////////////////////
#include <Halide.h>

//////////////////////////////////////////////////////////////// Halide //// //

#include "HalideIdentity.h"
#include "HalideGaussian.h"
#include "HalideKawase.h"

using namespace Halide;

struct expr_node {
    std::string name;
    std::vector<expr_node *> children;
    Expr original;
    Expr modify;
};
