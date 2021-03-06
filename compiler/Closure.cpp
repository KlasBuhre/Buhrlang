#include "Closure.h"
#include "Tree.h"
#include "Type.h"
#include "Definition.h"
#include "Expression.h"
#include "Context.h"

namespace {
    class NonLocalVarVisitor: public Visitor {
    public:
        NonLocalVarVisitor(NameBindings& global, NameBindings& func);

        Traverse::Result visitBlock(BlockStatement&) override;
        void exitBlock() override;
        Traverse::Result visitNamedEntity(NamedEntityExpression&) override;
        Traverse::Result visitMemberSelector(MemberSelectorExpression&) override;

        const VariableDeclarationList& getNonLocalVariables() const {
            return nonLocalVariables;
        }

    private:
        void checkIfNonLocal(const NamedEntityExpression& namedEntity);

        using IdentifierSet = std::set<Identifier>;

        NameBindings& globalScope;
        NameBindings& funcScope;
        NameBindings* currentScope;
        VariableDeclarationList nonLocalVariables;
        IdentifierSet alreadyFound;
    };

    NonLocalVarVisitor::NonLocalVarVisitor(
        NameBindings& global,
        NameBindings& func) :
        Visitor(TraverseStatemets),
        globalScope(global),
        funcScope(func),
        currentScope(&func),
        nonLocalVariables(),
        alreadyFound() {}

    Traverse::Result NonLocalVarVisitor::visitBlock(BlockStatement& block) {
        currentScope = &(block.getNameBindings());
        return Traverse::Continue;
    }

    void NonLocalVarVisitor::exitBlock() {
        currentScope = currentScope->getEnclosing();
    }

    Traverse::Result NonLocalVarVisitor::visitNamedEntity(
        NamedEntityExpression& namedEntity) {

        checkIfNonLocal(namedEntity);
        return Traverse::Continue;
    }

    Traverse::Result NonLocalVarVisitor::visitMemberSelector(
        MemberSelectorExpression& memberSelector) {

        if (auto namedEntity =
                memberSelector.getLeft()->dynCast<NamedEntityExpression>()) {
            checkIfNonLocal(*namedEntity);
        }

        if (memberSelector.getRight()->isNamedEntity()) {
            // Don't visit named entities to the right of the dot (.) operator.
            return Traverse::Skip;
        }
        return Traverse::Continue;
    }

    void NonLocalVarVisitor::checkIfNonLocal(
        const NamedEntityExpression& namedEntity) {

        const Identifier& identifier = namedEntity.getIdentifier();
        if (alreadyFound.find(identifier) != alreadyFound.end()) {
            return;
        }

        auto funcEnclosingScope = funcScope.getEnclosing();
        funcScope.setEnclosing(&globalScope);
        bool mayBeNonLocalVar = (currentScope->lookup(identifier) == nullptr);
        funcScope.setEnclosing(funcEnclosingScope);

        if (!mayBeNonLocalVar) {
            return;
        }

        // Could not resolve the named entity, which could mean that it is a
        // non-local variable. We will check that by trying to resolve it when
        // the function scope is reconnected to its outer scope.
        auto binding = currentScope->lookup(identifier);
        if (binding == nullptr) {
            // This is an unknown identifier.
            return;
        }

        auto nonLocalType = binding->getVariableType();
        if (nonLocalType != nullptr) {
            nonLocalVariables.push_back(
                VariableDeclaration::create(nonLocalType->clone(),
                                            identifier,
                                            namedEntity.getLocation()));
            alreadyFound.insert(identifier);
        }
    }

    class GenericTypeVisitor: public Visitor {
    public:
        explicit GenericTypeVisitor(const Context& context);

        Traverse::Result visitVariableDeclaration(
            VariableDeclarationStatement&) override;
        Traverse::Result visitHeapAllocation(HeapAllocationExpression&) override;
        Traverse::Result visitArrayAllocation(
            ArrayAllocationExpression&) override;
        Traverse::Result visitTypeCast(TypeCastExpression&) override;
        Traverse::Result visitClassDecomposition(
            ClassDecompositionExpression&) override;
        Traverse::Result visitTypedExpression(TypedExpression&) override;

    private:
        const Context& context;
    };

    GenericTypeVisitor::GenericTypeVisitor(const Context& c) :
        Visitor(TraverseStatemets),
        context(c) {}

    Traverse::Result GenericTypeVisitor::visitVariableDeclaration(
        VariableDeclarationStatement& varDeclaration) {

        varDeclaration.lookupType(context);
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitHeapAllocation(
        HeapAllocationExpression& heapAllocation) {

        heapAllocation.lookupType(context);
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitArrayAllocation(
        ArrayAllocationExpression& arrayAllocation) {

        arrayAllocation.lookupType(context);
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitTypeCast(
        TypeCastExpression& typeCast) {

        typeCast.lookupTargetType(context);
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitClassDecomposition(
        ClassDecompositionExpression& classDecomposition) {

        classDecomposition.lookupType(context);
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitTypedExpression(
        TypedExpression& typedExpression) {

        typedExpression.lookupType(context);
        return Traverse::Continue;
    }

    Type* handleReturnType(MethodDefinition* callMethod) {
        auto body = callMethod->getBody();
        const BlockStatement::StatementList& statements = body->getStatements();

        auto nrOfStatements = statements.size();
        if (nrOfStatements == 0) {
            return &Type::voidType();
        }

        auto lastStatement = statements.back();
        if (auto returnStatement = lastStatement->dynCast<ReturnStatement>()) {
            return returnStatement->getExpression()->getType();
        }

        if (nrOfStatements == 1) {
            if (auto expression = lastStatement->dynCast<Expression>()) {
                // If the AnonymousFunctionExpression consists of a single
                // non-void expression then that expression is implicitly
                // returned.
                auto expressionType = expression->getType();
                if (!expressionType->isVoid()) {
                    auto returnStatement =
                        ReturnStatement::create(expression,
                                                expression->getLocation());
                    body->replaceLastStatement(returnStatement);
                    return expressionType;
                }
            }
        }

        return &Type::voidType();
    }

    // Generate the following method signature:
    //
    // [ClosureReturnType] call([ClosureArguments]...)
    //
    MethodDefinition* generateCallMethodSignature(
        Tree& tree,
        const Type* closureType) {

        auto closureSignature = closureType->getFunctionSignature();
        auto methodSignature =
            MethodDefinition::create(CommonNames::callMethodName,
                                     closureSignature->getReturnType(),
                                     tree.getCurrentClass());

        int index = 0;
        for (auto argumentType: closureSignature->getArguments()) {
            methodSignature->addArgument(argumentType->clone(),
                                         Symbol::makeTemp(index));
            index++;
        }

        return methodSignature;
    }

    // Generate the following class:
    //
    // class $Closure$N([NonLocalVars]...) {
    //     implicit call(implicit args...) {
    //         ...
    //     }
    // }
    //
    ClassDefinition* startGeneratingClass(
        Tree& tree,
        AnonymousFunctionExpression* function,
        const VariableDeclarationList& nonLocalVariables,
        const Context& context,
        MethodDefinition** callMethod) {

        ClassDefinition::Properties properties;
        properties.isClosure = true;
        Identifier closureClassName =
            Symbol::makeClosureClassName(context.getClassDefinition()->getName(),
                                         context.getMethodDefinition()->getName(),
                                         function->getLocation());
        tree.startGeneratedClass(closureClassName, properties);

        for (auto nonLocalVar: nonLocalVariables) {
            tree.addClassDataMember(nonLocalVar->getType()->clone(),
                                    nonLocalVar->getIdentifier());
        }
        tree.getCurrentClass()->generateConstructor();

        auto callMethodDef =
            MethodDefinition::create(CommonNames::callMethodName,
                                     Type::create(Type::Implicit),
                                     AccessLevel::Public,
                                     false,
                                     tree.getCurrentClass(),
                                     function->getLocation());
        callMethodDef->setIsClosure(true);
        callMethodDef->setBody(function->getBody());
        callMethodDef->addArguments(function->getArgumentList());
        tree.addClassMember(callMethodDef);
        *callMethod = callMethodDef;

        return tree.finishClass();
    }

    Type* getClosureInterfaceType(Tree& tree, MethodDefinition* callMethod) {
        auto closureType = Type::create(Type::Function);

        auto closureSignature =
            FunctionSignature::create(callMethod->getReturnType()->clone());
        for (auto argument: callMethod->getArgumentList()) {
            closureSignature->addArgument(argument->getType()->clone());
        }

        closureType->setFunctionSignature(closureSignature);

        return tree.convertToClosureInterfaceInCurrentTree(closureType);
    }
}

// Generate the following closure interface:
//
// class [ClosureInterfaceName] {
//     [ClosureReturnType] call([ClosureArguments]...)
// }
//
ClassDefinition* Closure::generateInterface(
    Tree& tree,
    const Type* closureType) {

    ClassDefinition::Properties properties;
    properties.isInterface = true;
    properties.isClosure = true;
    tree.startGeneratedClass(closureType->getClosureInterfaceName(),
                             properties);

    tree.addClassMember(generateCallMethodSignature(tree, closureType));

    return tree.finishClass();
}

// Generate the following closure class:
//
// class $Closure$N([NonLocalVars]...): [ClosureInterface] {
//     [ClosureReturnType] call([ClosureArguments]...) {
//         return ...
//     }
// }
//
void Closure::generateClass(
    Tree& tree,
    AnonymousFunctionExpression* function,
    const Context& context,
    Info& info) {

    auto body = function->getBody();

    GenericTypeVisitor genericTypeVisitor(context);
    body->traverse(genericTypeVisitor);

    // Begin by finding all used variables in the closure that are declared
    // outside the closure. We need to store those variables in the closure
    // class.
    NonLocalVarVisitor nonLocalVarVisitor(tree.getGlobalNameBindings(),
                                          body->getNameBindings());
    body->traverse(nonLocalVarVisitor);
    const VariableDeclarationList& nonLocalVariables =
        nonLocalVarVisitor.getNonLocalVariables();
    info.nonLocalVars = nonLocalVariables;

    // Start generating the closure class.
    MethodDefinition* callMethod = nullptr;
    auto closureClass = startGeneratingClass(tree,
                                             function,
                                             nonLocalVariables,
                                             context,
                                             &callMethod);
    info.className = closureClass->getName();

    // Infer any implicit types in the closure signature by running the
    // typeCheckAndTransform pass on the closure function body.
    callMethod->typeCheckAndTransform();

    // Calculate the return type of the closure by inspecting the last
    // statement.
    auto returnType = handleReturnType(callMethod);
    callMethod->setReturnType(returnType->clone());

    // Get the closure interface type from the call method signature.
    auto closureInterfaceType = getClosureInterfaceType(tree, callMethod);
    info.closureInterfaceType = closureInterfaceType;

    tree.insertClassPostParse(closureClass, false);

    // Finally, add the closure interface as a parent interface.
    closureClass->addParent(
        closureInterfaceType->getDefinition()->cast<ClassDefinition>());
}
