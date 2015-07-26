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

    BinaryExpression* makeReturnValueAssignment(
        Expression* returnExpression,
        Type* returnType,
        const Identifier& retvalTmpIdentifier) {

        Location returnExpressionLocation(returnExpression->getLocation());
        LocalVariableExpression* retvalTmp =
            new LocalVariableExpression(returnType,
                                        retvalTmpIdentifier,
                                        returnExpressionLocation);
        return new BinaryExpression(Operator::Assignment,
                                    retvalTmp,
                                    returnExpression,
                                    returnExpressionLocation);
    }
}

Statement::Statement(StatementType t, const Location& l) :
    Node(l),
    statementType(t) {}

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
    addToNameBindingsWhenTypeChecked(false) {}

VariableDeclarationStatement::VariableDeclarationStatement(
    Type* t,
    Expression* p,
    Expression* e,
    const Location& l) :
    Statement(Statement::VarDeclaration, l),
    declaration(t, "", l),
    patternExpression(p),
    initExpression(e),
    isNameUnique(false),
    addToNameBindingsWhenTypeChecked(false) {}

VariableDeclarationStatement::VariableDeclarationStatement(
    const Identifier& i,
    Expression* e) :
    VariableDeclarationStatement(new Type(Type::Implicit), i, e, Location()) {}

VariableDeclarationStatement::VariableDeclarationStatement(
    const VariableDeclarationStatement& other) :
    Statement(other),
    declaration(other.declaration),
    patternExpression(other.patternExpression ? patternExpression->clone() :
                                                nullptr),
    initExpression(other.initExpression ? other.initExpression->clone() :
                                          nullptr),
    isNameUnique(other.isNameUnique),
    addToNameBindingsWhenTypeChecked(other.addToNameBindingsWhenTypeChecked) {}

VariableDeclarationStatement* VariableDeclarationStatement::clone() const {
    return new VariableDeclarationStatement(*this);
}

Type* VariableDeclarationStatement::typeCheck(Context& context) {
    if (patternExpression != nullptr) {
        generateDeclarationsFromPattern(context);
        delete this;
        return &Type::voidType();
    }

    changeTypeIfGeneric(context);

    Type* declaredType = declaration.getType();
    const Identifier& name = declaration.getIdentifier();
    if (initExpression == nullptr) {
        if (declaredType->isImplicit()) {
            Trace::error(
                "Implicitly typed variables must be initialized: " + name,
                this);
        } else if (declaredType->isEnumeration() &&
                   !context.getMethodDefinition()->isEnumConstructor()) {
            Trace::error(
                "Variables of enumeration type must be initialized: " + name,
                this);
        } else {
            initExpression =
                Expression::generateDefaultInitialization(declaredType,
                                                          getLocation());
        }
    }

    processInitExpression(context);

    if (addToNameBindingsWhenTypeChecked) {
        if (!context.getNameBindings()->insertLocalObject(&declaration)) {
            Trace::error("Variable already declared: " + name, this);
        }
        addToNameBindingsWhenTypeChecked = false;
    }
    makeIdentifierUniqueIfTakingLambda(context);
    return &Type::voidType();
}

void VariableDeclarationStatement::processInitExpression(Context& context) {
    if (initExpression == nullptr) {
        return;
    }

    Type* declaredType = declaration.getType();
    initExpression = initExpression->transform(context);
    Type* initType = initExpression->typeCheck(context);
    if (declaredType->isImplicit()) {
        if (initType->isVoid()) {
            Trace::error("Initialization expression is of void type.",
                         this);
        }
        Type* copiedInitType = initType->clone();
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
    Expression* initRefExpr = generateInitTemporary(context);

    if (!patternExpression->isClassDecomposition() &&
        patternExpression->dynCast<MethodCallExpression>() == nullptr) {
        Trace::error("Unexpected pattern.", this);
    }

    Pattern* pattern = Pattern::create(patternExpression, context);
    MatchCoverage coverage(initRefExpr->getType());
    if (!pattern->isMatchExhaustive(initRefExpr, coverage, false, context)) {
        Trace::error("Pattern used in a variable declaration must always "
                     "match.",
                     this);
    }

    pattern->generateComparisonExpression(initRefExpr, context);

    BlockStatement* currentBlock = context.getBlock();
    const VariableDeclarationStatementList& variablesCreatedByPattern =
         pattern->getVariablesCreatedByPattern();

    if (variablesCreatedByPattern.empty()) {
        Trace::error("No variables found in pattern.", patternExpression);
    }

    for (VariableDeclarationStatementList::const_iterator i =
             variablesCreatedByPattern.begin();
         i != variablesCreatedByPattern.end(); ) {
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
        VariableDeclarationStatement* initTmpDeclaration =
            new VariableDeclarationStatement(initExpressionType,
                                             initTmpName,
                                             initExpression,
                                             location);
        BlockStatement* currentBlock = context.getBlock();
        currentBlock->insertBeforeCurrentStatement(initTmpDeclaration);
        initTmpDeclaration->typeCheck(context);

        initRefExpr =
            new LocalVariableExpression(initExpressionType,
                                        initTmpName,
                                        location);
    }
    return initRefExpr;
}

void VariableDeclarationStatement::changeTypeIfGeneric(Context& context) {
    Type* concreteType = context.makeGenericTypeConcrete(declaration.getType(),
                                                         getLocation());
    if (concreteType != nullptr) {
        // The variable type is a generic type parameter that has been assigned
        // a concrete type. Change the variable type to the concrete type.
        declaration.setType(concreteType);
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

void BlockStatement::copyStatements(const BlockStatement& from) {
    for (StatementList::const_iterator i = from.statements.begin();
         i != from.statements.end();
         i++) {
        Statement* statement = (*i)->clone();
        switch (statement->getStatementType()) {
            case Statement::Block: {
                BlockStatement* block = statement->cast<BlockStatement>();
                block->setEnclosingBlock(this);
                break;
            }
            case Statement::If: {
                IfStatement* ifStatement = statement->cast<IfStatement>();
                BlockStatement* ifBlock = ifStatement->getBlock();
                if (ifBlock != nullptr) {
                    ifBlock->setEnclosingBlock(this);
                }
                BlockStatement* elseBlock = ifStatement->getElseBlock();
                if (elseBlock != nullptr) {
                    elseBlock->setEnclosingBlock(this);
                }
                break;
            }
            case Statement::While: {
                BlockStatement* whileBlock =
                    statement->cast<WhileStatement>()->getBlock();
                if (whileBlock != nullptr) {
                    whileBlock->setEnclosingBlock(this);
                }
                break;
            }
            case Statement::ExpressionStatement: {
                if (WrappedStatementExpression* wrappedStatementExpr =
                        statement->dynCast<WrappedStatementExpression>()) {
                    Statement* wrappedStatement =
                        wrappedStatementExpr->getStatement();
                    if (BlockStatement* block =
                            wrappedStatement->dynCast<BlockStatement>()) {
                        block->setEnclosingBlock(this);
                    }
                }
                break;
            }
            default:
                break;
        }
        addStatement(statement);
    }
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
        StatementList::iterator i = statements.begin();
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
    switch (statement->getStatementType()) {
        case VarDeclaration: {
            VariableDeclarationStatement* varDeclarationStatement =
                statement->cast<VariableDeclarationStatement>();
            VariableDeclaration* varDeclaration =
                varDeclarationStatement->getDeclaration();
            if (varDeclarationStatement->getAddToNameBindingsWhenTypeChecked()
                || varDeclarationStatement->hasPattern()) {
                Tree::lookupAndSetTypeDefinition(varDeclaration->getType(),
                                                 nameBindings,
                                                 varDeclaration->getLocation());
            } else {
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
        Statement* statement = *iterator;
        if (statement->getStatementType() == Statement::ExpressionStatement) {
            *iterator = statement->cast<Expression>()->transform(context);
            statement = *iterator;
        }
        statement->typeCheck(context);
    }
    context.exitBlock();
    return &Type::voidType();
}

void BlockStatement::returnLastExpression(
    VariableDeclarationStatement* retvalTmpDeclaration) {

    if (statements.empty()) {
        Trace::error("Must return a value.", getLocation());
    }

    StatementList::iterator i = statements.end();
    i--;
    Statement* lastStatement = *i;
    if (lastStatement->getStatementType() != Statement::ExpressionStatement) {
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
    if (firstStatement->getStatementType() == Statement::ConstructorCall) {
        return firstStatement->cast<ConstructorCallStatement>();
    }
    return nullptr;
}

void BlockStatement::replaceLastStatement(Statement* statement) {
    if (statement != nullptr) {
        initialStatementCheck(statement);
        StatementList::iterator i = statements.end();
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
        StatementList::const_iterator i = statements.end();
        i--;
        Statement* lastStatement = *i;
        if (lastStatement->isExpression()) {
            return lastStatement->cast<Expression>();
        }
    }
    return nullptr;
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

Statement* IfStatement::clone() const {
    return new IfStatement(expression->clone(),
                           block->clone(),
                           elseBlock ? elseBlock->clone() : nullptr,
                           getLocation());
}

Type* IfStatement::typeCheck(Context& context) {
    expression = expression->transform(context);
    Type* type = expression->typeCheck(context);
    if (type->isBoolean()) {
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

WhileStatement::WhileStatement(
    Expression* e,
    BlockStatement* b,
    const Location& l) :
    Statement(Statement::While, l), 
    expression(e ? e : new BooleanLiteralExpression(true, getLocation())),
    block(b) {}

Statement* WhileStatement::clone() const {
    return new WhileStatement(expression->clone(),
                              block->clone(),
                              getLocation());
}

Type* WhileStatement::typeCheck(Context& context) {
    expression = expression->transform(context);
    Type* type = expression->typeCheck(context);
    if (!type->isBoolean()) {
        Trace::error(
            "Resulting type from expression should be a boolean type.",
            getLocation());
    }
    bool wasAlreadyInWhileStatement = context.isInsideWhileStatement();
    context.setIsInsideWhileStatement(true);
    block->typeCheck(context);
    context.setIsInsideWhileStatement(wasAlreadyInWhileStatement);
    return &Type::voidType();
}

BreakStatement::BreakStatement(const Location& l) : 
    Statement(Statement::Break, l) {}

Statement* BreakStatement::clone() const {
    return new BreakStatement(getLocation());
}

Type* BreakStatement::typeCheck(Context& context) {
    if (!context.isInsideWhileStatement()) {
        Trace::error("Break statement must be inside a loop.", getLocation());
    }
    return &Type::voidType();
}

ContinueStatement::ContinueStatement(const Location& l) :
    Statement(Statement::Continue, l) {}

Statement* ContinueStatement::clone() const {
    return new ContinueStatement(getLocation());
}

Type* ContinueStatement::typeCheck(Context& context) {
    if (!context.isInsideWhileStatement()) {
        Trace::error("Continue statement must be inside a loop.",
                     getLocation());
    }
    return &Type::voidType();
}

ReturnStatement::ReturnStatement(Expression* e, const Location& l) : 
    Statement(Statement::Return, l), 
    expression(e),
    originalMethod(nullptr) {}

ReturnStatement::ReturnStatement(Expression* e) :
    ReturnStatement(e, Location()) {}

ReturnStatement::ReturnStatement(const ReturnStatement& other) :
    Statement(other), 
    expression(other.expression ? other.expression->clone() : nullptr),
    originalMethod(other.originalMethod) {}

Statement* ReturnStatement::clone() const {
    return new ReturnStatement(*this);
}

Type* ReturnStatement::typeCheck(Context& context) {
    VariableDeclaration* temporaryRetvalDeclaration = 
        context.getTemporaryRetvalDeclaration();
    if (originalMethod != nullptr &&
        originalMethod != context.getMethodDefinition() && 
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
        delete this;
        return &Type::voidType();
    }

    originalMethod = context.getMethodDefinition();
    Type* returnType = nullptr;
    Type* returnTypeInSignature =
        context.getMethodDefinition()->getReturnType();
    if (expression != nullptr) {
        if (returnTypeInSignature->isVoid()) {
            Trace::error(
                "Cannot return a value when the declared return type is void.",
                this);
        }
        expression = expression->transform(context);
        returnType = expression->typeCheck(context);
    } else {
        returnType = new Type(Type::Void);
    }
    if (!Type::areInitializable(returnTypeInSignature, returnType)) {
        Trace::error(
            "Returned type is incompatible with declared return type in method "
            "definition. Returned type: " + returnType->toString() +
            ". Return type in signature: " + returnTypeInSignature->toString() +
            ".",
            this);
    }
    return &Type::voidType();
}

ConstructorCallStatement::ConstructorCallStatement(
    MethodCallExpression* c) :
    Statement(Statement::ConstructorCall, c->getLocation()),
    constructorCall(c),
    isBaseClassCtorCall(constructorCall->getName() != Keyword::initString) {}

Statement* ConstructorCallStatement::clone() const {
    return new ConstructorCallStatement(constructorCall->clone());
}

Type* ConstructorCallStatement::typeCheck(Context& context) {
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
    return &Type::voidType();
}

LabelStatement::LabelStatement(
    const Identifier& n,
    const Location& l) :
    Statement(Statement::Label, l),
    name(n) {}

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

Statement* JumpStatement::clone() const {
    return new JumpStatement(labelName, getLocation());
}

Type* JumpStatement::typeCheck(Context& context) {
    Binding* binding = context.lookup(labelName);
    if (binding == nullptr) {
        Trace::error("Unknown identifier: " + labelName, this);
    }
    if (binding->getReferencedEntity() != Binding::Label) {
        Trace::error("Not a label: " + labelName, this);
    }
    return &Type::voidType();
}
