#ifndef Closure_h
#define Closure_h

#include "CommonTypes.h"

class ClassDefinition;
class Tree;
class AnonymousFunctionExpression;
class Context;

using VariableDeclarationList = std::vector<VariableDeclaration*>;

namespace Closure {
    struct Info {
        Type* closureInterfaceType {nullptr};
        Identifier className;
        VariableDeclarationList nonLocalVars;
    };

    ClassDefinition* generateInterface(Tree& tree, const Type* closureType);
    void generateClass(
        Tree& tree,
        AnonymousFunctionExpression* function,
        const Context& context,
        Info& info);
}

#endif
