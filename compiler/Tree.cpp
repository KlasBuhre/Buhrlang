#include "Tree.h"

#include <assert.h>
#include <stdio.h>

#include "Statement.h"
#include "CloneGenerator.h"
#include "Context.h"
#include "Closure.h"

namespace {
    const Type* findRecursiveType(const TypeList& types) {
        for (auto type: types) {
            if (type->getClass()->isRecursive()) {
                return type;
            }
        }
        return nullptr;
    }

    ClassDefinition* findInnerClass(const TypeList& types) {
        for (auto type: types) {
            auto classDef = type->getClass();
            if (classDef->getEnclosingDefinition() != nullptr) {
                return classDef;
            }
        }
        return nullptr;
    }

    class GenericTypeVisitor: public Visitor {
    public:
        GenericTypeVisitor();

        Traverse::Result visitClass(ClassDefinition& classDefinition) override;
        Traverse::Result visitDataMember(
            DataMemberDefinition& dataMember) override;
        Traverse::Result visitMethod(MethodDefinition& method) override;
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

        Traverse::Result visitClass(ClassDefinition& classDefinition) override;
        Traverse::Result visitDataMember(
            DataMemberDefinition& dataMember) override;
        Traverse::Result visitMethod(MethodDefinition& method) override;
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

        Traverse::Result visitMethod(MethodDefinition& method) override;
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

        Traverse::Result visitClass(ClassDefinition& classDefinition) override;
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

        Traverse::Result visitClass(ClassDefinition& classDefinition) override;
        Traverse::Result visitMethod(MethodDefinition& method) override;
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
    insertBuiltInTypesInGlobalNameBindings();
    globalFunctionsClass = insertBuiltInType("_Global_Functions_");
    generateArrayClass();
    generateNoArgsClosureInterface();
    generateDeferClass();
}

void Tree::insertBuiltInTypesInGlobalNameBindings() {

    // Start generating the object class.
    auto objectClass =
        ClassDefinition::create(Keyword::objectString, &globalNameBindings);
    globalNameBindings.insertClass(Keyword::objectString, objectClass);

    objectClass->generateDefaultConstructor();

    // Add method:
    // virtual bool equals(object obj)
    auto equalsMethod =
        MethodDefinition::create(BuiltInTypes::objectEqualsMethodName,
                                 Type::create(Type::Boolean),
                                 false,
                                 objectClass);
    auto objectType = Type::create(Type::Object);
    objectType->setDefinition(objectClass);
    equalsMethod->addArgument(objectType, "obj");
    equalsMethod->setIsVirtual(true);
    objectClass->appendMember(equalsMethod);

    // Add method:
    // virtual int hash()
    auto hashMethod =
        MethodDefinition::create(BuiltInTypes::objectHashMethodName,
                                 Type::create(Type::Boolean),
                                 false,
                                 objectClass);
    hashMethod->setIsVirtual(true);
    objectClass->appendMember(hashMethod);

    // Create the primitive types (and some other types).
    auto viodClass = insertBuiltInType("void");
    insertBuiltInType("_");
    insertBuiltInType("lambda");
    insertBuiltInType(Keyword::funString);
    insertBuiltInType("implicit");
    auto byteClass = insertBuiltInType(Keyword::byteString);
    auto charClass = insertBuiltInType(Keyword::charString);
    auto floatClass = insertBuiltInType(Keyword::floatString);
    auto intClass = insertBuiltInType(Keyword::intString);
    auto longClass = insertBuiltInType(Keyword::longString);
    auto boolClass = insertBuiltInType(Keyword::boolString);

    // Now that the primitive types exist we can set the definitions of the
    // return types of the methods in the object class.
    objectClass->getDefaultConstructor()->getReturnType()->setDefinition(
        viodClass);
    equalsMethod->getReturnType()->setDefinition(boolClass);
    hashMethod->getReturnType()->setDefinition(intClass);

    // Add equals() methods to the primitive types.
    addEqualsMethod(byteClass, Type::Byte);
    addEqualsMethod(charClass, Type::Char);
    addEqualsMethod(floatClass, Type::Float);
    addEqualsMethod(intClass, Type::Integer);
    addEqualsMethod(longClass, Type::Integer);
    addEqualsMethod(boolClass, Type::Boolean);
}

ClassDefinition* Tree::insertBuiltInType(const Identifier& name) {
    ClassDefinition::Properties properties;
    startGeneratedClass(name, properties);
    return finishClass();
}

void Tree::addEqualsMethod(
    ClassDefinition* classDef,
    Type::BuiltInType builtInType) {

    // Add method:
    // virtual bool equals([builtInType] obj)
    auto equalsMethod =
        MethodDefinition::create(BuiltInTypes::objectEqualsMethodName,
                                 Type::create(Type::Boolean),
                                 false,
                                 classDef);
    equalsMethod->addArgument(Type::create(builtInType), "obj");
    classDef->appendMember(equalsMethod);
}

void Tree::generateArrayClass() {
    ClassDefinition::Properties properties;
    startGeneratedClass(BuiltInTypes::arrayTypeName, properties);
    ClassDefinition* arrayClass = getCurrentClass();

    // Add method:
    // int length()
    auto lengthMethod =
        MethodDefinition::create(BuiltInTypes::arrayLengthMethodName,
                                 Type::create(Type::Integer),
                                 false,
                                 arrayClass);
    addClassMember(lengthMethod);

    // Add method:
    // int size()
    auto sizeMethod =
        MethodDefinition::create(BuiltInTypes::arraySizeMethodName,
                                 Type::create(Type::Integer),
                                 false,
                                 arrayClass);
    addClassMember(sizeMethod);

    // Add method:
    // int capacity()
    auto capacityMethod =
        MethodDefinition::create(BuiltInTypes::arrayCapacityMethodName,
                                 Type::create(Type::Integer),
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

    // Add method:
    // append(_ element)
    auto appendMethod =
        MethodDefinition::create(BuiltInTypes::arrayAppendMethodName,
                                 nullptr,
                                 false,
                                 arrayClass);
    appendMethod->addArgument(Type::Placeholder, "element");
    addClassMember(appendMethod);

    // Add method:
    // appendAll(_[] array)
    auto appendAllMethod =
        MethodDefinition::create(BuiltInTypes::arrayAppendAllMethodName,
                                 nullptr,
                                 false,
                                 arrayClass);
    auto arrayType = Type::create(Type::Placeholder);
    arrayType->setArray(true);
    appendAllMethod->addArgument(arrayType, "array");
    addClassMember(appendAllMethod);

    // Add method:
    // _[] concat(_[] array)
    auto concatReturnType = Type::create(Type::Placeholder);
    concatReturnType->setArray(true);
    auto concatMethod =
        MethodDefinition::create(BuiltInTypes::arrayConcatMethodName,
                                 concatReturnType,
                                 false,
                                 arrayClass);
    auto concatArrayType = Type::create(Type::Placeholder);
    concatArrayType->setArray(true);
    concatMethod->addArgument(concatArrayType, "array");
    addClassMember(concatMethod);

    // Add method:
    // _[] slice(int begin, int end)
    auto sliceReturnType = Type::create(Type::Placeholder);
    sliceReturnType->setArray(true);
    auto sliceMethod =
        MethodDefinition::create(BuiltInTypes::arraySliceMethodName,
                                 sliceReturnType,
                                 false,
                                 arrayClass);
    sliceMethod->addArgument(Type::Integer, "begin");
    sliceMethod->addArgument(Type::Integer, "end");
    addClassMember(sliceMethod);

    // Add method:
    // each() (_)
    auto eachMethod =
        MethodDefinition::create(BuiltInTypes::arrayEachMethodName,
                                 Type::create(Type::Void),
                                 false,
                                 arrayClass);
    auto eachLambdaSignature =
        FunctionSignature::create(Type::create(Type::Void));
    eachLambdaSignature->addArgument(Type::create(Type::Integer));
    eachMethod->setLambdaSignature(eachLambdaSignature,
                                   arrayClass->getLocation());
    addClassMember(eachMethod);

    finishClass();
}

void Tree::generateNoArgsClosureInterface() {
    auto closureInterfaceType = Type::create(Type::Function);
    closureInterfaceType->setFunctionSignature(
        FunctionSignature::create(nullptr));
    Closure::generateInterface(*this, closureInterfaceType);
}

void Tree::generateDeferClass() {
    ClassDefinition::Properties properties;
    startGeneratedClass(CommonNames::deferTypeName, properties);
    auto deferClass = getCurrentClass();

    // addClosure(fun () closure)
    auto addClosureMethod =
        MethodDefinition::create(CommonNames::addClosureMethodName,
                                 nullptr,
                                 false,
                                 deferClass);
    auto closureType = Type::create(Type::Function);
    closureType->setFunctionSignature(FunctionSignature::create(nullptr));
    auto closureInterfaceType =
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
    auto newblock =
        BlockStatement::create(getCurrentClass(), getCurrentBlock(), location);
    openBlocks.push_back(newblock);
    return newblock;
}

BlockStatement* Tree::finishBlock() {
    auto block = openBlocks.back();
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
    auto containingClass = getCurrentClass();
    if (containingClass != nullptr) {
        containingNameBindings = &(containingClass->getNameBindings());
    }

    auto newClass = ClassDefinition::create(name,
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
    auto retVal = getCurrentClass();
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
    auto currentClass = getCurrentClass();
    currentClass->appendMember(definition);
}

void Tree::addClassDataMember(
    Type::BuiltInType type,
    const Identifier& name) {

    getCurrentClass()->appendMember(
        DataMemberDefinition::create(name, Type::create(type)));
}

void Tree::addClassDataMember(Type* type, const Identifier& name) {
    getCurrentClass()->appendMember(DataMemberDefinition::create(name, type));
}

void Tree::addGlobalDefinition(Definition* definition) {
    globalDefinitions.push_back(definition);
    if (auto classDef = definition->dynCast<ClassDefinition>()) {
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
        auto definition = *definitionIter;
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
    auto namespaceDefinition = currentNamespace.lookupType(namespaceName);
    if (namespaceDefinition == nullptr) {
        Trace::error("Unknown namespace: " + namespaceName, location);
    }

    assert(namespaceDefinition->isClass());
    auto classDef = namespaceDefinition->cast<ClassDefinition>();
    currentNamespace.use(classDef->getNameBindings());
}

NameBindings& Tree::getCurrentNameBindings() {
    auto currentBlock = getCurrentBlock();
    if (currentBlock != nullptr) {
        return currentBlock->getNameBindings();
    }

    auto currentClass = getCurrentClass();
    if (currentClass != nullptr) {
        return currentClass->getNameBindings();
    }

    return globalNameBindings;
}

MethodDefinition* Tree::getMainMethod() const {
    auto main = globalFunctionsClass->getMainMethod();
    if (main == nullptr) {
        for (auto definition: globalDefinitions) {
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

    auto definition = scope.lookupType(type->getName());
    if (definition == nullptr) {
        Trace::error("Unknown type: " + type->getName(), location);
    }

    type->setDefinition(definition);

    auto classDefinition = definition->dynCast<ClassDefinition>();
    if (classDefinition != nullptr && classDefinition == getCurrentClass()) {
        classDefinition->setRecursive(true);
    }

    if (type->isFunction()) {
        auto signature = type->getFunctionSignature();
        lookupAndSetTypeDefinitionInCurrentTree(signature->getReturnType(),
                                                scope,
                                                location);
        for (auto argumentType: signature->getArguments()) {
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

        for (auto typeParameter: type->getGenericTypeParameters()) {
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
    for (auto& typeParameter: typeParameters) {
        auto concreteType = makeGenericTypeConcreteInCurrentTree(typeParameter,
                                                                 scope,
                                                                 location);
        if (concreteType != nullptr) {
            // The type parameter was a generic type parameter which has been
            // assigned a concrete type. Change the type parameter to the
            // concrete type.
            typeParameter = concreteType;
        }
    }

    // Lookup the generated concrete class using the name of the generic class
    // and the type parameters.
    auto concreteClass = scope.lookupType(type->getFullConstructedName());
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

    auto typeDefinition = type->getDefinition();
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
        auto classDef = typeDefinition->cast<ClassDefinition>();
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

    auto concreteReturnType =
        makeGenericTypeConcreteInCurrentTree(signature->getReturnType(),
                                             scope,
                                             location);
    if (concreteReturnType != nullptr) {
        signature->setReturnType(concreteReturnType);
    }

    for (auto& argumentType: signature->getArguments()) {
        auto concreteType = makeGenericTypeConcreteInCurrentTree(argumentType,
                                                                  scope,
                                                                  location);
        if (concreteType != nullptr) {
            // The argument type was a generic type parameter which has been
            // assigned a concrete type. Change the argument type to the
            // concrete type.
            argumentType = concreteType;
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

    if (auto innerClass = findInnerClass(concreteTypeParameters)) {
        auto outerClass = innerClass->getEnclosingClass();
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
    auto forwardDeclaration =
        ForwardDeclarationDefinition::create(generatedClass->getName());
    auto recursiveClass = recursiveType->getClass();
    if (auto outerClass = recursiveClass->getEnclosingClass()) {
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
    auto forwardDeclaration =
        ForwardDeclarationDefinition::create(
            recursiveType->getFullConstructedName());
    auto recursiveClass = recursiveType->getClass();
    if (auto outerClass = recursiveClass->getEnclosingClass()) {
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

    auto closureInterfaceDef =
        globalNameBindings.lookupType(type->getClosureInterfaceName());
    if (closureInterfaceDef == nullptr) {
        auto closureInterfaceClass = Closure::generateInterface(*this, type);
        insertClassPostParse(closureInterfaceClass);
        closureInterfaceDef = closureInterfaceClass;
    }

    auto closureInterfaceType =
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
