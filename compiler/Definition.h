#ifndef Definition_h
#define Definition_h

#include "CommonTypes.h"
#include "NameBindings.h"
#include "Type.h"

class ClassDefinition;

class Definition: public Node {
public:

    enum Kind {
        Class,
        Member,
        GenericTypeParameter,
        ForwardDeclaration
    };

    Definition(Kind k, const Identifier& n, const Location& l);
    Definition(const Definition& other);

    virtual Definition* clone() const = 0;
    virtual void process() = 0;

    ClassDefinition* getEnclosingClass() const;

    Kind getKind() const {
        return kind;
    }

    const Identifier& getName() const {
        return name;
    }

    Definition* getEnclosingDefinition() const {
        return enclosingDefinition;
    }

    void setEnclosingDefinition(Definition* definition) {
        enclosingDefinition = definition;
    }

    bool isImported() const {
        return imported;
    }

    void setIsImported(bool i) {
        imported = i;
    }

    bool isClass() const {
        return kind == Class;
    }

    bool isMember() const {
        return kind == Member;
    }

    bool isGenericTypeParameter() const {
        return kind == GenericTypeParameter;
    }

protected:
    Identifier name;
    Definition* enclosingDefinition;

private:
    Kind kind;
    bool imported;
};

class DataMemberDefinition;
class MethodDefinition;
class Context;
class ClassMemberDefinition;
class GenericTypeParameterDefinition;
class BlockStatement;
class ConstructorCallStatement;
class Expression;

typedef std::list<Definition*> DefinitionList;
typedef std::vector<GenericTypeParameterDefinition*> GenericTypeParameterList;
typedef std::vector<ClassDefinition*> ClassList;
typedef std::vector<MethodDefinition*> MemberMethodList;
typedef std::vector<DataMemberDefinition*> DataMemberList;

class ClassDefinition: public Definition {
public:

    struct Properties {
        Properties() :
            isInterface(false),
            isProcess(false),
            isGenerated(false),
            isEnumeration(false),
            isEnumerationVariant(false) {}

        bool isInterface;
        bool isProcess;
        bool isGenerated;
        bool isEnumeration;
        bool isEnumerationVariant;
    };

    ClassDefinition(
        const Identifier& name, 
        const GenericTypeParameterList& genericTypeParameters,
        ClassDefinition* base,
        const ClassList& parents,
        NameBindings* enclosingBindings,
        const Properties& p,
        const Location& l);
    ClassDefinition(const ClassDefinition& other);

    static ClassDefinition* create(
        const Identifier& name,
        const GenericTypeParameterList& genericTypeParameters,
        const IdentifierList& parents,
        NameBindings* enclosingBindings,
        const Properties& properties,
        const Location& location);

    virtual ClassDefinition* clone() const;
    virtual void process();

    void appendMember(Definition* member);
    void insertMember(
        Definition* existingMember,
        Definition* newMember,
        bool insertAfterExistingMember = false);
    void addGenericTypeParameter(GenericTypeParameterDefinition* typeParameter);
    Context& getMemberInitializationContext();
    Context& getStaticMemberInitializationContext();
    void addPrimaryCtorArgsAsDataMembers(
        const ArgumentList& constructorArguments);
    void addPrimaryConstructor(
        const ArgumentList& constructorArguments,
        ConstructorCallStatement* constructorCall);
    void generateConstructor();
    void generateDefaultConstructor();
    void generateDefaultConstructorIfNeeded();
    MethodDefinition* getDefaultConstructor() const;
    bool isSubclassOf(const ClassDefinition* otherClass) const;
    bool isInheritingFromProcessInterface() const;
    void checkImplementsAllAbstractMethods(
        ClassList& treePath,
        const Location& loc);
    bool implements(const MethodDefinition* abstractMethod) const;
    bool isReferenceType();
    MethodDefinition* getMainMethod() const;
    ClassDefinition* getNestedClass(const Identifier& className) const;
    bool isGeneric() const;
    void setConcreteTypeParameters(
        const TypeList& typeParameters,
        const Location& loc);
    void transformIntoInterface();
    void copyMembers(const DefinitionList& from);
    Identifier getFullName() const;

    void setProcessed(bool p) {
        hasBeenProcessed = p;
    }

    void setRecursive(bool r) {
        isRec = r;
    }

    bool isRecursive() const {
        return isRec;
    }

    const DefinitionList& getMembers() const {
        return members;
    }

    const MemberMethodList& getMethods() const {
        return methods;
    }

    const DataMemberList& getDataMembers() const {
        return dataMembers;
    }

    NameBindings& getNameBindings() {
        return nameBindings;
    }

    ClassDefinition* getBaseClass() const {
        return baseClass;
    }

    const ClassList& getParentClasses() const {
        return parentClasses;
    }

    const DataMemberList& getPrimaryCtorArgDataMembers() const {
        return primaryCtorArgDataMembers;
    }

    bool isInterface() const {
        return properties.isInterface;
    }

    bool isProcess() const {
        return properties.isProcess;
    }

    bool isGenerated() const {
        return properties.isGenerated;
    }

    bool isEnumeration() const {
        return properties.isEnumeration;
    }

    bool isEnumerationVariant() const {
        return properties.isEnumerationVariant;
    }

private:
    void processDataMembers();
    void processMethodSignatures();
    void addMember(Definition* member);
    void addNestedClass(ClassDefinition* classDefinition);
    void addClassMemberDefinition(ClassMemberDefinition* member);
    void addDataMember(DataMemberDefinition* dataMember);
    void addMethod(MethodDefinition* newMethod);
    void copyParentClassesNameBindings();
    void copyGenericTypeParameters(const GenericTypeParameterList& from);
    void updateConstructorName();
    MethodDefinition* generateEmptyConstructor();
    void finishGeneratedConstructor(MethodDefinition* constructor);
    bool isMethodImplementingParentInterfaceMethod(
        MethodDefinition* method) const;
    Context* createInitializationContext(
        const Identifier& methodName,
        bool isStatic);

    ClassDefinition* baseClass;   // Concrete base class.
    ClassList parentClasses;      // Concrete base class and interfaces.
    DefinitionList members;
    MemberMethodList methods;
    DataMemberList dataMembers;
    DataMemberList primaryCtorArgDataMembers;
    GenericTypeParameterList genericTypeParameters;
    NameBindings nameBindings;
    Context* memberInitializationContext;
    Context* staticMemberInitializationContext;
    Properties properties;
    bool hasConstructor;
    bool hasBeenProcessed;
    bool isRec;
};

class ClassMemberDefinition: public Definition {
public:

    enum Kind {
        DataMember,
        Method
    };

    ClassMemberDefinition(
        Kind k,
        const Identifier& name,
        AccessLevel::Kind a,
        bool s, 
        const Location& l);
    ClassMemberDefinition(const ClassMemberDefinition& other);

    Kind getKind() const {
        return kind;
    }

    AccessLevel::Kind getAccessLevelModifier() const {
        return access;
    }

    bool isStatic() const {
        return staticMember;
    }

    bool isDataMember() const {
        return kind == DataMember;
    }

    bool isMethod() const {
        return kind == Method;
    }

    bool isPrivate() const {
        return access == AccessLevel::Private;
    }

private:
    Kind kind;
    AccessLevel::Kind access;
    bool staticMember;
};

class MethodDefinition: public ClassMemberDefinition {
public:
    MethodDefinition(const Identifier& name, Type* retType, Definition* e);
    MethodDefinition(
        const Identifier& name, 
        Type* retType,
        AccessLevel::Kind access,
        bool isStatic, 
        Definition* e,
        const Location& l);
    MethodDefinition(const MethodDefinition& other);

    static MethodDefinition* create(
        const Identifier& name,
        Type* retType,
        bool isStatic,
        ClassDefinition* classDefinition);

    virtual Definition* clone() const;
    virtual void process();

    void processSignature();
    std::string toString() const;
    void addArgument(VariableDeclaration* argument);
    void addArgument(Type::BuiltInType type, const Identifier& argumentName);
    void addArgument(Type* type, const Identifier& argumentName);
    void addArgument(
        const Identifier& typeName,
        const Identifier& argumentName);
    void addArguments(const ArgumentList& arguments);
    void setLambdaSignature(LambdaSignature* s, const Location& loc);
    void generateBaseClassConstructorCall(const Identifier& baseClassName);
    void generateMemberInitializationsFromConstructorArgumets(
        const ArgumentList& constructorArguments);
    void generateMemberInitializations(const DataMemberList& dataMembers);
    void generateMemberDefaultInitializations(
        const DataMemberList& dataMembers);
    bool isCompatible(const ArgumentList& arguments) const;
    bool isCompatible(const TypeList& arguments) const;
    bool implements(const MethodDefinition* abstractMethod) const;
    const ClassDefinition* getClass() const;

    void transformIntoAbstract() {
        body = nullptr;
    }

    void setBody(BlockStatement* block) {
        body = block;
    }

    Type* getReturnType() const {
        return returnType;
    }

    const ArgumentList& getArgumentList() const {
        return argumentList;
    }

    BlockStatement* getBody() const {
        return body;
    }

    LambdaSignature* getLambdaSignature() const {
        return lambdaSignature;
    }

    bool isConstructor() const {
        return isCtor;
    }

    bool isEnumConstructor() const {
        return isEnumCtor;
    }

    bool isFunction() const {
        return isFunc;
    }

    bool isAbstract() const {
        return body == nullptr;
    }

    bool hasBeenProcessedBefore() const {
        return hasBeenProcessed;
    }

    void setIsEnumConstructor(bool e) {
        isEnumCtor = e;
    }

    void setIsFunction(bool f) {
        isFunc = f;
    }

    void setName(const Identifier& n) {
        name = n;
    }

private:
    void updateGenericReturnType(const NameBindings& nameBindings);
    void updateGenericTypesInArgumentList(const NameBindings& nameBindings);
    void updateGenericTypesInLambdaSignature(const NameBindings& nameBindings);
    void makeArgumentNamesUnique();
    void processConstructor();
    void checkForReturnStatement();
    void copyArgumentList(const ArgumentList& from);

    Type* returnType;
    ArgumentList argumentList;
    BlockStatement* body;
    LambdaSignature* lambdaSignature;
    bool isCtor;
    bool isEnumCtor;
    bool isFunc;
    bool hasBeenProcessed;
    bool hasSignatureBeenProcessed;
};

class DataMemberDefinition: public ClassMemberDefinition {
public:

    DataMemberDefinition(const Identifier& name, Type* typ);
    DataMemberDefinition(
        const Identifier& name, 
        Type* typ,
        AccessLevel::Kind access,
        bool isStatic,
        bool isPrimaryCtorArg,
        const Location& l);
    DataMemberDefinition(const DataMemberDefinition& other);

    virtual Definition* clone() const;
    virtual void process();

    static bool isDataMember(Definition* definition);

    void setExpression(Expression* e) {
        expression = e;
    }

    Expression* getExpression() const {
        return expression;
    }

    Type* getType() const {
        return type;
    }

    bool isPrimaryConstructorArgument() const {
        return isPrimaryCtorArgument;
    }

private:
    void changeTypeIfGeneric(const NameBindings& nameBindings);
    Expression* typeCheckInitExpression(Context& context);

    Type* type;
    Expression* expression;
    bool isPrimaryCtorArgument;
};

class GenericTypeParameterDefinition: public Definition {
public:
    GenericTypeParameterDefinition(const Identifier& name, const Location& l);
    GenericTypeParameterDefinition(const GenericTypeParameterDefinition& other);

    virtual GenericTypeParameterDefinition* clone() const;
    virtual void process() {}

    void setConcreteType(Type* type) {
        concreteType = type;
    }

    Type* getConcreteType() const {
        return concreteType;
    }

private:
    Type* concreteType;
};

class ForwardDeclarationDefinition: public Definition {
public:
    ForwardDeclarationDefinition(const Identifier& name);
    ForwardDeclarationDefinition(const ForwardDeclarationDefinition& other);

    virtual ForwardDeclarationDefinition* clone() const;
    virtual void process() {}
};

#endif
