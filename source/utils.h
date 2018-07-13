#include <cstdlib>
#include <string>
#include <vector>

struct expr_node {
    std::string name;
    std::vector<expr_node *> children;
};
