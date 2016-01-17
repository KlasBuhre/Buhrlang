#include <assert.h>
#include <stdio.h>

#include "Tree.h"
#include "Statement.h"
#include "CloneGenerator.h"
#include "Context.h"
#include "Closure.h"

namespace {
    const Type* findRecursiveType(const TypeList& types) {
        for (auto i = types.cbegin(); i != types.cend(); i++) {
            const Type* type = *i;
            if (type->getClass()->isRecursive()) {
                return type;
            }
        }
        return nullptr;
    }

    ClassDefinition* findInnerClass(const TypeList& types) {
        for (auto i = types.cbegin(); i != types.cend(); i++) {
            const Type* type = *i;
            ClassDefinition* classDef = type->getClass();
            if (classDef->getEnclosingDefinition() != nullptr) {
                return classDef;
            }
        }
        return nullptr;
    }

    class GenericTypeVisitor: public Visitor {
    public:
        GenericTypeVisitor();

        virtual Traverse::Result visitClass(ClassDefinition& classDefinition);
        virtual Traverse::Result visitDataMember(
            DataMemberDefinition& dataMember);
        virtual Traverse::Result visitMethod(MethodDefinition& method);
    };

    GenericTypeVisitor::GenericTypeVisitor() :
        Visitor(TraverseClasses | TraverseDataMembers | TraverseMethods) {}

    Traverse::Result GenericTypeVisitor::visitClass(
        ClassDefinition& classDefinition) {

        if (classDefinition.isGeneric()) {
            // Don't traverse generic classes. They only act as templates for
            // concrete classes.
            return Traverse::Skip;
        }
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitDataMember(
        DataMemberDefinition& dataMember) {

        dataMember.typeCheckAndTransform();
        return Traverse::Continue;
    }

    Traverse::Result GenericTypeVisitor::visitMethod(MethodDefinition& method) {
        method.updateGenericTypesInSignature();
        return Traverse::Continue;
    }

    class ClosureTypeVisitor: public Visitor {
    public:
        ClosureTypeVisitor();

        virtual Traverse::Result visitClass(ClassDefinition& classDefinition);
        virtual Traverse::Result visitDataMember(
            DataMemberDefinition& dataMember);
        virtual Traverse::Result visitMethod(MethodDefinition& method);
    };

    ClosureTypeVisitor::ClosureTypeVisitor() :
        Visitor(TraverseClasses | TraverseDataMembers | TraverseMethods) {}

    Traverse::Result ClosureTypeVisitor::visitClass(
        ClassDefinition& classDefinition) {

        if (classDefinition.isGeneric()) {
            // Don't traverse generic classes. They only act as templates for
            // concrete classes.
            return Traverse::Skip;
        }
        return Traverse::Continue;
    }

    Traverse::Result ClosureTypeVisitor::visitDataMember(
        DataMemberDefinition& dataMember) {

        dataMember.convertClosureType();
        return Traverse::Continue;
    }

    Traverse::Result ClosureTypeVisitor::visitMethod(MethodDefinition& method) {
        method.convertClosureTypesInSignature();
        return Traverse::Continue;
    }

    class CheckReturnStatementsVisitor: public Visitor {
    public:
        CheckReturnStatementsVisitor();

        virtual Traverse::Result visitMethod(MethodDefinition& method);
    };

    CheckReturnStatementsVisitor::CheckReturnStatementsVisitor() :
        Visitor(TraverseClasses | TraverseMethods) {}

    Traverse::Result CheckReturnStatementsVisitor::visitMethod(
        MethodDefinition& method) {

        if (!method.isGenerated()) {
            method.checkReturnStatements();
        }

        return Traverse::Continue;
    }

    class GenerateCloneMethodsVisitor: public Visitor {
    public:
        GenerateCloneMethodsVisitor();

        virtual Traverse::Result visitClass(ClassDefinition& classDefinition);
    };

    GenerateCloneMethodsVisitor::GenerateCloneMethodsVisitor() :
        Visitor(TraverseClasses) {}

    Traverse::Result GenerateCloneMethodsVisitor::visitClass(
        ClassDefinition& classDefinition) {

        if (classDefinition.isGeneric()) {
            // Don't traverse generic classes. They only act as templates for
            // concrete classes.
            return Traverse::Skip;
        }

        if (classDefinition.isMessage()) {
            classDefinition.generateCloneMethod();
        }

        return Traverse::Continue;
    }

    class TypeCheckAndTransformVisitor: public Visitor {
    public:
        TypeCheckAndTransformVisitor();

        virtual Traverse::Result visitClass(ClassDefinition& classDefinition);
        virtual Traverse::Result visitMethod(MethodDefinition& method);
    };

    TypeCheckAndTransformVisitor::TypeCheckAndTransformVisitor() :
        Visitor(TraverseClasses | TraverseMethods) {}

    Traverse::Result TypeCheckAndTransformVisitor::visitClass(
        ClassDefinition& classDefinition) {

        if (classDefinition.isGeneric()) {
            // Don't traverse generic classes. They only act as templates for
            // concrete classes.
            return Traverse::Skip;
        }
        return Traverse::Continue;
    }

    Traverse::Result TypeCheckAndTransformVisitor::visitMethod(
        MethodDefinition& method) {

        method.typeCheckAndTransform();
        return Traverse::Continue;
    }
}

Tree* Tree::currentTree = nullptr;

Visitor::Visitor(unsigned int m) : mask(m) {}

Traverse::Result Visitor::visitClass(ClassDefinition&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitDataMember(DataMemberDefinition&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitMethod(MethodDefinition&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitStatement(Statement&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitBlock(BlockStatement&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitVariableDeclaration(
    VariableDeclarationStatement&) {

    return Traverse::Continue;
}

Traverse::Result Visitor::visitHeapAllocation(HeapAllocationExpression&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitArrayAllocation(ArrayAllocationExpression&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitTypeCast(TypeCastExpression&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitClassDecomposition(
    ClassDecompositionExpression&) {

    return Traverse::Continue;
}

Traverse::Result Visitor::visitTypedExpression(TypedExpression&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitNamedEntity(NamedEntityExpression&) {
    return Traverse::Continue;
}

Traverse::Result Visitor::visitMemberSelector(MemberSelectorExpression&) {
    return Traverse::Continue;
}

Tree::Tree() : 
    globalDefinitions(),
    definitionIter(globalDefinitions.end()),
    globalNameBindings(nullptr),
    globalFunctionsClass(nullptr),
    openBlocks(), 
    openClasses(),
    importedModules(),
    currentPass(Parse) {

    setCurrentTree();
    insertObjectClassAndVoidInGlobalNameBindings();
    insertBuiltInType("_");
    insertBuiltInType("lambda");
    insertBuiltInType(Keyword::funString);
    insertBuiltInType("implicit");
    insertBuiltInType(Keyword::byteString);
    insertBuiltInType(Keyword::charString);
    insertBuiltInType(Keyword::intString);
    insertBuiltInType(Keyword::floatString);
    insertBuiltInType(Keyword::boolString);
    globalFunctionsClass = insertBuiltInType("_Global_Functions_");
    generateArrayClass();
    generateNoArgsClosureInterface();
    generateDeferClass();
}

void Tree::insertObjectClassAndVoidInGlobalNameBindings() {
    GenericTypeParameterList typeParameters;
    ClassDefinition::Properties properties;
    ClassList parents;
    ClassDefinition* objectClass = 
        new ClassDefinition(Keyword::objectString,
                            typeParameters,
                            nullptr,
                            parents,
                            &globalNameBindings,
                            properties,
                            Location());
    objectClass->generateDefaultConstructor();
    globalNameBindings.insertClass(Keyword::objectString, objectClass);

    ClassDefinition* voidClass = insertBuiltInType("void");

    // We have to set the return type definition of the object class constructor
    // to void. That was not done automatically beacuse the void type did not
    // exist when the constructor was generated.
    objectClass->getDefaultConstructor()->getReturnType()->setDefinition(
        voidClass);
}

ClassDefinition* Tree::insertBuiltInType(const Identifier& name) {
    ClassDefinition::Properties properties;
    startGeneratedClass(name, properties);
    return finishClass();
}

void Tree::generateArrayClass() {
    ClassDefinition::Properties properties;
    startGeneratedClass(BuiltInTypes::arrayTypeName, properties);
    ClassDefinition* arrayClass = getCurrentClass();

    // int length()
    MethodDefinition* lengthMethod =
        MethodDefinition::create(BuiltInTypes::arrayLengthMethodName,
                                 new Type(Type::Integer),
                                 false,
                                 arrayClass);
    addClassMember(lengthMethod);

    // int size()
    MethodDefinition* sizeMethod =
        MethodDefinition::create(BuiltInTypes::arraySizeMethodName,
                                 new Type(Type::Integer),
                                 false,
                                 arrayClass);
    addClassMember(sizeMethod);

    // int capacity()
    MethodDefinition* capacityMethod =
        MethodDefinition::create(BuiltInTypes::arrayCapacityMethodName,
                                 new Type(Type::Integer),
                                 false,
                                 arrayClass);
    addClassMember(capacityMethod);

    // The methods:
    //
    // append(_ element)
    // appendAll(_[] array)
    // _[] concat(_[] array)
    // _[] slice(int begin, int end)
    //
    // are used like this:
    //
    // array.append(1)
    // array.appendAll(array2)
    // array3 = array.concat(array2)
    // array4 = array.slice(1, 3)
    //
    // Which is equal to:
    //
    // array += 1
    // array += array2
    // array3 = array + array2
    // array4 = array[1...3]
    //

    // append(_ element)
    MethodDefinition* appendMethod =
        MethodDefinition::create(BuiltInTypes::arrayAppendMethodName,
                                 nullptr,
                                 false,
                                 arrayClass);
    appendMethod->addArgument(Type::Placeholder, "element");
    addClassMember(appendMethod);

    // appendAll(_[] array)
    MethodDefinition* appendAllMethod =
        MethodDefinition::create(BuiltInTypes::arrayAppendAllMethodName,
                                 nullptr,
                                 false,
                                 arrayClass);
    Type* arrayType = new Type(Type::Placeholder);
    arrayType->setArray(true);
    appendAllMethod->addArgument(arrayType, "array");
    addClassMember(appendAllMethod);

    // _[] concat(_[] array)
    Type* concatReturnType = new Type(Type::Placeholder);
    concatReturnType->setArray(true);
    MethodDefinition* concatMethod =
        MethodDefinition::create(BuiltInTypes::arrayConcatMethodName,
                                 concatReturnType,
                                 false,
                                 arrayClass);
    Type* concatArrayType = new Type(Type::Placeholder);
    concatArrayType->setArray(true);
    concatMethod->addArgument(concatArrayType, "array");
    addClassMember(concatMethod);

    // _[] slice(int begin, int end)
    Type* sliceReturnType = new Type(Type::Placeholder);
    sliceReturnType->setArray(true);
    MethodDefinition* sliceMethod =
        MethodDefinition::create(BuiltInTypes::arraySliceMethodName,
                                 sliceReturnType,
                                 false,
                                 arrayClass);
    sliceMethod->addArgument(Type::Integer, "begin");
    sliceMethod->addArgument(Type::Integer, "end");
    addClassMember(sliceMethod);

    // each() (_)
    MethodDefinition* eachMethod =
        MethodDefinition::create(BuiltInTypes::arrayEachMethodName,
                                 new Type(Type::Void),
                                 false,
                                 arrayClass);
    FunctionSignature* eachLambdaSignature =
        new FunctionSignature(new Type(Type::Void));
    eachLambdaSignature->addArgument(new Type(Type::Integer));
    eachMethod->setLambdaSignature(eachLambdaSignature,
                                   arrayClass->getLocation());
    addClassMember(eachMethod);

    finishClass();
}

void Tree::generateNoArgsClosureInterface() {
    Type* closureInterfaceType = new Type(Type::Function);
    closureInterfaceType->setFunctionSignature(new FunctionSignature(nullptr));

    Closure closure(*this);
    closure.generateInterface(closureInterfaceType);
}

void Tree::generateDeferClass() {
    ClassDefinition::Properties properties;
    startGeneratedClass(CommonNames::deferTypeName, properties);
    ClassDefinition* deferClass = getCurrentClass();

    // addClosure(fun () closure)
    MethodDefinition* addClosureMethod =
        MethodDefinition::create(CommonNames::addClosureMethodName,
                                 nullptr,
                                 false,
                                 deferClass);
    Type* closureType = new Type(Type::Function);
    closureType->setFunctionSignature(new FunctionSignature(nullptr));
    Type* closureInterfaceType =
        Type::create(closureType->getClosureInterfaceName());
    addClosureMethod->addArgument(closureInterfaceType, "closure");
    addClassMember(addClosureMethod);

    deferClass->generateDefaultConstructor();
    finishClass();
}

BlockStatement* Tree::startBlock() {
    Location location;
    return startBlock(location);
}

BlockStatement* Tree::startBlock(const Location& location) {
    BlockStatement* newblock = new BlockStatement(getCurrentClass(),
                                                  getCurrentBlock(),
                                                  location);
    openBlocks.push_back(newblock);
    return newblock;
}

BlockStatement* Tree::finishBlock() {
    BlockStatement* block = openBlocks.back();
    openBlocks.pop_back();
    return block;
}

void Tree::setCurrentBlock(BlockStatement* block) {
    openBlocks.push_back(block);
}

BlockStatement* Tree::getCurrentBlock() const {
    return openBlocks.size() ? openBlocks.back() : nullptr;
}

Tree& Tree::getCurrentTree() {
    assert(currentTree != nullptr);
    return *currentTree;
}

void Tree::addStatement(Statement* statement) {
    openBlocks.back()->addStatement(statement);
}

ClassDefinition* Tree::getCurrentClass() const {
    return openClasses.size() ? openClasses.back() : nullptr;
}

void Tree::startGeneratedClass(
    const Identifier& name,
    ClassDefinition::Properties& properties) {

    Location location;
    IdentifierList parents;
    GenericTypeParameterList typeParameters;
    properties.isGenerated = true;
    startClass(name, typeParameters, parents, properties, location);
}

void Tree::startGeneratedClass(
    const Identifier& name,
    ClassDefinition::Properties& properties,
    const IdentifierList& parents) {

    Location location;
    GenericTypeParameterList typeParameters;
    properties.isGenerated = true;
    startClass(name, typeParameters, parents, properties, location);
}

ClassDefinition* Tree::startClass(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    const IdentifierList& parents,
    ClassDefinition::Properties& properties,
    const Location& location) {

    NameBindings* containingNameBindings = &globalNameBindings;
    ClassDefinition* containingClass = getCurrentClass();
    if (containingClass != nullptr) {
        containingNameBindings = &(containingClass->getNameBindings());
    }

    ClassDefinition* newClass = ClassDefinition::create(name,
                                                        genericTypeParameters,
                                                        parents,
                                                        containingNameBindings,
                                                        properties,
                                                        location);
    if (containingClass != nullptr) {
        newClass->setIsImported(containingClass->isImported());
    }

    if (!containingNameBindings->insertClass(name, newClass)) {
        Trace::error("Class already declared at the same scope: " + name,
                     location);
    }

    if (!newClass->isGeneric()) {
        // For generic message classes, the empty copy constructor will be
        // generated when the concrete class is created from the generic one.
        if (newClass->needsCloneMethod()) {
            // The body of the copy constructor will be generated later by the
            // CloneGenerator, here we just generate an empty copy constructor.
            // The reason for generating an empty copy constructor at this stage
            // is that the copy constructor needs to be in the name bindings of
            // the new class before any other class can inherit from it.
            // Otherwise, the copy constructor will not be in the name bindings
            // of the derived class since name bindings are copied from the base
            // class to the derived class.
            newClass->generateEmptyCopyConstructor();
            CloneGenerator::generateEmptyCloneMethod(newClass);
        }
    }

    openClasses.push_back(newClass);
    return newClass;
}

void Tree::reopenClass(ClassDefinition* classDefinition) {
    openClasses.push_back(classDefinition);
}

ClassDefinition* Tree::finishClass() {
    ClassDefinition* retVal = getCurrentClass();
    openClasses.pop_back();
    return retVal;
}

void Tree::startFunction() {
    reopenClass(globalFunctionsClass);
}

void Tree::finishFunction(MethodDefinition* function) {
    function->setIsFunction(true);
    addClassMember(function);
    globalNameBindings.overloadMethod(function->getName(), function);
    finishClass();
}

void Tree::addClassMember(Definition* definition) {
    ClassDefinition* currentClass = getCurrentClass();
    currentClass->appendMember(definition);
}

void Tree::addClassDataMember(
    Type::BuiltInType type,
    const Identifier& name) {

    DataMemberDefinition* dataMember =
        new DataMemberDefinition(name, new Type(type));
    getCurrentClass()->appendMember(dataMember);
}

void Tree::addClassDataMember(Type* type, const Identifier& name) {
    DataMemberDefinition* dataMember = new DataMemberDefinition(name, type);
    getCurrentClass()->appendMember(dataMember);
}

void Tree::addGlobalDefinition(Definition* definition) {
    globalDefinitions.push_back(definition);
    if (ClassDefinition* classDef = definition->dynCast<ClassDefinition>()) {
        classDef->generateDefaultConstructorIfNeeded();
    }
}

void Tree::checkReturnStatements() {
    currentPass = CheckReturnStatements;

    CheckReturnStatementsVisitor visitor;
    traverse(visitor);
}

void Tree::makeGenericTypesConcreteInSignatures() {
    currentPass = MakeGenericTypesConcrete;

    GenericTypeVisitor visitor;
    traverse(visitor);
}

void Tree::convertClosureTypesInSigntures() {
    currentPass = ConvertClosureTypes;

    ClosureTypeVisitor visitor;
    traverse(visitor);
}

void Tree::generateCloneMethods() {
    currentPass = GenerateCloneMethods;

    GenerateCloneMethodsVisitor visitor;
    traverse(visitor);
}

void Tree::typeCheckAndTransform() {
    currentPass = TypeCheckAndTransform;

    TypeCheckAndTransformVisitor visitor;
    traverse(visitor);
}

void Tree::traverse(Visitor& visitor) {
    for (definitionIter = globalDefinitions.begin();
         definitionIter != globalDefinitions.end();
         definitionIter++) {
        Definition* definition = *definitionIter;
        unsigned int traverseMask = visitor.getTraverseMask();
        switch (definition->getKind()) {
            case Definition::Class:
                if (traverseMask & Visitor::TraverseClasses) {
                    definition->traverse(visitor);
                }
                break;
            case Definition::Member:
                if (traverseMask & Visitor::TraverseMethods) {
                    definition->traverse(visitor);
                }
                break;
            default:
                break;
        }
    }
}

void Tree::useNamespace(
    const Identifier& namespaceName,
    const Location& location) {

    NameBindings& currentNamespace = getCurrentNameBindings();
    Definition* namespaceDefinition =
        currentNamespace.lookupType(namespaceName);
    if (namespaceDefinition == nullptr) {
        Trace::error("Unknown namespace: " + namespaceName, location);
    }

    assert(namespaceDefinition->isClass());
    ClassDefinition* classDef = namespaceDefinition->cast<ClassDefinition>();
    currentNamespace.use(classDef->getNameBindings());
}

NameBindings& Tree::getCurrentNameBindings() {
    BlockStatement* currentBlock = getCurrentBlock();
    if (currentBlock != nullptr) {
        return currentBlock->getNameBindings();
    }

    ClassDefinition* currentClass = getCurrentClass();
    if (currentClass != nullptr) {
        return currentClass->getNameBindings();
    }

    return globalNameBindings;
}

MethodDefinition* Tree::getMainMethod() const {
    MethodDefinition* main = globalFunctionsClass->getMainMethod();
    if (main == nullptr) {
        for (auto i = globalDefinitions.cbegin();
             i != globalDefinitions.cend();
             i++) {
            Definition* definition = *i;
            if (definition->isClass()) {
                main = definition->cast<ClassDefinition>()->getMainMethod();
                if (main != nullptr) {
                    break;
                }
            }
        }
    }
    return main;
}

void Tree::addImportedModule(const std::string& moduleName) {
    importedModules.insert(moduleName);
}

bool Tree::isModuleAlreadyImported(const std::string& moduleName) const {
    return (importedModules.find(moduleName) != importedModules.end());
}

void Tree::lookupAndSetTypeDefinition(Type* type, const Location& location) {
    assert(currentTree != nullptr);
    currentTree->lookupAndSetTypeDefinitionInCurrentTree(
        type,
        currentTree->globalNameBindings,
        location);
}

void Tree::lookupAndSetTypeDefinition(
    Type* type,
    const NameBindings& scope,
    const Location& location) {

    assert(currentTree != nullptr);
    currentTree->lookupAndSetTypeDefinitionInCurrentTree(type, scope, location);
}

void Tree::lookupAndSetTypeDefinitionInCurrentTree(
    Type* type,
    const NameBindings& scope,
    const Location& location) {

    Definition* definition = scope.lookupType(type->getName());
    if (definition == nullptr) {
        Trace::error("Unknown type: " + type->getName(), location);
    }

    type->setDefinition(definition);

    ClassDefinition* classDefinition = definition->dynCast<ClassDefinition>();
    if (classDefinition != nullptr && classDefinition == getCurrentClass()) {
        classDefinition->setRecursive(true);
    }

    if (type->isFunction()) {
        FunctionSignature* signature = type->getFunctionSignature();
        lookupAndSetTypeDefinitionInCurrentTree(signature->getReturnType(),
                                                scope,
                                                location);
        const TypeList& arguments = signature->getArguments();
        for (auto i = arguments.cbegin(); i != arguments.cend(); i++) {
            Type* argumentType = *i;
            lookupAndSetTypeDefinitionInCurrentTree(argumentType,
                                                    scope,
                                                    location);
        }
    }

    if (type->hasGenericTypeParameters()) {
        // The type has generic type parameters. This means the type must refer
        // to a generic class.
        if (classDefinition == nullptr) {
            Trace::error("Only classes can take type parameters: " +
                          type->getName(),
                          location);
        }

        if (!classDefinition->isGeneric()) {
            Trace::error("Only generic classes can take type parameters: " +
                          type->getName(),
                          location);
        }

        const TypeList& typeParameters = type->getGenericTypeParameters();
        for (auto i = typeParameters.cbegin();
             i != typeParameters.cend();
             i++) {
            Type* typeParameter = *i;
            lookupAndSetTypeDefinitionInCurrentTree(typeParameter,
                                                    scope,
                                                    location);
        }
    }
}

Definition* Tree::getConcreteClassFromTypeParameterList(
    Type* type,
    const NameBindings& scope,
    ClassDefinition* genericClass,
    const Location& location) {

    TypeList& typeParameters = type->getGenericTypeParameters();
    for (auto i = typeParameters.begin(); i != typeParameters.end(); i++) {
        Type* typeParameter = *i;
        Type* concreteType = makeGenericTypeConcreteInCurrentTree(typeParameter,
                                                                  scope,
                                                                  location);
        if (concreteType != nullptr) {
            // The type parameter was a generic type parameter which has been
            // assigned a concrete type. Change the type parameter to the
            // concrete type.
            *i = concreteType;
        }
    }

    // Lookup the generated concrete class using the name of the generic class
    // and the type parameters.
    Definition* concreteClass =
        scope.lookupType(type->getFullConstructedName());
    if (concreteClass == nullptr) {
        concreteClass = generateConcreteClassFromGeneric(genericClass,
                                                         typeParameters,
                                                         location);
    }
    return concreteClass;
}

Type* Tree::makeGenericTypeConcrete(
    Type* type,
    const NameBindings& scope,
    const Location& location) {

    assert(currentTree != nullptr);
    return currentTree->makeGenericTypeConcreteInCurrentTree(type,
                                                             scope,
                                                             location);
}

Type* Tree::makeGenericTypeConcreteInCurrentTree(
    Type* type,
    const NameBindings& scope,
    const Location& location) {

    if (type->isFunction()) {
        makeSignatureTypesConcrete(type->getFunctionSignature(),
                                   scope,
                                   location);
    }

    Definition* typeDefinition = type->getDefinition();
    assert(typeDefinition != nullptr);
    if (typeDefinition->isGenericTypeParameter()) {
        // Since the type is a generic type parameter, we must lookup the type
        // definition even though the type already has a definition. This is
        // because the generic type parameter definitions are cloned when a
        // concrete class is generated from a generic class, and the type needs
        // to refer to the cloned generic type parameter definition.
        lookupAndSetTypeDefinitionInCurrentTree(type, scope, location);
        return type->getConcreteTypeAssignedToGenericTypeParameter();
    }

    if (type->hasGenericTypeParameters()) {
        // The type has type parameters. For example, the type could be like 
        // this: 'Foo<T>' where T is a generic type parameter which we have to
        // map to a concrete type. Or, the type could be like this 'Foo<int>'.
        // In all cases we must make sure that the definition of the type is
        // the generated concrete class.
        assert(typeDefinition->getKind() == Definition::Class);
        ClassDefinition* classDef = typeDefinition->cast<ClassDefinition>();
        if (classDef->isGeneric()) {
            // The class is still a generic, which means we must get the
            // generated concrete class.
            typeDefinition = getConcreteClassFromTypeParameterList(type,
                                                                   scope,
                                                                   classDef,
                                                                   location);
            type->setDefinition(typeDefinition);
            return type;
        }
        // The class is not generic so it is already the generated concrete
        // class. Do nothing.
    }

    return nullptr;
}

void Tree::makeSignatureTypesConcrete(
    FunctionSignature* signature,
    const NameBindings& scope,
    const Location& location) {

    Type* concreteReturnType =
        makeGenericTypeConcreteInCurrentTree(signature->getReturnType(),
                                             scope,
                                             location);
    if (concreteReturnType != nullptr) {
        signature->setReturnType(concreteReturnType);
    }

    TypeList& arguments = signature->getArguments();
    for (auto i = arguments.begin(); i != arguments.end(); i++) {
        Type* argumentType = *i;
        Type* concreteType = makeGenericTypeConcreteInCurrentTree(argumentType,
                                                                  scope,
                                                                  location);
        if (concreteType != nullptr) {
            // The argument type was a generic type parameter which has been
            // assigned a concrete type. Change the argument type to the
            // concrete type.
            *i = concreteType;
        }
    }
}

ClassDefinition* Tree::generateConcreteClassFromGeneric(
    ClassDefinition* genericClass,
    const TypeList& concreteTypeParameters,
    const Location& location) {

    ClassDefinition* generatedClass = genericClass->clone();

    // Set the concrete type parameters on the cloned class. When the
    // makeGenericTypesConcrete pass is executed for the cloned class, each
    // occurence of a generic type will be changed into the respective concrete
    // type.
    generatedClass->setConcreteTypeParameters(concreteTypeParameters, location);

    if (!(*definitionIter)->isImported()) {
        // Even though the original generic class could have been imported, the
        // generated concrete class is not imported if the using class is not
        // imported.
        generatedClass->setIsImported(false);
    }

    if (const Type* recursiveType = findRecursiveType(concreteTypeParameters)) {
        // A class definition of one of the type parameters contains data of the
        // same type as the class (like a recursive data structure such as a
        // node in a linked list). In order to make the C++ code compile, we add
        // a forward declaration.
        if (generatedClass->isReferenceType()) {
            // The generated class is of reference type so we can forward
            // declare the generated class. Also, insert the generated class
            // after the class that uses it.
            insertGeneratedConcreteReferenceTypeWithFwdDecl(generatedClass,
                                                            recursiveType);
        } else {
            // The generated class is of value type. Forward declare the
            // recursive type.
            insertGeneratedConcreteValueTypeWithFwdDecl(generatedClass,
                                                        recursiveType);
        }
    } else {
        // No type parameters are recurseive so no need to forward declare
        // anything. Insert the generated class in the global definitions before
        // the class that uses it. Also, We must trigger the
        // makeGenericTypesConcrete pass for the generated class since we are
        // inserting it in front of the current class and it would have not been
        // triggered otherwise.
        insertGeneratedConcreteType(generatedClass, concreteTypeParameters);
    }

    return generatedClass;
}

void Tree::insertGeneratedConcreteType(
    ClassDefinition* generatedClass,
    const TypeList& concreteTypeParameters) {

    if (ClassDefinition* innerClass = findInnerClass(concreteTypeParameters)) {
        ClassDefinition* outerClass = innerClass->getEnclosingClass();
        // Insert the generated class after the inner class.
        outerClass->insertMember(innerClass, generatedClass, true);
        runPassesOnGeneratedClass(generatedClass);
    } else {
        // Insert the generated class in the global name bindings.
        globalNameBindings.insertClass(generatedClass->getName(),
                                       generatedClass);
        runPassesOnGeneratedClass(generatedClass);

        // Insert the definition in front of the definition currently being
        // traversed.
        globalDefinitions.insert(definitionIter, generatedClass);
    }
}

void Tree::insertGeneratedConcreteReferenceTypeWithFwdDecl(
    ClassDefinition* generatedClass,
    const Type* recursiveType) {

    // Add a forward declaration of the generated class.
    ForwardDeclarationDefinition* forwardDeclaration =
        new ForwardDeclarationDefinition(generatedClass->getName());
    ClassDefinition* recursiveClass = recursiveType->getClass();
    if (ClassDefinition* outerClass = recursiveClass->getEnclosingClass()) {
        // The recursive class is contained in an outer class. Insert the
        // forward declaration before the recursive class in the outer class.
        outerClass->insertMember(recursiveClass, forwardDeclaration);

        // Insert the generated class after the recursive class.
        outerClass->insertMember(recursiveClass, generatedClass, true);
        runPassesOnGeneratedClass(generatedClass);
    } else {
        // Insert the generated class in the global name bindings.
        globalNameBindings.insertClass(generatedClass->getName(),
                                       generatedClass);

        // Insert the forward declaration in front of the current class.
        globalDefinitions.insert(definitionIter, forwardDeclaration);

        // We must trigger the makeGenericTypesConcrete pass for the generated
        // class since it will be used by the current class. This is because all
        // generic types in the generated class must be changed into their
        // respective concrete types before we can use it.
        runPassesOnGeneratedClass(generatedClass);

        // Insert the generated class after the current class.
        auto it = definitionIter;
        globalDefinitions.insert(++it, generatedClass);
    }
}

void Tree::insertGeneratedConcreteValueTypeWithFwdDecl(
    ClassDefinition* generatedClass,
    const Type* recursiveType) {

    // Add a forward declaration of the recursive class.
    ForwardDeclarationDefinition* forwardDeclaration =
        new ForwardDeclarationDefinition(
            recursiveType->getFullConstructedName());
    ClassDefinition* recursiveClass = recursiveType->getClass();
    if (ClassDefinition* outerClass = recursiveClass->getEnclosingClass()) {
        // The recursive class is contained in an outer class. Insert its
        // forward declaration and the generatedclass in the outer class, both
        // before the recursive class.
        outerClass->insertMember(recursiveClass, forwardDeclaration);
        outerClass->insertMember(recursiveClass, generatedClass);
        runPassesOnGeneratedClass(generatedClass);
    } else {
        // Insert the generated class in the global name bindings.
        globalNameBindings.insertClass(generatedClass->getName(),
                                       generatedClass);

        // Insert the generated class and its forward declaration before the
        // current class.
        globalDefinitions.insert(definitionIter, forwardDeclaration);
        runPassesOnGeneratedClass(generatedClass);
        globalDefinitions.insert(definitionIter, generatedClass);
    }
}

void Tree::makeGenericTypesConcreteInSignatures(
    ClassDefinition* generatedClass) {

    GenericTypeVisitor visitor;
    generatedClass->traverse(visitor);
}

void Tree::convertClosureTypesInSigntures(ClassDefinition* generatedClass) {
    ClosureTypeVisitor visitor;
    generatedClass->traverse(visitor);
}

void Tree::generateCloneMethods(ClassDefinition* generatedClass) {
    GenerateCloneMethodsVisitor visitor;
    generatedClass->traverse(visitor);
}

void Tree::typeCheckAndTransform(ClassDefinition* generatedClass) {
    TypeCheckAndTransformVisitor visitor;
    generatedClass->traverse(visitor);
}

void Tree::runPassesOnGeneratedClass(
    ClassDefinition* generatedClass,
    bool mayAddCloneMethod) {

    if (generatedClass->needsCloneMethod() && mayAddCloneMethod) {
        // The body of the copy constructor and clone method will be
        // generated later by the CloneGenerator. See comment in
        // Tree::startClass().
        generatedClass->generateEmptyCopyConstructor();
        CloneGenerator::generateEmptyCloneMethod(generatedClass);
    }

    switch (currentPass) {
        case MakeGenericTypesConcrete:
            makeGenericTypesConcreteInSignatures(generatedClass);
            break;
        case TypeCheckAndTransform:
            makeGenericTypesConcreteInSignatures(generatedClass);
            convertClosureTypesInSigntures(generatedClass);
            generateCloneMethods(generatedClass);
            typeCheckAndTransform(generatedClass);
            break;
        default:
            break;
    }
}

Type* Tree::convertToClosureInterface(Type* type) {
    assert(currentTree != nullptr);
    return currentTree->convertToClosureInterfaceInCurrentTree(type);
}

Type* Tree::convertToClosureInterfaceInCurrentTree(Type* type) {
    if (!type->isFunction()) {
        return nullptr;
    }

    Definition* closureInterfaceDef =
        globalNameBindings.lookupType(type->getClosureInterfaceName());
    if (closureInterfaceDef == nullptr) {
        Closure closure(*this);
        ClassDefinition* closureInterfaceClass =
            closure.generateInterface(type);
        insertClassPostParse(closureInterfaceClass);
        closureInterfaceDef = closureInterfaceClass;
    }

    Type* closureInterfaceType =
        Type::create(closureInterfaceDef->cast<ClassDefinition>()->getName());
    closureInterfaceType->setDefinition(closureInterfaceDef);
    return closureInterfaceType;
}

void Tree::insertClassPostParse(
    ClassDefinition* classDefinition,
    bool insertBefore) {

    // If the class that uses the inserted class is imported, then set the
    // inserted class as imported as well.
    classDefinition->setIsImported((*definitionIter)->isImported());

    runPassesOnGeneratedClass(classDefinition, false);

    if (insertBefore) {
        // Insert the definition in front of the definition currently being
        // traversed.
        globalDefinitions.insert(definitionIter, classDefinition);
    } else {
        // Insert the definition after the current class.
        auto it = definitionIter;
        globalDefinitions.insert(++it, classDefinition);
    }
}
