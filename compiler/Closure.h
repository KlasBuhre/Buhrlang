#ifndef Closure_h
#define Closure_h

#include "CommonTypes.h"

class ClassDefinition;
class MethodDefinition;
class Tree;
class AnonymousFunctionExpression;
class Context;

typedef std::vector<VariableDeclaration*> VariableDeclarationList;

struct ClosureInfo {
    ClosureInfo() :
        closureInterfaceType(nullptr),
        className(),
        nonLocalVars() {}

    Type* closureInterfaceType;
    Identifier className;
    VariableDeclarationList nonLocalVars;
};

class Closure {
public:
    explicit Closure(Tree& t);

    ClassDefinition* generateInterface(const Type* closureType);
    void generateClass(
        AnonymousFunctionExpression* function,
        const Context& context,
        ClosureInfo* info);

private:
    MethodDefinition* generateCallMethodSignature(const Type* closureType);
    ClassDefinition* startGeneratingClass(
        AnonymousFunctionExpression* function,
        const VariableDeclarationList& nonLocalVariables,
        const Context& context,
        MethodDefinition** callMethod);
    Type* getClosureInterfaceType(MethodDefinition* callMethod);

    Tree& tree;
};

#endif
