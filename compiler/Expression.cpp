#include <iterator>
#include <memory>

#include "Expression.h"
#include "Context.h"
#include "Definition.h"
#include "Tree.h"
#include "Pattern.h"

namespace {
    const Identifier thisPointerName("__thisPointer");
    const Identifier indexVaraibleName("__i");
    const Identifier arrayReferenceName("__array");
    const Identifier lambdaRetvalName("__lambda_retval");
    const Identifier retvalName("__inlined_retval");
    const Identifier matchResultName("__match_result");
    const Identifier matchEndName("__match_end");
}

Expression::Expression(ExpressionType t, const Location& l) : 
    Statement(Statement::ExpressionStatement, l),
    type(nullptr),
    expressionType(t) {}

Expression::Expression(const Expression& other) :
    Statement(other),
    type(other.type ? other.type->clone() : nullptr),
    expressionType(other.expressionType) {}

Expression* Expression::transform(Context&) {
    return this;
}

bool Expression::isVariable() const {
    return false;
}

Identifier Expression::generateVariableName() const {
    return Identifier();
}

Expression::ExpressionType Expression::getRightmostExpressionType() const {
    return expressionType;
}

Expression* Expression::generateDefaultInitialization(
    Type* type, 
    const Location& location) {

    if (type->isReference()) {
        return new NullExpression(location);
    } else if (type->isEnumeration()) {
        return nullptr;
    } else {
        return LiteralExpression::generateDefault(type, location);
    }
}

LiteralExpression::LiteralExpression(
    LiteralType l,
    Type* t,
    const Location& loc) :
    Expression(Expression::Literal, loc),
    literalType(l) {

    type = t;
    Tree::lookupAndSetTypeDefinition(type, getLocation());
}

Expression* LiteralExpression::generateDefault(
    Type* type, 
    const Location& location) {

    Expression* expression = nullptr;
    switch (type->getBuiltInType()) {
        case Type::Byte:
        case Type::Integer:
            expression = new IntegerLiteralExpression(0, location);
            break;
        case Type::Float:
            expression = new FloatLiteralExpression(0, location);
            break;
        case Type::Char:
            expression = new CharacterLiteralExpression(0, location);
            break;
        case Type::String:
            expression = new StringLiteralExpression("", location);
            break;
        case Type::Boolean:  
            expression = new BooleanLiteralExpression(false, location);
            break;
        default:
            Trace::internalError("LiteralExpression::generateDefault");
            break;
    }
    return expression;
}

CharacterLiteralExpression::CharacterLiteralExpression(
    char c,
    const Location& loc) :
    LiteralExpression(LiteralExpression::Character, new Type(Type::Char), loc),
    value(c) {}

Expression* CharacterLiteralExpression::clone() const {
    return new CharacterLiteralExpression(value, getLocation());
}

IntegerLiteralExpression::IntegerLiteralExpression(int i, const Location& loc) :
    LiteralExpression(LiteralExpression::Integer, new Type(Type::Integer), loc),
    value(i) {}

IntegerLiteralExpression::IntegerLiteralExpression(int i) :
    IntegerLiteralExpression(i, Location()) {}

Expression* IntegerLiteralExpression::clone() const {
    return new IntegerLiteralExpression(value, getLocation());
}

FloatLiteralExpression::FloatLiteralExpression(float f, const Location& loc) :
    LiteralExpression(LiteralExpression::Float, new Type(Type::Float), loc),
    value(f) {}

FloatLiteralExpression::FloatLiteralExpression(float f) :
    FloatLiteralExpression(f, Location()) {}

Expression* FloatLiteralExpression::clone() const {
    return new FloatLiteralExpression(value, getLocation());
}

StringLiteralExpression::StringLiteralExpression(
    const std::string& s,
    const Location& loc) :
    LiteralExpression(LiteralExpression::String, new Type(Type::Char), loc),
    value(s) {

    type->setArray(true);
}

Expression* StringLiteralExpression::clone() const {
    return new StringLiteralExpression(value, getLocation());
}

Expression* StringLiteralExpression::transform(Context& context) {
    Expression* transformedExpression = nullptr;
    if (context.isStringConstructorCall()) {
        transformedExpression = createCharArrayExpression(context);
    } else {
        MethodCallExpression* stringCtorCall = 
            new MethodCallExpression(Keyword::stringString, getLocation());
        stringCtorCall->addArgument(createCharArrayExpression(context));
        transformedExpression = new HeapAllocationExpression(stringCtorCall);
    }
    delete this;
    return transformedExpression;
}

Expression* StringLiteralExpression::createCharArrayExpression(
    Context& context) {
    const Location& location = getLocation();
    ArrayLiteralExpression* charArrayLiteral = new ArrayLiteralExpression(
        new Type(Type::Char), 
        location);
    for (std::string::const_iterator i = value.begin();
         i != value.end();
         i++) {
        charArrayLiteral->addElement(new CharacterLiteralExpression(*i,
                                                                    location));
    }
    return charArrayLiteral->transform(context);
}

BooleanLiteralExpression::BooleanLiteralExpression(
    bool b,
    const Location& loc) :
    LiteralExpression(LiteralExpression::Boolean, new Type(Type::Boolean), loc),
    value(b) {}

Expression* BooleanLiteralExpression::clone() const {
    return new BooleanLiteralExpression(value, getLocation());
}

ArrayLiteralExpression::ArrayLiteralExpression(const Location& loc) :
    LiteralExpression(LiteralExpression::Array, new Type(Type::Implicit), loc),
    elements() {}

ArrayLiteralExpression::ArrayLiteralExpression(Type* t, const Location& loc) : 
LiteralExpression(LiteralExpression::Array, t, loc),
    elements() {}

ArrayLiteralExpression::ArrayLiteralExpression(
    const ArrayLiteralExpression& other) :
    LiteralExpression(other),
    elements() {

    Utils::cloneList(elements, other.elements);
}

ArrayLiteralExpression* ArrayLiteralExpression::clone() const {
    return new ArrayLiteralExpression(*this);
}

Expression* ArrayLiteralExpression::transform(Context& context) {
    checkElements(context);

    IntegerLiteralExpression* capacity =
        new IntegerLiteralExpression(elements.size(), getLocation());
    ArrayAllocationExpression* arrayAllocation = 
        new ArrayAllocationExpression(type, capacity, getLocation());
    arrayAllocation->setInitExpression(this);
    return arrayAllocation;
}

void ArrayLiteralExpression::checkElements(Context& context) {
    const Type* commonType = nullptr;
    for (ExpressionList::iterator i = elements.begin();
         i != elements.end();
         i++) {
        *i = (*i)->transform(context);
        Expression* element = *i;
        Type* elementType = element->typeCheck(context);
        const Type* previousCommonType = commonType;
        commonType = Type::calculateCommonType(commonType, elementType);
        if (commonType == nullptr) {
            Trace::error("Array element types are not compatible. Previous "
                         "elements: " + previousCommonType->toString() +
                         ". This element: " + elementType->toString() + ".",
                         element);
        }
    }

    if (commonType != nullptr) {
        type = commonType->clone();
        type->setArray(true);
    }
}

NamedEntityExpression::NamedEntityExpression(const Identifier& i) :
    NamedEntityExpression(i, Location()) {}

NamedEntityExpression::NamedEntityExpression(
    const Identifier& i,
    const Location& loc) :
    Expression(Expression::NamedEntity, loc),
    identifier(i),
    binding(nullptr) {}

NamedEntityExpression* NamedEntityExpression::clone() const {
    return new NamedEntityExpression(identifier, getLocation());
}

bool NamedEntityExpression::resolve(Context& context) {
    if (binding == nullptr) {
        binding = context.lookup(identifier);
        if (binding == nullptr) {
            return false;
        }
    }
    return true;
}

Expression* NamedEntityExpression::transform(Context& context) {
    if (!resolve(context)) {
        Trace::error("Unknown identifier: " + identifier, this);
    }

    Expression* resolvedExpression = nullptr;
    switch (binding->getReferencedEntity()) { 
        case Binding::LocalObject: {
            Type* type = binding->getLocalObject()->getType();
            resolvedExpression =
                new LocalVariableExpression(type, identifier, getLocation());
            break;
        }
        case Binding::DataMember: {
            DataMemberDefinition* dataMember = 
                binding->getDefinition()->cast<DataMemberDefinition>();
            resolvedExpression = new DataMemberExpression(dataMember,
                                                          getLocation());
            break;
        }
        case Binding::Class: {
            ClassDefinition* classDef =
                binding->getDefinition()->cast<ClassDefinition>();
            resolvedExpression = new ClassNameExpression(classDef,
                                                         getLocation());
            break;
        }
        case Binding::Method:
            resolvedExpression = new MethodCallExpression(identifier,
                                                          getLocation());
            break;
        default:
            Trace::internalError("NamedEntityExpression::transform");
            break;
    }

    delete this;
    return resolvedExpression->transform(context);
}

Type* NamedEntityExpression::typeCheck(Context&) {
    // The NamedEntityExpression should have been transformed into another type
    // of expression by the transform() method.
    Trace::internalError("NamedEntityExpression::typeCheck");            
    return nullptr;
}

MethodCallExpression* NamedEntityExpression::getCall(
    Context& context,
    bool allowUnknownIdentifier) {

    if (!resolve(context)) {
        if (allowUnknownIdentifier) {
            return nullptr;
        }
        Trace::error("Unknown identifier: " + identifier, this);
    }

    switch (binding->getReferencedEntity()) {
        case Binding::Class:
        case Binding::Method: {
            MethodCallExpression* methodCall =
                new MethodCallExpression(identifier, getLocation());
            methodCall->tryResolveEnumConstructor(context);
            return methodCall;
        }
        default:
            break;
    }

    return nullptr;
}

bool NamedEntityExpression::isReferencingStaticDataMember(Context& context) {
    if (resolve(context) &&
        binding->getReferencedEntity() == Binding::DataMember) {
        DataMemberDefinition* dataMember =
            binding->getDefinition()->cast<DataMemberDefinition>();
        if (dataMember->isStatic()) {
            return true;
        }
    }
    return false;
}

bool NamedEntityExpression::isReferencingName(Expression* name) {
    switch (name->getExpressionType()) {
        case Expression::NamedEntity: {
            NamedEntityExpression* namedEntity =
                name->cast<NamedEntityExpression>();
            if (namedEntity->getIdentifier().compare(identifier) == 0) {
                return true;
            }
            break;
        }
        case Expression::LocalVariable: {
            LocalVariableExpression* local =
                name->cast<LocalVariableExpression>();
            if (local->getName().compare(identifier) == 0) {
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

LocalVariableExpression::LocalVariableExpression(
    Type* t, const Identifier& i, const Location& loc) : 
    Expression(Expression::LocalVariable, loc),
    identifier(i),
    hasTransformed(false) {

    type = t;
}

LocalVariableExpression::LocalVariableExpression(
    const LocalVariableExpression& other) :
    Expression(other),
    identifier(other.identifier),
    hasTransformed(other.hasTransformed) {}

Expression* LocalVariableExpression::clone() const {
    return new LocalVariableExpression(*this);
}

Expression* LocalVariableExpression::transform(Context& context) {
    const MethodDefinition* method = context.getMethodDefinition();
    if (method->getLambdaSignature() != nullptr && !hasTransformed) {
        // Transform the variable name to avoid name collisions with names in
        // methods that call this method. Since this method takes a lambda it
        // will be inlined in the calling method.
        identifier =
            Symbol::makeUnique(identifier,
                               context.getClassDefinition()->getName(),
                               method->getName());
        hasTransformed = true;
    }
    return this;
}

Type* LocalVariableExpression::typeCheck(Context&) {
    return type;
}

bool LocalVariableExpression::isVariable() const {
    return true;
}

Identifier LocalVariableExpression::generateVariableName() const {
    return identifier;
}

ClassNameExpression::ClassNameExpression(
    ClassDefinition* c,
    const Location& loc) :
    Expression(Expression::ClassName, loc), 
    classDefinition(c),
    hasTransformed(false) {

    type = &Type::voidType();
}

ClassNameExpression::ClassNameExpression(const ClassNameExpression& other) :
    Expression(other),
    classDefinition(other.classDefinition),
    hasTransformed(other.hasTransformed) {}

Expression* ClassNameExpression::clone() const {
    return new ClassNameExpression(*this);
}

Expression* ClassNameExpression::transform(Context& context) {
    if (hasTransformed) {
        return this;
    }

    if (ClassDefinition* outerClass = classDefinition->getEnclosingClass()) {
        // The referenced class is contained in an outer class.
        if (outerClass != context.getClassDefinition()) {
            const Location& loc = getLocation();
            ClassNameExpression* className = new ClassNameExpression(outerClass,
                                                                     loc);
            MemberSelectorExpression* memberSelector =
                new MemberSelectorExpression(className, this, loc);
            hasTransformed = true;
            return memberSelector->transform(context);
        }
    }

    return this;
}

MemberSelectorExpression::MemberSelectorExpression(
    Expression* l,
    Expression* r,
    const Location& loc) :
    Expression(Expression::MemberSelector, loc), 
    left(l),
    right(r) {}

MemberSelectorExpression::MemberSelectorExpression(
    Expression* l,
    Expression* r) :
    MemberSelectorExpression(l, r, Location()) {}

MemberSelectorExpression::MemberSelectorExpression(
    const Identifier& l,
    Expression* r) :
    MemberSelectorExpression(new NamedEntityExpression(l, Location()),
                             r,
                             Location()) {}

MemberSelectorExpression::MemberSelectorExpression(
    const Identifier& l,
    const Identifier& r) :
    MemberSelectorExpression(new NamedEntityExpression(l, Location()),
                             new NamedEntityExpression(r, Location()),
                             Location()) {}

MemberSelectorExpression::MemberSelectorExpression(
    const Identifier& l,
    const Identifier& r,
    const Location& loc) :
    MemberSelectorExpression(new NamedEntityExpression(l, Location()),
                             new NamedEntityExpression(r, Location()),
                             loc) {}

Expression* MemberSelectorExpression::clone() const {
    return new MemberSelectorExpression(left->clone(),
                                        right->clone(),
                                        getLocation());
}

MemberSelectorExpression* MemberSelectorExpression::transformMemberSelector(
    MemberSelectorExpression* memberSelector,
    Context& context) {

    Expression* transformedExpression = memberSelector->transform(context);
    if (transformedExpression->getExpressionType() !=
        Expression::MemberSelector) {
        Trace::internalError(
            "MemberSelectorExpression::transformMemberSelector");
    }
    return transformedExpression->cast<MemberSelectorExpression>();
}

MethodCallExpression* MemberSelectorExpression::getRhsCall(Context& context) {
    MethodCallExpression* retval = nullptr;

    left = left->transform(context);
    if (left->getExpressionType() != Expression::ClassName) {
        return retval;
    }

    ClassNameExpression* className = left->cast<ClassNameExpression>();
    Context::BindingsGuard guard(context,
                                 &(className->getClassDefinition()->
                                       getNameBindings()));

    switch (right->getExpressionType()) {
        case Expression::MemberSelector:
            retval =
                right->cast<MemberSelectorExpression>()->getRhsCall(context);
            break;
        case Expression::NamedEntity:
            retval = right->cast<NamedEntityExpression>()->getCall(context,
                                                                   false);
            break;
        case Expression::Member: {
            if (MethodCallExpression* methodCall =
                    right->dynCast<MethodCallExpression>()) {
                methodCall->tryResolveEnumConstructor(context);
                retval = methodCall;
            }
            break;
        }
        default:
            break;
    }

    return retval;
}

Expression* MemberSelectorExpression::transform(Context& context) {
    left = left->transform(context);

    Context::BindingsGuard guard(context, bindingScopeOfLeft(context));

    Type* arrayType = nullptr;
    Type* oldArrayType = context.getArrayType();
    if (left->getType()->isArray()) {
        arrayType = left->getType();
        context.setArrayType(arrayType);
    }

    right = right->transform(context);

    if (arrayType != nullptr) {
        context.setArrayType(oldArrayType);
    }

    switch (right->getExpressionType()) {
        case Expression::WrappedStatement:
            // Right-hand side has transformed itself into a block statement.
            // This can happen if the right-hand side was a method call to a
            // method that takes a lambda. Then the method that takes the lambda
            // is inlined here together with the lambda so the whole expression
            // statement (including this member selector expression) is
            // transformed into a block that contains the inlined code.
            return transformIntoBlockStatement(
                right->cast<WrappedStatementExpression>());
        case Expression::Temporary:
            // Right-hand side has transformed itself into a temporary variable.
            // This can happen if the right-hand side was a method call to a
            // method that returns a value and takes a lambda. Then the method
            // that takes the lambda is inlined at the location before the
            // current statement and the method call expression is transformed
            // into a temporary variable that holds the return value of the
            // inlined method.
            return transformIntoTemporaryExpression(
                right->cast<TemporaryExpression>());
        default:
            break;
    }

    type = right->typeCheck(context);
    return this;
}

NameBindings* MemberSelectorExpression::bindingScopeOfLeft(Context& context) {
    NameBindings* localBindings = nullptr;
    if (left->getExpressionType() == Expression::ClassName) {
        context.setIsStatic(true);
        ClassNameExpression* expr = left->cast<ClassNameExpression>();
        localBindings = &(expr->getClassDefinition()->getNameBindings());
    } else {
        context.setIsStatic(false);
        Type* resultingLeftType = left->typeCheck(context);
        Definition* definition = nullptr;
        if (resultingLeftType->isArray()) {
            definition = context.lookupType(BuiltInTypes::arrayTypeName);
        } else {
            definition = resultingLeftType->getDefinition();
            if (definition == nullptr) {
                resultingLeftType =
                    context.lookupConcreteType(resultingLeftType,
                                               getLocation());
                definition = resultingLeftType->getDefinition();
            }
        }
        localBindings =
            &(definition->cast<ClassDefinition>()->getNameBindings());
    }
    return localBindings;
}

WrappedStatementExpression*
MemberSelectorExpression::transformIntoBlockStatement(
    WrappedStatementExpression* wrappedBlockStatement) {

    if (wrappedBlockStatement->isInlinedArrayForEach() || 
        wrappedBlockStatement->isInlinedNonStaticMethod()) {
        Statement* statement = wrappedBlockStatement->getStatement();
        if (statement->getStatementType() != Statement::Block) {
            Trace::internalError(
                "MemberSelectorExpression::transformIntoBlockStatement");
        }
        if (wrappedBlockStatement->isInlinedArrayForEach()) {
            // Right-hand side was a method call to the each() method in the
            // array class. The whole expression statement (including the this
            // member selector expression) is transformed into a while loop.
            // We need to generate a variable declaration for a reference to the
            // array that the array.each() method is iterating over.
            generateThisPointerDeclaration(statement->cast<BlockStatement>(),
                                           arrayReferenceName);
        }
        else if (wrappedBlockStatement->isInlinedNonStaticMethod()) {
            // Right-hand side was a method call to an non-static inlined method
            // that takes a lambda. We Need to generate a variable declaration
            // for a 'this' pointer that points to the object on which we called
            // the method.
            generateThisPointerDeclaration(statement->cast<BlockStatement>(),
                                           thisPointerName);
        }
    }

    left = nullptr;
    right = nullptr;
    delete this;
    return wrappedBlockStatement;
}

TemporaryExpression* MemberSelectorExpression::transformIntoTemporaryExpression(
    TemporaryExpression* temporary) {

    BlockStatement* nonStaticInlinedMethodBody =
        temporary->getNonStaticInlinedMethodBody();
    if (nonStaticInlinedMethodBody != nullptr) {
        // Right-hand side was a method call to an non-static inlined method
        // that takes a lambda. We Need to generate a variable declaration for a
        // 'this' pointer that points to the object on which we called the
        // method.
        generateThisPointerDeclaration(nonStaticInlinedMethodBody,
                                       thisPointerName);
    }
    return temporary;
}

void MemberSelectorExpression::generateThisPointerDeclaration(
    BlockStatement* block,
    const Identifier& thisPointerIdentifier) {

    const Location& location = getLocation();
    VariableDeclarationStatement* thisPointerDeclaration = nullptr;
    Statement* firstStatement = *(block->getStatements().begin());
    if (firstStatement->getStatementType() == Statement::VarDeclaration) {
        VariableDeclarationStatement* variableDeclaration = 
            firstStatement->cast<VariableDeclarationStatement>();
        if (variableDeclaration->getIdentifier().
            compare(thisPointerIdentifier) == 0) {
            thisPointerDeclaration = variableDeclaration;
        }
    } 
    if (thisPointerDeclaration != nullptr) {
        // First statement in the block is a 'this' pointer declaration. Modify
        // the init expression in the declaration to include the left side of
        // this MemberSelectorExpression.
        Expression* initExpression =
            thisPointerDeclaration->getInitExpression();
        Expression* modifiedInitExpression = 
            new MemberSelectorExpression(left, initExpression, getLocation());
        thisPointerDeclaration->setInitExpression(modifiedInitExpression);
    } else {
        thisPointerDeclaration = new VariableDeclarationStatement(
            new Type(Type::Implicit), 
            thisPointerIdentifier, 
            left,
            location);
        block->insertStatementAtFront(thisPointerDeclaration);
    }
}

Type* MemberSelectorExpression::typeCheck(Context&) {
    if (type == nullptr) {
        // Type should have been calculated in the transform() method.
        Trace::internalError("MemberSelectorExpression::typeCheck");    
    }
    return type;
}

Identifier MemberSelectorExpression::generateVariableName() const {
    return left->generateVariableName() + "_" + right->generateVariableName();
}

Expression::ExpressionType
MemberSelectorExpression::getRightmostExpressionType() const {
    return right->getRightmostExpressionType();
}

MemberExpression::MemberExpression(
    MemberExpressionType t, 
    ClassMemberDefinition* m, 
    const Location& loc) :
    Expression(Expression::Member, loc),
    memberDefinition(m),
    memberExpressionType(t),
    hasTransformedIntoMemberSelector(false) {}

MemberExpression::MemberExpression(const MemberExpression& other) :
    Expression(other),
    memberDefinition(other.memberDefinition),
    memberExpressionType(other.memberExpressionType),
    hasTransformedIntoMemberSelector(other.hasTransformedIntoMemberSelector) {}

Expression* MemberExpression::transformIntoMemberSelector(Context& context) {
    if (hasTransformedIntoMemberSelector) {
        // We have already transformed into a 'this' pointer expression.
        return this;
    }

    if (context.getClassLocalNameBindings() != nullptr) {
        // This means we have an expression like "obj.member", i.e. we access
        // the member through 'obj' rather than an explicit this pointer.
        return this;
    }

    const ClassDefinition* memberClass = memberDefinition->getEnclosingClass();
    if (context.getClassDefinition() == memberClass) {
        // The MemberExpression refers to a member of the same class as the
        // current method. This means we have implicit access to the 'this'
        // pointer.
        return this;
    }
    if (memberClass != nullptr &&
        context.getClassDefinition()->isSubclassOf(memberClass)) {
        // The MemberExpression refers to a member of a parent class of the
        // current class. This means we have implicit access to the 'this'
        // pointer even though the BlockStatement may suggest otherwise.
        return this;
    }

    const Location& location = getLocation();
    Expression* left = nullptr;
    if (memberDefinition->isStatic()) {
        // The MemberExpression refences a static member. Transform this
        // DataMemberExpression into a MemberSelectorExpression of this kind:
        // 'ClassName.member'.
        left = new NamedEntityExpression(memberClass->getFullName(), location);
    } else {
        // The MemberExpression refences a non-static member and the 'this'
        // pointer is not implicit. Transform this DataMemberExpression into a
        // MemberSelectorExpression in order to explicitly reference the member
        // through the 'this' pointer: 'this.member'.
        left = new NamedEntityExpression(thisPointerName, location);
    }
    MemberSelectorExpression* memberSelector =
        new MemberSelectorExpression(left, this, location);
    hasTransformedIntoMemberSelector = true;
    return memberSelector->transform(context);
}

void MemberExpression::accessCheck(Context& context) {
    if (context.isStatic() && !memberDefinition->isStatic()) {
        if (memberDefinition->getMemberType() ==
            ClassMemberDefinition::Method) {
            MethodDefinition* method =
                memberDefinition->cast<MethodDefinition>();
            if (!method->isConstructor()) {
                Trace::error(
                    "Cannot access a non-static method from a static context.",
                    this);
            }
        } else {
            Trace::error(
                "Cannot access a non-static member from a static context.",
                this);
        }
    }
    // TODO: perform access level checks such as accessing private member
    // from outside class. Class of the caller we get from the context.
    // The context just needs a pointer to Method and we get the class from the 
    // method. Class of the called method we get from 
    // binding->getMethodDefinition()->getEnclosingDefinition().
}

DataMemberExpression::DataMemberExpression(
    DataMemberDefinition* d,
    const Location& loc) :
    MemberExpression(MemberExpression::DataMember, d, loc) {

    type = d->getType();
}

DataMemberExpression::DataMemberExpression(const DataMemberExpression& other) :
    MemberExpression(other) {}

Expression* DataMemberExpression::clone() const {
    return new DataMemberExpression(*this);
}

Expression* DataMemberExpression::transform(Context& context) {
    if (context.isConstructorCallStatement()) {
        // In constructors, uninitialized data must not be passed to super class
        // costructor like this: 'class Derived(int member): Base(member)'.
        // 'class Derived(int member)' is actually
        // 'class Derived(int member_Arg)' so transform 'Base(member)' into:
        // 'Base(member_Arg)'.
        return transformIntoConstructorArgumentReference(context);
    } else {
        return transformIntoMemberSelector(context);
    }
}

Type* DataMemberExpression::typeCheck(Context& context) {
    accessCheck(context);
    return type;
}

Expression* DataMemberExpression::transformIntoConstructorArgumentReference(
    Context& context) {

    Expression* argument =
        new NamedEntityExpression(memberDefinition->getName() + "_Arg",
                                  getLocation());
    return argument->transform(context);
}

bool DataMemberExpression::isVariable() const {
    return true;
}

Identifier DataMemberExpression::generateVariableName() const {
    return getName();
}

const Identifier& DataMemberExpression::getName() const {
    return memberDefinition->getName();
}

MethodCallExpression::MethodCallExpression(
    const Identifier& n,
    const Location& l) :
    MemberExpression(MemberExpression::MethodCall, nullptr, l),
    name(n),
    lambda(nullptr),
    isCtorCall(false),
    inferredConcreteType(nullptr) {}

MethodCallExpression::MethodCallExpression(const Identifier& n) :
    MethodCallExpression(n, Location()) {}

MethodCallExpression::MethodCallExpression(const MethodCallExpression& other) :
    MemberExpression(other),
    name(other.name),
    arguments(),
    lambda(other.lambda ? other.lambda->clone() : nullptr),
    isCtorCall(other.isCtorCall),
    inferredConcreteType(other.inferredConcreteType ?
                         other.inferredConcreteType->clone() : nullptr) {

    Utils::cloneList(arguments, other.arguments);
}

MethodCallExpression* MethodCallExpression::clone() const {
    return new MethodCallExpression(*this);
}

void MethodCallExpression::addArgument(const Identifier& argument) {
    arguments.push_back(new NamedEntityExpression(argument, Location()));
}

MethodDefinition* MethodCallExpression::getEnumCtorMethodDefinition() const {
    if (memberDefinition != nullptr) {
        MethodDefinition* methodDef =
            memberDefinition->cast<MethodDefinition>();
        if (methodDef->isEnumConstructor()) {
            return methodDef;
        }
    }
    return nullptr;
}

void MethodCallExpression::tryResolveEnumConstructor(Context& context) {
    const Binding* binding = context.lookup(name);
    if (binding != nullptr &&
        binding->getReferencedEntity() == Binding::Method) {

        const Binding::MethodList& candidates = binding->getMethodList();
        assert(!candidates.empty());
        MethodDefinition* methodDefinition = candidates.front();
        if (methodDefinition->isEnumConstructor()) {
            memberDefinition = methodDefinition;
        }
    }
}

void MethodCallExpression::setIsConstructorCall() {
    isCtorCall = true;
    name += "_" + Keyword::initString;
}

MethodCallExpression* MethodCallExpression::transformMethodCall(
    MethodCallExpression* methodCall,
    Context& context) {

    Expression* transformedExpression = methodCall->transform(context);
    return transformedExpression->dynCast<MethodCallExpression>();
}

void MethodCallExpression::resolve(Context& context) {
    Context subcontext(context);
    subcontext.reset();
    TypeList argumentTypes;
    resolveArgumentTypes(argumentTypes, subcontext);

    const Binding::MethodList& candidates = resolveCandidates(context);
    findCompatibleMethod(candidates, argumentTypes);

    if (memberDefinition == nullptr) {
        bool candidateClassIsGeneric = false;
        for (Binding::MethodList::const_iterator i = candidates.begin();
             i != candidates.end();
             i++) {
            MethodDefinition* candidate = *i;
            if (candidate->getClass()->isGeneric()) {
                candidateClassIsGeneric = true;
                if (candidate->isConstructor() || candidate->isStatic()) {
                    resolveByInferringConcreteClass(candidate,
                                                    argumentTypes,
                                                    subcontext);
                    if (memberDefinition != nullptr) {
                        return;
                    }
                }
            }
        }
        if (candidateClassIsGeneric) {
            Trace::error("Can not infer concrete type: " + name, this);
        }
        reportError(argumentTypes, candidates);
    }
}

void MethodCallExpression::resolveByInferringConcreteClass(
    const MethodDefinition* candidate,
    const TypeList& argumentTypes,
    Context& context) {

    if (candidate->getArgumentList().size() != argumentTypes.size()) {
        return;
    }

    Type* concreteType = inferConcreteType(candidate, argumentTypes, context);
    if (concreteType == nullptr) {
        return;
    }

    if (candidate->isConstructor()) {
        setConstructorCallName(concreteType);
    }
    ClassDefinition* concreteClass =
        concreteType->getDefinition()->cast<ClassDefinition>();

    Context::BindingsGuard guard(context, &(concreteClass->getNameBindings()));

    const Binding::MethodList& candidates = resolveCandidates(context);
    findCompatibleMethod(candidates, argumentTypes);
    if (memberDefinition != nullptr) {
        inferredConcreteType = concreteType;
    }
}

Type* MethodCallExpression::inferConcreteType(
    const MethodDefinition* candidate,
    const TypeList& argumentTypes,
    Context& context) {

    const ArgumentList& candidateArgumentList = candidate->getArgumentList();
    const Identifier& candidateClassName = candidate->getClass()->getName();

    if (candidateArgumentList.size() > 0) {
        // Infer concrete class by matching the types of the arguments in the
        // call expression with the argument types in the candidate signature
        // that are generic type parameters.
        Type* concreteType = Type::create(candidateClassName);
        ArgumentList::const_iterator i = candidateArgumentList.begin();
        TypeList::const_iterator j = argumentTypes.begin();
        while (i != candidateArgumentList.end()) {
            VariableDeclaration* candidateArgument = *i;
            Type* argumentType = *j;
            Type* candidateArgumentType = candidateArgument->getType();
            Definition* candidateArgumentTypeDef =
                candidateArgumentType->getDefinition();
            if (candidateArgumentTypeDef->isGenericTypeParameter()) {
                // TODO: Must add the generic type parameter in the correct
                // order so that the type parameter list in 'concreteType'
                // matches the list of generic type parameter definitions in
                // 'candidateClass'.
                concreteType->addGenericTypeParameter(argumentType->clone());
            } else {
                if (!Type::areInitializable(candidateArgumentType,
                                            argumentType)) {
                    return nullptr;
                }
            }
            i++;
            j++;
        }
        if (concreteType->hasGenericTypeParameters()) {
            return context.lookupConcreteType(concreteType, getLocation());
        }
        return nullptr;
    } else {
        // The candidate method has no arguments.
        if (candidate->isEnumConstructor()) {
            // The enumeration is a generic and the variant constructor takes no
            // data. This variant constructor is in a concrete class named
            // [EnumName]<_>. The class [EnumName]<_> is implicitly convertable
            // to [EnumName].
            Type* convertableEnumType = Type::create(candidateClassName);
            convertableEnumType->addGenericTypeParameter(
                new Type(Type::Placeholder));
            return context.lookupConcreteType(convertableEnumType,
                                              getLocation());
        }
        return nullptr;
    }
}

const Binding::MethodList& MethodCallExpression::resolveCandidates(
    Context& context) {

    const Binding* binding = context.lookup(name);
    if (binding == nullptr) {
        Trace::error("Unknown method: " + name, this);
    }
    if (binding->getReferencedEntity() != Binding::Method) {
        Trace::error("Not a method: " + name, this);
    }

    return binding->getMethodList();
}

void MethodCallExpression::findCompatibleMethod(
    const Binding::MethodList& candidates,
    const TypeList& argumentTypes) {

    for (Binding::MethodList::const_iterator i = candidates.begin();
         i != candidates.end();
         i++) {
        MethodDefinition* candidate = *i;
        if (candidate->isCompatible(argumentTypes) &&
            !candidate->getClass()->isGeneric()) {
            memberDefinition = candidate;
            return;
        }
    }
}

void MethodCallExpression::setConstructorCallName(Type* allocatedObjectType) {
    if (allocatedObjectType->hasGenericTypeParameters()) {
        // Need to update the name of the constructor call to match the name of
        // the constructed concrete class which includes the concrete type
        // parameters.
        name = allocatedObjectType->getFullConstructedName();
    }
    setIsConstructorCall();
}

Expression* MethodCallExpression::transform(Context& context) {
    MethodDefinition* methodDefinition = nullptr;
    {
        Context::BindingsGuard guard(context);

        if (memberDefinition == nullptr) {
            resolve(context);
        }
        accessCheck(context);
        methodDefinition = memberDefinition->cast<MethodDefinition>();
        type = methodDefinition->getReturnType();

        if (isBuiltInArrayMethod()) {
            // Special treatment for built-in array methods that have
            // placeholder types in their signatures.
            checkBuiltInArrayMethodPlaceholderTypes(context);
        }

        if (!methodDefinition->isFunction()) {
            // Depending on the context, calls to methods may need to be
            // transformed from 'method()' into 'ClassName.method()' or
            // 'this.method()'.
            Expression* transformed = transformIntoMemberSelector(context);
            if (transformed != this) {
                // This expression has been transformed into a
                // MemberSelectorExpression.
                return transformed;
            }
        }
        // Reset the context since a MethodCallExpression means the end of a
        // chain of MemberSelectorExpressions. Example: 'a.b.method()'. The
        // BindingsGuard resets the context.
    }

    if (lambda != nullptr) {
        LambdaSignature* lambdaSignature =
            methodDefinition->getLambdaSignature();
        lambda->setLambdaSignature(lambdaSignature);
        if (lambda->getArguments().size() !=
            lambdaSignature->getArguments().size()) {
            Trace::error("Wrong number of arguments in lambda expression.",
                         this);
        }

        if (isBuiltInArrayMethod() &&
            name.compare(BuiltInTypes::arrayEachMethodName) == 0) {
            // For special array.each() lambda: transform this
            // MethodCallExpression into a block statement with a while loop
            // containing the lambda.
            return transformIntoWhileStatement(context);
        } else {
            // For normal lambda: Clone and inline the called method and lambda
            // in the calling method.
            return inlineCalledMethod(context);
        }
    }
    return this;
}

bool MethodCallExpression::isBuiltInArrayMethod() {
    MethodDefinition* methodDefinition =
        memberDefinition->cast<MethodDefinition>();
    ClassDefinition* classDef = methodDefinition->getEnclosingClass();
    return classDef->getName().compare(BuiltInTypes::arrayTypeName) == 0;
}

void MethodCallExpression::checkBuiltInArrayMethodPlaceholderTypes(
    Context& context) {

    // Some of the methods in the built-in array class has arguments and return
    // values of placeholder type. These arguments and return values need to be
    // checked here so that they match the type of the array. '_' denotes the
    // placeholder type:
    //
    //     append(_ element)
    //     appendAll(_[] array)
    //     _[] concat(_[] array)
    //     _[] slice(int begin, int end)
    //
    Type* arrayType = context.getArrayType();
    if (name.compare(BuiltInTypes::arrayAppendAllMethodName) == 0 ||
        name.compare(BuiltInTypes::arrayAppendMethodName) == 0) {
        checkArrayAppend(arrayType);
    } else if (name.compare(BuiltInTypes::arrayConcatMethodName) == 0) {
        checkArrayConcatenation(arrayType);
    } else if (name.compare(BuiltInTypes::arraySliceMethodName) == 0) {
        // Change the return type to the type of the array.
        type = arrayType->clone();
    }
}

void MethodCallExpression::checkArrayAppend(const Type* arrayType) {
    assert(arrayType->isArray());

    Expression* argument = arguments.front();
    const Type* argumentType = argument->getType();

    if (arrayType->isConstant()) {
        Trace::error("Cannot change the value of a constant.",
                     arrayType,
                     argumentType,
                     this);
    }

    if (name.compare(BuiltInTypes::arrayAppendAllMethodName) == 0) {
        if (!Type::areEqualNoConstCheck(arrayType, argumentType)) {
            Trace::error("Cannot append arrays of different types.",
                         arrayType,
                         argumentType,
                         this);
        }
    } else {
        std::auto_ptr<Type>
            elementType(Type::createArrayElementType(arrayType));
        if (!Type::isAssignableByExpression(elementType.get(), argument)) {
            Trace::error("Cannot append data of incompatible type to array.",
                         arrayType,
                         argumentType,
                         this);
        }
    }
}

void MethodCallExpression::checkArrayConcatenation(const Type* arrayType) {
    assert(arrayType->isArray());

    const Type* argumentType = arguments.front()->getType();

    if (!argumentType->isArray()) {
        Trace::error("Right-hand side must be an array.",
                     arrayType,
                     argumentType,
                     this);
    }
    if (!Type::areEqualNoConstCheck(arrayType, argumentType)) {
        Trace::error("Cannot concatenate arrays of different types.",
                     arrayType,
                     argumentType,
                     this);
    }

    // Change the return type to the type of the array.
    type = arrayType->clone();
}

void MethodCallExpression::reportError(
    const TypeList& argumentTypes,
    const Binding::MethodList& candidates) {

    std::string methodName(name);
    if (isConstructorCall()) {
        methodName[methodName.size() - Keyword::initString.size() - 1] = '.';
    }
    std::string error("Method argument mismatch: " + methodName + "(");
    bool addComma = false;
    for (TypeList::const_iterator i = argumentTypes.begin();
         i != argumentTypes.end();
         i++) {
        if (addComma) {
            error += ", ";
        }
        Type* argumentType = *i;
        error += argumentType->toString();
        addComma = true;
    }
    error += ")\nCandidates are:\n";
    for (Binding::MethodList::const_iterator i = candidates.begin();
         i != candidates.end();
         i++) {
        MethodDefinition* candidate = *i;
        error += candidate->toString() + "\n";
    }
    Trace::error(error, this);
}

Expression* MethodCallExpression::inlineCalledMethod(Context& context) {
    MethodDefinition* calledMethod = memberDefinition->cast<MethodDefinition>();

    if (!calledMethod->hasBeenProcessedBefore()) {
        // The inlined method must be processed in order to make the local
        // variable names unique.
        calledMethod->process();
    }

    BlockStatement* clonedBody = calledMethod->getBody()->clone();
    clonedBody->setEnclosingBlock(context.getBlock());
    addArgumentsToInlinedMethodBody(clonedBody);

    // Save the lambda in the context so that the YieldExpression can transform
    // itself into the lambda block when the transform() pass is executed on the
    // inlined code.
    context.setLambdaExpression(lambda);

    if (calledMethod->getReturnType()->isVoid()) {
        // The called method does not return anything so we can simply replace
        // method call expression with the code of the inlined called method.
        WrappedStatementExpression* wrappedBlockStatement =
            new WrappedStatementExpression(clonedBody, getLocation());
        if (!calledMethod->isStatic()) {
            wrappedBlockStatement->setInlinedNonStaticMethod(true);
        }
        lambda = nullptr;
        delete this;
        return wrappedBlockStatement;
    } else {
        // The called method returns a value. Inline the method at the location
        // right before the current statement and transform this method call
        // into a reference to the temporary that holds the return value.
        TemporaryExpression* temporary =
            inlineMethodWithReturnValue(clonedBody, calledMethod, context);
        lambda = nullptr;
        delete this;
        return temporary;
    }
}

void MethodCallExpression::addArgumentsToInlinedMethodBody(
    BlockStatement* clonedBody) {

    const Location& location = getLocation();
    MethodDefinition* calledMethod =
        memberDefinition->cast<MethodDefinition>();
    const ArgumentList& argumentsFromSignature =
        calledMethod->getArgumentList();
    ArgumentList::const_iterator i = argumentsFromSignature.begin();
    ExpressionList::iterator j = arguments.begin();

    while (j != arguments.end()) {
        VariableDeclaration* signatureArgument = *i;
        Expression* argumentExpression = *j;
        VariableDeclarationStatement* argumentDeclaration =
            new VariableDeclarationStatement(signatureArgument->getType(),
                                             signatureArgument->getIdentifier(),
                                             argumentExpression,
                                             location);

        // Since the inlined method takes a lambda, the arguments names have
        // already been changed into being unique during processing of the
        // method. Therefore, flag the variable name as unique so that we do not
        // change the name again when the variable declaration is type checked.
        argumentDeclaration->setIsNameUnique(true);

        clonedBody->insertStatementAtFront(argumentDeclaration);
        j++;
        i++;
    }
}

TemporaryExpression*
MethodCallExpression::inlineMethodWithReturnValue(
    BlockStatement* clonedMethodBody,
    MethodDefinition* calledMethod,
    Context& context) {

    const Location& location = getLocation();
    Type* returnType = calledMethod->getReturnType()->clone();
    BlockStatement* currentBlock = context.getBlock();

    // Remove any write-protection from the temporary return variable so that it
    // can be assigned a value in the inlined code.
    returnType->setConstant(false);

    // Insert the declaration of the temporary that holds the return value.
    VariableDeclarationStatement* retvalTmpDeclarationStatement =
        VariableDeclarationStatement::generateTemporary(returnType,
                                                        retvalName,
                                                        nullptr,
                                                        location);
    currentBlock->insertBeforeCurrentStatement(retvalTmpDeclarationStatement);
    VariableDeclaration* retvalTmpDeclaration =
        retvalTmpDeclarationStatement->getDeclaration();

    // Store pointer to the return value declaration so that return statements
    // in the inlined code can be transformed into assignments to the return
    // value.
    context.setTemporaryRetvalDeclaration(retvalTmpDeclaration);

    // Inline the called method body.
    currentBlock->insertBeforeCurrentStatement(clonedMethodBody);

    TemporaryExpression* temporary =
        new TemporaryExpression(retvalTmpDeclaration, location);
    if (calledMethod->isStatic()) {
        clonedMethodBody->typeCheck(context);
        context.setTemporaryRetvalDeclaration(nullptr);
    } else {
        // We cannot run the type-check pass on the inlined code yet since the
        // inlined method could have bene called through a member selector
        // expression 'a.method(...)' and the inlined code would then not have
        // access to the non-static members in the 'a' object. The member
        // selector needs to insert a 'this' pointer declaration in the inlined
        // code. To request the member selector to do this, the inlined method
        // body is included in the returned temporary.
        temporary->setNonStaticInlinedMethodBody(clonedMethodBody);
    }

    return temporary;
}

WrappedStatementExpression* MethodCallExpression::transformIntoWhileStatement(
    Context& context) {

    const Location& location = getLocation();

    BlockStatement* outerBlock = new BlockStatement(
        context.getClassDefinition(), 
        context.getBlock(), 
        location);

    Type* indexVariableType = new Type(Type::Integer);
    indexVariableType->setConstant(false);
    VariableDeclarationStatement* indexDeclaration =
        new VariableDeclarationStatement(
            indexVariableType,
            indexVaraibleName,
            new IntegerLiteralExpression(0, location),
            location);
    outerBlock->addStatement(indexDeclaration);

    BinaryExpression* whileCondition = new BinaryExpression(
        Operator::Less,
        new NamedEntityExpression(indexVaraibleName, location),
        new MemberSelectorExpression(
            new NamedEntityExpression(arrayReferenceName, location),
            new MethodCallExpression(BuiltInTypes::arrayLengthMethodName,
                                     location),
            location),
        location);

    BlockStatement* whileBlock = new BlockStatement(
        context.getClassDefinition(), 
        outerBlock, 
        location);

    BlockStatement* lambdaBlock = addLamdaArgumentsToLambdaBlock(
        whileBlock, 
        indexVaraibleName, 
        arrayReferenceName);
    whileBlock->insertStatementAtFront(lambdaBlock);

    BinaryExpression* increaseIndex = new BinaryExpression(
        Operator::Assignment,
        new NamedEntityExpression(indexVaraibleName, location),
        new BinaryExpression(
            Operator::Addition,
            new NamedEntityExpression(indexVaraibleName, location),
            new IntegerLiteralExpression(1, location),
            location),
        location);
    whileBlock->addStatement(increaseIndex);

    WhileStatement* whileStatement =
        new WhileStatement(whileCondition, whileBlock, location);
    outerBlock->addStatement(whileStatement);
    WrappedStatementExpression* wrappedBlockStatement = 
            new WrappedStatementExpression(outerBlock, location);
    wrappedBlockStatement->setInlinedArrayForEach(true);
    lambda = nullptr;
    delete this;
    return wrappedBlockStatement;
}

BlockStatement* MethodCallExpression::addLamdaArgumentsToLambdaBlock(
    BlockStatement* whileBlock,
    const Identifier& indexVaraibleName,
    const Identifier& arrayName) {

    const Location& location = getLocation();
    BlockStatement* lambdaBlock = lambda->getBlock();
    lambdaBlock->setEnclosingBlock(whileBlock);

    const VariableDeclarationStatementList& lambdaArguments =
        lambda->getArguments();
    VariableDeclarationStatement* elementArgument = *lambdaArguments.begin();

    ArraySubscriptExpression* arraySubscript = new ArraySubscriptExpression(
        new NamedEntityExpression(arrayName, location), 
        new NamedEntityExpression(indexVaraibleName, location));

    elementArgument->setInitExpression(arraySubscript);
    lambdaBlock->insertStatementAtFront(elementArgument);

    return lambdaBlock;
}

Type* MethodCallExpression::typeCheck(Context&) {
    if (type == nullptr) {
        // Type should have been calculated in the transform() method.
        Trace::internalError("MethodCallExpression::typeCheck");    
    }
    return type;
}

void MethodCallExpression::resolveArgumentTypes(
    TypeList& typeList, 
    Context& context) {

    if (isConstructorCall() &&
        name.compare(Keyword::stringString + "_" + Keyword::initString) == 0) {
        context.setIsStringConstructorCall(true);
    }
    for (ExpressionList::iterator i = arguments.begin(); 
         i != arguments.end();
         i++) {
        *i = (*i)->transform(context);
        Expression* expression = *i;
        typeList.push_back(expression->typeCheck(context));
    }
    context.setIsStringConstructorCall(false);
    if (lambda != nullptr) {
        typeList.push_back(new Type(Type::Lambda));
    }
}

HeapAllocationExpression::HeapAllocationExpression(
    MethodCallExpression* m) :
    HeapAllocationExpression(Type::create(m->getName()), m) {}

HeapAllocationExpression::HeapAllocationExpression(
    Type* t, 
    MethodCallExpression* m) :
    Expression(Expression::HeapAllocation, m->getLocation()),
    allocatedObjectType(t),
    classDefinition(nullptr),
    constructorCall(m),
    processName(nullptr) {}

HeapAllocationExpression::HeapAllocationExpression(
    const HeapAllocationExpression& other) :
    Expression(other),
    allocatedObjectType(other.allocatedObjectType->clone()),
    classDefinition(other.classDefinition),
    constructorCall(other.constructorCall->clone()),
    processName(other.processName) {}

Expression* HeapAllocationExpression::clone() const {
    return new HeapAllocationExpression(*this);
}

Expression* HeapAllocationExpression::transform(Context& context) {
    if (type != nullptr) {
        return this;
    }

    classDefinition = lookupClass(context);
    if (classDefinition->isProcess() && classDefinition->isInterface() &&
        classDefinition->isGenerated()) {
        // The type of the object is generated process interface. Transform the
        // HeapAllocationExpression:
        //     new ProcessType
        // into:
        //     (ProcessType) new ProcessType_Proxy(name)
        const Location& loc = getLocation();
        MethodCallExpression* proxyConstructorCall =
            new MethodCallExpression(classDefinition->getName() + "_Proxy",
                                     loc);
        if (processName != nullptr) {
            processName = processName->transform(context);
            if (!processName->typeCheck(context)->isString()) {
                Trace::error("Process name must be of string type.", this);
            }
            proxyConstructorCall->addArgument(processName);
        }

        return new TypeCastExpression(
            allocatedObjectType->clone(),
            new HeapAllocationExpression(proxyConstructorCall),
            loc);
    }

    return this;
}

Type* HeapAllocationExpression::typeCheck(Context& context) {
    if (type != nullptr) {
        return type;
    }
    if (classDefinition == nullptr) {
        classDefinition = lookupClass(context);
    }

    NameBindings& bindingsForConstructor = classDefinition->getNameBindings();
    Context::BindingsGuard guard(context, &bindingsForConstructor);

    // All expressions must be transformed before calling typeCheck() even if we
    // don't expect the type of expression to change from MethodCallExpression. 
    constructorCall = MethodCallExpression::transformMethodCall(constructorCall,
                                                                context);
    constructorCall->typeCheck(context);

    Type* inferredConcreteType = constructorCall->getInferredConcreteType();
    if (inferredConcreteType != nullptr) {
        type = inferredConcreteType;
    } else {
        type = allocatedObjectType;
    }
    return type;
}

ClassDefinition* HeapAllocationExpression::lookupClass(Context& context) {
    allocatedObjectType = context.lookupConcreteType(allocatedObjectType,
                                                     getLocation());
    if (!allocatedObjectType->isReference()) {
        Trace::error("Only objects of reference type can be allocated using the"
                     " 'new' keyword. Allocated object type: " +
                     allocatedObjectType->toString(),
                     this);
    }

    constructorCall->setConstructorCallName(allocatedObjectType);
    Definition* definition = allocatedObjectType->getDefinition();
    assert(definition->isClass());
    return definition->cast<ClassDefinition>();
}

ArrayAllocationExpression::ArrayAllocationExpression(
    Type* t,
    Expression* c,
    const Location& l) :
    Expression(Expression::ArrayAllocation, l),
    arrayType(t),
    arrayCapacityExpression(c),
    initExpression(nullptr) {}

ArrayAllocationExpression::ArrayAllocationExpression(
    const ArrayAllocationExpression& other) :
    Expression(other),
    arrayType(other.arrayType->clone()),
    arrayCapacityExpression(other.arrayCapacityExpression ?
                            other.arrayCapacityExpression->clone() :
                            nullptr),
    initExpression(other.initExpression ?
                   other.initExpression->clone() :
                   nullptr) {}

Expression* ArrayAllocationExpression::clone() const {
    return new ArrayAllocationExpression(*this);
}

Type* ArrayAllocationExpression::typeCheck(Context& context) {
    if (type != nullptr) {
        return type;
    }

    if (arrayCapacityExpression != nullptr) {
        arrayCapacityExpression = arrayCapacityExpression->transform(context);
        Type* capacityExpressionReturnType =
            arrayCapacityExpression->typeCheck(context);
        if (!capacityExpressionReturnType->isIntegerNumber()) {
            Trace::error("Array capacity must be of integer type.", this);
        }
    }

    // Lookup the array type.
    arrayType = context.lookupConcreteType(arrayType, getLocation());

    type = arrayType; 
    type->setArray(true);
    return type;
}

ArraySubscriptExpression::ArraySubscriptExpression(
    Expression* n,
    Expression* i) :
    Expression(Expression::ArraySubscript, n->getLocation()),
    arrayNameExpression(n),
    indexExpression(i) {}

Expression* ArraySubscriptExpression::clone() const {
    return new ArraySubscriptExpression(arrayNameExpression->clone(),
                                        indexExpression->clone());
}

Expression* ArraySubscriptExpression::transform(Context& context) {
    {
        Context::BindingsGuard guard(context);

        arrayNameExpression = arrayNameExpression->transform(context);
        Type* arrayType = arrayNameExpression->typeCheck(context);
        if (!arrayType->isArray())
        {
            Trace::error("Not an array.", this);
        }
        type = Type::createArrayElementType(arrayType);
    }

    indexExpression = indexExpression->transform(context);
    Type* indexExpressionReturnType = indexExpression->typeCheck(context);
    if (!indexExpressionReturnType->isIntegerNumber()) {
        Trace::error("Array index must be of integer type.", this);
    }

    if (BinaryExpression* binExpression =
            indexExpression->dynCast<BinaryExpression>()) {
        if (binExpression->getOperator() == Operator::Range) {
            MemberSelectorExpression* transformedExpression =
                createSliceMethodCall(binExpression, context);
            delete this;
            return transformedExpression;
        }
    }

    return this;
}

Type* ArraySubscriptExpression::typeCheck(Context&) {
    if (type == nullptr) {
        // Type should have been calculated in the transform() method.
        Trace::internalError("ArraySubscriptExpression::typeCheck");
    }
    return type;
}

MemberSelectorExpression* ArraySubscriptExpression::createSliceMethodCall(
    BinaryExpression* range,
    Context& context) {

    MethodCallExpression* sliceCall =
        new MethodCallExpression(BuiltInTypes::arraySliceMethodName,
                                 getLocation());
    sliceCall->addArgument(range->getLeft());
    sliceCall->addArgument(range->getRight());

    MemberSelectorExpression* memberSelector =
        new MemberSelectorExpression(arrayNameExpression,
                                     sliceCall,
                                     getLocation());
    return MemberSelectorExpression::transformMemberSelector(memberSelector,
                                                             context);
}

TypeCastExpression::TypeCastExpression(
    Type* target,
    Expression* o,
    const Location& l) :
    Expression(Expression::TypeCast, l),
    targetType(target),
    operand(o),
    staticCast(false),
    isGenerated(true) {}

TypeCastExpression::TypeCastExpression(Type* target, Expression* o) :
    TypeCastExpression(target, o, Location()) {}

TypeCastExpression::TypeCastExpression(const TypeCastExpression& other) :
    Expression(other),
    targetType(other.targetType->clone()),
    operand(other.operand->clone()),
    staticCast(other.staticCast),
    isGenerated(other.isGenerated) {}

Expression* TypeCastExpression::clone() const {
    return new TypeCastExpression(*this);
}

Type* TypeCastExpression::typeCheck(Context& context) {
    if (type != nullptr) {
        return type;
    }
    operand = operand->transform(context);
    Type* fromType = operand->typeCheck(context);

    // Lookup the target type.
    targetType = context.lookupConcreteType(targetType, getLocation());

    bool isDowncast = fromType->isDowncast(targetType);
    if (isDowncast && !isGenerated) {
        Trace::error("Cast from " +
                     fromType->toString() +
                     " to " +
                     targetType->toString() +
                     " is unsafe. Use pattern matching instead.",
                     this);
    }

    staticCast = fromType->isUpcast(targetType);
    if (staticCast || isDowncast || *fromType == *targetType ||
        isCastBetweenObjectAndInterface(fromType)) {
        type = targetType;
    } else if (!fromType->isReference() && !targetType->isReference() &&
               Type::areBuiltInsConvertable(fromType->getBuiltInType(),
                                            targetType->getBuiltInType())) {
        staticCast = true;
        type = targetType;
    } else {
        Trace::error("Can not cast from " +
                     fromType->toString() +
                     " to " +
                     targetType->toString() +
                     ".",
                     this);
    }
    return type;
}

bool TypeCastExpression::isCastBetweenObjectAndInterface(const Type* fromType) {
    if ((fromType->isObject() && targetType->isInterface()) ||
        (targetType->isObject() && fromType->isInterface())) {
        return true;
    }
    return false;
}

NullExpression::NullExpression(const Location& l) :
    Expression(Expression::Null, l) {

    type = new Type(Type::Null);
}

Expression* NullExpression::clone() const {
    return new NullExpression(getLocation());
}

ThisExpression::ThisExpression() :
    ThisExpression(Location()) {}

ThisExpression::ThisExpression(const Location& l) :
    Expression(Expression::This, l) {}

Expression* ThisExpression::clone() const {
    return new ThisExpression(getLocation());
}

Type* ThisExpression::typeCheck(Context& context) {
    if (type != nullptr) {
        return type;
    }
    if (context.isStatic()) {
        Trace::error("Cannot access 'this' from a static context.", this);
    }
    Definition* definition = context.getClassDefinition();
    type = new Type(definition->getName());
    type->setDefinition(definition);
    return type;
}

BinaryExpression::BinaryExpression(
    Operator::OperatorType oper, 
    Expression* l, 
    Expression* r, 
    const Location& loc) :
    Expression(Expression::Binary, loc),
    op(oper),
    left(l),
    right(r) {}

BinaryExpression::BinaryExpression(
    Operator::OperatorType oper,
    Expression* l,
    Expression* r) :
    BinaryExpression(oper, l, r, Location()) {}

Expression* BinaryExpression::clone() const {
    return new BinaryExpression(op,
                                left->clone(),
                                right->clone(),
                                getLocation());
}

Type* BinaryExpression::typeCheck(Context& context) {
    // Left and right have already been transformed in
    // BinaryExpression::transform().
    Type* leftType = left->typeCheck(context);
    Type* rightType = right->typeCheck(context);
    switch (op) {
        case Operator::Equal:
        case Operator::NotEqual:
            if (leftType->isEnumeration() || rightType->isEnumeration()) {
                Trace::error("Comparison operator is not compatible for "
                             "enumerated types.",
                             leftType,
                             rightType,
                             this);
            }
            if (!Type::areInitializable(leftType, rightType)) {
                Trace::error("Incompatible types for comparison.",
                             leftType,
                             rightType,
                             this);
            }
            break;
        case Operator::Assignment:
        case Operator::AssignmentExpression:
            checkAssignment(leftType, rightType, context);
            break;
        case Operator::Addition:
        case Operator::Subtraction:
        case Operator::Multiplication:
        case Operator::Division:
        case Operator::Greater:
        case Operator::Less:
        case Operator::GreaterOrEqual:
        case Operator::LessOrEqual:
            if (!leftType->isNumber() || !rightType->isNumber()) {
                Trace::error("Operator requires number types.",
                             leftType,
                             rightType,
                             this);
            }
            break;
        case Operator::Range:
            if (!leftType->isIntegerNumber() || !rightType->isIntegerNumber()) {
                Trace::error("Range operator requires integer number types.",
                             leftType,
                             rightType,
                             this);
            }
            break;
        case Operator::LogicalAnd:
        case Operator::LogicalOr:
            if (!leftType->isBoolean() || !rightType->isBoolean()) {
                Trace::error("Operator requires boolean types.",
                             leftType,
                             rightType,
                             this);
            }
            break;
        default:
            Trace::error("Operator is incompatible with a binary expression.", 
                         leftType, 
                         rightType, 
                         this);
            break;
    }
    type = resultingType(leftType);
    return type;
}

void BinaryExpression::checkAssignment(
    const Type* leftType,
    const Type* rightType,
    const Context& context) {

    if (leftType->isConstant()) {
        const MethodDefinition* method = context.getMethodDefinition();
        if (method->isEnumConstructor() ||
            (leftIsMemberConstant() && method->isConstructor())) {
            // Member constant initialization.
            if (!Type::isInitializableByExpression(leftType, right)) {
                Trace::error("Incompatible types for member initialization.",
                             leftType,
                             rightType,
                             this);
            }
         } else {
            Trace::error("Cannot change the value of a constant.",
                         leftType,
                         rightType,
                         this);
         }
    } else {
        if (!Type::isAssignableByExpression(leftType, right)) {
            Trace::error("Incompatible types for assignment.",
                         leftType,
                         rightType,
                         this);
        }
    }
}

bool BinaryExpression::leftIsMemberConstant() {
    Expression* leftExpression = left;
    if (leftExpression->getExpressionType() == Expression::ArraySubscript) {
        ArraySubscriptExpression* arraySubscript =
            leftExpression->cast<ArraySubscriptExpression>();
        leftExpression = arraySubscript->getArrayNameExpression();
    }
    if (DataMemberExpression* dataMember =
        leftExpression->dynCast<DataMemberExpression>()) {
        if (dataMember->getType()->isConstant()) {
            return true;
        }
    }
    return false;
}

Type* BinaryExpression::resultingType(Type* leftType) {
    switch (op) {
        case Operator::Equal:
        case Operator::NotEqual:
        case Operator::Greater:
        case Operator::Less:
        case Operator::GreaterOrEqual:
        case Operator::LessOrEqual:
        case Operator::LogicalAnd:
        case Operator::LogicalOr:
            return new Type(Type::Boolean);
        case Operator::Assignment:
            return new Type(Type::Void);
        default:
            return leftType;
    }
}

Expression* BinaryExpression::transform(Context& context) {
    left = left->transform(context);
    Type* leftType = left->typeCheck(context);
    right = right->transform(context);
    Type* rightType = right->typeCheck(context);
    if (leftType->isString() && rightType->isString() &&
        op != Operator::Assignment && op != Operator::AssignmentExpression) {
        if (!Type::areInitializable(leftType, rightType)) {
            Trace::error("String types are incompatible.",
                         leftType,
                         rightType,
                         this);
        }
        MemberSelectorExpression* transformedExpression =
            createStringOperation(context);
        delete this;
        return transformedExpression;
    } else if (leftType->isArray() && rightType->isArray() &&
               op != Operator::Assignment && op != Operator::Equal &&
               op != Operator::NotEqual) {
        MemberSelectorExpression* transformedExpression =
            createArrayOperation(context);
        delete this;
        return transformedExpression;
    } else if (Operator::isCompoundAssignment(op) && !leftType->isArray()) {
        BinaryExpression* transformedExpression = decomposeCompoundAssignment();
        delete this;
        return transformedExpression;
    } else {
        return this;
    }    
}

MemberSelectorExpression* BinaryExpression::createStringOperation(
    Context& context) {

    Identifier operationName;
    switch (op) {
        case Operator::Equal:
            operationName = "equals";
            break;
        case Operator::NotEqual:
            operationName = "notEquals";
            break;
        case Operator::Addition:
            operationName = "concat";
            break;
        case Operator::AdditionAssignment:
            operationName = "append";
            break;
        default:
            Trace::error("Incompatible operator for string types.", this);
            break;
    }
    MethodCallExpression* operation = new MethodCallExpression(operationName,
                                                               getLocation());
    operation->addArgument(right);

    MemberSelectorExpression* memberSelector =
        new MemberSelectorExpression(left, operation, getLocation());
    return MemberSelectorExpression::transformMemberSelector(memberSelector,
                                                             context);
}

MemberSelectorExpression* BinaryExpression::createArrayOperation(
    Context& context) {

    Identifier operationName;
    switch (op) {
        case Operator::Addition:
            operationName = BuiltInTypes::arrayConcatMethodName;
            break;
        case Operator::AdditionAssignment:
            operationName = BuiltInTypes::arrayAppendAllMethodName;
            break;
        default:
            Trace::error("Incompatible operator for array types.", this);
            break;
    }
    MethodCallExpression* operation = new MethodCallExpression(operationName,
                                                               getLocation());
    operation->addArgument(right);

    MemberSelectorExpression* memberSelector =
        new MemberSelectorExpression(left, operation, getLocation());
    return MemberSelectorExpression::transformMemberSelector(memberSelector,
                                                             context);
}

BinaryExpression* BinaryExpression::decomposeCompoundAssignment() {
    BinaryExpression* bin =
        new BinaryExpression(Operator::getDecomposedArithmeticOperator(op),
                             left,
                             right,
                             getLocation());
    return new BinaryExpression(Operator::Assignment, left, bin, getLocation());
}

UnaryExpression::UnaryExpression(
    Operator::OperatorType oper, 
    Expression* o, 
    bool p, 
    const Location& loc) :
    Expression(Expression::Unary, loc),
    op(oper),
    operand(o),
    prefix(p) {}

Expression* UnaryExpression::clone() const {
    return new UnaryExpression(op, operand->clone(), prefix, getLocation());
}

Type* UnaryExpression::typeCheck(Context& context) {
    operand = operand->transform(context);
    type = operand->typeCheck(context);
    switch (op) {
        case Operator::Addition:
        case Operator::Subtraction:
            if (!type->isNumber()) {
                Trace::error("Operator requires number type operand.", this);
            }
            break;
        case Operator::Increment:
        case Operator::Decrement:
            if (!type->isNumber()) {
                Trace::error("Operator requires number type operand.", this);
            }
            if (type->isConstant()) {
                Trace::error("Can not modify constant.", this);            
            }
            break;
        case Operator::LogicalNegation:
            if (!type->isBoolean()) {
                Trace::error("Operator requires boolean type operand.", this);
            }
            type = new Type(Type::Boolean);
            break;
        default:
            Trace::error("Operator is incompatible with a unary expression.",
                         this);
            break;
    }
    return type;
}

LambdaExpression::LambdaExpression(BlockStatement* b, const Location& l) :
    Expression(Expression::Lambda, l),
    block(b),
    signature(nullptr) {}

LambdaExpression::LambdaExpression(const LambdaExpression& other) :
    Expression(other),
    arguments(),
    block(other.block->clone()),
    signature(other.signature) {

    Utils::cloneList(arguments, other.arguments);
}

LambdaExpression* LambdaExpression::clone() const {
    return new LambdaExpression(*this);
}

void LambdaExpression::addArgument(VariableDeclarationStatement* argument) {
    arguments.push_back(argument);
}

Type* LambdaExpression::typeCheck(Context& context) {
    return type;
}

YieldExpression::YieldExpression(const Location& l) :
    Expression(Expression::Yield, l),
    arguments() {}

YieldExpression::YieldExpression(const YieldExpression& other) :
    Expression(other),
    arguments() {

    Utils::cloneList(arguments, other.arguments);
}

Expression* YieldExpression::clone() const {
    return new YieldExpression(*this);
}

Expression* YieldExpression::transform(Context& context) {
    LambdaExpression* lambdaExpression = context.getLambdaExpression();
    if (lambdaExpression == nullptr) {
        return this;
    }

    // The lambda expression is set in the context. This should mean that we
    // are in an inlined method that takes a lambda. Inline the lambda at this
    // locatation.
    if (lambdaExpression->getLambdaSignature()->getReturnType()->isVoid()) {
        // Inline the lambda by transforming this YieldExpression into the
        // inlined lambda.
        BlockStatement* inlinedLambda = inlineLambdaExpression(lambdaExpression,
                                                               context);
        WrappedStatementExpression* wrappedBlockStatement =
            new WrappedStatementExpression(inlinedLambda, getLocation());

        // Disallow any yield transformation while processing the inlined lambda
        // by temporarily resetting the lambda expression in the context. This
        // is to prevent yields from transforming in a lambda expression.
        wrappedBlockStatement->setDisallowYieldTransformation(true);
        delete this;
        return wrappedBlockStatement;
    } else {
        // The lambda returns a value so transform into a
        // LocalVariableExpression that references the returned value. Also,
        // inline the lambda before the current statement.
        LocalVariableExpression* lambdaRetvalTmp =
            inlineLambdaExpressionWithReturnValue(lambdaExpression, context);
        delete this;
        return lambdaRetvalTmp;
    }
}

LocalVariableExpression*
YieldExpression::inlineLambdaExpressionWithReturnValue(
    LambdaExpression* lambdaExpression,
    Context& context) {

    const Location& location = getLocation();

    Type* lambdaRetvalTmpType =
        lambdaExpression->getLambdaSignature()->getReturnType()->clone();

    // Remove any write-protection from the temporary variable so that it can
    // be assigned a value in the lambda block.
    lambdaRetvalTmpType->setConstant(false);

    VariableDeclarationStatement* lambdaRetvalTmpDeclaration =
        VariableDeclarationStatement::generateTemporary(lambdaRetvalTmpType,
                                                        lambdaRetvalName,
                                                        nullptr,
                                                        location);

    BlockStatement* currentBlock = context.getBlock();
    currentBlock->insertBeforeCurrentStatement(lambdaRetvalTmpDeclaration);
    BlockStatement* inlinedLambda = inlineLambdaExpression(lambdaExpression,
                                                           context);

    // Run typeCheck() pass on the inlined lambda. Disallow any yield
    // transformation while processing the inlined lambda by temporarily
    // resetting the lambda expression in the context. This is to prevent
    // yields from transforming in a lambda expression.
    context.setLambdaExpression(nullptr);
    inlinedLambda->typeCheck(context);
    context.setLambdaExpression(lambdaExpression);

    inlinedLambda->returnLastExpression(lambdaRetvalTmpDeclaration);
    currentBlock->insertBeforeCurrentStatement(inlinedLambda);
    LocalVariableExpression* lambdaRetvalTmp =
        new LocalVariableExpression(lambdaRetvalTmpType,
                                    lambdaRetvalTmpDeclaration->getIdentifier(),
                                    location);
    return lambdaRetvalTmp;
}

BlockStatement* YieldExpression::inlineLambdaExpression(
    LambdaExpression* lambdaExpression,
    Context& context) {

    const Location& location = lambdaExpression->getLocation();

    BlockStatement* clonedLambdaBody = lambdaExpression->getBlock()->clone();
    clonedLambdaBody->setEnclosingBlock(context.getBlock());

    const VariableDeclarationStatementList& lambdaArguments =
        lambdaExpression->getArguments();
    VariableDeclarationStatementList::const_iterator i =
        lambdaArguments.begin();
    ExpressionList::const_iterator j = arguments.begin();
    while (i != lambdaArguments.end()) {
        VariableDeclarationStatement* lambdaArgument = *i;
        Expression* yieldArgumentExpression = *j;
        VariableDeclarationStatement* argumentDeclaration =
            new VariableDeclarationStatement(lambdaArgument->getType(),
                                             lambdaArgument->getIdentifier(),
                                             yieldArgumentExpression,
                                             location);
        clonedLambdaBody->insertStatementAtFront(argumentDeclaration);
        i++;
        j++;
    }

    return clonedLambdaBody;
}

Type* YieldExpression::typeCheck(Context& context) {
    LambdaSignature* lambdaSignature =
        context.getMethodDefinition()->getLambdaSignature();
    const TypeList& signatureArgumentTypes = lambdaSignature->getArguments();
    if (signatureArgumentTypes.size() != arguments.size()) {
        Trace::error("Wrong number of arguments in yield expression.", this);    
    }
    TypeList::const_iterator i = signatureArgumentTypes.begin();
    ExpressionList::iterator j = arguments.begin();

    while (j != arguments.end()) {
        *j = (*j)->transform(context);
        Expression* argumentExpression = *j;
        Type* argumentType = argumentExpression->typeCheck(context);
        Type* typeFromSignature = *i;
        if (!Type::areInitializable(typeFromSignature, argumentType)) {
            Trace::error("Incompatible argument in yield expression.",
                         argumentExpression);
        }
        i++;
        j++;
    }

    type = lambdaSignature->getReturnType();
    return type;
}

MatchCase::MatchCase(const Location& loc) :
    Node(loc),
    patternExpressions(),
    patterns(),
    patternGuard(nullptr),
    resultBlock(nullptr),
    isExhaustive(false) {}

MatchCase::MatchCase() : MatchCase(Location()) {}

MatchCase::MatchCase(const MatchCase& other) :
    Node(other),
    patternExpressions(),
    patterns(),
    patternGuard(other.patternGuard ? other.patternGuard->clone() : nullptr),
    resultBlock(other.resultBlock ? other.resultBlock->clone() : nullptr),
    isExhaustive(other.isExhaustive) {

    Utils::cloneList(patternExpressions, other.patternExpressions);
    Utils::cloneList(patterns, other.patterns);
}

MatchCase* MatchCase::clone() const {
    return new MatchCase(*this);
}

void MatchCase::addPatternExpression(Expression* e) {
    patternExpressions.push_back(e);
}

void MatchCase::setResultExpression(
    Expression* resultExpr,
    ClassDefinition* currentClass,
    BlockStatement* currentBlock) {

    resultBlock = new BlockStatement(currentClass, currentBlock, getLocation());
    resultBlock->addStatement(resultExpr);
}

void MatchCase::buildPatterns(Context& context) {
    for (ExpressionList::const_iterator i = patternExpressions.begin();
         i != patternExpressions.end();
         i++) {
        Expression* expression = *i;
        patterns.push_back(Pattern::create(expression, context));
    }
}

bool MatchCase::isMatchExhaustive(
    Expression* subject,
    MatchCoverage& coverage,
    Context& context) {

    for (PatternList::const_iterator i = patterns.begin();
         i != patterns.end();
         i++) {
        Pattern* pattern = *i;
        if (pattern->isMatchExhaustive(subject,
                                       coverage,
                                       patternGuard != nullptr,
                                       context)) {
            isExhaustive = true;
            break;
        }
    }
    return isExhaustive;
}

BinaryExpression* MatchCase::generateComparisonExpression(
    Expression* subject,
    Context& context) {

    PatternList::const_iterator i = patterns.begin();
    Pattern* pattern = *i;
    BinaryExpression* binExpression =
        pattern->generateComparisonExpression(subject, context);
    i++;
    while (i != patterns.end()) {
        pattern = *i;
        binExpression = new BinaryExpression(
            Operator::LogicalOr,
            binExpression,
            pattern->generateComparisonExpression(subject, context),
            getLocation());
        i++;
    }
    return binExpression;
}

void MatchCase::generateTemporariesCreatedByPatterns(BlockStatement* block) {
    for (PatternList::const_iterator i = patterns.begin();
         i != patterns.end();
         i++) {
        const Pattern* pattern = *i;
        const VariableDeclarationStatementList& temps =
            pattern->getTemporariesCreatedByPattern();
        for (VariableDeclarationStatementList::const_iterator j = temps.begin();
             j != temps.end();
             j++) {
            VariableDeclarationStatement* varDeclaration = *j;
            block->addStatement(varDeclaration);
        }
    }
}

Type* MatchCase::generateCaseBlock(
    BlockStatement* caseBlock,
    Context& context,
    Expression* subject,
    const Identifier& matchResultTmpName,
    const Identifier& matchEndLabelName) {

    Expression* expression = generateComparisonExpression(subject, context);

    if (isExhaustive) {
        return generateCaseResultBlock(caseBlock,
                                       context,
                                       matchResultTmpName,
                                       matchEndLabelName);
    }

    generateTemporariesCreatedByPatterns(caseBlock);

    const Location& location = getLocation();
    BlockStatement* caseResultBlock =
        new BlockStatement(context.getClassDefinition(),
                           caseBlock,
                           location);
    Type* caseResultType = generateCaseResultBlock(caseResultBlock,
                                                   context,
                                                   matchResultTmpName,
                                                   matchEndLabelName);
    IfStatement* ifStatement = new IfStatement(expression,
                                               caseResultBlock,
                                               nullptr,
                                               location);
    caseBlock->addStatement(ifStatement);
    return caseResultType;
}

Type* MatchCase::generateCaseResultBlock(
    BlockStatement* block,
    Context& context,
    const Identifier& matchResultTmpName,
    const Identifier& matchEndLabelName) {

    generateVariablesCreatedByPatterns(block);

    Type* caseResultType = nullptr;
    BlockStatement* caseResultBlock = chooseCaseResultBlock(block, context);

    if (resultBlock != nullptr) {
        caseResultBlock->copyStatements(*resultBlock);
    }

    Context blockContext(context);
    block->typeCheck(blockContext);
    Expression* lastExpression =
        caseResultBlock->getLastStatementAsExpression();
    if (lastExpression != nullptr) {
        caseResultType = lastExpression->getType();
        if (!caseResultType->isVoid()) {
            const Location& resultLocation = lastExpression->getLocation();
            caseResultBlock->replaceLastStatement(
                new BinaryExpression(Operator::Assignment,
                                     new NamedEntityExpression(
                                        matchResultTmpName,
                                        resultLocation),
                                     lastExpression,
                                     resultLocation));
        }
    }

    if (caseResultType == nullptr) {
        caseResultType = &Type::voidType();
    }

    if (!isExhaustive && !matchEndLabelName.empty()) {
        caseResultBlock->addStatement(new JumpStatement(matchEndLabelName,
                                                        getLocation()));
    }
    return caseResultType;
}

BlockStatement* MatchCase::chooseCaseResultBlock(
    BlockStatement* outerBlock,
    Context& context) {

    if (patternGuard == nullptr) {
        return outerBlock;
    } else {
        BlockStatement* caseResultBlock =
             new BlockStatement(context.getClassDefinition(),
                                outerBlock,
                                getLocation());
        outerBlock->addStatement(new IfStatement(patternGuard,
                                                 caseResultBlock,
                                                 nullptr,
                                                 patternGuard->getLocation()));
        return caseResultBlock;
    }
}

void MatchCase::generateVariablesCreatedByPatterns(BlockStatement* block) {
    assert(!patterns.empty());

    checkVariablesCreatedByPatterns();
    const Pattern* pattern = patterns.front();
    const VariableDeclarationStatementList& variablesCreatedByPattern =
         pattern->getVariablesCreatedByPattern();
    for (VariableDeclarationStatementList::const_iterator i =
             variablesCreatedByPattern.begin();
         i != variablesCreatedByPattern.end();
         i++) {
        VariableDeclarationStatement* varDeclaration = *i;
        block->addStatement(varDeclaration);
    }
}

void MatchCase::checkVariablesCreatedByPatterns() {
    PatternList::const_iterator i = patterns.begin();
    const Pattern* firstPattern = *i;
    const VariableDeclarationStatementList& firstPatternVariables =
        firstPattern->getVariablesCreatedByPattern();
    i++;
    while (i != patterns.end()) {
        const Pattern* pattern = *i;
        const VariableDeclarationStatementList& patternVariables =
            pattern->getVariablesCreatedByPattern();
        if (firstPatternVariables.size() != patternVariables.size()) {
            Trace::error("All patterns in a case must bind the same variables.",
                         this);
        }
        for (VariableDeclarationStatementList::const_iterator j =
                 firstPatternVariables.begin();
             j != firstPatternVariables.end();
             j++) {
            bool variableFound = false;
            VariableDeclarationStatement* firstPatternVariable = *j;
            for (VariableDeclarationStatementList::const_iterator k =
                     patternVariables.begin();
                 k != patternVariables.end();
                 k++) {
                VariableDeclarationStatement* patternVariable = *k;
                if (*(firstPatternVariable->getDeclaration()) ==
                    *(patternVariable->getDeclaration())) {
                    variableFound = true;
                    break;
                }
            }
            if (!variableFound) {
                Trace::error("Variable '" +
                             firstPatternVariable->getDeclaration()->toString()
                             + "' is not found in all patterns for this case.",
                             firstPatternVariable->getLocation());
            }
        }
        i++;
    }
}

MatchExpression::MatchExpression(Expression* s, const Location& loc) :
    Expression(Expression::Match, loc),
    subject(s),
    cases(),
    isGenerated(false) {}

MatchExpression::MatchExpression(Expression* s) :
    MatchExpression(s, Location()) {}

MatchExpression::MatchExpression(const MatchExpression& other) :
    Expression(other),
    subject(other.subject->clone()),
    cases(),
    isGenerated(other.isGenerated) {

    for (CaseList::const_iterator i = other.cases.begin();
         i != other.cases.end();
         i++) {
        cases.push_back((*i)->clone());
    }
}

Expression* MatchExpression::clone() const {
    return new MatchExpression(*this);
}

Expression* MatchExpression::transform(Context& context) {
    const Location& location = getLocation();

    subject = subject->transform(context);
    subject->typeCheck(context);

    buildCasePatterns(context);
    checkCases(context);

    Identifier resultTmpName =
        VariableDeclarationStatement::generateTemporaryName(matchResultName);
    BlockStatement* matchLogic = generateMatchLogic(context, resultTmpName);
    if (type->isVoid()) {
        // The MatchExpression does not return anything so transform it into a
        // WrappedStatement that contains the match logic. No need to run
        // typeCheck() on the generated match logic code from here, since the
        // typeCheck() method on the WrappedStatementExpression will be called
        // by the containing BlockStatement.
        return new WrappedStatementExpression(matchLogic, location);
    } else {
        // The MatchExpression returns a value so transform it into a
        // LocalVariableExpression that references the match result.
        VariableDeclarationStatement* resultTmpDeclaration =
            new VariableDeclarationStatement(type,
                                             resultTmpName,
                                             nullptr,
                                             location);

        // Temporarily remove the write-protection from the result temp variable
        // so that results can be assigned in the match logic.
        type->setConstant(false);

        BlockStatement* currentBlock = context.getBlock();
        currentBlock->insertBeforeCurrentStatement(resultTmpDeclaration);

        // Run typeCheck() pass on the generated match logic code.
        matchLogic->typeCheck(context);
        type->setConstant(true);
        currentBlock->insertBeforeCurrentStatement(matchLogic);

        LocalVariableExpression* resultTemporary =
            new LocalVariableExpression(type,
                                        resultTmpName,
                                        location);
        return resultTemporary;
    }
}

BlockStatement* MatchExpression::generateMatchLogic(
    Context& context,
    const Identifier& resultTmpName) {

    const Location& location = getLocation();

    BlockStatement* matchLogicBlock =
        new BlockStatement(context.getClassDefinition(),
                           context.getBlock(),
                           location);

    Expression* subjectRefExpr = generateSubjectTemporary(matchLogicBlock);

    if (subjectRefExpr->getType()->isArray()) {
        matchLogicBlock->addStatement(
            ArrayPattern::generateMatchSubjectLengthDeclaration(
                subjectRefExpr));
    }

    Identifier matchEndLabelName = generateMatchEndLabelName();
        // VariableDeclarationStatement::generateTemporaryName(matchEndName);

    for (CaseList::const_iterator i = cases.begin(); i != cases.end(); i++) {
        MatchCase* matchCase = *i;
        BlockStatement* caseBlock =
            new BlockStatement(context.getClassDefinition(),
                               matchLogicBlock,
                               location);
        Type* caseResultType = matchCase->generateCaseBlock(caseBlock,
                                                            context,
                                                            subjectRefExpr,
                                                            resultTmpName,
                                                            matchEndLabelName);
        checkResultType(caseResultType, matchCase);
        matchLogicBlock->addStatement(caseBlock);
    }

    if (!matchEndLabelName.empty()) {
        LabelStatement* matchEndLabel = new LabelStatement(matchEndLabelName,
                                                           location);
        matchLogicBlock->addStatement(matchEndLabel);
    }

    return matchLogicBlock;
}

Expression* MatchExpression::generateSubjectTemporary(
    BlockStatement* matchLogicBlock) {

    Expression* subjectRefExpr = nullptr;
    if (subject->isVariable()) {
        subjectRefExpr = subject;
    } else {
        const Location& location = subject->getLocation();
        Type* subjectType = subject->getType();
        VariableDeclarationStatement* subjectTmpDeclaration =
            new VariableDeclarationStatement(subjectType,
                                             CommonNames::matchSubjectName,
                                             subject,
                                             location);
        matchLogicBlock->addStatement(subjectTmpDeclaration);
        subjectRefExpr =
            new LocalVariableExpression(subjectType,
                                        CommonNames::matchSubjectName,
                                        location);
    }
    return subjectRefExpr;
}

Identifier MatchExpression::generateMatchEndLabelName() {
    if (cases.size() > 1) {
        return VariableDeclarationStatement::generateTemporaryName(
            matchEndName);
    }
    return "";
}

void MatchExpression::checkResultType(
    Type* caseResultType,
    const MatchCase* matchCase) {

    const Type* commonType = Type::calculateCommonType(type, caseResultType);
    if (commonType == nullptr) {
        Trace::error("Case return types are not compatible. Previous "
                     "cases: " + type->toString() + ". This case: " +
                     caseResultType->toString() + ".",
                     matchCase->getResultBlock()->getLocation());
    }
    type = commonType->getAsMutable();
}

void MatchExpression::buildCasePatterns(Context& context) {
    for (CaseList::const_iterator i = cases.begin(); i != cases.end(); i++) {
        MatchCase* matchCase = *i;
        matchCase->buildPatterns(context);
    }
}

void MatchExpression::checkCases(Context& context) {
    MatchCoverage coverage(subject->getType());
    for (CaseList::const_iterator i = cases.begin(); i != cases.end(); i++) {
        MatchCase* matchCase = *i;
        if (matchCase->isMatchExhaustive(subject, coverage, context)) {
            if (i + 1 != cases.end()) {
                MatchCase* unreachableMatchCase = *(++i);
                Trace::error("Pattern is unreachable.", unreachableMatchCase);
            }
            return;
        }
    }

    if (!isGenerated) {
        Trace::error("Not all cases are covered.", this);
    }
}

Type* MatchExpression::typeCheck(Context&) {
    // The MatchExpression should have been transformed by the transform()
    // method.
    Trace::internalError("MatchExpression::typeCheck");
    return nullptr;
}

ClassDecompositionExpression::ClassDecompositionExpression(
    Type* t,
    const Location& l) :
    Expression(Expression::ClassDecomposition, l),
    members(),
    enumVariantName() {

    type = t;
}

ClassDecompositionExpression::ClassDecompositionExpression(
    const ClassDecompositionExpression& other) :
    Expression(other),   
    members(),
    enumVariantName(other.enumVariantName) {

    const MemberList& otherMembers = other.members;
    for (MemberList::const_iterator i = otherMembers.begin();
         i != otherMembers.end();
         i++) {
        Member member;
        member.nameExpr = i->nameExpr->clone();
        member.patternExpr = i->patternExpr ? i->patternExpr->clone() : nullptr;
        members.push_back(member);
    }
}

ClassDecompositionExpression* ClassDecompositionExpression::clone() const {
    return new ClassDecompositionExpression(*this);
}

Type* ClassDecompositionExpression::typeCheck(Context& context) {
    type = context.lookupConcreteType(type, getLocation());
    return type;
}

void ClassDecompositionExpression::addMember(
    Expression* nameExpr,
    Expression* patternExpr) {

    Member member;
    member.nameExpr = nameExpr;
    member.patternExpr = patternExpr;
    members.push_back(member);
}

TypedExpression::TypedExpression(
    Type* targetType,
    Expression* n,
    const Location& l) :
    Expression(Expression::Typed, l),
    resultName(n) {

    type = targetType;
}

TypedExpression::TypedExpression(const TypedExpression& other) :
    Expression(other),
    resultName(other.resultName->clone()) {}

TypedExpression* TypedExpression::clone() const {
    return new TypedExpression(*this);
}

Type* TypedExpression::typeCheck(Context& context) {
    type = context.lookupConcreteType(type, getLocation());
    return type;
}

PlaceholderExpression::PlaceholderExpression(const Location& l) :
    Expression(Expression::Placeholder, l) {}

PlaceholderExpression::PlaceholderExpression() :
    Expression(Expression::Placeholder, Location()) {}

Expression* PlaceholderExpression::clone() const {
    return new PlaceholderExpression(getLocation());
}

WildcardExpression::WildcardExpression(const Location& l) :
    Expression(Expression::Wildcard, l) {}

Expression* WildcardExpression::clone() const {
    return new WildcardExpression(getLocation());
}

TemporaryExpression::TemporaryExpression(
    VariableDeclaration* d,
    const Location& l) :
    Expression(Expression::Temporary, l),
    declaration(d),
    nonStaticInlinedMethodBody(nullptr) {}

Expression* TemporaryExpression::clone() const {
    return new TemporaryExpression(new VariableDeclaration(*declaration),
                                   getLocation());
}

Type* TemporaryExpression::typeCheck(Context& context) {
    if (type != nullptr) {
        return type;
    }
    if (nonStaticInlinedMethodBody != nullptr) {
        // This temporary holds the return value of an inlined non-static
        // method. At this point it should be safe to run the type-check pass
        // since any missing 'this' pointer declaration should have been added
        // to the inlined code by now.
        nonStaticInlinedMethodBody->typeCheck(context);
        context.setTemporaryRetvalDeclaration(nullptr);
    }
    type = declaration->getType();
    return type;
}

WrappedStatementExpression::WrappedStatementExpression(
    Statement* s,
    const Location& l) :
    Expression(Expression::WrappedStatement, l),
    statement(s),
    inlinedNonStaticMethod(false),
    inlinedArrayForEach(false),
    disallowYieldTransformation(false) {}

WrappedStatementExpression::WrappedStatementExpression(
    const WrappedStatementExpression& other) :
    Expression(other),
    statement(other.statement->clone()),
    inlinedNonStaticMethod(other.inlinedNonStaticMethod),
    inlinedArrayForEach(other.inlinedArrayForEach),
    disallowYieldTransformation(other.disallowYieldTransformation) {}

Expression* WrappedStatementExpression::clone() const {
    return new WrappedStatementExpression(*this);
}

Type* WrappedStatementExpression::typeCheck(Context& context) {
    if (disallowYieldTransformation) {
        LambdaExpression* lambdaExpression = context.getLambdaExpression();
        context.setLambdaExpression(nullptr);
        type = statement->typeCheck(context);
        context.setLambdaExpression(lambdaExpression);
        disallowYieldTransformation = false;
    } else {
        type = statement->typeCheck(context);
    }
    return type;
}
