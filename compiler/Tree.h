#ifndef Tree_h
#define Tree_h

#include <set>

#include "CommonTypes.h"
#include "NameBindings.h"
#include "Definition.h"

class Statement;
class BlockStatement;
class VariableDeclarationStatement;
class HeapAllocationExpression;
class ArrayAllocationExpression;
class TypeCastExpression;
class ClassDecompositionExpression;
class TypedExpression;
class NamedEntityExpression;
class MemberSelectorExpression;

class Visitor {
public:
    enum TraverseMask {
        TraverseClasses     = 0x1,
        TraverseDataMembers = 0x2,
        TraverseMethods     = 0x4,
        TraverseStatemets   = 0x8
    };

    explicit Visitor(unsigned int m);
    virtual ~Visitor() {}

    virtual Traverse::Result visitClass(ClassDefinition&);
    virtual Traverse::Result visitDataMember(DataMemberDefinition&);
    virtual Traverse::Result visitMethod(MethodDefinition&);
    virtual Traverse::Result visitStatement(Statement&);
    virtual Traverse::Result visitBlock(BlockStatement&);
    virtual Traverse::Result visitVariableDeclaration(
        VariableDeclarationStatement&);
    virtual Traverse::Result visitHeapAllocation(HeapAllocationExpression&);
    virtual Traverse::Result visitArrayAllocation(ArrayAllocationExpression&);
    virtual Traverse::Result visitTypeCast(TypeCastExpression&);
    virtual Traverse::Result visitClassDecomposition(
        ClassDecompositionExpression&);
    virtual Traverse::Result visitTypedExpression(TypedExpression&);
    virtual Traverse::Result visitNamedEntity(NamedEntityExpression&);
    virtual Traverse::Result visitMemberSelector(MemberSelectorExpression&);
    virtual void exitBlock() {}

    unsigned int getTraverseMask() const {
        return mask;
    }

private:
    unsigned int mask;
};

class Tree {
public:
    Tree();

    BlockStatement* startBlock();
    BlockStatement* startBlock(const Location& location);
    void setCurrentBlock(BlockStatement* block);
    BlockStatement* finishBlock();
    void addStatement(Statement* statement);
    void startGeneratedClass(
        const Identifier& name,
        ClassDefinition::Properties& properties);
    void startGeneratedClass(
        const Identifier& name,
        ClassDefinition::Properties& properties,
        const IdentifierList& parents);
    ClassDefinition* startClass(
        const Identifier& name, 
        const GenericTypeParameterList& genericTypeParameters,
        const IdentifierList& parents,
        ClassDefinition::Properties& properties,
        const Location& location);
    void reopenClass(ClassDefinition* classDefinition);
    ClassDefinition* finishClass();
    void startFunction();
    void finishFunction(MethodDefinition* function);
    void checkReturnStatements();
    void makeGenericTypesConcreteInSignatures();
    void convertClosureTypesInSigntures();
    void generateCloneMethods();
    void typeCheckAndTransform();
    void traverse(Visitor& visitor);
    void addClassMember(Definition* definition);
    void addClassDataMember(Type::BuiltInType type, const Identifier& name);
    void addClassDataMember(Type* type, const Identifier& name);
    void addGlobalDefinition(Definition* definition);
    void useNamespace(
        const Identifier& namespaceName,
        const Location& location);
    MethodDefinition* getMainMethod() const;
    void addImportedModule(const std::string& moduleName);
    bool isModuleAlreadyImported(const std::string& moduleName) const;
    ClassDefinition* getCurrentClass() const;
    BlockStatement* getCurrentBlock() const;
    void insertClassPostParse(
        ClassDefinition* closureClass,
        bool insertBefore = true);
    Type* convertToClosureInterfaceInCurrentTree(Type* type);

    static Tree& getCurrentTree();
    static void lookupAndSetTypeDefinition(
        Type* type,
        const Location& location);
    static void lookupAndSetTypeDefinition(
        Type* type,
        const NameBindings& scope,
        const Location& location);
    static Type* makeGenericTypeConcrete(
        Type* type,
        const NameBindings& scope,
        const Location& location);
    static Type* convertToClosureInterface(Type* type);

    const DefinitionList& getGlobalDefinitions() const {
        return globalDefinitions;
    }

    NameBindings& getGlobalNameBindings() {
        return globalNameBindings;
    }

    void setCurrentTree() {
        currentTree = this;
    }

private:

    enum Pass {
        Parse,
        CheckReturnStatements,
        MakeGenericTypesConcrete,
        ConvertClosureTypes,
        GenerateCloneMethods,
        TypeCheckAndTransform
    };

    ClassDefinition* insertBuiltInType(const Identifier& name);
    void insertBuiltInTypesInGlobalNameBindings();
    void addEqualsMethod(
        ClassDefinition* classDef,
        Type::BuiltInType builtInType);
    void generateNoArgsClosureInterface();
    void generateDeferClass();
    void generateArrayClass();
    NameBindings& getCurrentNameBindings();
    void lookupAndSetTypeDefinitionInCurrentTree(
        Type* type,
        const NameBindings& scope,
        const Location& location);
    Definition* getConcreteClassFromTypeParameterList(
        Type* type,
        const NameBindings& scope,
        ClassDefinition* genericClass,
        const Location& location);
    Type* makeGenericTypeConcreteInCurrentTree(
        Type* typeParameter,
        const NameBindings& scope,
        const Location& location);
    void makeSignatureTypesConcrete(
        FunctionSignature* signature,
        const NameBindings& scope,
        const Location& location);
    ClassDefinition* generateConcreteClassFromGeneric(
        ClassDefinition* genericClass,
        const TypeList& concreteTypeParameters,
        const Location& location);
    void insertGeneratedConcreteType(
        ClassDefinition* generatedClass,
        const TypeList& concreteTypeParameters);
    void insertGeneratedConcreteReferenceTypeWithFwdDecl(
        ClassDefinition* generatedClass,
        const Type* recursiveType);
    void insertGeneratedConcreteValueTypeWithFwdDecl(
        ClassDefinition* generatedClass,
        const Type* recursiveType);
    void makeGenericTypesConcreteInSignatures(ClassDefinition* generatedClass);
    void convertClosureTypesInSigntures(ClassDefinition* generatedClass);
    void generateCloneMethods(ClassDefinition* generatedClass);
    void typeCheckAndTransform(ClassDefinition* generatedClass);
    void runPassesOnGeneratedClass(
        ClassDefinition* generatedClass,
        bool mayAddCloneMethod = true);

    DefinitionList globalDefinitions;
    DefinitionList::iterator definitionIter;
    NameBindings globalNameBindings;
    ClassDefinition* globalFunctionsClass;
    std::vector<BlockStatement*> openBlocks;
    std::vector<ClassDefinition*> openClasses;
    std::set<std::string> importedModules;
    Pass currentPass;

    static Tree* currentTree;
};

#endif
