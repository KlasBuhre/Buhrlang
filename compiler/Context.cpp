#include "Context.h"
#include "Definition.h"
#include "Statement.h"
#include "Expression.h"
#include "Tree.h"

Context::Context(MethodDefinition* m) :
    methodDefinition(m),
    block(nullptr),
    bindings(nullptr),
    classLocalNameBindings(nullptr),
    lambdaExpression(nullptr),
    temporaryRetvalDeclaration(nullptr),
    arrayType(nullptr),
    staticContext(m->isStatic()),
    stringConstructorCall(false),
    insideLoop(false),
    constructorCallStatement(false) {}

Binding* Context::lookup(const Identifier& name) const {
    if (classLocalNameBindings) {
        return classLocalNameBindings->lookupLocal(name);
    }
    return bindings->lookup(name);
}

Definition* Context::lookupType(const Identifier& name) const {
    return bindings->lookupType(name);
}

Type* Context::lookupConcreteType(Type* type, const Location& location) const {
    Tree::lookupAndSetTypeDefinition(type, *bindings, location);
    auto concreteType =
        Tree::makeGenericTypeConcrete(type, *bindings, location);
    if (concreteType != nullptr) {
        // The type is a generic type parameter that has been assigned a
        // concrete type. Change the type to the concrete type.
        return concreteType;
    }
    return type;
}

ClassDefinition* Context::getClassDefinition() const {
    return methodDefinition->getEnclosingClass();
}

void Context::enterBlock(BlockStatement* b) {
    block = b;
    bindings = &(block->getNameBindings());
}

void Context::exitBlock() {
    bindings->removeObsoleteLocalBindings();
    block = block->getEnclosingBlock();
    bindings = bindings->getEnclosing();
}

void Context::reset() {
    classLocalNameBindings = nullptr;
    staticContext = methodDefinition->isStatic();
}

Context::BindingsGuard::BindingsGuard(Context& c) : context(c) {}

Context::BindingsGuard::BindingsGuard(Context& c, NameBindings* classLocals) :
    BindingsGuard(c) {

    context.setClassLocalNameBindings(classLocals);
}

Context::BindingsGuard::~BindingsGuard() {
    context.reset();
}
