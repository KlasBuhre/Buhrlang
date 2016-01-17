#include <assert.h>

#include "Type.h"
#include "Definition.h"
#include "Expression.h"

Type Type::voidTypeInstance(Void);
Type Type::nullTypeInstance(Null);

Type::Type(const Identifier& n) : 
    builtInType(NotBuiltIn),
    name(n),
    genericTypeParameters(),
    definition(nullptr),
    functionSignature(nullptr),
    constant(true),
    reference(true), 
    array(false) {

    if (name.compare(Keyword::stringString) == 0) {
        builtInType = String;
    } else if (name.compare(Keyword::objectString) == 0) {
        builtInType = Object;
    }
}

Type::Type(BuiltInType t) : 
    builtInType(t), 
    name(),
    genericTypeParameters(),
    definition(nullptr),
    functionSignature(nullptr),
    constant(true),
    reference(false), 
    array(false) {

    switch (builtInType) {
        case Void:
            name = "void";
            break;
        case Placeholder:
            name = "_";
            break;
        case Implicit:
            name = "implicit";
            break;
        case Byte:
            name = Keyword::byteString;
            break;
        case Char:
            name = Keyword::charString;
            break;
        case Integer:
            name = Keyword::intString;
            break;
        case Float:
            name = Keyword::floatString;
            break;
        case Boolean:
            name = Keyword::boolString;
            break;
        case String:
            name = Keyword::stringString;
            reference = true;
            break;
        case Lambda:
            name = "lambda";
            reference = true;
            break;
        case Function:
            name = Keyword::funString;
            reference = true;
            break;
        case Object:
            name = Keyword::objectString;
            reference = true;
            break;
        default:
            break;
    }
}

Type::Type(const Type& other) :
    builtInType(other.builtInType),
    name(other.name),
    genericTypeParameters(),
    definition(other.definition),
    functionSignature(other.functionSignature ?
                      other.functionSignature->clone() : nullptr),
    constant(other.constant),
    reference(other.reference),
    array(other.array) {

    Utils::cloneList(genericTypeParameters, other.genericTypeParameters);
}

Type* Type::clone() const {
    return new Type(*this);
}

Type* Type::getAsMutable() const {
    return const_cast<Type*>(this);
}

Type* Type::create(const Identifier& name) {
    if (name.compare("void") == 0) {
        return new Type(Void);
    } else if (name.compare(Keyword::varString) == 0) {
        return new Type(Implicit);
    } else if (name.compare(Keyword::byteString) == 0) {
        return new Type(Byte);
    } else if (name.compare(Keyword::charString) == 0) {
        return new Type(Char);
    } else if (name.compare(Keyword::intString) == 0) {
        return new Type(Integer);
    } else if (name.compare(Keyword::floatString) == 0) {
        return new Type(Float);
    } else if (name.compare(Keyword::boolString) == 0) {
        return new Type(Boolean);
    } else if (name.compare(Keyword::stringString) == 0) {
        return new Type(String);
    } else if (name.compare(Keyword::objectString) == 0) {
        return new Type(Object);
    } else {
        return new Type(name);
    } 
}

Type* Type::createArrayElementType(const Type* arrayType) {
    if (!arrayType->isArray()) {
        return nullptr;
    }
    Type* elementType = arrayType->clone();
    elementType->setArray(false);
    if (!isReferenceType(elementType->getBuiltInType())) {
        elementType->setReference(false);
    }
    return elementType;
}

std::string Type::toString() const {
    std::string str;
    if (builtInType == Null) {
        str = "null";
    } else {
        if (!constant) {
            str += "var ";
        }
        if (hasGenericTypeParameters()) {
            str += getFullConstructedName();
        } else if (isFunction()) {
            str += getClosureInterfaceName();
        } else {
            str += name;
        }
        if (array) {
            str += "[]";
        }
    }
    return str;
}

bool Type::isReferenceType(BuiltInType builtInType) {
    switch (builtInType) {
        case Byte:
        case Char:
        case Integer:
        case Float:
        case Boolean:
        case Enumeration:
            return false;
        default:
            return true;
    }
}

bool Type::isNumber() const {
    switch (builtInType) {
        case Byte:
        case Integer:
        case Float:
            return true;
        default:
            return false;
    }
}

bool Type::isIntegerNumber() const {
    switch (builtInType) {
        case Byte:
        case Integer:
            return true;
        default:
            return false;
    }
}

bool Type::isPrimitive() const {
    switch (builtInType) {
        case Byte:
        case Char:
        case Integer:
        case Float:
        case Boolean:
            return true;
        default:
            return false;
    }
}

bool Type::isInterface() const {
    if (definition != nullptr && definition->isClass()) {
        ClassDefinition* classDef = definition->cast<ClassDefinition>();
        if (classDef->isInterface()) {
            return true;
        }
    }
    return false;
}

ClassDefinition* Type::getClass() const {
    if (definition != nullptr && definition->isClass()) {
        return definition->cast<ClassDefinition>();
    }
    return nullptr;
}

bool Type::operator==(const Type& other) const {
    if (areEqualNoConstCheck(this, &other) && constant == other.constant) {
        return true;
    }
    return false;
}

bool Type::operator != (const Type& other) const {
    return !(*this == other);
}

bool Type::areTypeParametersMatching(const Type* other) const {
    if (genericTypeParameters.size() != other->genericTypeParameters.size()) {
        return false;
    }

    auto i = genericTypeParameters.cbegin();
    auto j = other->genericTypeParameters.cbegin();
    while (i != genericTypeParameters.cend()) {
        Type* typeParameter = *i;
        Type* otherTypeParameter = *j;
        if (*typeParameter != *otherTypeParameter) {
            return false;
        }
        i++;
        j++;
    }
    return true;
}

bool Type::isMessageOrPrimitive() const {
    if (ClassDefinition* classDef = definition->cast<ClassDefinition>()) {
        if (!isPrimitive() && !classDef->isMessage()) {
            return false;
        }

        for (auto i = genericTypeParameters.cbegin();
             i != genericTypeParameters.cend();
             i++) {
            Type* typeParameter = *i;
            if (!typeParameter->isMessageOrPrimitive()) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void Type::setDefinition(Definition* d) {
    definition = d;
    if (definition->isClass()) {
        ClassDefinition* classDef = definition->cast<ClassDefinition>();
        if (classDef->isEnumeration()) {
            builtInType = Enumeration;
            if (!array) {
                reference = false;
            }
        }
        if (classDef->isEnumerationVariant()) {
            reference = false;
        }
    }
}

void Type::setReference(bool r) {
    reference = r;
}

void Type::setArray(bool a) {
    array = a;
    if (array) {
        reference = true;
    }
}

void Type::addGenericTypeParameter(Type* typeParameter) {
    genericTypeParameters.push_back(typeParameter);
}

bool Type::hasGenericTypeParameters() const {
    return !genericTypeParameters.empty();
}

Type* Type::getConcreteTypeAssignedToGenericTypeParameter() {
    assert(definition != nullptr);

    if (definition->isGenericTypeParameter()) {
        GenericTypeParameterDefinition* genericTypeParameter =
            definition->cast<GenericTypeParameterDefinition>();
        Type* concreteType = genericTypeParameter->getConcreteType();
        if (concreteType != nullptr) {
            Type* copiedConcreteType = concreteType->clone();
            copiedConcreteType->setArray(array);
            copiedConcreteType->setConstant(constant);
            return copiedConcreteType;
        }
    }
    return nullptr;
}

Identifier Type::getFullConstructedName() const {
    if (genericTypeParameters.empty()) {
        return name;
    }

    bool insertComma = false;
    Identifier fullName = name + '<';
    for (auto i = genericTypeParameters.cbegin();
         i != genericTypeParameters.cend();
         i++) {
        const Type* typeParameter = *i;
        if (insertComma) {
            fullName += ',';
        }
        fullName += typeParameter->getFullConstructedName();
        insertComma = true;
    }
    fullName += '>';

    return fullName;
}

Identifier Type::getClosureInterfaceName() const {
    assert(functionSignature != nullptr);

    Identifier interfaceName = Keyword::funString + ' ';
    const Type* returnType = functionSignature->getReturnType();
    if (returnType != nullptr) {
        interfaceName += returnType->toString();
    }

    bool insertComma = false;
    interfaceName += '(';
    const TypeList& arguments = functionSignature->getArguments();
    for (auto i = arguments.cbegin(); i != arguments.cend(); i++) {
        const Type* argumentType = *i;
        if (insertComma) {
            interfaceName += ',';
        }
        interfaceName += argumentType->toString();
        insertComma = true;
    }
    interfaceName += ')';

    return interfaceName;
}

bool Type::areEqualNoConstCheck(
    const Type* left,
    const Type* right,
    bool checkTypeParameters) {

    if (left->isPlaceholder() || right->isPlaceholder()) {
        if (left->isArray() != right->isArray()) {
            return false;
        }
        return true;
    }

    if (left->builtInType == right->builtInType &&
        left->name.compare(right->name) == 0 &&
        left->reference == right->reference &&
        left->array == right->array) {

        if (left->isFunction() && !left->getFunctionSignature()->equals(
                *(right->getFunctionSignature()))) {
            return false;
        }

        if (checkTypeParameters) {
            return left->areTypeParametersMatching(right);
        }
        return true;
    }
    return false;
}

bool Type::areInitializable(const Type* left, const Type* right) {
    if (left->isPlaceholder() || right->isPlaceholder()) {
        if (left->isArray() != right->isArray()) {
            return false;
        }
        return true;
    }

    if (left->isReference() && right->isNull()) {
        return true;
    }

    if (left->isEnumeration() && right->isEnumeration()) {
        if (left->name.compare(right->name) != 0 ||
            !left->areTypeParametersMatching(right)) {
            return false;
        }
    } else if (left->isFunction() && right->isFunction()) {
        if (!left->getFunctionSignature()->equals(
                *(right->getFunctionSignature()))) {
            return false;
        }
    } else if (left->isBuiltIn() && right->isBuiltIn()) {
        if (left->builtInType != right->builtInType &&
            !areBuiltInsImplicitlyConvertable(right->builtInType,
                                              left->builtInType)) {
            return false;
        }
    } else {
        // At least one type is not built-in. Check the class hierarchy.
        if (!areConvertable(left, right)) {
            return false;
        }
    }

    if (left->array != right->array) {
        return false;
    }
    return true;
}

bool Type::areAssignable(const Type* left, const Type* right) {
    if (left->isConstant()) {
        return false;
    }
    return areInitializable(left, right);
}

bool Type::isAssignableByExpression(const Type* left, Expression* expression) {
    if (left->isConstant()) {
        return false;
    }
    return isInitializableByExpression(left, expression);
}

bool Type::isUpcast(const Type* targetType) {
    if (isInterface() && targetType->isObject()) {
        return true;
    }

    if (definition->isClass() && targetType->definition->isClass()) {
        ClassDefinition* fromClass = definition->cast<ClassDefinition>();
        ClassDefinition* targetClass =
            targetType->definition->cast<ClassDefinition>();
        if (fromClass->isSubclassOf(targetClass)) {
            return true;
        }
    }
    return false;
}

bool Type::isDowncast(const Type* targetType) {
    if (isObject() && targetType->isInterface()) {
        return true;
    }

    if (definition->isClass() && targetType->definition->isClass()) {
        ClassDefinition* fromClass = definition->cast<ClassDefinition>();
        ClassDefinition* targetClass =
            targetType->definition->cast<ClassDefinition>();
        if (targetClass->isSubclassOf(fromClass)) {
            return true;
        }
    }
    return false;
}

bool Type::areConvertable(const Type* left, const Type* right) {
    if (left->name.compare(right->name) == 0 && 
        left->areTypeParametersMatching(right)) {
        return true;
    }

    if (left->isObject() && right->isInterface()) {
        return true;
    }

    if (left->definition->isClass() && right->definition->isClass()) {
        ClassDefinition* leftClass =
            left->definition->cast<ClassDefinition>();
        ClassDefinition* rightClass =
            right->definition->cast<ClassDefinition>();
        if (rightClass->isSubclassOf(leftClass)) {
            return true;
        }
    }
    return false;
}

bool Type::areBuiltInsImplicitlyConvertable(BuiltInType from, BuiltInType to) {
    switch (from) {
        case String:
            switch (to) {
                case Object:
                    return true;
                default:
                    return false;
            }
            break;
        case Byte:
            switch (to) {
                case Char:
                case Integer:
                case Float:
                    return true;
                default:
                    return false;
            }
            break;
        case Char:
            switch (to) {
                case Byte:
                case Integer:
                case Float:
                    return true;
                default:
                    return false;
            }
            break;
        default:
            return false;
    }
}

bool Type::areBuiltInsConvertable(BuiltInType from, BuiltInType to) {
    if (from == to) {
        return true;
    }

    switch (from) {
        case String:
            switch (to) {
                case Object:
                    return true;
                default:
                    return false;
            }
            break;
        case Byte:
            switch (to) {
                case Char:
                case Integer:
                case Float:
                    return true;
                default:
                    return false;
            }
            break;
        case Char:
            switch (to) {
                case Byte:
                case Integer:
                case Float:
                    return true;
                default:
                    return false;
            }
            break;
        case Integer:
            switch (to) {
                case Byte:
                case Char:
                case Float:
                    return true;
                default:
                    return false;
            }
            break;
        default:
            return false;
    }
}

bool Type::isInitializableByExpression(
    const Type* left,
    Expression* expression) {

    const Type* right = expression->getType();
    if (right == nullptr) {
        return false;
    }
    if (IntegerLiteralExpression* integerLiteral =
            expression->dynCast<IntegerLiteralExpression>()) {
        if (integerLiteral->getValue() < 256) {
            // Implicitly convert to byte.
            right = new Type(Type::Byte);
        }
    }
    return areInitializable(left, right);
}

const Type* Type::calculateCommonType(
    const Type* previousType,
    const Type* currentType) {

    if (previousType == nullptr) {
        return currentType;
    }

    if (currentType->isNull() && previousType->isReference()) {
        return previousType;
    } else if (previousType->isNull() && currentType->isReference()) {
        return currentType;
    } else {
        if (!areInitializable(previousType, currentType)) {
            return nullptr;
        }

        if (previousType->isEnumeration() && currentType->isEnumeration()) {
            const TypeList& previousTypeParameters =
                previousType->getGenericTypeParameters();
            const TypeList& currentTypeParameters =
                currentType->getGenericTypeParameters();

            assert(previousTypeParameters.size() ==
                   currentTypeParameters.size());

            auto i = previousTypeParameters.cbegin();
            auto j = currentTypeParameters.cbegin();
            while (i != previousTypeParameters.cend()) {
                const Type* previous = *i;
                const Type* current = *j;
                if (previous->isPlaceholder() && !current->isPlaceholder()) {
                    return currentType;
                }
                i++;
                j++;
            }
        }
        return previousType;
    }
}
