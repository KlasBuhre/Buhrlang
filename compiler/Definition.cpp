#include <memory>
#include <algorithm>
#include <assert.h>

#include "Definition.h"
#include "Statement.h"
#include "Expression.h"
#include "Context.h"
#include "Tree.h"
#include "CloneGenerator.h"
#include "EnumGenerator.h"

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

    bool isParentClassMessage(const ClassList& parents) {
        for (auto i = parents.cbegin(); i != parents.cend(); i++) {
            ClassDefinition* parent = *i;
            if (parent->isMessage()) {
                return true;
            }
        }
        return false;
    }
}

Definition::Definition(Kind k, const Identifier& n, const Location& l) :
    Node(l),
    name(n),
    enclosingDefinition(nullptr),
    kind(k),
    imported(false) {}

Definition::Definition(const Definition& other) :
    Node(other),
    name(other.name),
    enclosingDefinition(other.enclosingDefinition),
    kind(other.kind),
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
    Properties& properties,
    const Location& location) {

    ClassParents classParents;
    for (auto i = parentNames.cbegin(); i != parentNames.cend(); i++) {
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

    bool parentClassIsMessage = isParentClassMessage(classParents.definitions);
    if (properties.isMessage && !properties.isEnumeration &&
        !parentClassIsMessage &&
        name.compare(CommonNames::cloneableTypeName) != 0) {
        addParentDefinition(CommonNames::cloneableTypeName,
                            classParents,
                            enclosingBindings,
                            location,
                            properties);
    }

    if (parentClassIsMessage) {
        properties.isMessage = true;
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
    for (auto i = parentClasses.begin(); i != parentClasses.end(); i++) {
        ClassDefinition* parentClassDef = *i;
        nameBindings.copyFrom(parentClassDef->getNameBindings());
    }
}

void ClassDefinition::copyMembers(const DefinitionList& from) {
    for (auto i = from.cbegin(); i != from.cend(); i++) {
        appendMember((*i)->clone());
    }
}

void ClassDefinition::copyGenericTypeParameters(
    const GenericTypeParameterList& from) {

    for (auto i = from.cbegin(); i != from.cend(); i++) {
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
    for (auto i = genericTypeParameters.begin();
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

    switch (definition->getKind()) {
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
        if (type->isImplicit() && !properties.isClosure) {
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

    if (isInterface()) {
        newMethod->setIsVirtual(true);
    }

    Binding* methodBinding = nameBindings.lookupLocal(newMethod->getName());
    if (methodBinding != nullptr) {
        if (methodBinding->getReferencedEntity() != Binding::Method) {
            Trace::error("Identifier already defined: " + newMethod->getName(), 
                         newMethod->getLocation());
        }

        Binding::MethodList& methodList = methodBinding->getMethodList();
        for (auto i = methodList.cbegin(); i != methodList.cend(); i++) {
            const MethodDefinition* method = *i;
            if (method->argumentsAreEqual(newMethod->getArgumentList())) {
                if (method->getClass() == this) {
                    Trace::error("Method with same arguments already defined in"
                                 " this class. Cannot overload: " +
                                 newMethod->getName(),
                                 newMethod->getLocation());
                }
                if (method->isVirtual()) {
                    newMethod->setIsVirtual(true);
                }
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

void ClassDefinition::addParent(ClassDefinition* parent) {
    parentClasses.push_back(parent);
}

void ClassDefinition::addPrimaryCtorArgsAsDataMembers(
    const ArgumentList& constructorArguments) {

    assert(primaryCtorArgDataMembers.size() == 0);

    for (auto i = constructorArguments.cbegin();
         i != constructorArguments.cend();
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
    constructor->setIsPrimaryConstructor(true);
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
    appendMember(primaryConstructor);
}

void ClassDefinition::generateDefaultConstructor() {
    MethodDefinition* defaultConstructor = generateEmptyConstructor();
    appendMember(defaultConstructor);
}

void ClassDefinition::generateDefaultConstructorIfNeeded() {
    if (!hasConstructor && !isEnumeration() && !isEnumerationVariant() &&
        !isInterface()) {
        generateDefaultConstructor();
    }
}

void ClassDefinition::generateEmptyCopyConstructor() {
    // This constructor does not count as a normal constructor since it is
    // generated for message classes. We still want the default constructor to
    // be generated for message classes.
    bool tmp = hasConstructor;
    MethodDefinition* copyConstructor =
        MethodDefinition::create(Keyword::initString, nullptr, false, this);
    copyConstructor->addArgument(name, CommonNames::otherVariableName);
    appendMember(copyConstructor);
    hasConstructor = tmp;
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

MethodDefinition* ClassDefinition::getDefaultConstructor() const {
    for (auto i = methods.cbegin(); i != methods.cend(); i++) {
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

Traverse::Result ClassDefinition::traverse(Visitor& visitor) {
    if (visitor.visitClass(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (auto i = members.cbegin(); i != members.cend(); i++) {
        Definition* member = *i;
        switch (member->getKind()) {
            case Definition::Class:
                member->traverse(visitor);
                break;
            case Definition::Member: {
                unsigned int traverseMask = visitor.getTraverseMask();
                ClassMemberDefinition* classMember =
                    member->cast<ClassMemberDefinition>();
                switch (classMember->getKind()) {
                    case ClassMemberDefinition::Method:
                        if (traverseMask & Visitor::TraverseMethods) {
                            classMember->traverse(visitor);
                        }
                        break;
                    case ClassMemberDefinition::DataMember:
                        if (traverseMask & Visitor::TraverseDataMembers) {
                            classMember->traverse(visitor);
                        }
                        break;
                    default:
                        break;
                }
            }
            default:
                break;
        }
    }

    return Traverse::Continue;
}

void ClassDefinition::generateCloneMethod() {
    if (needsCloneMethod()) {
        if (allTypeParametersAreMessagesOrPrimitives()) {
            // Generate the cloning code for message classes.
            CloneGenerator cloneGenerator(this, Tree::getCurrentTree());
            cloneGenerator.generate();
        } else {
            // Not all type parameters are messages or primitive types. This
            // means this class cannot implement the _Cloneable interface. This
            // can happen when the class is a generated class from a generic
            // class and the type parameters are not messages or primitives.
            removeCloneableParent();
            removeCopyConstructor();
            removeMethod(CommonNames::cloneMethodName);
        }
    } else if (properties.isEnumeration) {
        if (allTypeParametersAreMessagesOrPrimitives()) {
            // Generate the deepCopy code for message enum classes.
            EnumGenerator enumGenerator(this, Tree::getCurrentTree());
            enumGenerator.generateDeepCopyMethod();
        } else {
            // Not all type parameters are messages or primitive types. This
            // means this enum cannot have the deepCopy method. This can happen
            // when the enum is a generated enum class from a generic enum class
            // and the type parameters are not messages or primitives.
            removeMethod(CommonNames::deepCopyMethodName);
        }
    }
}

bool ClassDefinition::isSubclassOf(const ClassDefinition* otherClass) const {
    for (auto i = parentClasses.cbegin(); i != parentClasses.cend(); i++) {
        const ClassDefinition* parentClassDef = *i;
        if (parentClassDef->getName().compare(otherClass->getName()) == 0 ||
            parentClassDef->isSubclassOf(otherClass)) {
            return true;
        }
    }
    return false;
}

bool ClassDefinition::isInheritingFromProcessInterface() const {
    for (auto i = parentClasses.begin(); i != parentClasses.end(); i++) {
        const ClassDefinition* parentClassDef = *i;
        if ((parentClassDef->isProcess() && parentClassDef->isInterface()) ||
             parentClassDef->isInheritingFromProcessInterface()) {
            return true;
        }
    }
    return false;
}

void ClassDefinition::checkImplementsAllAbstractMethods(
    ClassList& treePath,
    const Location& loc) {

    treePath.push_back(this);

    for (auto i = parentClasses.cbegin(); i != parentClasses.cend(); i++) {
        ClassDefinition* parentClassDef = *i;
        parentClassDef->checkImplementsAllAbstractMethods(treePath, loc);
    }

    treePath.pop_back();

    if (!isInterface()) {
        return;
    }

    for (auto i = methods.cbegin(); i != methods.cend(); i++) {
        MethodDefinition* abstractMethod = *i;
        if (!abstractMethod->isAbstract()) {
            continue;
        }
        bool methodImplemented = false;
        for (ClassList::const_iterator j = treePath.begin();
             j != treePath.end();
             j++) {
            ClassDefinition* subclass = *j;
            if (subclass->implements(abstractMethod)) {
                methodImplemented = true;
            }
        }

        if (!methodImplemented) {
            Trace::error("Can not instantiate class with abstract methods. "
                         "Abstract method not implemented: " +
                         abstractMethod->toString() +
                         ". Constructor was called here: ",
                         loc);
        }
    }
}

bool ClassDefinition::implements(const MethodDefinition* abstractMethod) const {
    for (auto i = methods.cbegin(); i != methods.cend(); i++) {
        const MethodDefinition* method = *i;
        if (method->implements(abstractMethod)) {
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
        for (auto i = methodList.cbegin(); i != methodList.cend(); i++) {
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

MethodDefinition* ClassDefinition::getCopyConstructor() const {
    for (auto i = methods.cbegin(); i != methods.cend(); i++) {
        MethodDefinition* method = *i;
        if (method->isConstructor()) {
            const ArgumentList& arguments = method->getArgumentList();
            if (arguments.size() == 1) {
                const VariableDeclaration* argument = arguments.front();
                if (argument->getType()->getDefinition() == this) {
                    return method;
                }
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
    for (auto i = genericTypeParameters.cbegin();
         i != genericTypeParameters.cend();
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
    auto i = genericTypeParameters.cbegin();
    auto j = concreteTypeParameters.cbegin();
    while (i != genericTypeParameters.cend()) {
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

bool ClassDefinition::allTypeParametersAreMessagesOrPrimitives() const {
    for (auto i = genericTypeParameters.cbegin();
         i != genericTypeParameters.cend();
         i++) {
        GenericTypeParameterDefinition* genericTypeParameter = *i;
        Type* concreteType = genericTypeParameter->getConcreteType();
        assert(concreteType != nullptr);
        if (!concreteType->isMessageOrPrimitive()) {
            return false;
        }
    }
    return true;
}

bool ClassDefinition::needsCloneMethod() {
    return properties.isMessage && !properties.isEnumeration &&
           !properties.isInterface;
}

void ClassDefinition::removeCloneableParent() {
    for (auto i = parentClasses.begin(); i != parentClasses.end(); i++) {
        ClassDefinition* parentClassDef = *i;
        if (parentClassDef->isInterface() &&
            parentClassDef->getName().compare(
                CommonNames::cloneableTypeName) == 0) {
            parentClasses.erase(i);
            break;
        }
    }
}

void ClassDefinition::removeMethod(const Identifier& methodName) {
    for (auto i = members.begin(); i!= members.end(); i++) {
        Definition* definition = *i;
        if (MethodDefinition* method =
                definition->dynCast<MethodDefinition>()) {
            if (method->getName().compare(methodName) == 0) {
                members.erase(i);
                break;
            }
        }
    }
}

void ClassDefinition::removeCopyConstructor() {
    for (auto i = members.begin(); i!= members.end(); i++) {
        Definition* definition = *i;
        if (MethodDefinition* method =
                definition->dynCast<MethodDefinition>()) {
            if (method->isConstructor()) {
                const ArgumentList& arguments = method->getArgumentList();
                if (arguments.size() == 1) {
                    const VariableDeclaration* argument = arguments.front();
                    if (argument->getType()->getDefinition() == this) {
                        members.erase(i);
                        break;
                    }
                }
            }
        }
    }
}

void ClassDefinition::updateConstructorName() {
    Identifier newName = name + "_" + Keyword::initString;
    for (auto i = methods.cbegin(); i != methods.cend(); i++) {
        MethodDefinition* method = *i;
        if (method->isConstructor()) {
            nameBindings.updateMethodName(method->getName(), newName);
            method->setName(newName);
        }
    }
}

void ClassDefinition::transformIntoInterface() {
    auto i = members.begin();
    while (i != members.end()) {
        Definition* definition = *i;
        if (definition->getKind() != Definition::Member) {
            continue;
        }
        ClassMemberDefinition* member =
            definition->cast<ClassMemberDefinition>();
        MethodDefinition* method = member->dynCast<MethodDefinition>();
        bool removeMethod = false;
        if (method != nullptr) {
            if (isMethodImplementingParentInterfaceMethod(method) ||
                method->isConstructor() ||
                method->isPrivate()) {
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
        for (auto i = methods.cbegin(); i != methods.cend(); i++) {
            MethodDefinition* memberMethod = *i;
            if (method->implements(memberMethod)) {
                return true;
            }
        }
    }

    for (auto i = parentClasses.cbegin(); i != parentClasses.cend(); i++) {
        ClassDefinition* parent = *i;
        if (parent->isMethodImplementingParentInterfaceMethod(method)) {
            return true;
        }
    }

    return false;
}

ClassMemberDefinition::ClassMemberDefinition(
    Kind k,
    const Identifier& name,
    AccessLevel::Kind a,
    bool s, 
    const Location& l) :
    Definition(Definition::Member, name, l),
    kind(k),
    access(a),
    staticMember(s) {}

ClassMemberDefinition::ClassMemberDefinition(
    const ClassMemberDefinition& other) :
    Definition(other),
    kind(other.kind),
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
    AccessLevel::Kind access,
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
    isPrimaryCtor(false),
    isEnumCtor(false),
    isEnumCopyCtor(false),
    isFunc(false),
    isClosure(false),
    isVirt(false),
    generated(true),
    hasBeenTypeCheckedAndTransformed(false) {

    setEnclosingDefinition(e);
}

MethodDefinition::MethodDefinition(const MethodDefinition& other) :
    ClassMemberDefinition(other),
    returnType(other.returnType->clone()),
    argumentList(),
    body(other.body ? other.body->clone() : nullptr),
    lambdaSignature(other.lambdaSignature ?
                    new FunctionSignature(*(other.lambdaSignature)) : nullptr),
    isCtor(other.isCtor),
    isPrimaryCtor(other.isPrimaryCtor),
    isEnumCtor(other.isEnumCtor),
    isEnumCopyCtor(other.isEnumCopyCtor),
    isFunc(other.isFunc),
    isClosure(other.isClosure),
    isVirt(other.isVirt),
    generated(other.generated),
    hasBeenTypeCheckedAndTransformed(other.hasBeenTypeCheckedAndTransformed) {

    copyArgumentList(other.argumentList);
}

void MethodDefinition::copyArgumentList(const ArgumentList& from) {
    for (auto i = from.cbegin(); i != from.cend(); i++) {
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
    for (auto i = argumentList.cbegin(); i != argumentList.cend(); i++) {
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
    if (type->isImplicit() && !isClosure) {
        Trace::error("Method arguments can not have implicit type.",
                     argument->getLocation());
    }

    argumentList.push_back(argument);
    if (body != nullptr && !isClosure && !type->isLambda()) {
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
    for (auto i = arguments.cbegin(); i != arguments.cend(); i++) {
        VariableDeclaration* argument = *i;
        addArgument(argument);
    }
}

void MethodDefinition::updateGenericTypesInSignature() {
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
}

Traverse::Result MethodDefinition::traverse(Visitor& visitor) {
    if (visitor.visitMethod(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    if (visitor.getTraverseMask() & Visitor::TraverseStatemets &&
        body != nullptr) {
        body->traverse(visitor);
    }

    return Traverse::Continue;
}

void MethodDefinition::typeCheckAndTransform() {
    if (hasBeenTypeCheckedAndTransformed) {
        return;
    }

    if (isCtor) {
        finishConstructor();
    }

    if (body != nullptr) {
        Context context(this);
        body->typeCheck(context);
    }

    hasBeenTypeCheckedAndTransformed = true;
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

    for (auto i = argumentList.cbegin(); i != argumentList.cend(); i++) {
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
    for (auto i = lambdaArgumentTypes.begin();
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

void MethodDefinition::convertClosureTypesInSignature() {
    Type* closureInterfaceType = Tree::convertToClosureInterface(returnType);
    if (closureInterfaceType != nullptr) {
        returnType = closureInterfaceType;
    }

    convertClosureTypesInArgumentList();
    if (lambdaSignature != nullptr) {
        convertClosureTypesInLambdaSignature();
    }
}

void MethodDefinition::convertClosureTypesInArgumentList() {
    for (auto i = argumentList.cbegin(); i != argumentList.cend(); i++) {
        VariableDeclaration* argument = *i;
        Type* closureInterfaceType =
            Tree::convertToClosureInterface(argument->getType());
        if (closureInterfaceType != nullptr) {
            argument->setType(closureInterfaceType);
        }
    }
}

void MethodDefinition::convertClosureTypesInLambdaSignature() {
    assert(lambdaSignature != nullptr);

    Type* closureInterfaceType =
        Tree::convertToClosureInterface(lambdaSignature->getReturnType());
    if (closureInterfaceType != nullptr) {
        lambdaSignature->setReturnType(closureInterfaceType);
    }

    TypeList& lambdaArgumentTypes = lambdaSignature->getArguments();
    for (auto i = lambdaArgumentTypes.begin();
         i != lambdaArgumentTypes.end();
         i++) {
        Type* argumentType = *i;
        Type* argumentClosureInterfaceType =
            Tree::convertToClosureInterface(argumentType);
        if (argumentClosureInterfaceType != nullptr) {
            *i = argumentClosureInterfaceType;
        }
    }
}

void MethodDefinition::makeArgumentNamesUnique() {
    assert(body != nullptr);

    NameBindings& nameBindings = body->getNameBindings();
    for (auto i = argumentList.cbegin(); i != argumentList.cend(); i++) {
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

void MethodDefinition::finishConstructor() {
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

void MethodDefinition::checkReturnStatements() {
    if (returnType->isVoid() || body == nullptr) {
        return;
    }

    if (body->mayFallThrough()) {
        Trace::error("Missing return at end of method.", this);
    }
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

    for (auto i = constructorArguments.cbegin();
         i != constructorArguments.cend();
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

    for (auto i = dataMembers.cbegin(); i != dataMembers.cend(); i++) {
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
    for (DataMemberList::const_reverse_iterator i = dataMembers.rbegin();
         i != dataMembers.rend();
         i++) {
        DataMemberDefinition* dataMember = *i;
        if (dataMember->isStatic() ||
            (isPrimaryCtor && dataMember->isPrimaryConstructorArgument())) {
            // Initializations of data members that are primary ctor arguments
            // have already been created when the primary ctor was generated.
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
            if (isPrimaryCtor) {
                // Add the member initialization after the primary ctor argument
                // initializations.
                body->addStatement(init);
            } else {
                body->insertStatementAfterFront(init);
            }
        }
    }
}

bool MethodDefinition::isCompatible(const TypeList& arguments) const {
    if (arguments.size() != argumentList.size()) {
        return false;
    }

    auto j = argumentList.cbegin();
    auto i = arguments.cbegin();
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

bool MethodDefinition::argumentsAreEqual(const ArgumentList& arguments) const {
    if (arguments.size() != argumentList.size()) {
        return false;
    }

    auto j = argumentList.cbegin();
    auto i = arguments.cbegin();
    while (i != arguments.end()) {
        Type* type = (*i)->getType();
        Type* typeInSignature = (*j)->getType();
        if (!Type::areEqualNoConstCheck(typeInSignature, type)) {
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
           argumentsAreEqual(abstractMethod->getArgumentList());
}

const ClassDefinition* MethodDefinition::getClass() const {
    return getEnclosingDefinition()->dynCast<ClassDefinition>();
}

void MethodDefinition::transformIntoAbstract() {
    body = nullptr;
    setIsVirtual(true);
}

void MethodDefinition::setLambdaSignature(
    FunctionSignature* s,
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
    for (auto i = lambdaArgumentTypes.cbegin();
         i != lambdaArgumentTypes.cend();
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
    AccessLevel::Kind access,
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
    isPrimaryCtorArgument(isPrimaryCtorArg),
    hasBeenTypeCheckedAndTransformed(false) {}

DataMemberDefinition::DataMemberDefinition(const DataMemberDefinition& other) :
    ClassMemberDefinition(other),
    type(other.type->clone()),
    expression(other.expression ? other.expression->clone() : nullptr),
    isPrimaryCtorArgument(other.isPrimaryCtorArgument),
    hasBeenTypeCheckedAndTransformed(other.hasBeenTypeCheckedAndTransformed) {}

Definition* DataMemberDefinition::clone() const {
    return new DataMemberDefinition(*this);
}

Traverse::Result DataMemberDefinition::traverse(Visitor& visitor) {
    if (visitor.visitDataMember(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    if (expression != nullptr) {
        expression->traverse(visitor);
    }

    return Traverse::Continue;
}

void DataMemberDefinition::typeCheckAndTransform() {
    if (hasBeenTypeCheckedAndTransformed) {
        return;
    }

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
        if (isStatic()) {
            typeCheckInitExpression(
               classDef->getStaticMemberInitializationContext());
        } else {
            typeCheckInitExpression(classDef->getMemberInitializationContext());
        }
        const Type* initType = expression->getType();
        if (type->isImplicit()) {
            Type* copiedInitType = initType->clone();
            copiedInitType->setConstant(type->isConstant());
            type = copiedInitType;
        } else if (!Type::isInitializableByExpression(type, expression)) {
            Trace::error("Type mismatch.", type, initType, this);
        }
    }

    hasBeenTypeCheckedAndTransformed = true;
}

void DataMemberDefinition::changeTypeIfGeneric(
    const NameBindings& nameBindings) {

    const Location& loc = getLocation();

    Tree::lookupAndSetTypeDefinition(type, nameBindings, loc);
    Type* concreteType = Tree::makeGenericTypeConcrete(type, nameBindings, loc);
    if (concreteType != nullptr) {
        // The data member type is a generic type parameter that has been
        // assigned a concrete type. Change the data member type to the concrete
        // type.
        type = concreteType;
    }
}

void DataMemberDefinition::typeCheckInitExpression(Context& context) {
    expression = expression->transform(context);
    expression->typeCheck(context);
}

bool DataMemberDefinition::isDataMember(Definition* definition) {
    return definition->dynCast<DataMemberDefinition>() != nullptr;
}

void DataMemberDefinition::convertClosureType() {
    Type* closureInterfaceType = Tree::convertToClosureInterface(type);
    if (closureInterfaceType != nullptr) {
        type = closureInterfaceType;
    }
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
