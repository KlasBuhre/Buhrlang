#include "Definition.h"

#include <memory>
#include <algorithm>
#include <assert.h>

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

        auto parentDefinition = enclosingBindings->lookupType(parentName);
        if (parentDefinition == nullptr) {
            Trace::error("Unknown class: " + parentName, location);
        }

        auto parentClass = parentDefinition->dynCast<ClassDefinition>();
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
        for (auto parent: parents) {
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
    NameBindings* enclosingBindings) {

    GenericTypeParameterList typeParams;
    ClassDefinition::Properties properties;
    ClassList parents;
    Location loc;
    return new ClassDefinition(name,
                               typeParams,
                               nullptr,
                               parents,
                               enclosingBindings,
                               properties,
                               loc);
}

ClassDefinition* ClassDefinition::create(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    const IdentifierList& parentNames,
    NameBindings* enclosingBindings,
    Properties& properties,
    const Location& location) {

    ClassParents classParents;
    for (const auto& parentName: parentNames) {
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
    for (auto parentClassDef: parentClasses) {
        nameBindings.copyFrom(parentClassDef->getNameBindings());
    }
}

void ClassDefinition::copyMembers(const DefinitionList& from) {
    for (auto i: from) {
        appendMember(i->clone());
    }
}

void ClassDefinition::copyGenericTypeParameters(
    const GenericTypeParameterList& from) {

    for (auto i: from) {
        addGenericTypeParameter(i->clone());
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
    for (auto gericTypeParameterDef: genericTypeParameters) {
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

    auto methodBinding = nameBindings.lookupLocal(newMethod->getName());
    if (methodBinding != nullptr) {
        if (methodBinding->getReferencedEntity() != Binding::Method) {
            Trace::error("Identifier already defined: " + newMethod->getName(), 
                         newMethod->getLocation());
        }

        Binding::MethodList& methodList = methodBinding->getMethodList();
        for (auto method: methodList) {
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

    auto body = newMethod->getBody();
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

    for (auto varDeclaration: constructorArguments) {
        if (varDeclaration->isDataMember()) {
            auto dataMember =
                DataMemberDefinition::create(varDeclaration->getIdentifier(),
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

    auto constructor =
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
    auto primaryConstructor = generateEmptyConstructor();
    primaryConstructor->generateMemberInitializations(dataMembers);
    appendMember(primaryConstructor);
}

void ClassDefinition::generateDefaultConstructor() {
    auto defaultConstructor = generateEmptyConstructor();
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
    auto copyConstructor =
        MethodDefinition::create(Keyword::initString, nullptr, false, this);
    copyConstructor->addArgument(name, CommonNames::otherVariableName);
    appendMember(copyConstructor);
    hasConstructor = tmp;
}

MethodDefinition* ClassDefinition::generateEmptyConstructor() {
    auto emptyConstructor =
        MethodDefinition::create(Keyword::initString, nullptr, false, this);
    if (baseClass) {
        emptyConstructor->generateBaseClassConstructorCall(
            baseClass->getName());
    }
    return emptyConstructor;
}

MethodDefinition* ClassDefinition::getDefaultConstructor() const {
    for (auto method: methods) {
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

    auto method = MethodDefinition::create(methodName, nullptr, isStatic, this);
    auto context = new Context(method);
    context->enterBlock(method->getBody());
    return context;
}

Traverse::Result ClassDefinition::traverse(Visitor& visitor) {
    if (visitor.visitClass(*this) == Traverse::Skip) {
        return Traverse::Continue;
    }

    for (auto member: members) {
        switch (member->getKind()) {
            case Definition::Class:
                member->traverse(visitor);
                break;
            case Definition::Member: {
                unsigned int traverseMask = visitor.getTraverseMask();
                auto classMember = member->cast<ClassMemberDefinition>();
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
            // Generate the cloning code for message class.
            CloneGenerator::generate(this, Tree::getCurrentTree());
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
    for (auto parentClassDef: parentClasses) {
        if (parentClassDef->getName().compare(otherClass->getName()) == 0 ||
            parentClassDef->isSubclassOf(otherClass)) {
            return true;
        }
    }
    return false;
}

bool ClassDefinition::isInheritingFromProcessInterface() const {
    for (auto parentClassDef: parentClasses) {
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

    for (auto parentClassDef: parentClasses) {
        parentClassDef->checkImplementsAllAbstractMethods(treePath, loc);
    }

    treePath.pop_back();

    if (!isInterface()) {
        return;
    }

    for (auto abstractMethod: methods) {
        if (!abstractMethod->isAbstract()) {
            continue;
        }
        bool methodImplemented = false;
        for (auto subclass: treePath) {
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
    for (auto method: methods) {
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
    const auto methodBinding = nameBindings.lookupLocal("main");
    if (methodBinding &&
        methodBinding->getReferencedEntity() == Binding::Method) {
        for (auto method: methodBinding->getMethodList()) {
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
    for (auto method: methods) {
        if (method->isConstructor()) {
            const ArgumentList& arguments = method->getArgumentList();
            if (arguments.size() == 1) {
                auto argument = arguments.front();
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

    auto binding = nameBindings.lookupLocal(className);
    if (binding != nullptr &&
        binding->getReferencedEntity() == Binding::Class) {
        return binding->getDefinition()->cast<ClassDefinition>();
    }
    return nullptr;
}

bool ClassDefinition::isGeneric() const {
    for (auto genericTypeParameter: genericTypeParameters) {
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
    for (auto genericTypeParameter: genericTypeParameters) {
        auto concreteType = genericTypeParameter->getConcreteType();
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
    auto i = std::find_if(
        parentClasses.begin(),
        parentClasses.end(),
        [] (const ClassDefinition* parent) {
            if (parent->isInterface() &&
                parent->getName().compare(CommonNames::cloneableTypeName) == 0) {
                return true;
            }
            return false;
        });
    if (i != parentClasses.end()) {
        parentClasses.erase(i);
    }
}

void ClassDefinition::removeMethod(const Identifier& methodName) {
    members.erase(
        std::remove_if(
            members.begin(),
            members.end(),
            [&] (const Definition* member) {
                if (auto method = member->dynCast<MethodDefinition>()) {
                    if (method->getName().compare(methodName) == 0) {
                        return true;
                    }
                }
                return false;
            }),
        members.end());
}

void ClassDefinition::removeCopyConstructor() {
    auto thisPtr = this;
    auto i = std::find_if(
        members.begin(),
        members.end(),
        [=] (const Definition* member) {
            if (auto method = member->dynCast<MethodDefinition>()) {
                if (method->isConstructor()) {
                    const ArgumentList& arguments = method->getArgumentList();
                    if (arguments.size() == 1) {
                        auto argument = arguments.front();
                        return argument->getType()->getDefinition() == thisPtr;
                    }
                }
            }
            return false;
        });
    if (i != members.end()) {
        members.erase(i);
    }
}

void ClassDefinition::updateConstructorName() {
    Identifier newName = name + "_" + Keyword::initString;
    for (auto method: methods) {
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
        auto member = definition->cast<ClassMemberDefinition>();
        auto method = member->dynCast<MethodDefinition>();
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
        for (auto memberMethod: methods) {
            if (method->implements(memberMethod)) {
                return true;
            }
        }
    }

    for (auto parent: parentClasses) {
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
    AccessLevel::Kind access,
    bool isStatic, 
    Definition* e,
    const Location& l) :
    ClassMemberDefinition(ClassMemberDefinition::Method,
                          name,
                          access,
                          isStatic,
                          l),
    returnType(retType ? retType : Type::create(Type::Void)),
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
                    other.lambdaSignature->clone() : nullptr),
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
    for (auto argument: from) {
        addArgument(argument->clone());
    }
}

MethodDefinition* MethodDefinition::create(
    const Identifier& name,
    Type* retType,
    AccessLevel::Kind access,
    bool isStatic,
    Definition* e,
    const Location& l) {

    return new MethodDefinition(name, retType, access, isStatic, e, l);
}

MethodDefinition* MethodDefinition::create(
    const Identifier& name,
    Type* retType,
    Definition* e) {

    return new MethodDefinition(name,
                                retType,
                                AccessLevel::Public,
                                false,
                                e,
                                e->getLocation());
}

MethodDefinition* MethodDefinition::create(
    const Identifier& name,
    Type* retType,
    bool isStatic,
    ClassDefinition* classDefinition) {

    const Location& location = classDefinition->getLocation();
    auto emptyMethod =
        new MethodDefinition(name,
                             retType,
                             AccessLevel::Public,
                             isStatic,
                             classDefinition,
                             location);
    auto body = BlockStatement::create(classDefinition, nullptr, location);
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
    for (auto variableDeclaration: argumentList) {
        if (addComma) {
            str += ", ";
        }
        str += variableDeclaration->getType()->toString();
        addComma = true;
    }
    str += ")";
    return str;
}

void MethodDefinition::addArgument(VariableDeclaration* argument) {
    auto type = argument->getType();
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

    addArgument(VariableDeclaration::create(
        Type::create(type),
        argumentName,
        enclosingDefinition->getLocation()));
}

void MethodDefinition::addArgument(Type* type, const Identifier& argumentName) {
    addArgument(VariableDeclaration::create(
        type->clone(),
        argumentName,
        enclosingDefinition->getLocation()));
}

void MethodDefinition::addArgument(
    const Identifier& typeName,
    const Identifier& argumentName) {

    addArgument(VariableDeclaration::create(
        Type::create(typeName),
        argumentName,
        enclosingDefinition->getLocation()));
}

void MethodDefinition::addArguments(const ArgumentList& arguments) {
    for (auto argument: arguments) {
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
    auto concreteType = Tree::makeGenericTypeConcrete(returnType,
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

    for (auto argument: argumentList) {
        auto argumentType = argument->getType();
        const Location& loc = argument->getLocation();

        // It could be that the class has been cloned when creating a concrete
        // class from a generic class. Therefore, in order to use the name
        // bindings of the generated concrete class, we can lookup the
        // definition again.
        Tree::lookupAndSetTypeDefinition(argumentType, nameBindings, loc);

        auto concreteType =
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
    auto lambdaReturnType = lambdaSignature->getReturnType();
    Tree::lookupAndSetTypeDefinition(lambdaReturnType, nameBindings, loc);
    auto concreteType =
        Tree::makeGenericTypeConcrete(lambdaReturnType, nameBindings, loc);
    if (concreteType != nullptr) {
        // The return type is a generic type parameter that has been assigned a
        // concrete type. Change the return type to the concrete type.
        lambdaSignature->setReturnType(concreteType);
    }

    for (auto& argumentType: lambdaSignature->getArguments()) {
        Tree::lookupAndSetTypeDefinition(argumentType, nameBindings, loc);
        auto argumentConcreteType =
            Tree::makeGenericTypeConcrete(argumentType, nameBindings, loc);
        if (argumentConcreteType != nullptr) {
            // The argument type is a generic type parameter that has been
            // assigned a concrete type. Change the argument type to the
            // concrete type.
            argumentType = argumentConcreteType;
        }
    }
}

void MethodDefinition::convertClosureTypesInSignature() {
    auto closureInterfaceType = Tree::convertToClosureInterface(returnType);
    if (closureInterfaceType != nullptr) {
        returnType = closureInterfaceType;
    }

    convertClosureTypesInArgumentList();
    if (lambdaSignature != nullptr) {
        convertClosureTypesInLambdaSignature();
    }
}

void MethodDefinition::convertClosureTypesInArgumentList() {
    for (auto argument: argumentList) {
        auto closureInterfaceType =
            Tree::convertToClosureInterface(argument->getType());
        if (closureInterfaceType != nullptr) {
            argument->setType(closureInterfaceType);
        }
    }
}

void MethodDefinition::convertClosureTypesInLambdaSignature() {
    assert(lambdaSignature != nullptr);

    auto closureInterfaceType =
        Tree::convertToClosureInterface(lambdaSignature->getReturnType());
    if (closureInterfaceType != nullptr) {
        lambdaSignature->setReturnType(closureInterfaceType);
    }

    for (auto& argumentType: lambdaSignature->getArguments()) {
        auto argumentClosureInterfaceType =
            Tree::convertToClosureInterface(argumentType);
        if (argumentClosureInterfaceType != nullptr) {
            argumentType = argumentClosureInterfaceType;
        }
    }
}

void MethodDefinition::makeArgumentNamesUnique() {
    assert(body != nullptr);

    NameBindings& nameBindings = body->getNameBindings();
    for (auto argument: argumentList) {
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

    auto classDefinition = getEnclosingClass();
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

    auto constructorCall =
        MethodCallExpression::create(baseClassName, getLocation());
    body->insertStatementAtFront(
        ConstructorCallStatement::create(constructorCall));
}

void MethodDefinition::generateMemberInitializationsFromConstructorArgumets(
    const ArgumentList& constructorArguments) {

    const Location& location = getLocation();

    for (auto varDeclaration: constructorArguments) {
        auto varDeclarationType = varDeclaration->getType();
        const Identifier& varDeclarationName = varDeclaration->getIdentifier();

        if (varDeclaration->isDataMember()) {
            // The variable is a data member. Initialize it.
            Identifier argumentName(varDeclarationName + "_Arg");
            addArgument(varDeclarationType, argumentName);

            auto left = NamedEntityExpression::create(varDeclarationName,
                                                      location);
            auto right = NamedEntityExpression::create(argumentName,
                                                       location);

            // Add member initialization to constructor.
            auto init = BinaryExpression::create(Operator::Assignment,
                                                 left,
                                                 right,
                                                 location);
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

    for (auto dataMember: dataMembers) {
        if (dataMember->isStatic()) {
            continue;
        }

        auto dataMemberType = dataMember->getType();
        auto left = DataMemberExpression::create(dataMember, location);
        auto right = dataMember->getExpression();
        if (right == nullptr) {
            // Add argument to constructor.
            Identifier argumentName(dataMember->getName() + "_Arg");
            addArgument(dataMemberType, argumentName);

            right = NamedEntityExpression::create(argumentName, location);
        } else {
            right = right->clone();
        }

        // Add member initialization to constructor.
        auto init = BinaryExpression::create(Operator::Assignment,
                                             left,
                                             right,
                                             location);
        body->insertStatementAfterFront(init);
    }
}

void MethodDefinition::generateMemberDefaultInitializations(
    const DataMemberList& dataMembers) {

    auto constructorCall = body->getFirstStatementAsConstructorCall();
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

        auto left = DataMemberExpression::create(dataMember, location);
        auto right = dataMember->getExpression();
        if (right == nullptr) {
            right = Expression::generateDefaultInitialization(
                dataMember->getType(),
                location);
        } else {
            right = right->clone();
        }

        if (right != nullptr) {
            auto init = BinaryExpression::create(Operator::Assignment,
                                                 left,
                                                 right,
                                                 location);
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
        auto typeInCall = *i;
        auto typeInSignature = (*j)->getType();
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
        auto type = (*i)->getType();
        auto typeInSignature = (*j)->getType();
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
    auto lambdaArgument =
        VariableDeclaration::create(Type::create(Type::Lambda),
                                    "",
                                    getLocation());
    addArgument(lambdaArgument);

    assert(enclosingDefinition->isClass());
    const NameBindings& nameBindings =
        enclosingDefinition->cast<ClassDefinition>()->getNameBindings();

    Tree::lookupAndSetTypeDefinition(lambdaSignature->getReturnType(),
                                     nameBindings,
                                     loc);
    for (auto argumentType: lambdaSignature->getArguments()) {
        Tree::lookupAndSetTypeDefinition(argumentType, nameBindings, loc);
    }
}

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

DataMemberDefinition* DataMemberDefinition::create(
    const Identifier& name,
    Type* typ) {

    return create(name, typ, AccessLevel::Public, false, false, Location());
}

DataMemberDefinition* DataMemberDefinition::create(
    const Identifier& name,
    Type* typ,
    AccessLevel::Kind access,
    bool isStatic,
    bool isPrimaryCtorArg,
    const Location& l) {

    return new DataMemberDefinition(name,
                                    typ,
                                    access,
                                    isStatic,
                                    isPrimaryCtorArg,
                                    l);
}

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
    auto classDef = enclosingDefinition->cast<ClassDefinition>();

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
    auto concreteType = Tree::makeGenericTypeConcrete(type, nameBindings, loc);
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
    auto closureInterfaceType = Tree::convertToClosureInterface(type);
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

GenericTypeParameterDefinition* GenericTypeParameterDefinition::create(
    const Identifier& name,
    const Location& l) {

    return new GenericTypeParameterDefinition(name, l);
}

GenericTypeParameterDefinition* GenericTypeParameterDefinition::clone() const {
    return new GenericTypeParameterDefinition(*this);
}

ForwardDeclarationDefinition::ForwardDeclarationDefinition(
    const Identifier& name) :
    Definition(Definition::ForwardDeclaration, name, Location()) {}

ForwardDeclarationDefinition::ForwardDeclarationDefinition(
    const ForwardDeclarationDefinition& other) :
    Definition(other) {}

ForwardDeclarationDefinition* ForwardDeclarationDefinition::create(
    const Identifier& name) {

    return new ForwardDeclarationDefinition(name);
}

ForwardDeclarationDefinition* ForwardDeclarationDefinition::clone() const {
    return new ForwardDeclarationDefinition(*this);
}
