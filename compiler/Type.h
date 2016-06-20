#ifndef Type_h
#define Type_h

#include "CommonTypes.h"

class Definition;
class Expression;
class ClassDefinition;

class Type {
public:

    enum BuiltInType
    {
        NotBuiltIn,
        Void,
        Null,
        Placeholder,
        Object,
        Implicit,
        Byte,
        Char,
        Integer,
        Long,
        Float,
        Boolean,
        String,
        Lambda,
        Function,
        Enumeration
    };

    Type* clone() const;
    Type* getAsMutable() const;

    bool operator == (const Type& other) const;
    bool operator != (const Type& other) const;    
    std::string toString() const;

    static Type* create(BuiltInType t);
    static Type* create(const Identifier& name);
    static Type* createArrayElementType(const Type* arrayType);
    static bool isReferenceType(BuiltInType builtInType);
    static bool areEqualNoConstCheck(
        const Type* left,
        const Type* right,
        bool checkTypeParameters = true);
    static bool areAssignable(const Type* left, const Type* right);
    static bool isAssignableByExpression(
        const Type* left,
        Expression* expression);
    static bool areInitializable(const Type* left, const Type* right);
    static bool isInitializableByExpression(
        const Type* left,
        Expression* expression);
    static bool areBuiltInsConvertable(BuiltInType from, BuiltInType to);
    static const Type* calculateCommonType(
        const Type* previousType,
        const Type* currentType);

    void setDefinition(Definition* d);
    void setReference(bool r);
    void setArray(bool a);
    bool isUpcast(const Type* targetType);
    bool isDowncast(const Type* targetType);
    bool isNumber() const;
    bool isIntegerNumber() const;
    bool isPrimitive() const;
    bool isInterface() const;
    ClassDefinition* getClass() const;
    void addGenericTypeParameter(Type* typeParameter);
    bool hasGenericTypeParameters() const;
    Type* getConcreteTypeAssignedToGenericTypeParameter();
    Identifier getFullConstructedName() const;
    bool areTypeParametersMatching(const Type* other) const;
    Identifier getClosureInterfaceName() const;
    bool isMessageOrPrimitive() const;

    static Type& voidType() {
        return voidTypeInstance;
    }

    static Type& nullType() {
        return nullTypeInstance;
    }

    BuiltInType getBuiltInType() const {
        return builtInType;
    }

    void setBuiltInType(BuiltInType t) {
        builtInType = t;
    }

    bool isBuiltIn() const {
        return builtInType != NotBuiltIn;
    }

    void setFunctionSignature(FunctionSignature* s) {
        functionSignature = s;
    }

    const Identifier& getName() const {
        return name;
    }

    void setConstant(bool c) {
        constant = c;
    }

    TypeList& getGenericTypeParameters() {
        return genericTypeParameters;
    }

    const TypeList& getGenericTypeParameters() const {
        return genericTypeParameters;
    }

    Definition* getDefinition() {
        return definition;
    }

    const Definition* getDefinition() const {
        return definition;
    }

    FunctionSignature* getFunctionSignature() const {
        return functionSignature;
    }

    bool isVoid() const {
        return builtInType == Void;
    }

    bool isPlaceholder() const {
        return builtInType == Placeholder;
    }

    bool isObject() const {
        return builtInType == Object;
    }

    bool isImplicit() const {
        return builtInType == Implicit;
    }

    bool isString() const {
        return builtInType == String;
    }

    bool isBoolean() const {
        return builtInType == Boolean;
    }

    bool isReference() const {
        return reference;
    }

    bool isNull() const {
        return builtInType == Null;
    }

    bool isLambda() const {
        return builtInType == Lambda;
    }

    bool isFunction() const {
        return builtInType == Function;
    }

    bool isArray() const {
        return array;
    }

    bool isConstant() const {
        return constant;
    }

    bool isEnumeration() const {
        return builtInType == Enumeration;
    }

private:
    explicit Type(BuiltInType t);
    explicit Type(const Identifier& n);
    Type(const Type& other);

    static bool areConvertable(const Type* left, const Type* right);
    static bool areBuiltInsImplicitlyConvertable(
        BuiltInType from,
        BuiltInType to);

    static Type voidTypeInstance;
    static Type nullTypeInstance;

    BuiltInType builtInType;
    Identifier name;
    TypeList genericTypeParameters;
    Definition* definition;
    FunctionSignature* functionSignature;
    bool constant;
    bool reference;
    bool array;
};

#endif
