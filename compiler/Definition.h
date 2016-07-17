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

    virtual Definition* clone() const = 0;

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
    Definition(Kind k, const Identifier& n, const Location& l);
    Definition(const Definition& other);

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

using DefinitionList = std::list<Definition*>;
using GenericTypeParameterList = std::vector<GenericTypeParameterDefinition*>;
using ClassList = std::vector<ClassDefinition*>;
using MemberMethodList = std::vector<MethodDefinition*>;
using DataMemberList = std::vector<DataMemberDefinition*>;

class ClassDefinition: public Definition {
public:

    struct Properties {
        bool isInterface {false};
        bool isProcess {false};
        bool isMessage {false};
        bool isClosure {false};
        bool isGenerated {false};
        bool isEnumeration {false};
        bool isEnumerationVariant {false};
    };

    static ClassDefinition* create(
        const Identifier& name,
        NameBindings* enclosingBindings);
    static ClassDefinition* create(
        const Identifier& name,
        const GenericTypeParameterList& genericTypeParameters,
        const IdentifierList& parents,
        NameBindings* enclosingBindings,
        Properties& properties,
        const Location& location);

    ClassDefinition* clone() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    void appendMember(Definition* member);
    void insertMember(
        Definition* existingMember,
        Definition* newMember,
        bool insertAfterExistingMember = false);
    void addGenericTypeParameter(GenericTypeParameterDefinition* typeParameter);
    void addParent(ClassDefinition* parent);
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
    void generateEmptyCopyConstructor();
    MethodDefinition* getDefaultConstructor() const;
    bool isSubclassOf(const ClassDefinition* otherClass) const;
    bool isInheritingFromProcessInterface() const;
    void checkImplementsAllAbstractMethods(
        ClassList& treePath,
        const Location& loc);
    bool implements(const MethodDefinition* abstractMethod) const;
    bool isReferenceType();
    MethodDefinition* getMainMethod() const;
    MethodDefinition* getCopyConstructor() const;
    ClassDefinition* getNestedClass(const Identifier& className) const;
    bool isGeneric() const;
    void setConcreteTypeParameters(
        const TypeList& typeParameters,
        const Location& loc);
    bool needsCloneMethod();
    void generateCloneMethod();
    void transformIntoInterface();
    void copyMembers(const DefinitionList& from);
    Identifier getFullName() const;

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

    bool isMessage() const {
        return properties.isMessage;
    }

    bool isClosure() const {
        return properties.isClosure;
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
    ClassDefinition(
        const Identifier& name,
        const GenericTypeParameterList& genericTypeParameters,
        ClassDefinition* base,
        const ClassList& parents,
        NameBindings* enclosingBindings,
        const Properties& p,
        const Location& l);
    ClassDefinition(const ClassDefinition& other);

    void addMember(Definition* member);
    void addNestedClass(ClassDefinition* classDefinition);
    void addClassMemberDefinition(ClassMemberDefinition* member);
    void addDataMember(DataMemberDefinition* dataMember);
    void addMethod(MethodDefinition* newMethod);
    void copyParentClassesNameBindings();
    void copyGenericTypeParameters(const GenericTypeParameterList& from);
    bool allTypeParametersAreMessagesOrPrimitives() const;
    void removeCloneableParent();
    void removeMethod(const Identifier& methodName);
    void removeCopyConstructor();
    void updateConstructorName();
    MethodDefinition* generateEmptyConstructor();
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
    bool isRec;
};

class ClassMemberDefinition: public Definition {
public:

    enum Kind {
        DataMember,
        Method
    };

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

protected:
    ClassMemberDefinition(
        Kind k,
        const Identifier& name,
        AccessLevel::Kind a,
        bool s,
        const Location& l);
    ClassMemberDefinition(const ClassMemberDefinition& other);

private:
    Kind kind;
    AccessLevel::Kind access;
    bool staticMember;
};

class MethodDefinition: public ClassMemberDefinition {
public:

    static MethodDefinition* create(
        const Identifier& name,
        Type* retType,
        AccessLevel::Kind access,
        bool isStatic,
        Definition* e,
        const Location& l);
    static MethodDefinition* create(
        const Identifier& name,
        Type* retType,
        Definition* e);
    static MethodDefinition* create(
        const Identifier& name,
        Type* retType,
        bool isStatic,
        ClassDefinition* classDefinition);

    Definition* clone() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    void typeCheckAndTransform();
    void updateGenericTypesInSignature();
    void convertClosureTypesInSignature();
    std::string toString() const;
    void addArgument(VariableDeclaration* argument);
    void addArgument(Type::BuiltInType type, const Identifier& argumentName);
    void addArgument(Type* type, const Identifier& argumentName);
    void addArgument(
        const Identifier& typeName,
        const Identifier& argumentName);
    void addArguments(const ArgumentList& arguments);
    void setLambdaSignature(FunctionSignature* s, const Location& loc);
    void generateBaseClassConstructorCall(const Identifier& baseClassName);
    void generateMemberInitializationsFromConstructorArgumets(
        const ArgumentList& constructorArguments);
    void generateMemberInitializations(const DataMemberList& dataMembers);
    void generateMemberDefaultInitializations(
        const DataMemberList& dataMembers);
    bool isCompatible(const TypeList& arguments) const;
    bool argumentsAreEqual(const ArgumentList& arguments) const;
    bool implements(const MethodDefinition* abstractMethod) const;
    void checkReturnStatements();
    const ClassDefinition* getClass() const;
    void transformIntoAbstract();

    void setBody(BlockStatement* block) {
        body = block;
    }

    Type* getReturnType() const {
        return returnType;
    }

    void setReturnType(Type* t) {
        returnType = t;
    }

    const ArgumentList& getArgumentList() const {
        return argumentList;
    }

    BlockStatement* getBody() const {
        return body;
    }

    FunctionSignature* getLambdaSignature() const {
        return lambdaSignature;
    }

    bool isConstructor() const {
        return isCtor;
    }

    bool isEnumConstructor() const {
        return isEnumCtor;
    }

    bool isEnumCopyConstructor() const {
        return isEnumCopyCtor;
    }

    bool isFunction() const {
        return isFunc;
    }

    bool isGenerated() const {
        return generated;
    }

    bool isVirtual() const {
        return isVirt;
    }

    bool isAbstract() const {
        return body == nullptr;
    }

    bool hasBeenTypeCheckedAndTransformedBefore() const {
        return hasBeenTypeCheckedAndTransformed;
    }

    void setIsPrimaryConstructor(bool p) {
        isPrimaryCtor = p;
    }

    void setIsEnumConstructor(bool e) {
        isEnumCtor = e;
    }

    void setIsEnumCopyConstructor(bool e) {
        isEnumCopyCtor = e;
    }

    void setIsFunction(bool f) {
        isFunc = f;
    }

    void setIsClosure(bool c) {
        isClosure = c;
    }

    void setIsVirtual(bool v) {
        isVirt = v;
    }

    void setIsGenerated(bool g) {
        generated = g;
    }

    void setName(const Identifier& n) {
        name = n;
    }

private:
    MethodDefinition(
        const Identifier& name,
        Type* retType,
        AccessLevel::Kind access,
        bool isStatic,
        Definition* e,
        const Location& l);
    MethodDefinition(const MethodDefinition& other);

    void updateGenericReturnType(const NameBindings& nameBindings);
    void updateGenericTypesInArgumentList(const NameBindings& nameBindings);
    void updateGenericTypesInLambdaSignature(const NameBindings& nameBindings);
    void convertClosureTypesInArgumentList();
    void convertClosureTypesInLambdaSignature();
    void makeArgumentNamesUnique();
    void finishConstructor();
    void copyArgumentList(const ArgumentList& from);

    Type* returnType;
    ArgumentList argumentList;
    BlockStatement* body;
    FunctionSignature* lambdaSignature;
    bool isCtor;
    bool isPrimaryCtor;
    bool isEnumCtor;
    bool isEnumCopyCtor;
    bool isFunc;
    bool isClosure;
    bool isVirt;
    bool generated;
    bool hasBeenTypeCheckedAndTransformed;
};

class DataMemberDefinition: public ClassMemberDefinition {
public:

    static DataMemberDefinition* create(const Identifier& name, Type* typ);
    static DataMemberDefinition* create(
        const Identifier& name,
        Type* typ,
        AccessLevel::Kind access,
        bool isStatic,
        bool isPrimaryCtorArg,
        const Location& l);

    Definition* clone() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    void typeCheckAndTransform();
    void convertClosureType();

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
    DataMemberDefinition(
        const Identifier& name,
        Type* typ,
        AccessLevel::Kind access,
        bool isStatic,
        bool isPrimaryCtorArg,
        const Location& l);
    DataMemberDefinition(const DataMemberDefinition& other);

    void changeTypeIfGeneric(const NameBindings& nameBindings);
    void typeCheckInitExpression(Context& context);

    Type* type;
    Expression* expression;
    bool isPrimaryCtorArgument;
    bool hasBeenTypeCheckedAndTransformed;
};

class GenericTypeParameterDefinition: public Definition {
public:
    static GenericTypeParameterDefinition* create(
        const Identifier& name,
        const Location& l);

    GenericTypeParameterDefinition* clone() const override;

    void setConcreteType(Type* type) {
        concreteType = type;
    }

    Type* getConcreteType() const {
        return concreteType;
    }

private:
    GenericTypeParameterDefinition(const Identifier& name, const Location& l);
    GenericTypeParameterDefinition(const GenericTypeParameterDefinition& other);

    Type* concreteType;
};

class ForwardDeclarationDefinition: public Definition {
public:
    static ForwardDeclarationDefinition* create(const Identifier& name);

    ForwardDeclarationDefinition* clone() const override;

private:
    explicit ForwardDeclarationDefinition(const Identifier& name);
    ForwardDeclarationDefinition(const ForwardDeclarationDefinition& other);
};

#endif
