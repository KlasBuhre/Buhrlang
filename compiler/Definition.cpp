#include <memory>
#include <algorithm>
#include <assert.h>

#include "Definition.h"
#include "Statement.h"
#include "Expression.h"
#include "Context.h"
#include "Tree.h"

namespace {

    struct ClassParents {
        ClassParents() :
            definitions(),
            concretBaseClass(nullptr),
            isProcessInterfacePresent(false) {}

        ClassList definitions;
        ClassDefinition* concretBaseClass;
        bool isProcessInterfacePresent;
    };

    void addParentDefinition(
        const Identifier& parentName,
        ClassParents& classParents,
        const NameBindings* enclosingBindings,
        const Location& location,
        const ClassDefinition::Properties& properties) {

        Definition* parentDefinition =
            enclosingBindings->lookupType(parentName);
        if (parentDefinition == nullptr) {
            Trace::error("Unknown class: " + parentName, location);
        }

        ClassDefinition* parentClass =
            parentDefinition->dynCast<ClassDefinition>();
        if (!parentClass->isGenerated() && parentClass->isProcess() &&
            parentClass->isInterface()) {
            classParents.isProcessInterfacePresent = true;
        }

        if (!parentClass->isInterface()) {
            if (properties.isInterface) {
                Trace::error(
                    "Interfaces cannot inherit from concrete classes: " +
                    parentName,
                    location);
            }
            if (classParents.concretBaseClass != nullptr) {
                Trace::error(
                    "Cannot inherit from more than one concrete base class: " +
                    parentName,
                    location);
            }
            classParents.concretBaseClass = parentClass;
        }
        classParents.definitions.push_back(parentClass);
    }
}

Definition::Definition(
    DefinitionType t,
    const Identifier& n,
    const Location& l) :
    Node(l),
    name(n),
    enclosingDefinition(nullptr),
    definitionType(t),
    imported(false) {}

Definition::Definition(const Definition& other) :
    Node(other),
    name(other.name),
    enclosingDefinition(other.enclosingDefinition),
    definitionType(other.definitionType),
    imported(other.imported) {}

ClassDefinition* Definition::getEnclosingClass() const {
    if (enclosingDefinition != nullptr && enclosingDefinition->isClass()) {
        return enclosingDefinition->cast<ClassDefinition>();
    }
    return nullptr;
}

ClassDefinition::ClassDefinition(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    ClassDefinition* base,
    const ClassList& parents,
    NameBindings* enclosingBindings,
    const Properties& p,
    const Location& l) :
    Definition(Definition::Class, name, l),
    baseClass(base),
    parentClasses(parents),
    members(),
    methods(),
    dataMembers(),
    primaryCtorArgDataMembers(),
    genericTypeParameters(),
    nameBindings(enclosingBindings),
    memberInitializationContext(nullptr),
    staticMemberInitializationContext(nullptr),
    properties(p),
    hasConstructor(false),
    hasBeenProcessed(false),
    isRec(false) {

    copyParentClassesNameBindings();
    copyGenericTypeParameters(genericTypeParameters);
}

ClassDefinition::ClassDefinition(const ClassDefinition& other) :
    Definition(other),
    baseClass(other.baseClass),
    parentClasses(other.parentClasses),
    members(),
    methods(),
    dataMembers(),
    primaryCtorArgDataMembers(),
    genericTypeParameters(),
    nameBindings(other.nameBindings.getEnclosing()),
    memberInitializationContext(other.memberInitializationContext),
    staticMemberInitializationContext(other.memberInitializationContext),
    properties(other.properties),
    hasConstructor(other.hasConstructor),
    hasBeenProcessed(other.hasBeenProcessed),
    isRec(other.isRec) {

    copyParentClassesNameBindings();
    copyGenericTypeParameters(other.genericTypeParameters);
    copyMembers(other.members);
}

ClassDefinition* ClassDefinition::create(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    const IdentifierList& parentNames,
    NameBindings* enclosingBindings,
    const Properties& properties,
    const Location& location) {

    ClassParents classParents;
    for (IdentifierList::const_iterator i = parentNames.begin();
         i != parentNames.end();
         i++) {
        const Identifier& parentName = *i;
        addParentDefinition(parentName,
                            classParents,
                            enclosingBindings,
                            location,
                            properties);
    }

    if (!properties.isInterface && !properties.isEnumeration &&
        !properties.isEnumerationVariant &&
        classParents.concretBaseClass == nullptr) {
        addParentDefinition(Keyword::objectString,
                            classParents,
                            enclosingBindings,
                            location,
                            properties);
    }

    if (!properties.isGenerated && !properties.isProcess &&
        !properties.isInterface && classParents.isProcessInterfacePresent) {
        // This is a regular class that implements a process interface. The
        // class must implement the MessageHandler interface.
        addParentDefinition(CommonNames::messageHandlerTypeName,
                            classParents,
                            enclosingBindings,
                            location,
                            properties);
    }

    return new ClassDefinition(name,
                               genericTypeParameters,
                               classParents.concretBaseClass,
                               classParents.definitions,
                               enclosingBindings,
                               properties,
                               location);
}

void ClassDefinition::copyParentClassesNameBindings() {
    for (ClassList::const_iterator i = parentClasses.begin();
         i != parentClasses.end();
         i++) {
        ClassDefinition* parentClassDef = *i;
        nameBindings.copyFrom(parentClassDef->getNameBindings());
    }
}

void ClassDefinition::copyMembers(const DefinitionList& from) {
    for (DefinitionList::const_iterator i = from.begin();
         i != from.end();
         i++) {
        appendMember((*i)->clone());
    }
}

void ClassDefinition::copyGenericTypeParameters(
    const GenericTypeParameterList& from) {

    for (GenericTypeParameterList::const_iterator i = from.begin();
         i != from.end();
         i++) {
        addGenericTypeParameter((*i)->clone());
    }
}

ClassDefinition* ClassDefinition::clone() const {
    return new ClassDefinition(*this);
}

Identifier ClassDefinition::getFullName() const {
    if (genericTypeParameters.empty() || name.find('<') != std::string::npos) {
        return name;
    }

    bool insertComma = false;
    Identifier fullName = name + '<';
    for (GenericTypeParameterList::const_iterator i =
            genericTypeParameters.begin();
         i != genericTypeParameters.end();
         i++) {
        GenericTypeParameterDefinition* gericTypeParameterDef = *i;
        if (insertComma) {
            fullName += ',';
        }
        if (Type* concreteTypeParam =
                gericTypeParameterDef->getConcreteType()) {
            fullName += concreteTypeParam->getFullConstructedName();
        } else {
            fullName += gericTypeParameterDef->getName();
        }
        insertComma = true;
    }
    fullName += '>';
    return fullName;
}

void ClassDefinition::appendMember(Definition* member) {
    addMember(member);
    members.push_back(member);
}

void ClassDefinition::insertMember(
    Definition* existingMember,
    Definition* newMember,
    bool insertAfterExistingMember) {

    addMember(newMember);

    DefinitionList::iterator i = std::find(members.begin(),
                                           members.end(),
                                           existingMember);
    assert(i != members.end());
    if (insertAfterExistingMember) {
        i++;
    }
    members.insert(i, newMember);
}

void ClassDefinition::addMember(Definition* definition) {
    definition->setEnclosingDefinition(this);

    switch (definition->getDefinitionType()) {
        case Definition::Member:
            addClassMemberDefinition(definition->cast<ClassMemberDefinition>());
            break;
        case Definition::Class:
            addNestedClass(definition->cast<ClassDefinition>());
            break;
        default:
            break;
    }
}

void ClassDefinition::addNestedClass(ClassDefinition* classDefinition) {
    classDefinition->getNameBindings().setEnclosing(&nameBindings);
    classDefinition->generateDefaultConstructorIfNeeded();
    nameBindings.insertClass(classDefinition->getName(), classDefinition);
}

void ClassDefinition::addClassMemberDefinition(ClassMemberDefinition* member) {
    Type* type = nullptr;
    if (member->isDataMember()) {
        type = member->cast<DataMemberDefinition>()->getType();
    } else {
        type = member->cast<MethodDefinition>()->getReturnType();
        if (type->isImplicit()) {
            Trace::error("Methods can not have implicit return type.",
                         member->getLocation());
        }
    }

    if (name.compare(Keyword::objectString) != 0) {
        // Lookup the definition of the data member type or method return type.
        // This cannot be done if we are adding members to the object class.
        // This is because no other types other than object exists at that stage
        // because object is the first built-in type that is defined.
        Tree::lookupAndSetTypeDefinition(type,
                                         nameBindings,
                                         member->getLocation());
    }

    if (member->isDataMember()) {
        addDataMember(member->cast<DataMemberDefinition>());
    } else {
        addMethod(member->cast<MethodDefinition>());
    }
}

void ClassDefinition::addDataMember(DataMemberDefinition* dataMember) {
    if (!nameBindings.insertDataMember(dataMember->getName(), dataMember)) {
        Trace::error("Identifier already declared: " + dataMember->getName(),
                     dataMember->getLocation());
    }
    if (dataMember->isPrimaryConstructorArgument()) {
        primaryCtorArgDataMembers.push_back(dataMember);
    }
    dataMembers.push_back(dataMember);
}

void ClassDefinition::addMethod(MethodDefinition* newMethod) {
    if (newMethod->isConstructor()) {
        newMethod->setName(name + "_" + Keyword::initString);
        hasConstructor = true;
    }
    Binding* methodBinding = nameBindings.lookupLocal(newMethod->getName());
    if (methodBinding) {
        if (methodBinding->getReferencedEntity() != Binding::Method) {
            Trace::error("Identifier already defined: " + newMethod->getName(), 
                         newMethod->getLocation());
        }
        Binding::MethodList& methodList = methodBinding->getMethodList();
        for (Binding::MethodList::const_iterator i = methodList.begin();
             i != methodList.end();
             i++) {
            const MethodDefinition* method = *i;
            if (!method->isAbstract() &&
                method->isCompatible(newMethod->getArgumentList())) {
                Trace::error("Method with same arguments already defined. "
                             "Cannot overload: " +
                             newMethod->getName(), 
                             newMethod->getLocation());
            }
        }
        methodList.push_back(newMethod);
    } else {
        nameBindings.insertMethod(newMethod->getName(), newMethod);
    }
    BlockStatement* body = newMethod->getBody();
    if (body != nullptr) {
        body->getNameBindings().setEnclosing(&nameBindings);
    }
    methods.push_back(newMethod);
}

void ClassDefinition::addGenericTypeParameter(
    GenericTypeParameterDefinition* typeParameter) {

    if (!nameBindings.insertGenericTypeParameter(typeParameter->getName(),
                                                 typeParameter)) {
        Trace::error("Identifier already declared: " + typeParameter->getName(),
                     typeParameter->getLocation());
    }
    genericTypeParameters.push_back(typeParameter);
}

void ClassDefinition::addPrimaryCtorArgsAsDataMembers(
    const ArgumentList& constructorArguments) {

    assert(primaryCtorArgDataMembers.size() == 0);

    for (ArgumentList::const_iterator i = constructorArguments.begin();
         i != constructorArguments.end();
         i++) {
        VariableDeclaration* varDeclaration = *i;
        if (varDeclaration->isDataMember()) {
            DataMemberDefinition* dataMember =
                new DataMemberDefinition(varDeclaration->getIdentifier(),
                                         varDeclaration->getType()->clone(),
                                         AccessLevel::Public,
                                         false,
                                         true,
                                         varDeclaration->getLocation());
            appendMember(dataMember);
        }
    }
}

void ClassDefinition::addPrimaryConstructor(
    const ArgumentList& constructorArguments,
    ConstructorCallStatement* constructorCall) {

    addPrimaryCtorArgsAsDataMembers(constructorArguments);

    MethodDefinition* constructor =
        MethodDefinition::create(Keyword::initString, nullptr, false, this);
    if (constructorCall != nullptr) {
        constructor->getBody()->insertStatementAtFront(constructorCall);
    }
    constructor->generateMemberInitializationsFromConstructorArgumets(
        constructorArguments);
    appendMember(constructor);
}

void ClassDefinition::generateConstructor() {
    MethodDefinition* primaryConstructor = generateEmptyConstructor();
    primaryConstructor->generateMemberInitializations(dataMembers);
    finishGeneratedConstructor(primaryConstructor);
}

void ClassDefinition::generateDefaultConstructor() {
    MethodDefinition* defaultConstructor = generateEmptyConstructor();
    finishGeneratedConstructor(defaultConstructor);
}

void ClassDefinition::generateDefaultConstructorIfNeeded() {
    if (!hasConstructor && !isEnumeration() && !isEnumerationVariant()) {
        generateDefaultConstructor();
    }
}

MethodDefinition* ClassDefinition::generateEmptyConstructor() {
    MethodDefinition* emptyConstructor =
        MethodDefinition::create(Keyword::initString, nullptr, false, this);
    if (baseClass) {
        emptyConstructor->generateBaseClassConstructorCall(
            baseClass->getName());
    }
    return emptyConstructor;
}

void ClassDefinition::finishGeneratedConstructor(
    MethodDefinition* constructor) {

    appendMember(constructor);
    if (hasBeenProcessed) {
        // We must trigger the processing of the generated constructor if this
        // class has already been processed.
        constructor->process();
    }
}

MethodDefinition* ClassDefinition::getDefaultConstructor() const {
    for (MemberMethodList::const_iterator i = methods.begin();
         i != methods.end();
         i++) {
        MethodDefinition* method = *i;
        if (method->isConstructor() && method->getArgumentList().empty()) {
            return method;
        }
    }
    return nullptr;
}

Context& ClassDefinition::getMemberInitializationContext() {
    if (memberInitializationContext == nullptr) {
        memberInitializationContext =
            createInitializationContext(Keyword::initString, false);
    }
    return *memberInitializationContext;
}

Context& ClassDefinition::getStaticMemberInitializationContext() {
    if (staticMemberInitializationContext == nullptr) {
        staticMemberInitializationContext =
            createInitializationContext("staticInitializer", true);
    }
    return *staticMemberInitializationContext;
}

Context* ClassDefinition::createInitializationContext(
    const Identifier& methodName,
    bool isStatic) {

    MethodDefinition* method = MethodDefinition::create(methodName,
                                                        nullptr,
                                                        isStatic,
                                                        this);
    Context* context = new Context(method);
    context->enterBlock(method->getBody());
    return context;
}

void ClassDefinition::process() {
    if (isGeneric() || hasBeenProcessed) {
        // Generic classes should not be processed. They only act as templates
        // for concrete classes.
        return;
    }

    // First process the method signatures. This is to ensure that return values
    // and arguments of generic types are assigned concrete types before any
    // further processing.
    processMethodSignatures();

    // Process the data members. This is to ensure that data members of generic/
    // implicit types are assigned concrete types before the method bodies are
    // processed.
    processDataMembers();

    // Process the rest of the members, such as method bodies, nested classes
    // and so on.
    for (DefinitionList::const_iterator i = members.begin();
         i != members.end(); 
         i++) {
        Definition* memberDefinition = *i;
        if (!DataMemberDefinition::isDataMember(memberDefinition)) {
            memberDefinition->process();
        }
    }

    hasBeenProcessed = true;
}

void ClassDefinition::processMethodSignatures() {
    for (MemberMethodList::const_iterator i = methods.begin();
         i != methods.end();
         i++) {
        MethodDefinition* method = *i;
        method->processSignature();
    }
}

void ClassDefinition::processDataMembers() {
    for (DataMemberList::const_iterator i = dataMembers.begin();
         i != dataMembers.end();
         i++) {
        DataMemberDefinition* dataMember = *i;
        dataMember->process();
    }
}

bool ClassDefinition::isSubclassOf(const ClassDefinition* otherClass) const {
    for (ClassList::const_iterator i = parentClasses.begin();
         i != parentClasses.end();
         i++) {
        ClassDefinition* parentClassDef = *i;
        if (parentClassDef->getName().compare(otherClass->getName()) == 0 ||
            parentClassDef->isSubclassOf(otherClass)) {
            return true;
        }
    }
    return false;
}

bool ClassDefinition::isInheritingFromProcessInterface() const {
    for (ClassList::const_iterator i = parentClasses.begin();
         i != parentClasses.end();
         i++) {
        ClassDefinition* parentClassDef = *i;
        if ((parentClassDef->isProcess() && parentClassDef->isInterface()) ||
             parentClassDef->isInheritingFromProcessInterface()) {
            return true;
        }
    }
    return false;
}

bool ClassDefinition::isReferenceType() {
    std::unique_ptr<Type> type(Type::create(name));
    type->setDefinition(this);
    return type->isReference();
}

MethodDefinition* ClassDefinition::getMainMethod() const {
    const Binding* methodBinding = nameBindings.lookupLocal("main");
    if (methodBinding &&
        methodBinding->getReferencedEntity() == Binding::Method) {
        const Binding::MethodList& methodList = methodBinding->getMethodList();
        for (Binding::MethodList::const_iterator i = methodList.begin();
             i != methodList.end();
             i++) {
            MethodDefinition* method = *i;
            if (method->isStatic() &&
                method->getReturnType()->isVoid() &&
                method->getArgumentList().empty()) {                
                return method;
            }
        }
    }
    return nullptr;
}

ClassDefinition* ClassDefinition::getNestedClass(
    const Identifier& className) const {

    Binding* binding = nameBindings.lookupLocal(className);
    if (binding != nullptr &&
        binding->getReferencedEntity() == Binding::Class) {
        return binding->getDefinition()->cast<ClassDefinition>();
    }
    return nullptr;
}

bool ClassDefinition::isGeneric() const {
    for (GenericTypeParameterList::const_iterator i =
             genericTypeParameters.begin();
         i != genericTypeParameters.end();
         i++) {
        GenericTypeParameterDefinition* genericTypeParameter = *i;
        if (genericTypeParameter->getConcreteType() == nullptr) {
            // No concrete type has been set for the type parameter, which means
            // the class is generic.
            return true;
        }
    }
    return false;
}

void ClassDefinition::setConcreteTypeParameters(
    const TypeList& concreteTypeParameters,
    const Location& loc) {

    if (genericTypeParameters.size() != concreteTypeParameters.size()) {
        Trace::error("Wrong number of type parameters.", loc);
    }

    // Set the concrete type parameters in the generic type parameter
    // definitions. Also, add the concrete types to the name of this class.
    name += '<';
    bool insertComma = false;
    GenericTypeParameterList::const_iterator i = genericTypeParameters.begin();
    TypeList::const_iterator j = concreteTypeParameters.begin();
    while (i != genericTypeParameters.end()) {
        GenericTypeParameterDefinition* genericTypeParameter = *i;
        Type* concreteTypeParameter = *j;
        genericTypeParameter->setConcreteType(concreteTypeParameter);
        if (insertComma) {
            name += ',';
        }
        name += concreteTypeParameter->getFullConstructedName();
        insertComma = true;
        i++;
        j++;
    }
    name += '>';

    // Since the class name is change, the names of the constructors must be
    // changed also.
    updateConstructorName();
}

void ClassDefinition::updateConstructorName() {
    Identifier newName = name + "_" + Keyword::initString;
    for (MemberMethodList::const_iterator i = methods.begin();
         i != methods.end();
         i++) {
        MethodDefinition* method = *i;
        if (method->isConstructor()) {
            nameBindings.updateMethodName(method->getName(), newName);
            method->setName(newName);
        }
    }
}

void ClassDefinition::transformIntoInterface() {
    DefinitionList::iterator i = members.begin();
    while (i != members.end()) {
        Definition* definition = *i;
        if (definition->getDefinitionType() != Definition::Member) {
            continue;
        }
        ClassMemberDefinition* member =
            definition->cast<ClassMemberDefinition>();
        MethodDefinition* method = member->dynCast<MethodDefinition>();
        bool removeMethod = false;
        if (method != nullptr) {
            if (isMethodImplementingParentInterfaceMethod(method) ||
                method->isConstructor()) {
                removeMethod = true;
            } else {
                method->transformIntoAbstract();
            }
        }
        if (removeMethod) {
            // Remove method.
            nameBindings.removeLastOverloadedMethod(method->getName());
            i = members.erase(i);
        } else if (member->isDataMember()) {
            // Remove data member.
            nameBindings.removeDataMember(member->getName());
            i = members.erase(i);
        } else {
            i++;
        }
    }

    properties.isInterface = true;
    properties.isGenerated = true;
}

bool ClassDefinition::isMethodImplementingParentInterfaceMethod(
    MethodDefinition* method) const {

    if (method->getEnclosingDefinition() != this) {
        for (MemberMethodList::const_iterator i = methods.begin();
             i != methods.end();
             i++) {
            MethodDefinition* memberMethod = *i;
            if (method->implements(memberMethod)) {
                return true;
            }
        }
    }

    for (ClassList::const_iterator i = parentClasses.begin();
         i != parentClasses.end();
         i++) {
        ClassDefinition* parent = *i;
        if (parent->isMethodImplementingParentInterfaceMethod(method)) {
            return true;
        }
    }

    return false;
}

ClassMemberDefinition::ClassMemberDefinition(
    MemberType m,
    const Identifier& name,
    AccessLevel::AccessLevelType a, 
    bool s, 
    const Location& l) :
    Definition(Definition::Member, name, l),
    memberType(m),
    access(a),
    staticMember(s) {}

ClassMemberDefinition::ClassMemberDefinition(
    const ClassMemberDefinition& other) :
    Definition(other),
    memberType(other.memberType),
    access(other.access),
    staticMember(other.staticMember) {}

MethodDefinition::MethodDefinition(
    const Identifier& name,
    Type* retType,
    Definition* e) :
    MethodDefinition(name,
                     retType,
                     AccessLevel::Public,
                     false,
                     e,
                     e->getLocation()) {}

MethodDefinition::MethodDefinition(
    const Identifier& name, 
    Type* retType,
    AccessLevel::AccessLevelType access, 
    bool isStatic, 
    Definition* e,
    const Location& l) :
    ClassMemberDefinition(ClassMemberDefinition::Method,
                          name,
                          access,
                          isStatic,
                          l),
    returnType(retType ? retType : new Type(Type::Void)),
    argumentList(),
    body(nullptr),
    lambdaSignature(nullptr),
    isCtor(name.compare(Keyword::initString) == 0),
    isEnumCtor(false),
    isFunc(false),
    hasBeenProcessed(false),
    hasSignatureBeenProcessed(false) {

    setEnclosingDefinition(e);
}

MethodDefinition::MethodDefinition(const MethodDefinition& other) :
    ClassMemberDefinition(other),
    returnType(other.returnType->clone()),
    argumentList(),
    body(other.body ? other.body->clone() : nullptr),
    lambdaSignature(other.lambdaSignature ?
                    new LambdaSignature(*(other.lambdaSignature)) : nullptr),
    isCtor(other.isCtor),
    isEnumCtor(other.isEnumCtor),
    isFunc(other.isFunc),
    hasBeenProcessed(other.hasBeenProcessed),
    hasSignatureBeenProcessed(other.hasSignatureBeenProcessed) {

    copyArgumentList(other.argumentList);
}

void MethodDefinition::copyArgumentList(const ArgumentList& from) {
    for (ArgumentList::const_iterator i = from.begin(); i != from.end(); i++) {
        VariableDeclaration* argument = *i;
        addArgument(new VariableDeclaration(*argument));
    }
}

MethodDefinition* MethodDefinition::create(
    const Identifier& name,
    Type* retType,
    bool isStatic,
    ClassDefinition* classDefinition) {

    const Location& location = classDefinition->getLocation();
    MethodDefinition* emptyMethod = new
        MethodDefinition(name,
                         retType,
                         AccessLevel::Public,
                         isStatic,
                         classDefinition,
                         location);
    BlockStatement* body = new BlockStatement(classDefinition,
                                              nullptr,
                                              location);
    emptyMethod->setBody(body);
    return emptyMethod;
}

Definition* MethodDefinition::clone() const {
    return new MethodDefinition(*this);
}

std::string MethodDefinition::toString() const {
    std::string str;
    if (isStatic() && !isFunction()) {
        str += "static ";
    }
    if (returnType != nullptr && !returnType->isVoid()) {
        str += returnType->toString() + " ";
    }
    if (!isFunction()) {
        if (const ClassDefinition* classDef = getEnclosingClass()) {
            str += classDef->getFullName();
        } else {
            str += getEnclosingDefinition()->getName();
        }
        str += ".";
    }
    if (isConstructor()) {
        str += Keyword::initString;
    } else {
        str += getName();
    }
    str += "(";
    bool addComma = false;
    for (ArgumentList::const_iterator i = argumentList.begin();
         i != argumentList.end();
         i++) {
        if (addComma) {
            str += ", ";
        }
        VariableDeclaration* variableDeclaration = *i;
        str += variableDeclaration->getType()->toString();
        addComma = true;
    }
    str += ")";
    return str;
}

void MethodDefinition::addArgument(VariableDeclaration* argument) {
    Type* type = argument->getType();
    if (type->isImplicit()) {
        Trace::error("Method arguments can not have implicit type.",
                     argument->getLocation());
    }
    argumentList.push_back(argument);
    if (body != nullptr && !type->isLambda()) {
        body->addLocalBinding(argument);
    } else {
        assert(enclosingDefinition->isClass());
        const NameBindings& nameBindings =
            enclosingDefinition->cast<ClassDefinition>()->getNameBindings();

        Tree::lookupAndSetTypeDefinition(type,
                                         nameBindings,
                                         argument->getLocation());
    }
}

void MethodDefinition::addArgument(
    Type::BuiltInType type,
    const Identifier& argumentName) {

    addArgument(new VariableDeclaration(new Type(type),
                                        argumentName,
                                        enclosingDefinition->getLocation()));
}

void MethodDefinition::addArgument(Type* type, const Identifier& argumentName) {
    addArgument(new VariableDeclaration(type->clone(),
                                        argumentName,
                                        enclosingDefinition->getLocation()));
}

void MethodDefinition::addArgument(
    const Identifier& typeName,
    const Identifier& argumentName) {

    addArgument(new VariableDeclaration(Type::create(typeName),
                                        argumentName,
                                        enclosingDefinition->getLocation()));
}

void MethodDefinition::addArguments(const ArgumentList& arguments) {
    for (ArgumentList::const_iterator i = arguments.begin();
         i != arguments.end();
         i++) {
        VariableDeclaration* argument = *i;
        addArgument(argument);
    }
}

void MethodDefinition::processSignature() {
    assert(enclosingDefinition->isClass());
    const NameBindings& nameBindings =
        enclosingDefinition->cast<ClassDefinition>()->getNameBindings();

    // Change any generic type parameters into concrete types in the signature
    // and return type.
    updateGenericReturnType(nameBindings);
    updateGenericTypesInArgumentList(nameBindings);
    if (lambdaSignature != nullptr) {
        updateGenericTypesInLambdaSignature(nameBindings);

        // Transform the argument names to avoid name collisions with names
        // in methods that call this method. Since this method takes a lambda
        // it will be inlined in the calling method.
        if (body != nullptr) {
            makeArgumentNamesUnique();
        }
    }

    hasSignatureBeenProcessed = true;
}

void MethodDefinition::process() {
    if (!hasSignatureBeenProcessed) {
        processSignature();
    }

    if (isCtor) {
        // Do constructor-specific processing.
        processConstructor();
    }

    if (body != nullptr) {
        // Process the method body.
        Context context(this);
        body->typeCheck(context);
        if (!returnType->isVoid()) {
            checkForReturnStatement();
        }
    }

    hasBeenProcessed = true;
}

void MethodDefinition::updateGenericReturnType(
    const NameBindings& nameBindings) {

    const Location& loc = getLocation();
    Tree::lookupAndSetTypeDefinition(returnType, nameBindings, loc);
    Type* concreteType = Tree::makeGenericTypeConcrete(returnType,
                                                       nameBindings,
                                                       loc);
    if (concreteType != nullptr) {
        // The return type is a generic type parameter that has been assigned a
        // concrete type. Change the return type to the concrete type.
        returnType = concreteType;
    }
}

void MethodDefinition::updateGenericTypesInArgumentList(
    const NameBindings& nameBindings) {

    for (ArgumentList::const_iterator i = argumentList.begin();
         i != argumentList.end();
         i++) {
        VariableDeclaration* argument = *i;
        Type* argumentType = argument->getType();
        const Location& loc = argument->getLocation();

        // It could be that the class has been cloned when creating a concrete
        // class from a generic class. Therefore, in order to use the name
        // bindings of the generated concrete class, we can lookup the
        // definition again.
        Tree::lookupAndSetTypeDefinition(argumentType, nameBindings, loc);

        Type* concreteType =
            Tree::makeGenericTypeConcrete(argumentType, nameBindings, loc);
        if (concreteType != nullptr) {
            // The argument type is a generic type parameter that has been
            // assigned a concrete type. Change the argument type to the
            // concrete type.
            argument->setType(concreteType);
        }
    }
}

void MethodDefinition::updateGenericTypesInLambdaSignature(
    const NameBindings& nameBindings) {

    assert(lambdaSignature != nullptr);

    const Location& loc = getLocation();
    Type* lambdaReturnType = lambdaSignature->getReturnType();
    Tree::lookupAndSetTypeDefinition(lambdaReturnType, nameBindings, loc);
    Type* concreteType = Tree::makeGenericTypeConcrete(lambdaReturnType,
                                                       nameBindings,
                                                       loc);
    if (concreteType != nullptr) {
        // The return type is a generic type parameter that has been assigned a
        // concrete type. Change the return type to the concrete type.
        lambdaSignature->setReturnType(concreteType);
    }

    TypeList& lambdaArgumentTypes = lambdaSignature->getArguments();
    for (TypeList::iterator i = lambdaArgumentTypes.begin();
         i != lambdaArgumentTypes.end();
         i++) {
        Type* argumentType = *i;
        Tree::lookupAndSetTypeDefinition(argumentType, nameBindings, loc);
        Type* argumentConcreteType = Tree::makeGenericTypeConcrete(argumentType,
                                                                   nameBindings,
                                                                   loc);
        if (argumentConcreteType != nullptr) {
            // The argument type is a generic type parameter that has been
            // assigned a concrete type. Change the argument type to the
            // concrete type.
            *i = argumentConcreteType;
        }
    }
}

void MethodDefinition::makeArgumentNamesUnique() {
    assert(body != nullptr);

    NameBindings& nameBindings = body->getNameBindings();
    for (ArgumentList::const_iterator i = argumentList.begin();
         i != argumentList.end();
         i++) {
        VariableDeclaration* argument = *i;
        if (argument->getType()->isLambda()) {
            continue;
        }

        Identifier uniqueIdentifier =
            Symbol::makeUnique(argument->getIdentifier(),
                               enclosingDefinition->getName(),
                               name);
        argument->setIdentifier(uniqueIdentifier);

        // Insert the new name in the name bindings so that the argument can be
        // resolved using the new name.
        if (!nameBindings.insertLocalObject(argument)) {
            Trace::error("Variable already declared: " +
                         argument->getIdentifier(),
                         argument->getLocation());
        }
    }
}

void MethodDefinition::processConstructor() {
    assert(isCtor);

    if (!returnType->isVoid()) {
        Trace::error("Constructor can not have return type.", this);
    }
    if (isStatic()) {
        Trace::error("Constructor can not be static.", this);
    }

    const ClassDefinition* classDefinition = getEnclosingClass();
    if (!classDefinition->isEnumeration()) {
        const ClassDefinition* baseClass = classDefinition->getBaseClass();
        if (baseClass != nullptr &&
            body->getFirstStatementAsConstructorCall() == nullptr) {
            generateBaseClassConstructorCall(baseClass->getName());
        }
        generateMemberDefaultInitializations(classDefinition->getDataMembers());
    }
}

void MethodDefinition::checkForReturnStatement() {
    const BlockStatement::StatementList& statements = body->getStatements();
    if (statements.empty()) {
    //    Trace::error("Method must return a value.", this);
    }
    // TODO: Implement a proper branch analysis algorithm to check that all
    // branches return a value.
}

void MethodDefinition::generateBaseClassConstructorCall(
    const Identifier& baseClassName) {

    MethodCallExpression* constructorCall = 
        new MethodCallExpression(baseClassName, getLocation());
    body->insertStatementAtFront(new ConstructorCallStatement(constructorCall));
}

void MethodDefinition::generateMemberInitializationsFromConstructorArgumets(
    const ArgumentList& constructorArguments) {

    const Location& location = getLocation();

    for (ArgumentList::const_iterator i = constructorArguments.begin();
         i != constructorArguments.end();
         i++) {
        VariableDeclaration* varDeclaration = *i;
        Type* varDeclarationType = varDeclaration->getType();
        const Identifier& varDeclarationName = varDeclaration->getIdentifier();

        if (varDeclaration->isDataMember()) {
            // The variable is a data member. Initialize it.
            Identifier argumentName(varDeclarationName + "_Arg");
            addArgument(varDeclarationType, argumentName);

            Expression* left = new NamedEntityExpression(varDeclarationName,
                                                         location);
            Expression* right = new NamedEntityExpression(argumentName,
                                                          location);

            // Add member initialization to constructor.
            Expression* init = new
                BinaryExpression(Operator::Assignment, left, right, location);
            body->insertStatementAfterFront(init);
        } else {
            // The variable is purely an argument and not a data member.
            addArgument(varDeclarationType, varDeclarationName);
        }
    }
}

void MethodDefinition::generateMemberInitializations(
    const DataMemberList& dataMembers) {

    const Location& location = getLocation();

    for (DataMemberList::const_iterator i = dataMembers.begin();
         i != dataMembers.end();
         i++) {
        DataMemberDefinition* dataMember = *i;
        if (dataMember->isStatic()) {
            continue;
        }

        Type* dataMemberType = dataMember->getType();
        Expression* left = new DataMemberExpression(dataMember, location);
        Expression* right = dataMember->getExpression();
        if (right == nullptr) {
            // Add argument to constructor.
            Identifier argumentName(dataMember->getName() + "_Arg");
            addArgument(dataMemberType, argumentName);

            right = new NamedEntityExpression(argumentName, location);
        } else {
            right = right->clone();
        }

        // Add member initialization to constructor.
        Expression* init = new
            BinaryExpression(Operator::Assignment, left, right, location);
        body->insertStatementAfterFront(init);
    }
}

void MethodDefinition::generateMemberDefaultInitializations(
    const DataMemberList& dataMembers) {

    ConstructorCallStatement* constructorCall =
        body->getFirstStatementAsConstructorCall();
    if (constructorCall != nullptr &&
        !constructorCall->isBaseClassConstructorCall()) {
        // The first statement in the constructor is a call to another
        // constructor in the same class. We should not default initialize data
        // members since that is done in the other constructor.
        return;
    }

    const Location& location = getLocation();
    for (DataMemberList::const_iterator i = dataMembers.begin();
         i != dataMembers.end();
         i++) {
        DataMemberDefinition* dataMember = *i;
        if (dataMember->isStatic()) {
            continue;
        }

        Expression* left = new DataMemberExpression(dataMember, location);
        Expression* right = dataMember->getExpression();
        if (right == nullptr) {
            right = Expression::generateDefaultInitialization(
                dataMember->getType(),
                location);
        } else {
            right = right->clone();
        }

        if (right != nullptr) {
            Expression* init = new
                BinaryExpression(Operator::Assignment, left, right, location);
            body->insertStatementAfterFront(init);
        }
    }
}

bool MethodDefinition::isCompatible(const ArgumentList& arguments) const {
    TypeList typeList;
    for (ArgumentList::const_iterator i = arguments.begin();
         i != arguments.end();
         i++) {
        typeList.push_back((*i)->getType());
    }
    return isCompatible(typeList);
}

bool MethodDefinition::isCompatible(const TypeList& arguments) const {
    if (arguments.size() != argumentList.size()) {
        return false;
    }
    ArgumentList::const_iterator j = argumentList.begin();
    TypeList::const_iterator i = arguments.begin();
    while (i != arguments.end()) {
        Type* typeInCall = *i;
        Type* typeInSignature = (*j)->getType();
        if (!Type::areInitializable(typeInSignature, typeInCall)) {
            return false;
        }
        i++;
        j++;
    }
    return true;
}

bool MethodDefinition::implements(
    const MethodDefinition* abstractMethod) const {

    return abstractMethod->isAbstract() &&
           name.compare(abstractMethod->name) == 0 &&
           isCompatible(abstractMethod->getArgumentList());
}

const ClassDefinition* MethodDefinition::getClass() const {
    return getEnclosingDefinition()->dynCast<ClassDefinition>();
}

void MethodDefinition::setLambdaSignature(
    LambdaSignature* s,
    const Location& loc) {

    lambdaSignature = s;
    VariableDeclaration* lambdaArgument = 
        new VariableDeclaration(new Type(Type::Lambda), "", getLocation());
    addArgument(lambdaArgument);

    assert(enclosingDefinition->isClass());
    const NameBindings& nameBindings =
        enclosingDefinition->cast<ClassDefinition>()->getNameBindings();

    Tree::lookupAndSetTypeDefinition(lambdaSignature->getReturnType(),
                                     nameBindings,
                                     loc);

    const TypeList& lambdaArgumentTypes = lambdaSignature->getArguments();
    for (TypeList::const_iterator i = lambdaArgumentTypes.begin();
         i != lambdaArgumentTypes.end();
         i++) {
        Type* argumentType = *i;
        Tree::lookupAndSetTypeDefinition(argumentType, nameBindings, loc);
    }
}

DataMemberDefinition::DataMemberDefinition(const Identifier& name, Type* typ) :
    DataMemberDefinition(name,
                         typ,
                         AccessLevel::Public,
                         false,
                         false,
                         Location()) {}

DataMemberDefinition::DataMemberDefinition(
    const Identifier& name, 
    Type* typ,
    AccessLevel::AccessLevelType access, 
    bool isStatic,
    bool isPrimaryCtorArg,
    const Location& l) :
    ClassMemberDefinition(ClassMemberDefinition::DataMember,
                          name,
                          access,
                          isStatic,
                          l),
    type(typ),
    expression(nullptr),
    isPrimaryCtorArgument(isPrimaryCtorArg) {}

DataMemberDefinition::DataMemberDefinition(const DataMemberDefinition& other) :
    ClassMemberDefinition(other),
    type(other.type->clone()),
    expression(other.expression ? other.expression->clone() : nullptr),
    isPrimaryCtorArgument(other.isPrimaryCtorArgument) {}

Definition* DataMemberDefinition::clone() const {
    return new DataMemberDefinition(*this);
}

void DataMemberDefinition::process() {
    assert(enclosingDefinition->isClass());
    ClassDefinition* classDef = enclosingDefinition->cast<ClassDefinition>();

    changeTypeIfGeneric(classDef->getNameBindings());
    if (expression == nullptr) {
        if (type->isImplicit()) {
            Trace::error("Implicitly typed data members must be initialized: " +
                         name,
                         this);
        }
    } else {
        Expression* typeCheckedInitExpression = nullptr;
        if (isStatic()) {
            typeCheckedInitExpression = typeCheckInitExpression(
                classDef->getStaticMemberInitializationContext());
        } else {
            typeCheckedInitExpression = typeCheckInitExpression(
                classDef->getMemberInitializationContext());
        }
        const Type* initType = typeCheckedInitExpression->getType();
        if (type->isImplicit()) {
            Type* copiedInitType = initType->clone();
            copiedInitType->setConstant(type->isConstant());
            type = copiedInitType;
        } else if (!Type::isInitializableByExpression(
                        type,
                        typeCheckedInitExpression)) {
            Trace::error("Type mismatch.", type, initType, this);
        }
    }
}

void DataMemberDefinition::changeTypeIfGeneric(
    const NameBindings& nameBindings) {

    const Location& loc = getLocation();

    // It could be that the type definition/class has been cloned when creating
    // a concrete class from a generic class. Therefore, to update the type to
    // refer to the definition of the concrete class, we can lookup the
    // definition again.
    Tree::lookupAndSetTypeDefinition(type, nameBindings, loc);
    Type* concreteType = Tree::makeGenericTypeConcrete(type, nameBindings, loc);
    if (concreteType != nullptr) {
        // The data member type is a generic type parameter that has been
        // assigned a concrete type. Change the data member type to the concrete
        // type.
        type = concreteType;
    }
}

Expression* DataMemberDefinition::typeCheckInitExpression(Context& context) {
    Expression* initExpression = expression->clone();
    initExpression = initExpression->transform(context);
    initExpression->typeCheck(context);
    return initExpression;
}

bool DataMemberDefinition::isDataMember(Definition* definition) {
    return definition->dynCast<DataMemberDefinition>() != nullptr;
}

GenericTypeParameterDefinition::GenericTypeParameterDefinition(
    const Identifier& name,
    const Location& l) :
    Definition(Definition::GenericTypeParameter, name, l),
    concreteType(nullptr) {
}

GenericTypeParameterDefinition::GenericTypeParameterDefinition(
    const GenericTypeParameterDefinition& other) :
    Definition(other),
    concreteType(other.concreteType ? other.concreteType->clone() : nullptr) {}

GenericTypeParameterDefinition* GenericTypeParameterDefinition::clone() const {
    return new GenericTypeParameterDefinition(*this);
}

ForwardDeclarationDefinition::ForwardDeclarationDefinition(
    const Identifier& name) :
    Definition(Definition::ForwardDeclaration, name, Location()) {}

ForwardDeclarationDefinition::ForwardDeclarationDefinition(
    const ForwardDeclarationDefinition& other) :
    Definition(other) {}

ForwardDeclarationDefinition* ForwardDeclarationDefinition::clone() const {
    return new ForwardDeclarationDefinition(*this);
}
