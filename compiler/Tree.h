#ifndef Tree_h
#define Tree_h

#include <set>

#include "CommonTypes.h"
#include "NameBindings.h"
#include "Definition.h"

class Statement;
class BlockStatement;

class Tree {
public:
    Tree();

    BlockStatement* startBlock();
    BlockStatement* startBlock(const Location& location);
    BlockStatement* finishBlock();
    void addStatement(Statement* statement);
    void startGeneratedClass(
        const Identifier& name,
        ClassDefinition::Properties& properties);
    void startGeneratedClass(
        const Identifier& name,
        const IdentifierList& parents);
    ClassDefinition* startClass(
        const Identifier& name, 
        const GenericTypeParameterList& genericTypeParameters,
        const IdentifierList& parents,
        const ClassDefinition::Properties& properties,
        const Location& location);
    void reopenClass(ClassDefinition* classDefinition);
    ClassDefinition* finishClass();
    void startFunction();
    void finishFunction(MethodDefinition* function);
    void process();
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

    const DefinitionList& getGlobalDefinitions() const {
        return globalDefinitions;
    }

    void setCurrentTree() {
        currentTree = this;
    }

private:
    ClassDefinition* insertBuiltInType(const Identifier& name);
    void insertObjectClassAndVoidInGlobalNameBindings();
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

    DefinitionList globalDefinitions;
    DefinitionList::iterator definitionBeingProcessed;
    NameBindings globalNameBindings;
    ClassDefinition* globalFunctionsClass;
    std::vector<BlockStatement*> openBlocks;
    std::vector<ClassDefinition*> openClasses;
    std::set<std::string> importedModules;

    static Tree* currentTree;
};

#endif
