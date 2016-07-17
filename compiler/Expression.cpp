#include "Expression.h"

#include <iterator>
#include <memory>

#include "Context.h"
#include "Definition.h"
#include "Tree.h"
#include "Pattern.h"
#include "Closure.h"

namespace {
    const Identifier thisPointerName("__thisPointer");
    const Identifier indexVaraibleName("__i");
    const Identifier arrayReferenceName("__array");
    const Identifier arrayLengthName("__array_length");
    const Identifier lambdaRetvalName("__lambda_retval");
    const Identifier retvalName("__inlined_retval");
    const Identifier matchResultName("__match_result");
    const Identifier matchEndName("__match_end");
}

Expression::Expression(Kind k, const Location& l) :
    Statement(Statement::ExpressionStatement, l),
    type(nullptr),
    kind(k) {}

Expression::Expression(const Expression& other) :
    Statement(other),
    type(other.type ? other.type->clone() : nullptr),
    kind(other.kind) {}

Expression* Expression::transform(Context&) {
    return this;
}

bool Expression::isVariable() const {
    return false;
}

Identifier Expression::generateVariableName() const {
    return Identifier();
}

Expression::Kind Expression::getRightmostExpressionKind() const {
    return kind;
}

Expression* Expression::generateDefaultInitialization(
    Type* type, 
    const Location& location) {

    if (type->isReference()) {
        return NullExpression::create(location);
    } else if (type->isEnumeration()) {
        return nullptr;
    } else {
        return LiteralExpression::generateDefault(type, location);
    }
}

LiteralExpression::LiteralExpression(Kind k, Type* t, const Location& loc) :
    Expression(Expression::Literal, loc),
    kind(k) {

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
            expression = IntegerLiteralExpression::create(0, location);
            break;
        case Type::Float:
            expression = FloatLiteralExpression::create(0, location);
            break;
        case Type::Char:
            expression = CharacterLiteralExpression::create(0, location);
            break;
        case Type::String:
            expression = StringLiteralExpression::create("", location);
            break;
        case Type::Boolean:  
            expression = BooleanLiteralExpression::create(false, location);
            break;
        default:
            break;
    }
    return expression;
}

CharacterLiteralExpression::CharacterLiteralExpression(
    char c,
    const Location& loc) :
    LiteralExpression(LiteralExpression::Character,
                      Type::create(Type::Char),
                      loc),
    value(c) {}

CharacterLiteralExpression* CharacterLiteralExpression::create(
    char c,
    const Location& loc) {

    return new CharacterLiteralExpression(c, loc);
}

Expression* CharacterLiteralExpression::clone() const {
    return new CharacterLiteralExpression(value, getLocation());
}

IntegerLiteralExpression::IntegerLiteralExpression(int i, const Location& loc) :
    LiteralExpression(LiteralExpression::Integer,
                      Type::create(Type::Integer),
                      loc),
    value(i) {}

IntegerLiteralExpression* IntegerLiteralExpression::create(
    int i,
    const Location& loc) {

    return new IntegerLiteralExpression(i, loc);
}

IntegerLiteralExpression* IntegerLiteralExpression::create(int i) {
    return new IntegerLiteralExpression(i, Location());
}

Expression* IntegerLiteralExpression::clone() const {
    return new IntegerLiteralExpression(value, getLocation());
}

FloatLiteralExpression::FloatLiteralExpression(float f, const Location& loc) :
    LiteralExpression(LiteralExpression::Float, Type::create(Type::Float), loc),
    value(f) {}

FloatLiteralExpression* FloatLiteralExpression::create(
    float f,
    const Location& loc) {

    return new FloatLiteralExpression(f, loc);
}

Expression* FloatLiteralExpression::clone() const {
    return new FloatLiteralExpression(value, getLocation());
}

StringLiteralExpression::StringLiteralExpression(
    const std::string& s,
    const Location& loc) :
    LiteralExpression(LiteralExpression::String, Type::create(Type::Char), loc),
    value(s) {

    type->setArray(true);
}

StringLiteralExpression* StringLiteralExpression::create(
    const std::string& s,
    const Location& loc) {

    return new StringLiteralExpression(s, loc);
}

Expression* StringLiteralExpression::clone() const {
    return new StringLiteralExpression(value, getLocation());
}

Expression* StringLiteralExpression::transform(Context& context) {
    Expression* transformedExpression = nullptr;
    if (context.isStringConstructorCall()) {
        transformedExpression = createCharArrayExpression(context);
    } else {
        auto stringCtorCall =
            MethodCallExpression::create(Keyword::stringString, getLocation());
        stringCtorCall->addArgument(createCharArrayExpression(context));
        transformedExpression =
            HeapAllocationExpression::create(stringCtorCall);
    }

    return transformedExpression;
}

Expression* StringLiteralExpression::createCharArrayExpression(
    Context& context) {

    const Location& location = getLocation();
    auto charArrayLiteral =
        ArrayLiteralExpression::create(Type::create(Type::Char), location);
    for (auto charVal: value) {
        charArrayLiteral->addElement(
            CharacterLiteralExpression::create(charVal, location));
    }
    return charArrayLiteral->transform(context);
}

BooleanLiteralExpression::BooleanLiteralExpression(
    bool b,
    const Location& loc) :
    LiteralExpression(LiteralExpression::Boolean,
                      Type::create(Type::Boolean),
                      loc),
    value(b) {}

BooleanLiteralExpression* BooleanLiteralExpression::create(
    bool b,
    const Location& loc) {

    return new BooleanLiteralExpression(b, loc);
}

Expression* BooleanLiteralExpression::clone() const {
    return new BooleanLiteralExpression(value, getLocation());
}

ArrayLiteralExpression::ArrayLiteralExpression(Type* t, const Location& loc) : 
LiteralExpression(LiteralExpression::Array, t, loc),
    elements() {}

ArrayLiteralExpression::ArrayLiteralExpression(
    const ArrayLiteralExpression& other) :
    LiteralExpression(other),
    elements() {

    Utils::cloneList(elements, other.elements);
}

ArrayLiteralExpression* ArrayLiteralExpression::create(const Location& loc) {
    return new ArrayLiteralExpression(Type::create(Type::Implicit), loc);
}

ArrayLiteralExpression* ArrayLiteralExpression::create(
    Type* t,
    const Location& loc) {

    return new ArrayLiteralExpression(t, loc);
}

ArrayLiteralExpression* ArrayLiteralExpression::clone() const {
    return new ArrayLiteralExpression(*this);
}

Expression* ArrayLiteralExpression::transform(Context& context) {
    checkElements(context);

    auto capacity =
        IntegerLiteralExpression::create(elements.size(), getLocation());
    auto arrayAllocation =
        ArrayAllocationExpression::create(type, capacity, getLocation());
    arrayAllocation->setInitExpression(this);
    return arrayAllocation;
}

Traverse::Result ArrayLiteralExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (auto element: elements) {
        element->traverse(visitor);
    }

    return Traverse::Continue;
}

void ArrayLiteralExpression::checkElements(Context& context) {
    const Type* commonType = nullptr;

    for (auto& element: elements) {
        element = element->transform(context);
        auto elementType = element->typeCheck(context);
        auto previousCommonType = commonType;
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

NamedEntityExpression::NamedEntityExpression(
    const Identifier& i,
    const Location& loc) :
    Expression(Expression::NamedEntity, loc),
    identifier(i),
    binding(nullptr) {}

NamedEntityExpression* NamedEntityExpression::create(const Identifier& i) {
    return new NamedEntityExpression(i, Location());
}

NamedEntityExpression* NamedEntityExpression::create(
    const Identifier& i,
    const Location& loc) {

    return new NamedEntityExpression(i, loc);
}

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
            auto type = binding->getLocalObject()->getType();
            resolvedExpression =
                LocalVariableExpression::create(type,
                                                identifier,
                                                getLocation());
            break;
        }
        case Binding::DataMember: {
            auto dataMember =
                binding->getDefinition()->cast<DataMemberDefinition>();
            resolvedExpression = DataMemberExpression::create(dataMember,
                                                              getLocation());
            break;
        }
        case Binding::Class: {
            auto classDef = binding->getDefinition()->cast<ClassDefinition>();
            resolvedExpression = ClassNameExpression::create(classDef,
                                                             getLocation());
            break;
        }
        case Binding::Method:
            resolvedExpression = MethodCallExpression::create(identifier,
                                                              getLocation());
            break;
        default:
            Trace::internalError("NamedEntityExpression::transform");
            break;
    }

    return resolvedExpression->transform(context);
}

Type* NamedEntityExpression::typeCheck(Context&) {
    // The NamedEntityExpression should have been transformed into another type
    // of expression by the transform() method.
    Trace::internalError("NamedEntityExpression::typeCheck");            
    return nullptr;
}

Traverse::Result NamedEntityExpression::traverse(Visitor& visitor) {
    visitor.visitNamedEntity(*this);
    return Traverse::Continue;
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
            auto methodCall =
                MethodCallExpression::create(identifier, getLocation());
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
        auto dataMember =
            binding->getDefinition()->cast<DataMemberDefinition>();
        if (dataMember->isStatic()) {
            return true;
        }
    }
    return false;
}

bool NamedEntityExpression::isReferencingName(const Expression* name) {
    switch (name->getKind()) {
        case Expression::NamedEntity: {
            auto namedEntity = name->cast<NamedEntityExpression>();
            if (namedEntity->getIdentifier().compare(identifier) == 0) {
                return true;
            }
            break;
        }
        case Expression::LocalVariable: {
            auto local = name->cast<LocalVariableExpression>();
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
    Type* t,
    const Identifier& i,
    const Location& loc) :
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

LocalVariableExpression* LocalVariableExpression::create(
    Type* t,
    const Identifier& i,
    const Location& loc) {

    return new LocalVariableExpression(t, i, loc);
}

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

ClassNameExpression* ClassNameExpression::create(
    ClassDefinition* c,
    const Location& loc) {

    return new ClassNameExpression(c, loc);
}

Expression* ClassNameExpression::clone() const {
    return new ClassNameExpression(*this);
}

Expression* ClassNameExpression::transform(Context& context) {
    if (hasTransformed) {
        return this;
    }

    if (auto outerClass = classDefinition->getEnclosingClass()) {
        // The referenced class is contained in an outer class.
        if (outerClass != context.getClassDefinition()) {
            const Location& loc = getLocation();
            auto className = ClassNameExpression::create(outerClass, loc);
            auto memberSelector =
                MemberSelectorExpression::create(className, this, loc);
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

MemberSelectorExpression* MemberSelectorExpression::create(
    Expression* l,
    Expression* r,
    const Location& loc) {

    return new MemberSelectorExpression(l, r, loc);
}

MemberSelectorExpression* MemberSelectorExpression::create(
    Expression* l,
    Expression* r) {

    return create(l, r, Location());
}

MemberSelectorExpression* MemberSelectorExpression::create(
    const Identifier& l,
    Expression* r) {

    return create(NamedEntityExpression::create(l, Location()), r, Location());
}

MemberSelectorExpression* MemberSelectorExpression::create(
    const Identifier& l,
    const Identifier& r) {

    return create(NamedEntityExpression::create(l, Location()),
                  NamedEntityExpression::create(r, Location()),
                  Location());
}

MemberSelectorExpression* MemberSelectorExpression::create(
    const Identifier& l,
    const Identifier& r,
    const Location& loc) {

    return create(NamedEntityExpression::create(l, Location()),
                  NamedEntityExpression::create(r, Location()),
                  loc);
}

Expression* MemberSelectorExpression::clone() const {
    return new MemberSelectorExpression(left->clone(),
                                        right->clone(),
                                        getLocation());
}

MemberSelectorExpression* MemberSelectorExpression::transformMemberSelector(
    MemberSelectorExpression* memberSelector,
    Context& context) {

    auto transformedExpression = memberSelector->transform(context);
    if (transformedExpression->getKind() != Expression::MemberSelector) {
        Trace::internalError(
            "MemberSelectorExpression::transformMemberSelector");
    }
    return transformedExpression->cast<MemberSelectorExpression>();
}

MethodCallExpression* MemberSelectorExpression::getRhsCall(Context& context) {
    MethodCallExpression* retval = nullptr;

    left = left->transform(context);
    if (left->getKind() != Expression::ClassName) {
        return retval;
    }

    auto className = left->cast<ClassNameExpression>();
    Context::BindingsGuard guard(context,
                                 &(className->getClassDefinition()->
                                       getNameBindings()));

    switch (right->getKind()) {
        case Expression::MemberSelector:
            retval =
                right->cast<MemberSelectorExpression>()->getRhsCall(context);
            break;
        case Expression::NamedEntity:
            retval = right->cast<NamedEntityExpression>()->getCall(context,
                                                                   false);
            break;
        case Expression::Member: {
            if (auto methodCall = right->dynCast<MethodCallExpression>()) {
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
    auto oldArrayType = context.getArrayType();
    auto leftType = left->getType();
    if (leftType->isArray()) {
        arrayType = leftType;
        context.setArrayType(arrayType);
    }

    right = right->transform(context);

    if (arrayType != nullptr) {
        context.setArrayType(oldArrayType);
    }

    if (leftType->isPrimitive()) {
        return transformPrimitiveTypeMethodCall(context);
    }

    switch (right->getKind()) {
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
    if (left->getKind() == Expression::ClassName) {
        context.setIsStatic(true);
        auto expr = left->cast<ClassNameExpression>();
        localBindings = &(expr->getClassDefinition()->getNameBindings());
    } else {
        auto resultingLeftType = left->typeCheck(context);
        context.setIsStatic(false);
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
        if (statement->getKind() != Statement::Block) {
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

    return wrappedBlockStatement;
}

TemporaryExpression* MemberSelectorExpression::transformIntoTemporaryExpression(
    TemporaryExpression* temporary) {

    auto nonStaticInlinedMethodBody =
        temporary->getNonStaticInlinedMethodBody();
    if (nonStaticInlinedMethodBody != nullptr) {
        // Right-hand side was a method call to an non-static inlined method
        // that takes a lambda. We need to generate a variable declaration for a
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
    auto firstStatement = *(block->getStatements().begin());
    if (firstStatement->getKind() == Statement::VarDeclaration) {
        auto variableDeclaration =
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
        auto initExpression =
            thisPointerDeclaration->getInitExpression();
        auto modifiedInitExpression =
            MemberSelectorExpression::create(left,
                                             initExpression,
                                             getLocation());
        thisPointerDeclaration->setInitExpression(modifiedInitExpression);
    } else {
        thisPointerDeclaration =
            VariableDeclarationStatement::create(Type::create(Type::Implicit),
                                                 thisPointerIdentifier,
                                                 left,
                                                 location);
        block->insertStatementAtFront(thisPointerDeclaration);
    }
}

Expression* MemberSelectorExpression::transformPrimitiveTypeMethodCall(
    Context& context) {

    auto call = right->dynCast<MethodCallExpression>();
    assert(call != nullptr);

    const Location& loc = getLocation();
    const Identifier& methodName = call->getName();
    if (methodName.compare(BuiltInTypes::objectEqualsMethodName) == 0) {
        const ExpressionList& arguments = call->getArguments();
        assert(arguments.size() == 1);
        Expression* argument = arguments.front();
        auto comparison =
            BinaryExpression::create(Operator::Equal, left, argument, loc);
        return comparison->transform(context);
    }

    auto selfCall = MethodCallExpression::create("_" + methodName, loc);
    selfCall->addArgument(left);
    return selfCall->transform(context);
}

Type* MemberSelectorExpression::typeCheck(Context&) {
    if (type == nullptr) {
        // Type should have been calculated in the transform() method.
        Trace::internalError("MemberSelectorExpression::typeCheck");    
    }
    return type;
}

Traverse::Result MemberSelectorExpression::traverse(Visitor& visitor) {
    if (visitor.visitMemberSelector(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    left->traverse(visitor);
    right->traverse(visitor);
    return Traverse::Continue;
}

Identifier MemberSelectorExpression::generateVariableName() const {
    return left->generateVariableName() + "_" + right->generateVariableName();
}

Expression::Kind
MemberSelectorExpression::getRightmostExpressionKind() const {
    return right->getRightmostExpressionKind();
}

MemberExpression::MemberExpression(
    Kind k,
    ClassMemberDefinition* m,
    const Location& loc) :
    Expression(Expression::Member, loc),
    memberDefinition(m),
    kind(k),
    hasTransformedIntoMemberSelector(false),
    hasCheckedAccess(false) {}

MemberExpression::MemberExpression(const MemberExpression& other) :
    Expression(other),
    memberDefinition(other.memberDefinition),
    kind(other.kind),
    hasTransformedIntoMemberSelector(other.hasTransformedIntoMemberSelector),
    hasCheckedAccess(other.hasCheckedAccess) {}

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
        left = NamedEntityExpression::create(memberClass->getFullName(),
                                             location);
    } else {
        // The MemberExpression refences a non-static member and the 'this'
        // pointer is not implicit. Transform this DataMemberExpression into a
        // MemberSelectorExpression in order to explicitly reference the member
        // through the 'this' pointer: 'this.member'.
        left = NamedEntityExpression::create(thisPointerName, location);
    }

    auto memberSelector =
        MemberSelectorExpression::create(left, this, location);
    hasTransformedIntoMemberSelector = true;
    return memberSelector->transform(context);
}

void MemberExpression::accessCheck(Context& context) {
    if (hasCheckedAccess) {
        // Only check access once for each member expression. It should be
        // checked before any code is inlined in a potentially different class,
        // which would make the private access check fail.
        return;
    }

    if (context.isStatic() && !memberDefinition->isStatic()) {
        if (memberDefinition->isMethod()) {
            auto method = memberDefinition->cast<MethodDefinition>();
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

    if (memberDefinition->isPrivate()) {
        if (memberDefinition->getEnclosingClass() !=
            context.getClassDefinition()) {
            Trace::error("Member " + memberDefinition->getName() +
                         " is private.",
                         this);
        }
    }

    hasCheckedAccess = true;
}

DataMemberExpression::DataMemberExpression(
    DataMemberDefinition* d,
    const Location& loc) :
    MemberExpression(MemberExpression::DataMember, d, loc) {

    type = d->getType();
}

DataMemberExpression::DataMemberExpression(const DataMemberExpression& other) :
    MemberExpression(other) {}

DataMemberExpression* DataMemberExpression::create(
    DataMemberDefinition* d,
    const Location& loc) {

    return new DataMemberExpression(d, loc);
}

Expression* DataMemberExpression::clone() const {
    return new DataMemberExpression(*this);
}

Expression* DataMemberExpression::transform(Context& context) {
    if (context.isConstructorCallStatement() &&
        context.getClassLocalNameBindings() == nullptr) {
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

    auto argument =
        NamedEntityExpression::create(memberDefinition->getName() + "_Arg",
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

MethodCallExpression* MethodCallExpression::create(
    const Identifier& n,
    const Location& l) {

    return new MethodCallExpression(n, l);
}

MethodCallExpression* MethodCallExpression::create(const Identifier& n) {
    return create(n, Location());
}

MethodCallExpression* MethodCallExpression::clone() const {
    return new MethodCallExpression(*this);
}

void MethodCallExpression::addArgument(const Identifier& argument) {
    arguments.push_back(NamedEntityExpression::create(argument, Location()));
}

MethodDefinition* MethodCallExpression::getEnumCtorMethodDefinition() const {
    if (memberDefinition != nullptr) {
        auto methodDef = memberDefinition->cast<MethodDefinition>();
        if (methodDef->isEnumConstructor()) {
            return methodDef;
        }
    }
    return nullptr;
}

void MethodCallExpression::tryResolveEnumConstructor(const Context& context) {
    const auto binding = context.lookup(name);
    if (binding != nullptr &&
        binding->getReferencedEntity() == Binding::Method) {

        const Binding::MethodList& candidates = binding->getMethodList();
        assert(!candidates.empty());
        auto methodDefinition = candidates.front();
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

    auto transformedExpression = methodCall->transform(context);
    return transformedExpression->dynCast<MethodCallExpression>();
}

void MethodCallExpression::resolve(Context& context) {
    Context subcontext(context);
    subcontext.reset();
    const Binding::MethodList& candidates = resolveCandidates(context);

    TypeList argumentTypes;
    resolveArgumentTypes(argumentTypes, candidates, subcontext);
    findCompatibleMethod(candidates, argumentTypes);

    if (memberDefinition == nullptr) {
        bool candidateClassIsGeneric = false;
        for (auto candidate: candidates) {
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

    auto concreteType = inferConcreteType(candidate, argumentTypes, context);
    if (concreteType == nullptr) {
        return;
    }

    if (candidate->isConstructor()) {
        setConstructorCallName(concreteType);
    }
    auto concreteClass = concreteType->getDefinition()->cast<ClassDefinition>();

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
        auto concreteType = Type::create(candidateClassName);
        auto i = candidateArgumentList.cbegin();
        auto j = argumentTypes.cbegin();
        while (i != candidateArgumentList.end()) {
            VariableDeclaration* candidateArgument = *i;
            auto argumentType = *j;
            auto candidateArgumentType = candidateArgument->getType();
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
            auto convertableEnumType = Type::create(candidateClassName);
            convertableEnumType->addGenericTypeParameter(
                Type::create(Type::Placeholder));
            return context.lookupConcreteType(convertableEnumType,
                                              getLocation());
        }
        return nullptr;
    }
}

const Binding::MethodList& MethodCallExpression::resolveCandidates(
    const Context& context) {

    const auto binding = context.lookup(name);
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

    for (Binding::MethodList::const_reverse_iterator i = candidates.rbegin();
         i != candidates.rend();
         i++) {
        auto candidate = *i;
        if (candidate->isCompatible(argumentTypes) &&
            !candidate->getClass()->isGeneric()) {
            memberDefinition = candidate;
            return;
        }
    }
}

void MethodCallExpression::setConstructorCallName(
    const Type* allocatedObjectType) {

    // We may need to update the name of the constructor call to match the name
    // of the constructed concrete class which includes the concrete type
    // parameters.
    name = allocatedObjectType->getFullConstructedName();

    setIsConstructorCall();
}

Expression* MethodCallExpression::transform(Context& context) {
    MethodDefinition* methodDefinition = nullptr;
    {
        Context::BindingsGuard guard(context);

        if (memberDefinition == nullptr) {
            if (resolvesToClosure(context)) {
                return transformIntoClosureCallMethod(context);
            }
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
        return transformDueToLambda(methodDefinition, context);
    }
    return this;
}

bool MethodCallExpression::resolvesToClosure(const Context& context) {
    auto binding = context.lookup(name);
    if (binding == nullptr) {
        Trace::error("Unknown method: " + name, this);
    }

    auto type = binding->getVariableType();
    if (type == nullptr) {
        return false;
    }

    auto definition = type->getDefinition();
    if (auto* classDef = definition->dynCast<ClassDefinition>()) {
        if (classDef->isClosure()) {
            return true;
        }
    }
    return false;
}

Expression* MethodCallExpression::transformIntoClosureCallMethod(
    Context& context) {

    const Location& location = getLocation();
    auto left = NamedEntityExpression::create(name, location);
    name = CommonNames::callMethodName;
    auto memberSelector =
        MemberSelectorExpression::create(left, this, location);
    return memberSelector->transform(context);
}

bool MethodCallExpression::isBuiltInArrayMethod() {
    auto methodDefinition = memberDefinition->cast<MethodDefinition>();
    auto classDef = methodDefinition->getEnclosingClass();
    return classDef->getName().compare(BuiltInTypes::arrayTypeName) == 0;
}

void MethodCallExpression::checkBuiltInArrayMethodPlaceholderTypes(
    const Context& context) {

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
    auto arrayType = context.getArrayType();
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

    auto argument = arguments.front();
    const auto argumentType = argument->getType();

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
        std::unique_ptr<Type>
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

    const auto argumentType = arguments.front()->getType();

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
    for (auto argumentType: argumentTypes) {
        if (addComma) {
            error += ", ";
        }
        error += argumentType->toString();
        addComma = true;
    }
    error += ")\nCandidates are:\n";
    for (auto candidate: candidates) {
        error += candidate->toString() + "\n";
    }
    Trace::error(error, this);
}

Expression* MethodCallExpression::transformDueToLambda(
    const MethodDefinition* methodDefinition,
    Context& context) {

    auto lambdaSignature = methodDefinition->getLambdaSignature();
    lambda->setLambdaSignature(lambdaSignature);
    if (lambda->getArguments().size() !=
        lambdaSignature->getArguments().size()) {
        Trace::error("Wrong number of arguments in lambda expression.",
                     this);
    }

    if (isBuiltInArrayMethod() &&
        name.compare(BuiltInTypes::arrayEachMethodName) == 0) {
        // For special array.each() lambda: transform this
        // MethodCallExpression into a block statement with a for loop
        // containing the lambda.
        return transformIntoForStatement(context);
    } else {
        // For normal lambda: Clone and inline the called method and lambda
        // in the calling method.
        return inlineCalledMethod(context);
    }
}

Expression* MethodCallExpression::inlineCalledMethod(Context& context) {
    auto calledMethod = memberDefinition->cast<MethodDefinition>();

    if (!calledMethod->hasBeenTypeCheckedAndTransformedBefore()) {
        // The inlined method must be transformed in order to turn the named
        // entities into data member expressions, which can be transformed into
        // thisPtr.datamember when the method is inlined into another class.
        calledMethod->typeCheckAndTransform();
    }

    auto clonedBody = calledMethod->getBody()->clone();
    clonedBody->setEnclosingBlock(context.getBlock());
    addArgumentsToInlinedMethodBody(clonedBody);

    // Save the lambda in the context so that the YieldExpression can transform
    // itself into the lambda block when the transform() pass is executed on the
    // inlined code.
    context.setLambdaExpression(lambda);

    if (calledMethod->getReturnType()->isVoid()) {
        // The called method does not return anything so we can simply replace
        // method call expression with the code of the inlined called method.
        auto wrappedBlockStatement =
            WrappedStatementExpression::create(clonedBody, getLocation());
        if (!calledMethod->isStatic()) {
            wrappedBlockStatement->setInlinedNonStaticMethod(true);
        }
        lambda = nullptr;
        return wrappedBlockStatement;
    } else {
        // The called method returns a value. Inline the method at the location
        // right before the current statement and transform this method call
        // into a reference to the temporary that holds the return value.
        auto temporary =
            inlineMethodWithReturnValue(clonedBody, calledMethod, context);
        lambda = nullptr;
        return temporary;
    }
}

void MethodCallExpression::addArgumentsToInlinedMethodBody(
    BlockStatement* clonedBody) {

    const Location& location = getLocation();
    auto calledMethod = memberDefinition->cast<MethodDefinition>();
    const ArgumentList& argumentsFromSignature =
        calledMethod->getArgumentList();
    auto i = argumentsFromSignature.cbegin();
    auto j = arguments.cbegin();

    while (j != arguments.end()) {
        VariableDeclaration* signatureArgument = *i;
        Expression* argumentExpression = *j;
        auto argumentDeclaration =
            VariableDeclarationStatement::create(
                signatureArgument->getType(),
                signatureArgument->getIdentifier(),
                argumentExpression,
                location);

        // Since the inlined method takes a lambda, the arguments names have
        // already been changed into being unique during an earlier pass.
        // Therefore, flag the variable name as unique so that we do not change
        // the name again when the variable declaration is type checked.
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
    auto returnType = calledMethod->getReturnType()->clone();
    auto currentBlock = context.getBlock();

    // Remove any write-protection from the temporary return variable so that it
    // can be assigned a value in the inlined code.
    returnType->setConstant(false);

    // Insert the declaration of the temporary that holds the return value.
    auto retvalTmpDeclarationStatement =
        VariableDeclarationStatement::generateTemporary(returnType,
                                                        retvalName,
                                                        nullptr,
                                                        location);
    currentBlock->insertBeforeCurrentStatement(retvalTmpDeclarationStatement);
    auto retvalTmpDeclaration = retvalTmpDeclarationStatement->getDeclaration();

    // Store pointer to the return value declaration so that return statements
    // in the inlined code can be transformed into assignments to the return
    // value.
    context.setTemporaryRetvalDeclaration(retvalTmpDeclaration);

    // Inline the called method body.
    currentBlock->insertBeforeCurrentStatement(clonedMethodBody);

    auto temporary =
        TemporaryExpression::create(retvalTmpDeclaration, location);
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

WrappedStatementExpression* MethodCallExpression::transformIntoForStatement(
    Context& context) {

    const Location& location = getLocation();

    auto outerBlock =
        BlockStatement::create(context.getClassDefinition(),
                               context.getBlock(),
                               location);

    auto indexVariableType = Type::create(Type::Integer);
    indexVariableType->setConstant(false);
    auto indexDeclaration =
        VariableDeclarationStatement::create(
            indexVariableType,
            indexVaraibleName,
            IntegerLiteralExpression::create(0, location),
            location);
    outerBlock->addStatement(indexDeclaration);

    auto arrayLengthDeclaration =
        VariableDeclarationStatement::create(
            Type::create(Type::Implicit),
            arrayLengthName,
            MemberSelectorExpression::create(
                NamedEntityExpression::create(arrayReferenceName, location),
                MethodCallExpression::create(
                    BuiltInTypes::arrayLengthMethodName,
                    location),
                location),
            location);
    outerBlock->addStatement(arrayLengthDeclaration);

    auto forCondition = BinaryExpression::create(
        Operator::Less,
        NamedEntityExpression::create(indexVaraibleName, location),
        NamedEntityExpression::create(arrayLengthName, location),
        location);

    auto forBlock =
        BlockStatement::create(context.getClassDefinition(),
                               outerBlock,
                               location);

    auto lambdaBlock =
        addLamdaArgumentsToLambdaBlock(forBlock,
                                       indexVaraibleName,
                                       arrayReferenceName);
    forBlock->insertStatementAtFront(lambdaBlock);

    auto increaseIndex =
        UnaryExpression::create(Operator::Increment,
                                NamedEntityExpression::create(indexVaraibleName,
                                                              location),
                                false,
                                location);

    auto forStatement = ForStatement::create(forCondition,
                                             increaseIndex,
                                             forBlock,
                                             location);
    outerBlock->addStatement(forStatement);

    auto wrappedBlockStatement =
        WrappedStatementExpression::create(outerBlock, location);
    wrappedBlockStatement->setInlinedArrayForEach(true);
    lambda = nullptr;
    return wrappedBlockStatement;
}

BlockStatement* MethodCallExpression::addLamdaArgumentsToLambdaBlock(
    BlockStatement* whileBlock,
    const Identifier& indexVaraibleName,
    const Identifier& arrayName) {

    const Location& location = getLocation();
    auto lambdaBlock = lambda->getBlock();
    lambdaBlock->setEnclosingBlock(whileBlock);

    const VariableDeclarationStatementList& lambdaArguments =
        lambda->getArguments();
    VariableDeclarationStatement* elementArgument = *lambdaArguments.begin();

    auto arraySubscript = ArraySubscriptExpression::create(
        NamedEntityExpression::create(arrayName, location),
        NamedEntityExpression::create(indexVaraibleName, location));

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

Traverse::Result MethodCallExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (auto argument: arguments) {
        argument->traverse(visitor);
    }

    if (lambda != nullptr) {
        lambda->traverse(visitor);
    }

    return Traverse::Continue;
}

void MethodCallExpression::resolveArgumentTypes(
    TypeList& typeList,
    const Binding::MethodList& candidates,
    Context& context) {

    if (isConstructorCall() &&
        name.compare(Keyword::stringString + "_" + Keyword::initString) == 0) {
        context.setIsStringConstructorCall(true);
    }

    unsigned int argumentIndex = 0;
    for (auto& expression: arguments) {
        if (auto anonymousFunction =
                expression->dynCast<AnonymousFunctionExpression>()) {
            anonymousFunction->inferArgumentTypes(candidates, argumentIndex);
        }
        expression = expression->transform(context);
        typeList.push_back(expression->typeCheck(context));
        argumentIndex++;
    }

    context.setIsStringConstructorCall(false);
    if (lambda != nullptr) {
        typeList.push_back(lambda->getType());
    }
}

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

HeapAllocationExpression* HeapAllocationExpression::create(
    MethodCallExpression* m) {

    return create(Type::create(m->getName()), m);
}

HeapAllocationExpression* HeapAllocationExpression::create(
    Type* t,
    MethodCallExpression* m) {

    return new HeapAllocationExpression(t, m);
}

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
        auto proxyConstructorCall =
            MethodCallExpression::create(classDefinition->getName() + "_Proxy",
                                     loc);
        if (processName != nullptr) {
            processName = processName->transform(context);
            if (!processName->typeCheck(context)->isString()) {
                Trace::error("Process name must be of string type.", this);
            }
            proxyConstructorCall->addArgument(processName);
        }

        return TypeCastExpression::create(
            allocatedObjectType->clone(),
            HeapAllocationExpression::create(proxyConstructorCall),
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

    auto inferredConcreteType = constructorCall->getInferredConcreteType();
    if (inferredConcreteType != nullptr) {
        type = inferredConcreteType;
    } else {
        type = allocatedObjectType;
    }

    if (auto def = type->getDefinition()->cast<ClassDefinition>()) {
        ClassList treePath;
        def->checkImplementsAllAbstractMethods(treePath, getLocation());
    }
    return type;
}

Traverse::Result HeapAllocationExpression::traverse(Visitor& visitor) {
    if (visitor.visitHeapAllocation(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    constructorCall->traverse(visitor);

    if (processName != nullptr) {
        processName->traverse(visitor);
    }

    return Traverse::Continue;
}

ClassDefinition* HeapAllocationExpression::lookupClass(Context& context) {
    lookupType(context);
    if (!allocatedObjectType->isReference()) {
        Trace::error("Only objects of reference type can be allocated using the"
                     " 'new' keyword. Allocated object type: " +
                     allocatedObjectType->toString(),
                     this);
    }

    constructorCall->setConstructorCallName(allocatedObjectType);
    auto definition = allocatedObjectType->getDefinition();
    assert(definition->isClass());
    return definition->cast<ClassDefinition>();
}

void HeapAllocationExpression::lookupType(const Context& context) {
    allocatedObjectType = context.lookupConcreteType(allocatedObjectType,
                                                     getLocation());
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

ArrayAllocationExpression* ArrayAllocationExpression::create(
    Type* t,
    Expression* c,
    const Location& l) {

    return new ArrayAllocationExpression(t, c, l);
}

ArrayAllocationExpression* ArrayAllocationExpression::create(
    Type* t,
    Expression* c) {

    return create(t, c, Location());
}

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
    lookupType(context);

    type = arrayType; 
    type->setArray(true);
    return type;
}

Traverse::Result ArrayAllocationExpression::traverse(Visitor& visitor) {
    if (visitor.visitArrayAllocation(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    if (arrayCapacityExpression != nullptr) {
        arrayCapacityExpression->traverse(visitor);
    }

    if (initExpression != nullptr) {
        initExpression->traverse(visitor);
    }

    return Traverse::Continue;
}

void ArrayAllocationExpression::lookupType(const Context& context) {
    arrayType = context.lookupConcreteType(arrayType, getLocation());
}

ArraySubscriptExpression::ArraySubscriptExpression(
    Expression* n,
    Expression* i) :
    Expression(Expression::ArraySubscript, n->getLocation()),
    arrayNameExpression(n),
    indexExpression(i) {}

ArraySubscriptExpression* ArraySubscriptExpression::create(
    Expression* n,
    Expression* i) {

    return new ArraySubscriptExpression(n, i);
}

Expression* ArraySubscriptExpression::clone() const {
    return new ArraySubscriptExpression(arrayNameExpression->clone(),
                                        indexExpression->clone());
}

Expression* ArraySubscriptExpression::transform(Context& context) {
    {
        Context::BindingsGuard guard(context);

        arrayNameExpression = arrayNameExpression->transform(context);
        auto arrayType = arrayNameExpression->typeCheck(context);
        if (!arrayType->isArray())
        {
            Trace::error("Not an array.", this);
        }
        type = Type::createArrayElementType(arrayType);
    }

    indexExpression = indexExpression->transform(context);
    auto indexExpressionReturnType = indexExpression->typeCheck(context);
    if (!indexExpressionReturnType->isIntegerNumber()) {
        Trace::error("Array index must be of integer type.", this);
    }

    if (auto binExpression = indexExpression->dynCast<BinaryExpression>()) {
        if (binExpression->getOperator() == Operator::Range) {
            auto transformedExpression =
                createSliceMethodCall(binExpression, context);
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

Traverse::Result ArraySubscriptExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    arrayNameExpression->traverse(visitor);
    indexExpression->traverse(visitor);
    return Traverse::Continue;
}

MemberSelectorExpression* ArraySubscriptExpression::createSliceMethodCall(
    BinaryExpression* range,
    Context& context) {

    auto sliceCall =
        MethodCallExpression::create(BuiltInTypes::arraySliceMethodName,
                                     getLocation());
    sliceCall->addArgument(range->getLeft());
    sliceCall->addArgument(range->getRight());

    auto memberSelector =
        MemberSelectorExpression::create(arrayNameExpression,
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

TypeCastExpression::TypeCastExpression(const TypeCastExpression& other) :
    Expression(other),
    targetType(other.targetType->clone()),
    operand(other.operand->clone()),
    staticCast(other.staticCast),
    isGenerated(other.isGenerated) {}

TypeCastExpression* TypeCastExpression::create(
    Type* target,
    Expression* o,
    const Location& l) {

    return new TypeCastExpression(target, o, l);
}

TypeCastExpression* TypeCastExpression::create(Type* target, Expression* o) {
    return create(target, o, Location());
}

Expression* TypeCastExpression::clone() const {
    return new TypeCastExpression(*this);
}

Type* TypeCastExpression::typeCheck(Context& context) {
    if (type != nullptr) {
        return type;
    }
    operand = operand->transform(context);
    auto fromType = operand->typeCheck(context);

    lookupTargetType(context);

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
    if (staticCast || isDowncast || isCastBetweenObjectAndInterface(fromType)) {
        type = targetType;
    } else if (!fromType->isReference() && !targetType->isReference() &&
               Type::areBuiltInsConvertable(fromType->getBuiltInType(),
                                            targetType->getBuiltInType())) {
        staticCast = true;
        type = targetType;
    } else if (Type::areEqualNoConstCheck(fromType, targetType) &&
               !fromType->isArray()){
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

void TypeCastExpression::lookupTargetType(const Context& context) {
    targetType = context.lookupConcreteType(targetType, getLocation());
}

Traverse::Result TypeCastExpression::traverse(Visitor& visitor) {
    if (visitor.visitTypeCast(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    operand->traverse(visitor);
    return Traverse::Continue;
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

    type = Type::create(Type::Null);
}

NullExpression* NullExpression::create(const Location& l) {
    return new NullExpression(l);
}

Expression* NullExpression::clone() const {
    return new NullExpression(getLocation());
}

ThisExpression::ThisExpression(const Location& l) :
    Expression(Expression::This, l) {}

ThisExpression* ThisExpression::create() {
    return create(Location());
}

ThisExpression* ThisExpression::create(const Location& l) {
    return new ThisExpression(l);
}

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
    type = Type::create(definition->getName());
    type->setDefinition(definition);
    return type;
}

BinaryExpression::BinaryExpression(
    Operator::Kind oper,
    Expression* l, 
    Expression* r, 
    const Location& loc) :
    Expression(Expression::Binary, loc),
    op(oper),
    left(l),
    right(r) {}

BinaryExpression* BinaryExpression::create(
    Operator::Kind oper,
    Expression* l,
    Expression* r,
    const Location& loc) {

    return new BinaryExpression(oper, l, r, loc);
}

BinaryExpression* BinaryExpression::create(
    Operator::Kind oper,
    Expression* l,
    Expression* r) {

    return create(oper, l, r, Location());
}

Expression* BinaryExpression::clone() const {
    return new BinaryExpression(op,
                                left->clone(),
                                right->clone(),
                                getLocation());
}

Type* BinaryExpression::typeCheck(Context& context) {
    // Left and right have already been transformed in
    // BinaryExpression::transform().
    auto leftType = left->typeCheck(context);
    auto rightType = right->typeCheck(context);

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
        case Operator::BitwiseAnd:
        case Operator::BitwiseOr:
        case Operator::BitwiseXor:
        case Operator::LeftShift:
        case Operator::RightShift:
        case Operator::Range:
        case Operator::Modulo:
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
        const auto method = context.getMethodDefinition();
        if (method->isEnumConstructor() || method->isEnumCopyConstructor() ||
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
    auto leftExpression = left;
    if (leftExpression->getKind() == Expression::ArraySubscript) {
        auto arraySubscript = leftExpression->cast<ArraySubscriptExpression>();
        leftExpression = arraySubscript->getArrayNameExpression();
    }
    if (auto dataMember = leftExpression->dynCast<DataMemberExpression>()) {
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
            return Type::create(Type::Boolean);
        case Operator::Assignment:
            return Type::create(Type::Void);
        default:
            return leftType;
    }
}

Expression* BinaryExpression::transform(Context& context) {
    left = left->transform(context);
    left->typeCheck(context);

    right = right->transform(context);
    right->typeCheck(context);

    inferTypes(context);
    auto leftType = left->getType();
    auto rightType = right->getType();

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
        return transformedExpression;
    } else if (leftType->isArray() && rightType->isArray() &&
               op != Operator::Assignment && op != Operator::Equal &&
               op != Operator::NotEqual) {
        MemberSelectorExpression* transformedExpression =
            createArrayOperation(context);
        return transformedExpression;
    } else if (Operator::isCompoundAssignment(op) && !leftType->isArray()) {
        BinaryExpression* transformedExpression = decomposeCompoundAssignment();
        return transformedExpression;
    } else {
        return this;
    }    
}

Traverse::Result BinaryExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    left->traverse(visitor);
    right->traverse(visitor);
    return Traverse::Continue;
}

void BinaryExpression::inferTypes(const Context& context) {
    Type* leftType = left->getType();
    Type* rightType = right->getType();

    if (leftType->isImplicit()) {
        if (rightType->isImplicit()) {
            Trace::error("Can not infer types.", leftType, rightType, this);
        }
        left->setType(inferTypeFromOtherSide(left, rightType, context));
    } else if (rightType->isImplicit()) {
        right->setType(inferTypeFromOtherSide(right, leftType, context));
    }
}

Type* BinaryExpression::inferTypeFromOtherSide(
    const Expression* implicitlyTypedExpr,
    const Type* otherSideType,
    const Context& context) {

    auto localVar = implicitlyTypedExpr->dynCast<LocalVariableExpression>();
    if (localVar == nullptr) {
        Trace::error("Can not infer type.",
                     left->getType(),
                     right->getType(),
                     this);
    }

    auto binding = context.lookup(localVar->getName());
    assert(binding != nullptr);
    if (binding->getReferencedEntity() != Binding::LocalObject) {
        Trace::error("Can not infer type.",
                     left->getType(),
                     right->getType(),
                     this);
    }

    auto varDeclaration = binding->getLocalObject();
    auto inferredType = otherSideType->clone();
    varDeclaration->setType(inferredType);
    return inferredType;
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
    auto operation = MethodCallExpression::create(operationName, getLocation());
    operation->addArgument(right);

    auto memberSelector =
        MemberSelectorExpression::create(left, operation, getLocation());
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
    auto operation = MethodCallExpression::create(operationName, getLocation());
    operation->addArgument(right);

    auto memberSelector =
        MemberSelectorExpression::create(left, operation, getLocation());
    return MemberSelectorExpression::transformMemberSelector(memberSelector,
                                                             context);
}

BinaryExpression* BinaryExpression::decomposeCompoundAssignment() {
    auto bin =
        BinaryExpression::create(Operator::getDecomposedArithmeticOperator(op),
                                 left,
                                 right,
                                 getLocation());
    return BinaryExpression::create(Operator::Assignment,
                                    left,
                                    bin,
                                    getLocation());
}

UnaryExpression::UnaryExpression(
    Operator::Kind oper,
    Expression* o, 
    bool p, 
    const Location& loc) :
    Expression(Expression::Unary, loc),
    op(oper),
    operand(o),
    prefix(p) {}

UnaryExpression* UnaryExpression::create(
    Operator::Kind oper,
    Expression* o,
    bool p,
    const Location& loc) {

    return new UnaryExpression(oper, o, p, loc);
}

Expression* UnaryExpression::clone() const {
    return new UnaryExpression(op, operand->clone(), prefix, getLocation());
}

Type* UnaryExpression::typeCheck(Context& context) {
    operand = operand->transform(context);
    type = operand->typeCheck(context);
    switch (op) {
        case Operator::BitwiseNot:
            if (!type->isIntegerNumber()) {
                Trace::error("Operator requires integer type operand.", this);
            }
            break;
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
            type = Type::create(Type::Boolean);
            break;
        default:
            Trace::error("Operator is incompatible with a unary expression.",
                         this);
            break;
    }
    return type;
}

Traverse::Result UnaryExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    operand->traverse(visitor);
    return Traverse::Continue;
}

LambdaExpression::LambdaExpression(BlockStatement* b, const Location& l) :
    Expression(Expression::Lambda, l),
    block(b),
    signature(nullptr) {

    type = Type::create(Type::Lambda);
    Tree::lookupAndSetTypeDefinition(type, l);
}

LambdaExpression::LambdaExpression(const LambdaExpression& other) :
    Expression(other),
    arguments(),
    block(other.block->clone()),
    signature(other.signature) {

    Utils::cloneList(arguments, other.arguments);
}

LambdaExpression* LambdaExpression::create(
    BlockStatement* b,
    const Location& l) {

    return new LambdaExpression(b, l);
}

LambdaExpression* LambdaExpression::create(BlockStatement* b) {
    return create(b, Location());
}

LambdaExpression* LambdaExpression::clone() const {
    return new LambdaExpression(*this);
}

void LambdaExpression::addArgument(VariableDeclarationStatement* argument) {
    arguments.push_back(argument);
}

Type* LambdaExpression::typeCheck(Context&) {
    return type;
}

Traverse::Result LambdaExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (auto argument: arguments) {
        argument->traverse(visitor);
    }

    block->traverse(visitor);
    return Traverse::Continue;
}

YieldExpression::YieldExpression(const Location& l) :
    Expression(Expression::Yield, l),
    arguments() {}

YieldExpression::YieldExpression(const YieldExpression& other) :
    Expression(other),
    arguments() {

    Utils::cloneList(arguments, other.arguments);
}

YieldExpression* YieldExpression::create(const Location& l) {
    return new YieldExpression(l);
}

Expression* YieldExpression::clone() const {
    return new YieldExpression(*this);
}

Expression* YieldExpression::transform(Context& context) {
    auto lambdaExpression = context.getLambdaExpression();
    if (lambdaExpression == nullptr) {
        return this;
    }

    // The lambda expression is set in the context. This should mean that we
    // are in an inlined method that takes a lambda. Inline the lambda at this
    // locatation.
    if (lambdaExpression->getLambdaSignature()->getReturnType()->isVoid()) {
        // Inline the lambda by transforming this YieldExpression into the
        // inlined lambda.
        auto inlinedLambda = inlineLambdaExpression(lambdaExpression,
                                                               context);
        auto wrappedBlockStatement =
            WrappedStatementExpression::create(inlinedLambda, getLocation());

        // Disallow any yield transformation while processing the inlined lambda
        // by temporarily resetting the lambda expression in the context. This
        // is to prevent yields from transforming in a lambda expression.
        wrappedBlockStatement->setDisallowYieldTransformation(true);
        return wrappedBlockStatement;
    } else {
        // The lambda returns a value so transform into a
        // LocalVariableExpression that references the returned value. Also,
        // inline the lambda before the current statement.
        auto lambdaRetvalTmp =
            inlineLambdaExpressionWithReturnValue(lambdaExpression, context);
        return lambdaRetvalTmp;
    }
}

LocalVariableExpression*
YieldExpression::inlineLambdaExpressionWithReturnValue(
    LambdaExpression* lambdaExpression,
    Context& context) {

    const Location& location = getLocation();

    auto lambdaRetvalTmpType =
        lambdaExpression->getLambdaSignature()->getReturnType()->clone();

    // Remove any write-protection from the temporary variable so that it can
    // be assigned a value in the lambda block.
    lambdaRetvalTmpType->setConstant(false);

    auto lambdaRetvalTmpDeclaration =
        VariableDeclarationStatement::generateTemporary(lambdaRetvalTmpType,
                                                        lambdaRetvalName,
                                                        nullptr,
                                                        location);

    auto currentBlock = context.getBlock();
    currentBlock->insertBeforeCurrentStatement(lambdaRetvalTmpDeclaration);
    auto inlinedLambda = inlineLambdaExpression(lambdaExpression, context);

    // Run typeCheck() pass on the inlined lambda. Disallow any yield
    // transformation while processing the inlined lambda by temporarily
    // resetting the lambda expression in the context. This is to prevent
    // yields from transforming in a lambda expression.
    context.setLambdaExpression(nullptr);
    inlinedLambda->typeCheck(context);
    context.setLambdaExpression(lambdaExpression);

    inlinedLambda->returnLastExpression(lambdaRetvalTmpDeclaration);
    currentBlock->insertBeforeCurrentStatement(inlinedLambda);
    auto lambdaRetvalTmp =
        LocalVariableExpression::create(
            lambdaRetvalTmpType,
            lambdaRetvalTmpDeclaration->getIdentifier(),
            location);
    return lambdaRetvalTmp;
}

BlockStatement* YieldExpression::inlineLambdaExpression(
    LambdaExpression* lambdaExpression,
    Context& context) {

    const Location& location = lambdaExpression->getLocation();

    auto clonedLambdaBody = lambdaExpression->getBlock()->clone();
    clonedLambdaBody->setEnclosingBlock(context.getBlock());

    const VariableDeclarationStatementList& lambdaArguments =
        lambdaExpression->getArguments();
    auto i = lambdaArguments.cbegin();
    auto j = arguments.cbegin();
    while (i != lambdaArguments.cend()) {
        VariableDeclarationStatement* lambdaArgument = *i;
        Expression* yieldArgumentExpression = *j;
        auto argumentDeclaration =
            VariableDeclarationStatement::create(lambdaArgument->getType(),
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
    auto lambdaSignature = context.getMethodDefinition()->getLambdaSignature();
    const TypeList& signatureArgumentTypes = lambdaSignature->getArguments();
    if (signatureArgumentTypes.size() != arguments.size()) {
        Trace::error("Wrong number of arguments in yield expression.", this);    
    }

    auto i = signatureArgumentTypes.cbegin();
    auto j = arguments.begin();

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

Traverse::Result YieldExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (auto argument: arguments) {
        argument->traverse(visitor);
    }

    return Traverse::Continue;
}

AnonymousFunctionExpression::AnonymousFunctionExpression(
    BlockStatement* b,
    const Location& l) :
    Expression(Expression::AnonymousFunction, l),
    argumentList(),
    body(b) {}

AnonymousFunctionExpression::AnonymousFunctionExpression(
    const AnonymousFunctionExpression& other) :
    Expression(other),
    argumentList(),
    body(other.body->clone()) {

    for (auto argument: other.argumentList) {
        addArgument(argument->clone());
    }
}

AnonymousFunctionExpression* AnonymousFunctionExpression::create(
    BlockStatement* b,
    const Location& l) {

    return new AnonymousFunctionExpression(b, l);
}

Expression* AnonymousFunctionExpression::clone() const {
    return new AnonymousFunctionExpression(*this);
}

Type* AnonymousFunctionExpression::typeCheck(Context&) {
    // The AnonymousFunctionExpression should have been transformed into another
    // type of expression by the transform() method.
    Trace::internalError("AnonymousFunctionExpression::typeCheck");
    return nullptr;
}

Expression* AnonymousFunctionExpression::transform(Context& context) {
    // The anonymous function will be converted into a reference to an object
    // containing both the function and the non-local variables used in the
    // function.
    Closure::Info closureInfo;
    Closure::generateClass(Tree::getCurrentTree(), this, context, closureInfo);

    const Location& loc = getLocation();
    auto constructorCall =
        MethodCallExpression::create(closureInfo.className, loc);
    for (auto nonLocalVarDecl: closureInfo.nonLocalVars) {
        constructorCall->addArgument(
            NamedEntityExpression::create(nonLocalVarDecl->getIdentifier(),
                                          loc));
    }

    return TypeCastExpression::create(
        closureInfo.closureInterfaceType,
        HeapAllocationExpression::create(constructorCall),
        loc);
}

Traverse::Result AnonymousFunctionExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    body->traverse(visitor);
    return Traverse::Continue;
}

void AnonymousFunctionExpression::addArgument(VariableDeclaration* argument) {
    argumentList.push_back(argument);
    body->addLocalBinding(argument);
}

void AnonymousFunctionExpression::inferArgumentTypes(
    const Binding::MethodList& candidates,
    unsigned int anonymousFunctionArgumentIndex) {

    if (argumentList.size() == 0) {
        return;
    }

    for (auto candidate: candidates) {
        const ArgumentList& candidateArguments = candidate->getArgumentList();
        if (anonymousFunctionArgumentIndex < candidateArguments.size()) {
            const auto argument =
                candidateArguments[anonymousFunctionArgumentIndex];
            const auto argumentClass = argument->getType()->getClass();
            if (argumentClass->isClosure()) {
                const MemberMethodList& closureClassMethods =
                    argumentClass->getMethods();
                for (auto method: closureClassMethods) {
                    if (method->getName().compare(CommonNames::callMethodName)
                            == 0) {
                        const ArgumentList& closureArguments =
                            method->getArgumentList();
                        if (argumentList.size() == closureArguments.size()) {
                            copyArgumentTypes(closureArguments);
                        }
                    }
                }
            }
        }
    }
}

void AnonymousFunctionExpression::copyArgumentTypes(const ArgumentList& from) {
    assert(argumentList.size() == from.size());

    auto i = argumentList.begin();
    auto j = from.cbegin();
    while (i != argumentList.end()) {
        VariableDeclaration* argument = *i;
        const VariableDeclaration* fromArgument = *j;
        argument->setType(fromArgument->getType()->clone());
        i++;
        j++;
    }
}

MatchCase::MatchCase(const Location& loc) :
    Node(loc),
    patternExpressions(),
    patterns(),
    patternGuard(nullptr),
    resultBlock(nullptr),
    isExhaustive(false) {}

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

MatchCase* MatchCase::create(const Location& loc) {
    return new MatchCase(loc);
}

MatchCase* MatchCase::create() {
    return create(Location());
}

Traverse::Result MatchCase::traverse(Visitor& visitor) {
    for (auto expression: patternExpressions) {
        expression->traverse(visitor);
    }

    if (patternGuard != nullptr) {
        patternGuard->traverse(visitor);
    }

    if (resultBlock != nullptr) {
        resultBlock->traverse(visitor);
    }

    return Traverse::Continue;
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

    resultBlock = BlockStatement::create(currentClass,
                                         currentBlock,
                                         getLocation());
    resultBlock->addStatement(resultExpr);
}

void MatchCase::buildPatterns(Context& context) {
    for (auto expression: patternExpressions) {
        patterns.push_back(Pattern::create(expression, context));
    }
}

bool MatchCase::isMatchExhaustive(
    const Expression* subject,
    MatchCoverage& coverage,
    Context& context) {

    for (auto pattern: patterns) {
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
    const Expression* subject,
    Context& context) {

    auto i = patterns.cbegin();
    Pattern* pattern = *i;
    auto binExpression =
        pattern->generateComparisonExpression(subject, context);
    i++;
    while (i != patterns.cend()) {
        pattern = *i;
        binExpression = BinaryExpression::create(
            Operator::LogicalOr,
            binExpression,
            pattern->generateComparisonExpression(subject, context),
            getLocation());
        i++;
    }
    return binExpression;
}

bool MatchCase::generateTemporariesCreatedByPatterns(BlockStatement* block) {
    bool anyTemporaries = false;

    for (auto pattern: patterns) {
        const VariableDeclarationStatementList& temps =
            pattern->getTemporariesCreatedByPattern();
        if (!temps.empty()) {
            anyTemporaries = true;
            for (auto varDeclaration: temps) {
                block->addStatement(varDeclaration);
            }
        }
    }

    return anyTemporaries;
}

Type* MatchCase::generateCaseBlock(
    BlockStatement* caseBlock,
    Context& context,
    const Expression* subject,
    const Identifier& matchResultTmpName,
    const Identifier& matchEndLabelName) {

    auto expression = generateComparisonExpression(subject, context);
    bool anyTemporaries = generateTemporariesCreatedByPatterns(caseBlock);

    if (isExhaustive && !anyTemporaries) {
        return generateCaseResultBlock(caseBlock,
                                       context,
                                       matchResultTmpName,
                                       matchEndLabelName);
    }

    const Location& location = getLocation();
    auto caseResultBlock =
        BlockStatement::create(context.getClassDefinition(),
                               caseBlock,
                               location);
    auto caseResultType = generateCaseResultBlock(caseResultBlock,
                                                  context,
                                                  matchResultTmpName,
                                                  matchEndLabelName);
    auto ifStatement = IfStatement::create(expression,
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
    auto caseResultBlock = chooseCaseResultBlock(block, context);

    if (resultBlock != nullptr) {
        caseResultBlock->copyStatements(*resultBlock);
    }

    Context blockContext(context);
    block->typeCheck(blockContext);
    auto lastExpression = caseResultBlock->getLastStatementAsExpression();
    if (lastExpression != nullptr) {
        caseResultType = lastExpression->getType();
        if (!caseResultType->isVoid()) {
            const Location& resultLocation = lastExpression->getLocation();
            caseResultBlock->replaceLastStatement(
                BinaryExpression::create(Operator::Assignment,
                                         NamedEntityExpression::create(
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
        caseResultBlock->addStatement(JumpStatement::create(matchEndLabelName,
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
        auto caseResultBlock =
            BlockStatement::create(context.getClassDefinition(),
                                   outerBlock,
                                   getLocation());
        outerBlock->addStatement(
            IfStatement::create(patternGuard,
                                caseResultBlock,
                                nullptr,
                                patternGuard->getLocation()));
        return caseResultBlock;
    }
}

void MatchCase::generateVariablesCreatedByPatterns(BlockStatement* block) {
    assert(!patterns.empty());

    checkVariablesCreatedByPatterns();
    const auto pattern = patterns.front();

    for (auto varDeclaration: pattern->getVariablesCreatedByPattern()) {
        block->addStatement(varDeclaration);
    }
}

void MatchCase::checkVariablesCreatedByPatterns() {
    auto i = patterns.cbegin();
    const Pattern* firstPattern = *i;
    const VariableDeclarationStatementList& firstPatternVariables =
        firstPattern->getVariablesCreatedByPattern();
    i++;
    while (i != patterns.cend()) {
        const Pattern* pattern = *i;
        const VariableDeclarationStatementList& patternVariables =
            pattern->getVariablesCreatedByPattern();
        if (firstPatternVariables.size() != patternVariables.size()) {
            Trace::error("All patterns in a case must bind the same variables.",
                         this);
        }

        for (auto firstPatternVariable: firstPatternVariables) {
            bool variableFound = false;
            for (auto patternVariable: patternVariables) {
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

MatchExpression::MatchExpression(const MatchExpression& other) :
    Expression(other),
    subject(other.subject->clone()),
    cases(),
    isGenerated(other.isGenerated) {

    Utils::cloneList(cases, other.cases);
}

MatchExpression* MatchExpression::create(Expression* s, const Location& loc) {
    return new MatchExpression(s, loc);
}

MatchExpression* MatchExpression::create(Expression* s) {
    return create(s, Location());
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
    auto matchLogic = generateMatchLogic(context, resultTmpName);
    if (type->isVoid()) {
        // The MatchExpression does not return anything so transform it into a
        // WrappedStatement that contains the match logic. No need to run
        // typeCheck() on the generated match logic code from here, since the
        // typeCheck() method on the WrappedStatementExpression will be called
        // by the containing BlockStatement.
        return WrappedStatementExpression::create(matchLogic, location);
    } else {
        // The MatchExpression returns a value so transform it into a
        // LocalVariableExpression that references the match result.
        auto resultTmpDeclaration =
            VariableDeclarationStatement::create(type,
                                                 resultTmpName,
                                                 nullptr,
                                                 location);

        // Temporarily remove the write-protection from the result temp variable
        // so that results can be assigned in the match logic.
        type->setConstant(false);

        auto currentBlock = context.getBlock();
        currentBlock->insertBeforeCurrentStatement(resultTmpDeclaration);

        // Run typeCheck() pass on the generated match logic code.
        matchLogic->typeCheck(context);
        type->setConstant(true);
        currentBlock->insertBeforeCurrentStatement(matchLogic);

        auto resultTemporary =
            LocalVariableExpression::create(type,
                                            resultTmpName,
                                            location);
        return resultTemporary;
    }
}

BlockStatement* MatchExpression::generateMatchLogic(
    Context& context,
    const Identifier& resultTmpName) {

    const Location& location = getLocation();

    auto matchLogicBlock =
        BlockStatement::create(context.getClassDefinition(),
                               context.getBlock(),
                               location);

    auto subjectRefExpr = generateSubjectTemporary(matchLogicBlock);

    if (subjectRefExpr->getType()->isArray()) {
        matchLogicBlock->addStatement(
            ArrayPattern::generateMatchSubjectLengthDeclaration(
                subjectRefExpr));
    }

    Identifier matchEndLabelName = generateMatchEndLabelName();

    for (auto matchCase: cases) {
        auto caseBlock =
            BlockStatement::create(context.getClassDefinition(),
                                   matchLogicBlock,
                                   location);
        auto caseResultType =
            matchCase->generateCaseBlock(caseBlock,
                                         context,
                                         subjectRefExpr,
                                         resultTmpName,
                                         matchEndLabelName);
        checkResultType(caseResultType, matchCase);
        matchLogicBlock->addStatement(caseBlock);
    }

    if (!matchEndLabelName.empty()) {
        auto matchEndLabel =
            LabelStatement::create(matchEndLabelName, location);
        matchLogicBlock->addStatement(matchEndLabel);
    }

    return matchLogicBlock;
}

Expression* MatchExpression::generateSubjectTemporary(
    BlockStatement* matchLogicBlock) {

    Expression* subjectRefExpr = nullptr;
    if (subject->isVariable() || subject->getKind() == Expression::This) {
        subjectRefExpr = subject;
    } else {
        const Location& location = subject->getLocation();
        auto subjectType = subject->getType();
        auto subjectTmpDeclaration =
            VariableDeclarationStatement::create(subjectType,
                                                 CommonNames::matchSubjectName,
                                                 subject,
                                                 location);
        matchLogicBlock->addStatement(subjectTmpDeclaration);
        subjectRefExpr =
            LocalVariableExpression::create(subjectType,
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
    const Type* caseResultType,
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
    for (auto matchCase: cases) {
        matchCase->buildPatterns(context);
    }
}

void MatchExpression::checkCases(Context& context) {
    MatchCoverage coverage(subject->getType());
    for (auto i = cases.cbegin(); i != cases.cend(); i++) {
        auto matchCase = *i;
        if (matchCase->isMatchExhaustive(subject, coverage, context)) {
            if (i + 1 != cases.end()) {
                auto unreachableMatchCase = *(++i);
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

bool MatchExpression::mayFallThrough() const {
    if (cases.empty()) {
        return true;
    }

    for (auto matchCase: cases) {
        if (matchCase->getResultBlock()->mayFallThrough()) {
            return true;
        }
    }
    return false;
}

Traverse::Result MatchExpression::traverse(Visitor& visitor) {
    if (visitor.visitStatement(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    subject->traverse(visitor);

    for (auto matchCase: cases) {
        matchCase->traverse(visitor);
    }

    return Traverse::Continue;
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

    for (const auto& otherMember: other.members) {
        Member member;
        member.nameExpr = otherMember.nameExpr->clone();
        member.patternExpr = otherMember.patternExpr ?
                             otherMember.patternExpr->clone() : nullptr;
        members.push_back(member);
    }
}

ClassDecompositionExpression* ClassDecompositionExpression::create(
    Type* t,
    const Location& l) {

    return new ClassDecompositionExpression(t, l);
}

ClassDecompositionExpression* ClassDecompositionExpression::clone() const {
    return new ClassDecompositionExpression(*this);
}

Type* ClassDecompositionExpression::typeCheck(Context& context) {
    lookupType(context);
    return type;
}

void ClassDecompositionExpression::lookupType(const Context& context) {
    type = context.lookupConcreteType(type, getLocation());
}

Traverse::Result ClassDecompositionExpression::traverse(Visitor& visitor) {
    if (visitor.visitClassDecomposition(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (const auto& member: members) {
        auto nameExpr = member.nameExpr;
        if (nameExpr != nullptr) {
            nameExpr->traverse(visitor);
        }

        auto patternExpr = member.patternExpr;
        if (patternExpr != nullptr) {
            patternExpr->traverse(visitor);
        }
    }

    return Traverse::Continue;
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

TypedExpression* TypedExpression::create(
    Type* targetType,
    Expression* n,
    const Location& l) {

    return new TypedExpression(targetType, n, l);
}

TypedExpression* TypedExpression::clone() const {
    return new TypedExpression(*this);
}

Type* TypedExpression::typeCheck(Context& context) {
    lookupType(context);
    return type;
}

Traverse::Result TypedExpression::traverse(Visitor& visitor) {
    if (visitor.visitTypedExpression(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    resultName->traverse(visitor);
    return Traverse::Continue;
}

void TypedExpression::lookupType(const Context& context) {
    type = context.lookupConcreteType(type, getLocation());
}

PlaceholderExpression::PlaceholderExpression(const Location& l) :
    Expression(Expression::Placeholder, l) {}

PlaceholderExpression::PlaceholderExpression() :
    Expression(Expression::Placeholder, Location()) {}

PlaceholderExpression* PlaceholderExpression::create(const Location& l) {
    return new PlaceholderExpression(l);
}

PlaceholderExpression* PlaceholderExpression::create() {
    return create(Location());
}

Expression* PlaceholderExpression::clone() const {
    return new PlaceholderExpression(getLocation());
}

WildcardExpression::WildcardExpression(const Location& l) :
    Expression(Expression::Wildcard, l) {}

WildcardExpression* WildcardExpression::create(const Location& l) {
    return new WildcardExpression(l);
}

Expression* WildcardExpression::clone() const {
    return new WildcardExpression(getLocation());
}

TemporaryExpression::TemporaryExpression(
    VariableDeclaration* d,
    const Location& l) :
    Expression(Expression::Temporary, l),
    declaration(d),
    nonStaticInlinedMethodBody(nullptr) {}

TemporaryExpression* TemporaryExpression::create(
    VariableDeclaration* d,
    const Location& l) {

    return new TemporaryExpression(d, l);
}

Expression* TemporaryExpression::clone() const {
    return new TemporaryExpression(declaration->clone(),
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

WrappedStatementExpression* WrappedStatementExpression::create(
    Statement* s,
    const Location& l) {

    return new WrappedStatementExpression(s, l);
}

Expression* WrappedStatementExpression::clone() const {
    return new WrappedStatementExpression(*this);
}

Type* WrappedStatementExpression::typeCheck(Context& context) {
    if (disallowYieldTransformation) {
        auto lambdaExpression = context.getLambdaExpression();
        context.setLambdaExpression(nullptr);
        type = statement->typeCheck(context);
        context.setLambdaExpression(lambdaExpression);
        disallowYieldTransformation = false;
    } else {
        type = statement->typeCheck(context);
    }
    return type;
}
