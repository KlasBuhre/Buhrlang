#include <sstream>

#include "Statement.h"
#include "Type.h"
#include "Definition.h"
#include "Context.h"
#include "Expression.h"
#include "Tree.h"
#include "Pattern.h"

namespace {
    int tempCounter = 0;
    const Identifier deferVariableName("$defer");

    BinaryExpression* makeReturnValueAssignment(
        Expression* returnExpression,
        Type* returnType,
        const Identifier& retvalTmpIdentifier) {

        Location returnExpressionLocation(returnExpression->getLocation());
        auto retvalTmp =
            LocalVariableExpression::create(returnType,
                                            retvalTmpIdentifier,
                                            returnExpressionLocation);
        return BinaryExpression::create(Operator::Assignment,
                                        retvalTmp,
                                        returnExpression,
                                        returnExpressionLocation);
    }
}

Statement::Statement(Kind k, const Location& l) :
    Node(l),
    kind(k) {}

bool Statement::mayFallThrough() const {
    return true;
}

Traverse::Result Statement::traverse(Visitor& visitor) {
    visitor.visitStatement(*this);
    return Traverse::Continue;
}

VariableDeclarationStatement::VariableDeclarationStatement(
    Type* t,
    const Identifier& i,
    Expression* e,
    const Location& l) :
    Statement(Statement::VarDeclaration, l),
    declaration(t, i, l),
    patternExpression(nullptr),
    initExpression(e),
    isNameUnique(false),
    addToNameBindingsWhenTypeChecked(false),
    hasLookedUpType(false) {}

VariableDeclarationStatement::VariableDeclarationStatement(
    const VariableDeclarationStatement& other) :
    Statement(other),
    declaration(other.declaration),
    patternExpression(other.patternExpression ? patternExpression->clone() :
                                                nullptr),
    initExpression(other.initExpression ? other.initExpression->clone() :
                                          nullptr),
    isNameUnique(other.isNameUnique),
    addToNameBindingsWhenTypeChecked(other.addToNameBindingsWhenTypeChecked),
    hasLookedUpType(other.hasLookedUpType) {}

VariableDeclarationStatement* VariableDeclarationStatement::create(
    const Identifier& i,
    Expression* e) {

    return new VariableDeclarationStatement(Type::create(Type::Implicit),
                                            i,
                                            e,
                                            Location());
}

VariableDeclarationStatement* VariableDeclarationStatement::create(
    Type* t,
    const Identifier& i,
    Expression* e,
    const Location& l) {

    return new VariableDeclarationStatement(t, i, e, l);
}

VariableDeclarationStatement* VariableDeclarationStatement::create(
    Type* t,
    Expression* p,
    Expression* e,
    const Location& l) {

    auto decl = new VariableDeclarationStatement(t, "", e, l);
    decl->patternExpression = p;
    return decl;
}

VariableDeclarationStatement* VariableDeclarationStatement::clone() const {
    return new VariableDeclarationStatement(*this);
}

Traverse::Result VariableDeclarationStatement::traverse(Visitor& visitor) {
    if (visitor.visitVariableDeclaration(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    if (patternExpression != nullptr) {
        patternExpression->traverse(visitor);
    }

    if (initExpression != nullptr) {
        initExpression->traverse(visitor);
    }

    return Traverse::Continue;
}

Type* VariableDeclarationStatement::typeCheck(Context& context) {
    if (patternExpression != nullptr) {
        generateDeclarationsFromPattern(context);
        return &Type::voidType();
    }

    lookupType(context);

    auto declaredType = declaration.getType();
    const Identifier& name = declaration.getIdentifier();
    if (initExpression == nullptr) {
        auto method = context.getMethodDefinition();
        if (declaredType->isImplicit()) {
            Trace::error(
                "Implicitly typed variables must be initialized: " + name,
                this);
        } else if (declaredType->isEnumeration() &&
                   !method->isEnumConstructor() &&
                   !method->isEnumCopyConstructor()) {
            Trace::error(
                "Variables of enumeration type must be initialized: " + name,
                this);
        } else {
            initExpression =
                Expression::generateDefaultInitialization(declaredType,
                                                          getLocation());
        }
    }

    typeCheckAndTransformInitExpression(context);

    if (addToNameBindingsWhenTypeChecked) {
        if (!context.getNameBindings()->insertLocalObject(&declaration)) {
            Trace::error("Variable already declared: " + name, this);
        }
        addToNameBindingsWhenTypeChecked = false;
    }
    makeIdentifierUniqueIfTakingLambda(context);
    return &Type::voidType();
}

void VariableDeclarationStatement::typeCheckAndTransformInitExpression(
    Context& context) {

    if (initExpression == nullptr) {
        return;
    }

    auto declaredType = declaration.getType();
    initExpression = initExpression->transform(context);
    auto initType = initExpression->typeCheck(context);
    if (declaredType->isImplicit()) {
        if (initType->isVoid()) {
            Trace::error("Initialization expression is of void type.",
                         this);
        }
        auto copiedInitType = initType->clone();
        copiedInitType->setConstant(declaredType->isConstant());
        declaration.setType(copiedInitType);
    } else {
        if (!Type::isInitializableByExpression(declaredType, initExpression)) {
            Trace::error("Type mismatch.", declaredType, initType, this);
        }
    }
}

void VariableDeclarationStatement::generateDeclarationsFromPattern(
    Context& context) {

    if (initExpression == nullptr) {
        Trace::error("Missing initialization expression for pattern matching.",
                     this);
    }

    if (!declaration.getType()->isImplicit()) {
        Trace::error("Type must be implicit for variables bound by pattern.",
                     this);
    }

    initExpression = initExpression->transform(context);
    initExpression->typeCheck(context);
    auto initRefExpr = generateInitTemporary(context);

    if (!patternExpression->isClassDecomposition() &&
        patternExpression->dynCast<MethodCallExpression>() == nullptr) {
        Trace::error("Unexpected pattern.", this);
    }

    auto pattern = Pattern::create(patternExpression, context);
    MatchCoverage coverage(initRefExpr->getType());
    if (!pattern->isMatchExhaustive(initRefExpr, coverage, false, context)) {
        Trace::error("Pattern used in a variable declaration must always "
                     "match.",
                     this);
    }

    pattern->generateComparisonExpression(initRefExpr, context);

    auto currentBlock = context.getBlock();
    const VariableDeclarationStatementList& variablesCreatedByPattern =
         pattern->getVariablesCreatedByPattern();

    if (variablesCreatedByPattern.empty()) {
        Trace::error("No variables found in pattern.", patternExpression);
    }

    for (auto i = variablesCreatedByPattern.cbegin();
         i != variablesCreatedByPattern.cend(); ) {
        VariableDeclarationStatement* varDeclaration = *i++;
        if (i == variablesCreatedByPattern.end()) {
            currentBlock->replaceCurrentStatement(varDeclaration);
        } else {
            currentBlock->insertBeforeCurrentStatement(varDeclaration);
        }
        varDeclaration->typeCheck(context);
    }
}

Expression* VariableDeclarationStatement::generateInitTemporary(
    Context& context) {

    Expression* initRefExpr = nullptr;
    if (initExpression->isVariable()) {
        initRefExpr = initExpression;
    } else {
        const Location& location = initExpression->getLocation();
        Type* initExpressionType = initExpression->getType();

        Identifier initTmpName =
            generateTemporaryName(CommonNames::matchSubjectName);
        auto initTmpDeclaration =
            VariableDeclarationStatement::create(initExpressionType,
                                                 initTmpName,
                                                 initExpression,
                                                 location);
        auto currentBlock = context.getBlock();
        currentBlock->insertBeforeCurrentStatement(initTmpDeclaration);
        initTmpDeclaration->typeCheck(context);

        initRefExpr =
            LocalVariableExpression::create(initExpressionType,
                                            initTmpName,
                                            location);
    }
    return initRefExpr;
}

void VariableDeclarationStatement::lookupType(const Context& context) {
    if (!hasLookedUpType) {
        Type* type = context.lookupConcreteType(declaration.getType(),
                                                getLocation());
        declaration.setType(type);
        hasLookedUpType = true;
    }
}

void VariableDeclarationStatement::makeIdentifierUniqueIfTakingLambda(
    Context& context) {

    const MethodDefinition* method = context.getMethodDefinition();
    if (method->getLambdaSignature() != nullptr && !isNameUnique) {
        // Transform the variable name to avoid name collisions with names in
        // methods that call this method. Since this method takes a lambda it
        // will be inlined in the calling method.
        Identifier uniqueIdentifier =
            Symbol::makeUnique(declaration.getIdentifier(),
                               context.getClassDefinition()->getName(),
                               method->getName());
        declaration.setIdentifier(uniqueIdentifier);

        // Insert the new name in the name bindings so that the variable can be
        // resolved using the new name.
        if (!context.getNameBindings()->insertLocalObject(&declaration)) {
            Trace::error("Variable already declared: " +
                         declaration.getIdentifier(),
                         declaration.getLocation());
        }
        isNameUnique = true;
    }
}

Identifier VariableDeclarationStatement::generateTemporaryName(
    const Identifier& name) {

    Identifier identifier(name);
    std::ostringstream stream;
    stream << tempCounter++;
    identifier += "_" + stream.str();
    return identifier;
}

VariableDeclarationStatement* VariableDeclarationStatement::generateTemporary(
    Type* t,
    const Identifier& name,
    Expression* init,
    const Location& loc) {

    return new VariableDeclarationStatement(t,
                                            generateTemporaryName(name),
                                            init,
                                            loc);
}

BlockStatement* BlockStatement::cloningBlock = nullptr;

BlockStatement::BlockStatement(
    ClassDefinition* classDef, 
    BlockStatement* enclosing, 
    const Location& l) :
    Statement(Statement::Block, l), 
    nameBindings(enclosing != nullptr ? &enclosing->nameBindings :
                 &(classDef->getNameBindings())),
    statements(),
    iterator(statements.begin()),
    enclosingBlock(enclosing) {}

BlockStatement::BlockStatement(const BlockStatement& other) :
    Statement(other),
    nameBindings(other.nameBindings.getEnclosing()),
    statements(),
    iterator(statements.begin()),
    enclosingBlock(nullptr) {

    copyStatements(other);
}

BlockStatement* BlockStatement::create(
    ClassDefinition* classDef,
    BlockStatement* enclosing,
    const Location& l) {

    return new BlockStatement(classDef, enclosing, l);
}

void BlockStatement::copyStatements(const BlockStatement& from) {
    if (cloningBlock != nullptr) {
        setEnclosingBlock(cloningBlock);
    }

    BlockStatement* tmp = cloningBlock;
    cloningBlock = this;

    for (auto statement: from.statements) {
        addStatement(statement->clone());
    }

    cloningBlock = tmp;
}

BlockStatement* BlockStatement::clone() const {
    return new BlockStatement(*this);
}

void BlockStatement::addStatement(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        statements.push_back(statement);
    }
}

void BlockStatement::insertStatementAtFront(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        statements.push_front(statement);
    }
}

void BlockStatement::insertStatementAfterFront(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        auto i = statements.begin();
        if (i != statements.end()) {
            i++;
        }
        statements.insert(i, statement);
    }
}

void BlockStatement::insertBeforeCurrentStatement(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        statements.insert(iterator, statement);
    }
}

void BlockStatement::initialStatementCheck(Statement* statement) {
    switch (statement->getKind()) {
        case VarDeclaration: {
            VariableDeclarationStatement* varDeclarationStatement =
                statement->cast<VariableDeclarationStatement>();
            VariableDeclaration* varDeclaration =
                varDeclarationStatement->getDeclaration();
            if (!(varDeclarationStatement->getAddToNameBindingsWhenTypeChecked()
                  || varDeclarationStatement->hasPattern())) {
                addLocalBinding(varDeclaration);
            }
            break;
        }
        case Label:
            addLabel(statement->cast<LabelStatement>());
            break;
        default:
            break;
    }
}

void BlockStatement::addLocalBinding(VariableDeclaration* localObject) {
    Type* type = localObject->getType();
    if (type->getDefinition() == nullptr) {
        Tree::lookupAndSetTypeDefinition(type,
                                         nameBindings,
                                         localObject->getLocation());
    }

    if (!nameBindings.insertLocalObject(localObject)) {
        Trace::error("Variable already declared: " +
                     localObject->getIdentifier(),
                     localObject->getLocation());
    }
}

void BlockStatement::addLabel(const LabelStatement* label) {
    if (!nameBindings.insertLabel(label->getName())) {
        Trace::error("Identifier already declared: " + label->getName(),
                     label->getLocation());
    }
}

void BlockStatement::setEnclosingBlock(BlockStatement* b) {
    enclosingBlock = b;
    if (enclosingBlock != nullptr) {
        nameBindings.setEnclosing(&(enclosingBlock->nameBindings));
    }
}

Type* BlockStatement::typeCheck(Context& context) {
    context.enterBlock(this);

    for (iterator = statements.begin();
         iterator != statements.end();
         iterator++) {
        auto statement = *iterator;
        if (statement->getKind() == Statement::ExpressionStatement) {
            *iterator = statement->cast<Expression>()->transform(context);
            statement = *iterator;
        }
        statement->typeCheck(context);
    }

    context.exitBlock();
    return &Type::voidType();
}

bool BlockStatement::mayFallThrough() const {
    if (statements.empty()) {
        return true;
    }

    return statements.back()->mayFallThrough();
}

Traverse::Result BlockStatement::traverse(Visitor& visitor) {
    if (visitor.visitBlock(*this) == Traverse::Skip) {
        visitor.exitBlock();
        return Traverse::Continue;
    }

    for (auto statement: statements) {
        statement->traverse(visitor);
    }

    visitor.exitBlock();
    return Traverse::Continue;
}

void BlockStatement::returnLastExpression(
    VariableDeclarationStatement* retvalTmpDeclaration) {

    if (statements.empty()) {
        Trace::error("Must return a value.", getLocation());
    }

    auto i = statements.end();
    i--;
    Statement* lastStatement = *i;
    if (lastStatement->getKind() != Statement::ExpressionStatement) {
        Trace::error("Must return a value.", getLocation());
    }
    Expression* returnExpression = lastStatement->cast<Expression>();

    const Type* returnTypeInSignature = retvalTmpDeclaration->getType();
    const Type* returnType = returnExpression->getType();
    if (!Type::areInitializable(returnTypeInSignature, returnType)) {
        Trace::error(
            "Returned type is incompatible with declared return type in lambda "
            "expression signature. Returned type: " + returnType->toString() +
            ". Return type in signature: " + returnTypeInSignature->toString(),
            returnExpression);
    }

    *i = makeReturnValueAssignment(returnExpression, 
                                   retvalTmpDeclaration->getType(),
                                   retvalTmpDeclaration->getIdentifier());
}

ConstructorCallStatement*
BlockStatement::getFirstStatementAsConstructorCall() const {
    if (statements.empty()) {
        return nullptr;
    }
    Statement* firstStatement = *statements.begin();
    if (firstStatement->getKind() == Statement::ConstructorCall) {
        return firstStatement->cast<ConstructorCallStatement>();
    }
    return nullptr;
}

void BlockStatement::replaceLastStatement(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        auto i = statements.end();
        i--;
        *i = statement;
    }
}

void BlockStatement::replaceCurrentStatement(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        *iterator = statement;
    }
}

Expression* BlockStatement::getLastStatementAsExpression() const {
    if (statements.size() > 0) {
        auto i = statements.cend();
        i--;
        Statement* lastStatement = *i;
        if (lastStatement->isExpression()) {
            return lastStatement->cast<Expression>();
        }
    }
    return nullptr;
}

bool BlockStatement::containsDeferDeclaration() const {
    if (VariableDeclarationStatement* varDeclaration =
            statements.front()->dynCast<VariableDeclarationStatement>()) {
        if (varDeclaration->getIdentifier().compare(deferVariableName) == 0) {
            return true;
        }
    }
    return false;
}

IfStatement::IfStatement(
    Expression* e,
    BlockStatement* b,
    BlockStatement* eb,
    const Location& l) :
    Statement(Statement::If, l), 
    expression(e),
    block(b),
    elseBlock(eb) {}

IfStatement* IfStatement::create(
    Expression* e,
    BlockStatement* b,
    BlockStatement* eb,
    const Location& l) {

    return new IfStatement(e, b, eb, l);
}

Statement* IfStatement::clone() const {
    return new IfStatement(expression->clone(),
                           block->clone(),
                           elseBlock ? elseBlock->clone() : nullptr,
                           getLocation());
}

Type* IfStatement::typeCheck(Context& context) {
    expression = expression->transform(context);
    Type* type = expression->typeCheck(context);

    if (type->isBoolean() || type->isNumber()) {
        block->typeCheck(context);
        if (elseBlock) {
            elseBlock->typeCheck(context);
        }
    } else {
        Trace::error(
            "Resulting type from expression in if statement must be a boolean "
            "type.",
            getLocation());
    }

    return &Type::voidType();
}

bool IfStatement::mayFallThrough() const {
    if (elseBlock == nullptr) {
        return true;
    }

    return block->mayFallThrough() || elseBlock->mayFallThrough();
}

Traverse::Result IfStatement::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    expression->traverse(visitor);
    block->traverse(visitor);
    if (elseBlock != nullptr) {
        elseBlock->traverse(visitor);
    }

    return Traverse::Continue;
}

WhileStatement::WhileStatement(
    Expression* e,
    BlockStatement* b,
    const Location& l) :
    Statement(Statement::While, l), 
    expression(e ? e : BooleanLiteralExpression::create(true, getLocation())),
    block(b) {}

WhileStatement* WhileStatement::create(
    Expression* e,
    BlockStatement* b,
    const Location& l) {

    return new WhileStatement(e, b, l);
}

Statement* WhileStatement::clone() const {
    return new WhileStatement(expression->clone(),
                              block->clone(),
                              getLocation());
}

Type* WhileStatement::typeCheck(Context& context) {
    expression = expression->transform(context);
    Type* type = expression->typeCheck(context);

    if (!type->isBoolean() && !type->isNumber()) {
        Trace::error(
            "Resulting type from expression should be a boolean type.",
            getLocation());
    }

    bool wasAlreadyInLoop = context.isInsideLoop();
    context.setIsInsideLoop(true);
    block->typeCheck(context);
    context.setIsInsideLoop(wasAlreadyInLoop);
    return &Type::voidType();
}

bool WhileStatement::mayFallThrough() const {
    if (LiteralExpression* literal = expression->dynCast<LiteralExpression>()) {
        if (BooleanLiteralExpression* boolLiteral =
                literal->dynCast<BooleanLiteralExpression>()) {
            if (boolLiteral->getValue() == true) {
                // TODO: check if the block contains a break. If not, we cannot
                // fall through to the next statement.
                return false;
            }
        }
    }
    return true;
}

Traverse::Result WhileStatement::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    expression->traverse(visitor);
    block->traverse(visitor);
    return Traverse::Continue;
}

ForStatement::ForStatement(
    Expression* cond,
    Expression* iter,
    BlockStatement* b,
    const Location& l) :
    Statement(Statement::For, l),
    conditionExpression(cond),
    iterExpression(iter),
    block(b) {}

ForStatement* ForStatement::create(
    Expression* cond,
    Expression* iter,
    BlockStatement* b,
    const Location& l) {

    return new ForStatement(cond, iter, b, l);
}

Statement* ForStatement::clone() const {
    return new ForStatement(conditionExpression ? conditionExpression->clone() :
                                                  nullptr,
                            iterExpression ? iterExpression->clone() : nullptr,
                            block->clone(),
                            getLocation());
}

Type* ForStatement::typeCheck(Context& context) {
    if (conditionExpression != nullptr) {
        conditionExpression = conditionExpression->transform(context);
        Type* type = conditionExpression->typeCheck(context);
        if (!type->isBoolean() && !type->isNumber()) {
            Trace::error(
                "Resulting type from expression should be a boolean type.",
                getLocation());
        }
    }

    if (iterExpression != nullptr) {
        iterExpression = iterExpression->transform(context);
        iterExpression->typeCheck(context);
    }

    bool wasAlreadyInLoop = context.isInsideLoop();
    context.setIsInsideLoop(true);
    block->typeCheck(context);
    context.setIsInsideLoop(wasAlreadyInLoop);
    return &Type::voidType();
}

bool ForStatement::mayFallThrough() const {
    if (conditionExpression != nullptr) {
        return true;
    }

    // TODO: check if the block contains a break. If not, we cannot fall through
    // to the next statement.
    return false;
}

Traverse::Result ForStatement::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    if (conditionExpression != nullptr) {
        conditionExpression->traverse(visitor);
    }

    if (iterExpression != nullptr) {
        iterExpression->traverse(visitor);
    }

    block->traverse(visitor);
    return Traverse::Continue;
}

BreakStatement::BreakStatement(const Location& l) : 
    Statement(Statement::Break, l) {}

BreakStatement* BreakStatement::create(const Location& l) {
    return new BreakStatement(l);
}

Statement* BreakStatement::clone() const {
    return new BreakStatement(getLocation());
}

Type* BreakStatement::typeCheck(Context& context) {
    if (!context.isInsideLoop()) {
        Trace::error("Break statement must be inside a loop.", getLocation());
    }
    return &Type::voidType();
}

bool BreakStatement::mayFallThrough() const {
    return false;
}

ContinueStatement::ContinueStatement(const Location& l) :
    Statement(Statement::Continue, l) {}

ContinueStatement* ContinueStatement::create(const Location& l) {
    return new ContinueStatement(l);
}

Statement* ContinueStatement::clone() const {
    return new ContinueStatement(getLocation());
}

Type* ContinueStatement::typeCheck(Context& context) {
    if (!context.isInsideLoop()) {
        Trace::error("Continue statement must be inside a loop.",
                     getLocation());
    }
    return &Type::voidType();
}

bool ContinueStatement::mayFallThrough() const {
    return false;
}

ReturnStatement::ReturnStatement(Expression* e, const Location& l) : 
    Statement(Statement::Return, l), 
    expression(e),
    originalMethod(nullptr) {}

ReturnStatement::ReturnStatement(const ReturnStatement& other) :
    Statement(other), 
    expression(other.expression ? other.expression->clone() : nullptr),
    originalMethod(other.originalMethod) {}

ReturnStatement* ReturnStatement::create(Expression* e, const Location& l) {
    return new ReturnStatement(e, l);
}

ReturnStatement* ReturnStatement::create(Expression* e) {
    return new ReturnStatement(e, Location());
}

Statement* ReturnStatement::clone() const {
    return new ReturnStatement(*this);
}

Type* ReturnStatement::typeCheck(Context& context) {
    MethodDefinition* method = context.getMethodDefinition();
    VariableDeclaration* temporaryRetvalDeclaration = 
        context.getTemporaryRetvalDeclaration();
    if (originalMethod != nullptr && originalMethod != method &&
        temporaryRetvalDeclaration != nullptr) {
        // The method in which the return statement was located in has been
        // inlined in another method. Transform the return statement into an
        // assignment expression to assign the return value of the inlined
        // method.
        BinaryExpression* returnValueAssignment = 
            makeReturnValueAssignment(
                expression, 
                temporaryRetvalDeclaration->getType(),
                temporaryRetvalDeclaration->getIdentifier());
        context.getBlock()->replaceCurrentStatement(returnValueAssignment);
        return &Type::voidType();
    }

    originalMethod = method;
    Type* returnType = nullptr;
    Type* returnTypeInSignature = method->getReturnType();
    if (expression != nullptr) {
        if (returnTypeInSignature->isVoid()) {
            Trace::error(
                "Cannot return a value when the declared return type is void.",
                this);
        }
        expression = expression->transform(context);
        returnType = expression->typeCheck(context);
    } else {
        returnType = Type::create(Type::Void);
    }

    if (!Type::areInitializable(returnTypeInSignature, returnType) &&
        !context.getClassDefinition()->isClosure()) {
        Trace::error(
            "Returned type is incompatible with declared return type in method "
            "definition. Returned type in return statement: " +
            returnType->toString() +
            ". Return type in signature: " + returnTypeInSignature->toString() +
            ".",
            this);
    }
    return &Type::voidType();
}

bool ReturnStatement::mayFallThrough() const {
    return false;
}

Traverse::Result ReturnStatement::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    if (expression != nullptr) {
        expression->traverse(visitor);
    }

    return Traverse::Continue;
}

DeferStatement::DeferStatement(BlockStatement* b, const Location& l) :
    Statement(Statement::Defer, l),
    block(b) {}

DeferStatement* DeferStatement::create(BlockStatement* b, const Location& l) {
    return new DeferStatement(b, l);
}

Statement* DeferStatement::clone() const {
    return new DeferStatement(block->clone(), getLocation());
}

Type* DeferStatement::typeCheck(Context& context) {
    BlockStatement* outerBlock = context.getBlock();
    if (!outerBlock->containsDeferDeclaration()) {
        Type* deferType = Type::create(CommonNames::deferTypeName);

        // A defer object is stored on the stack, not the heap.
        deferType->setReference(false);
        auto deferDeclaration =
            VariableDeclarationStatement::create(deferType,
                                                 deferVariableName,
                                                 nullptr,
                                                 outerBlock->getLocation());
        outerBlock->insertStatementAtFront(deferDeclaration);
        deferDeclaration->typeCheck(context);
    }

    const Location& loc = getLocation();
    MethodCallExpression* addClosureCall =
        new MethodCallExpression(CommonNames::addClosureMethodName, loc);
    addClosureCall->addArgument(new AnonymousFunctionExpression(block, loc));
    auto memberSelector =
        MemberSelectorExpression::create(NamedEntityExpression::create(
                                             deferVariableName),
                                         addClosureCall,
                                         loc);

    memberSelector =
        MemberSelectorExpression::transformMemberSelector(memberSelector,
                                                          context);
    outerBlock->replaceCurrentStatement(memberSelector);
    memberSelector->typeCheck(context);

    return &Type::voidType();
}

Traverse::Result DeferStatement::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    block->traverse(visitor);
    return Traverse::Continue;
}

ConstructorCallStatement::ConstructorCallStatement(
    MethodCallExpression* c) :
    Statement(Statement::ConstructorCall, c->getLocation()),
    constructorCall(c),
    isBaseClassCtorCall(constructorCall->getName() != Keyword::initString),
    isTypeChecked(false) {}

ConstructorCallStatement* ConstructorCallStatement::create(
    MethodCallExpression* c) {

    return new ConstructorCallStatement(c);
}

Statement* ConstructorCallStatement::clone() const {
    return new ConstructorCallStatement(constructorCall->clone());
}

Type* ConstructorCallStatement::typeCheck(Context& context) {
    if (isTypeChecked) {
        return &Type::voidType();
    }

    context.setIsConstructorCallStatement(true);
    ClassDefinition* thisClass = context.getClassDefinition();
    if (isBaseClassCtorCall) {
        ClassDefinition* baseClass = thisClass->getBaseClass();
        if (constructorCall->getName().compare(baseClass->getName()) != 0) {
            Trace::error(constructorCall->getName() +
                         " is not the base class of " +
                         thisClass->getName(),
                         this);
        }
    } else {
        // This is a call to another constructor in this class.
        constructorCall->setName(thisClass->getName());
    }
    constructorCall->setIsConstructorCall();

    // All expressions must be transformed before calling typeCheck() even if we
    // don't expect the type of expression to change from MethodCallExpression. 
    constructorCall = MethodCallExpression::transformMethodCall(constructorCall,
                                                                context);
    constructorCall->typeCheck(context);
    context.setIsConstructorCallStatement(false);
    isTypeChecked = true;
    return &Type::voidType();
}

Traverse::Result ConstructorCallStatement::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    constructorCall->traverse(visitor);
    return Traverse::Continue;
}

LabelStatement::LabelStatement(
    const Identifier& n,
    const Location& l) :
    Statement(Statement::Label, l),
    name(n) {}

LabelStatement* LabelStatement::create(const Identifier& n, const Location& l) {
    return new LabelStatement(n, l);
}

Statement* LabelStatement::clone() const {
    return new LabelStatement(name, getLocation());
}

Type* LabelStatement::typeCheck(Context&) {
    return &Type::voidType();
}

JumpStatement::JumpStatement(
    const Identifier& n,
    const Location& l) :
    Statement(Statement::Jump, l),
    labelName(n) {}

JumpStatement* JumpStatement::create(const Identifier& n, const Location& l) {
    return new JumpStatement(n, l);
}

Statement* JumpStatement::clone() const {
    return new JumpStatement(labelName, getLocation());
}

Type* JumpStatement::typeCheck(Context& context) {
    auto binding = context.lookup(labelName);
    if (binding == nullptr) {
        Trace::error("Unknown identifier: " + labelName, this);
    }
    if (binding->getReferencedEntity() != Binding::Label) {
        Trace::error("Not a label: " + labelName, this);
    }
    return &Type::voidType();
}

bool JumpStatement::mayFallThrough() const {
    return false;
}
