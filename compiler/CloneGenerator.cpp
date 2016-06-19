#include "CloneGenerator.h"
#include "Expression.h"

#include <stdio.h>

namespace {
    const Identifier elementVariableName("element");

    MethodDefinition* getCloneMethod(const ClassDefinition *classDef) {
        for (auto method: classDef->getMethods()) {
            if (method->getName().compare(CommonNames::cloneMethodName) == 0) {
                return method;
            }
        }
        return nullptr;
    }

    void checkNonPrimitiveMember(const DataMemberDefinition* dataMember) {
        if (!dataMember->getType()->isMessageOrPrimitive()) {
            Trace::error("Non-primitive members in a message class must be of "
                         "type message class.",
                         dataMember);
        }
    }
}

CloneGenerator::CloneGenerator(ClassDefinition *classDef, Tree &t) :
    inputClass(classDef),
    tree(t) {}

void CloneGenerator::generate() {
    tree.reopenClass(inputClass);

    generateCopyConstructor();
    generateCloneMethod();

    tree.finishClass();
}

// Generate the following method:
//
// object _clone() {}
//
void CloneGenerator::generateEmptyCloneMethod(ClassDefinition *classDef) {
    MethodDefinition* cloneMethod =
        MethodDefinition::create(CommonNames::cloneMethodName,
                                 new Type(Type::Object),
                                 false,
                                 classDef);
    classDef->appendMember(cloneMethod);
}

// Generate the following method:
//
// object _clone() {
//     return new ClassName(this)
// }
//
void CloneGenerator::generateCloneMethod() {    
    // An empty clone method was created for the message class when the class
    // was created. We will now generate the method body.
    MethodDefinition* cloneMethod = getCloneMethod(inputClass);
    tree.setCurrentBlock(cloneMethod->getBody());

    MethodCallExpression* constructorCall =
        new MethodCallExpression(inputClass->getName());
    constructorCall->addArgument(new ThisExpression());
    tree.addStatement(
        new ReturnStatement(new HeapAllocationExpression(constructorCall)));

    tree.finishBlock();
}

// Generate the following copy constructor:
//
// init(ClassName other): BaseClassName(other) {
//
//     // If a member is of primitive type:
//     member = other.member
//
//     // If a member is of reference type:
//     complexMember = (MemberType) other.complexMember._clone
//
//     // If a member is of enum type:
//     enumMember = MemberType._deepCopy(other.enumMember)
//
//     // If a member is an array:
//     memberArray = new MemberArrayType[other.memberArray.size]
//
//     // If array elements are of primitive type:
//     memberArray.appendAll(other.memberArray)
//
//     // Or, if array elements are of reference type or enum type:
//     other.memberArray.each |element| {
//
//         // If array elements are of reference type.
//         memberArray.append((ArrayType) element._clone)
//
//         // If array elements are of enum type.
//         memberArray.append(MemberType._deepCopy(element))
//     }
// }
//
void CloneGenerator::generateCopyConstructor() {
    // An empty copy constructor was created for the message class when the
    // class was created. We will now generate the method body.
    auto copyConstructor = inputClass->getCopyConstructor();
    tree.setCurrentBlock(copyConstructor->getBody());

    generateBaseClassConstructorCall();

    for (auto dataMember: inputClass->getDataMembers()) {
        if (dataMember->isStatic()) {
            continue;
        }

        const Type* dataMemberType = dataMember->getType();
        if (dataMemberType->isArray()) {
            generateArrayMemberInit(dataMember);
        } else if (dataMemberType->isPrimitive()) {
            generatePrimitiveMemberInit(dataMember);
        } else if (dataMemberType->isEnumeration()) {
            generateEnumMemberInit(dataMember);
        } else {
            generateReferenceMemberInit(dataMember);
        }
    }

    tree.finishBlock();
}

// Generate the following expression:
//
// BaseClassName(other)
//
void CloneGenerator::generateBaseClassConstructorCall() {
    ClassDefinition* baseClass = inputClass->getBaseClass();
    if (baseClass == nullptr ||
        baseClass->getName().compare(Keyword::objectString) == 0) {
        // We don't need to call the copy constructor on the 'object' class
        // since the reference count in the new instance should be zero.
        return;
    }

    if (!baseClass->isMessage()) {
        Trace::error("The base class of a message class must also be a message"
                     " class.",
                     inputClass);
    }

    MethodCallExpression* constructorCall =
        new MethodCallExpression(baseClass->getName());
    constructorCall->addArgument(CommonNames::otherVariableName);
    tree.addStatement(new ConstructorCallStatement(constructorCall));
}

// Generate the following expression:
//
// member = other.member
//
void CloneGenerator::generatePrimitiveMemberInit(
    const DataMemberDefinition* dataMember) {

    const Identifier& memberName = dataMember->getName();
    MemberSelectorExpression* otherMember =
        new MemberSelectorExpression(CommonNames::otherVariableName,
                                     memberName);
    BinaryExpression* initExpression =
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(memberName),
                             otherMember);
    tree.addStatement(initExpression);
}

// Generate the following expression:
//
// enumMember = MemberType._deepCopy(other.enumMember)
//
void CloneGenerator::generateEnumMemberInit(
    const DataMemberDefinition* dataMember) {

    checkNonPrimitiveMember(dataMember);

    const Identifier& memberName = dataMember->getName();
    MethodCallExpression* deepCopyCall =
        new MethodCallExpression(CommonNames::deepCopyMethodName);
    deepCopyCall->addArgument(
        new MemberSelectorExpression(CommonNames::otherVariableName,
                                     memberName));
    MemberSelectorExpression* rhs =
        new MemberSelectorExpression(
            dataMember->getType()->getFullConstructedName(),
            deepCopyCall);
    BinaryExpression* initExpression =
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(memberName),
                             rhs);
    tree.addStatement(initExpression);
}

// Generate the following code:
//
// memberArray = new MemberArrayType[other.memberArray.size]
//
// // If array elements are of primitive type:
// memberArray.appendAll(other.memberArray)
//
// // Or, if array elements are of reference type or enum type:
// other.memberArray.each |element| {
//
//     // If array elements are of reference type.
//     memberArray.append((ArrayType) element._clone)
//
//     // If array elements are of enum type.
//     memberArray.append(MemberType._deepCopy(element))
// }
//
void CloneGenerator::generateArrayMemberInit(
    const DataMemberDefinition* dataMember) {

    const Identifier& memberName = dataMember->getName();
    Type* dataMemberType = dataMember->getType();

    // Generate:
    // memberArray = new MemberArrayType[other.memberArray.size]
    ArrayAllocationExpression* allocation =
        new ArrayAllocationExpression(
           dataMemberType->clone(),
           new MemberSelectorExpression(
               new NamedEntityExpression(CommonNames::otherVariableName),
               new MemberSelectorExpression(
                   memberName,
                   BuiltInTypes::arraySizeMethodName)));
    BinaryExpression* memberArrayInit =
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(memberName),
                             allocation);
    tree.addStatement(memberArrayInit);

    Type* arrayElementType = Type::createArrayElementType(dataMemberType);
    if (arrayElementType->isPrimitive()) {
        // Generate:
        // memberArray.appendAll(other.memberArray)
        generateArrayAppendAllCall(memberName);
    } else {
        // Generate:
        // other.memberArray.each |element| {
        //     memberArray.append((ArrayType) element._clone)
        // }
        checkNonPrimitiveMember(dataMember);
        generateArrayForEachLoop(memberName, arrayElementType);
    }
}

// Generate the following code:
//
// memberArray.appendAll(other.memberArray)
//
void CloneGenerator::generateArrayAppendAllCall(const Identifier& memberName) {
    MethodCallExpression* appendAllCall =
        new MethodCallExpression(BuiltInTypes::arrayAppendAllMethodName);
    appendAllCall->addArgument(
        new MemberSelectorExpression(CommonNames::otherVariableName,
                                     memberName));
    tree.addStatement(new MemberSelectorExpression(memberName, appendAllCall));
}

// Generate the following code:
//
// other.memberArray.each |element| {
//
//     // If array elements are of reference type.
//     memberArray.append((ArrayType) element._clone)
//
//     // If array elements are of enum type.
//     memberArray.append(MemberType._deepCopy(element))
// }
//
void CloneGenerator::generateArrayForEachLoop(
    const Identifier& memberName,
    Type* arrayElementType) {

    // Generate:
    // other.memberArray.each |element| {
    MethodCallExpression* eachCall =
        new MethodCallExpression(BuiltInTypes::arrayEachMethodName);
    BlockStatement* lambdaBody = tree.startBlock();
    LambdaExpression* lambda = new LambdaExpression(lambdaBody);
    VariableDeclarationStatement* lambdaArgument =
        new VariableDeclarationStatement(new Type(Type::Implicit),
                                         elementVariableName,
                                         nullptr,
                                         Location());
    lambda->addArgument(lambdaArgument);

    MethodCallExpression* appendCall =
        new MethodCallExpression(BuiltInTypes::arrayAppendMethodName);
    if (arrayElementType->isEnumeration()) {
        // Generate:
        // memberArray.append(MemberType._deepCopy(element))
        MethodCallExpression* deepCopyCall =
            new MethodCallExpression(CommonNames::deepCopyMethodName);
        deepCopyCall->addArgument(elementVariableName);
        appendCall->addArgument(
            new MemberSelectorExpression(
                arrayElementType->getFullConstructedName(),
                deepCopyCall));
    } else {
        // Generate:
        // memberArray.append((ArrayType) element._clone)
        TypeCastExpression* cloneCall =
            new TypeCastExpression(arrayElementType,
                                   new MemberSelectorExpression(
                                       elementVariableName,
                                       CommonNames::cloneMethodName));
        appendCall->addArgument(cloneCall);
    }
    tree.addStatement(new MemberSelectorExpression(memberName, appendCall));

    tree.finishBlock();
    eachCall->setLambda(lambda);

    MemberSelectorExpression* eachCallMemberSelector =
        new MemberSelectorExpression(
            new NamedEntityExpression(CommonNames::otherVariableName),
                                      new MemberSelectorExpression(
                                          new NamedEntityExpression(memberName),
                                          eachCall));
    tree.addStatement(eachCallMemberSelector);
}

// Generate the following expression:
//
// member = (MemberType) other.member._clone
//
void CloneGenerator::generateReferenceMemberInit(
    const DataMemberDefinition* dataMember) {

    checkNonPrimitiveMember(dataMember);

    const Identifier& memberName = dataMember->getName();
    MemberSelectorExpression* clonedMember =
        new MemberSelectorExpression(
            new NamedEntityExpression(CommonNames::otherVariableName),
            new MemberSelectorExpression(memberName,
                                         CommonNames::cloneMethodName));
    TypeCastExpression* rhs =
        new TypeCastExpression(dataMember->getType()->clone(), clonedMember);
    BinaryExpression* initExpression =
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(memberName),
                             rhs);
    tree.addStatement(initExpression);
}
