#include <assert.h>

#include "Tree.h"
#include "Statement.h"

namespace {
    const Type* findRecursiveType(const TypeList& types) {
        for (TypeList::const_iterator i = types.begin();
             i != types.end();
             i++) {
            const Type* type = *i;
            if (type->getClass()->isRecursive()) {
                return type;
            }
        }
        return nullptr;
    }

    ClassDefinition* findInnerClass(const TypeList& types) {
        for (TypeList::const_iterator i = types.begin();
             i != types.end();
             i++) {
            const Type* type = *i;
            ClassDefinition* classDef = type->getClass();
            if (classDef->getEnclosingDefinition() != nullptr) {
                return classDef;
            }
        }
        return nullptr;
    }
}

Tree* Tree::currentTree = nullptr;

Tree::Tree() : 
    globalDefinitions(),
    definitionBeingProcessed(globalDefinitions.end()),
    globalNameBindings(nullptr),
    globalFunctionsClass(nullptr),
    openBlocks(), 
    openClasses(),
    importedModules() {

    setCurrentTree();
    insertObjectClassAndVoidInGlobalNameBindings();
    insertBuiltInType("_");
    insertBuiltInType("lambda");
    insertBuiltInType(Keyword::byteString);
    insertBuiltInType(Keyword::charString);
    insertBuiltInType(Keyword::varString);
    insertBuiltInType(Keyword::intString);
    insertBuiltInType(Keyword::floatString);
    insertBuiltInType(Keyword::boolString);
    globalFunctionsClass = insertBuiltInType("_Global_Functions_");
    generateArrayClass();
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
    objectClass->setProcessed(true);
}

ClassDefinition* Tree::insertBuiltInType(const Identifier& name) {
    ClassDefinition::Properties properties;
    startGeneratedClass(name, properties);
    ClassDefinition* classDef = finishClass();
    classDef->setProcessed(true);
    return classDef;
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
    LambdaSignature* eachLambdaSignature =
        new LambdaSignature(new Type(Type::Void));
    eachLambdaSignature->addArgument(new Type(Type::Integer));
    eachMethod->setLambdaSignature(eachLambdaSignature,
                                   arrayClass->getLocation());
    addClassMember(eachMethod);

    arrayClass->setProcessed(true);
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

BlockStatement* Tree::getCurrentBlock() const {
    return openBlocks.size() ? openBlocks.back() : nullptr;
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
    const IdentifierList& parents) {

    Location location;
    GenericTypeParameterList typeParameters;
    ClassDefinition::Properties properties;
    properties.isGenerated = true;
    startClass(name, typeParameters, parents, properties, location);
}

ClassDefinition* Tree::startClass(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    const IdentifierList& parents,
    const ClassDefinition::Properties& properties,
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

void Tree::process() {
    for (definitionBeingProcessed = globalDefinitions.begin();
         definitionBeingProcessed != globalDefinitions.end();
         definitionBeingProcessed++) {
        Definition* definition = *definitionBeingProcessed;
        definition->process();
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
        for (DefinitionList::const_iterator i = globalDefinitions.begin();
             i != globalDefinitions.end();
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
        for (TypeList::const_iterator i = typeParameters.begin();
             i != typeParameters.end();
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
    for (TypeList::iterator i = typeParameters.begin();
         i != typeParameters.end();
         i++) {
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
        assert(typeDefinition->getDefinitionType() == Definition::Class);
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

ClassDefinition* Tree::generateConcreteClassFromGeneric(
    ClassDefinition* genericClass,
    const TypeList& concreteTypeParameters,
    const Location& location) {

    ClassDefinition* generatedClass = genericClass->clone();

    // Set the concrete type parameters on the cloned class. When the process
    // stage is executed for the cloned class, each occurence of a generic type
    // will be changed into the respective concrete type.
    generatedClass->setConcreteTypeParameters(
        concreteTypeParameters,
        location);

    // Even though the original generic class could have been imported, the
    // generated concrete class is not.
    generatedClass->setIsImported(false);

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
        // the class that uses it. Also, We must trigger the process stage for
        // the generated class since we are inserting it in front of the current
        // class and it would have not been processed otherwise.
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
        generatedClass->process();
    } else {
        // Insert the generated class in the global name bindings.
        globalNameBindings.insertClass(generatedClass->getName(),
                                       generatedClass);
        generatedClass->process();

        // Insert the definition in front of the definition currently being
        // processed.
        globalDefinitions.insert(definitionBeingProcessed, generatedClass);
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
        generatedClass->process();
    } else {
        // Insert the generated class in the global name bindings.
        globalNameBindings.insertClass(generatedClass->getName(),
                                       generatedClass);

        // Insert the forward declaration in front of the current class.
        globalDefinitions.insert(definitionBeingProcessed,
                                 forwardDeclaration);

        // We must trigger the process stage for the generated class since it
        // will be used by the current class. This is because all generic types
        // in the generated class must be changed into their respective concrete
        // types before we can use it.
        generatedClass->process();

        // Insert the generated class after the current class.
        auto it = definitionBeingProcessed;
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
        generatedClass->process();
    } else {
        // Insert the generated class in the global name bindings.
        globalNameBindings.insertClass(generatedClass->getName(),
                                       generatedClass);

        // Insert the generated class and its forward declaration before the
        // current class.
        globalDefinitions.insert(definitionBeingProcessed,
                                 forwardDeclaration);
        generatedClass->process();
        globalDefinitions.insert(definitionBeingProcessed,
                                 generatedClass);
    }
}
