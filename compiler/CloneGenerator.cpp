#include "CloneGenerator.h"

// #include <stdio.h>

#include "Expression.h"

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

    // Generate the following method:
    //
    // object _clone() {
    //     return new ClassName(this)
    // }
    //
    void generateCloneMethod(ClassDefinition* inputClass, Tree& tree) {
        // An empty clone method was created for the message class when the
        // class was created. We will now generate the method body.
        auto cloneMethod = getCloneMethod(inputClass);
        tree.setCurrentBlock(cloneMethod->getBody());

        auto constructorCall =
            MethodCallExpression::create(inputClass->getName());
        constructorCall->addArgument(ThisExpression::create());
        tree.addStatement(
            ReturnStatement::create(
                HeapAllocationExpression::create(constructorCall)));

        tree.finishBlock();
    }

    // Generate the following expression:
    //
    // BaseClassName(other)
    //
    void generateBaseClassConstructorCall(
        ClassDefinition* inputClass,
        Tree& tree) {

        auto baseClass = inputClass->getBaseClass();
        if (baseClass == nullptr ||
            baseClass->getName().compare(Keyword::objectString) == 0) {
            // We don't need to call the copy constructor on the 'object' class
            // since the reference count in the new instance should be zero.
            return;
        }

        if (!baseClass->isMessage()) {
            Trace::error("The base class of a message class must also be a "
                         "message class.",
                         inputClass);
        }

        auto constructorCall =
            MethodCallExpression::create(baseClass->getName());
        constructorCall->addArgument(CommonNames::otherVariableName);
        tree.addStatement(ConstructorCallStatement::create(constructorCall));
    }

    // Generate the following expression:
    //
    // member = other.member
    //
    void generatePrimitiveMemberInit(
        const DataMemberDefinition* dataMember,
        Tree& tree) {

        const Identifier& memberName = dataMember->getName();
        auto otherMember =
            MemberSelectorExpression::create(CommonNames::otherVariableName,
                                             memberName);
        auto initExpression =
            BinaryExpression::create(Operator::Assignment,
                                     NamedEntityExpression::create(memberName),
                                     otherMember);
        tree.addStatement(initExpression);
    }

    // Generate the following expression:
    //
    // enumMember = MemberType._deepCopy(other.enumMember)
    //
    void generateEnumMemberInit(
        const DataMemberDefinition* dataMember,
        Tree& tree) {

        checkNonPrimitiveMember(dataMember);

        const Identifier& memberName = dataMember->getName();
        auto deepCopyCall =
            MethodCallExpression::create(CommonNames::deepCopyMethodName);
        deepCopyCall->addArgument(
            MemberSelectorExpression::create(CommonNames::otherVariableName,
                                             memberName));
        auto rhs =
            MemberSelectorExpression::create(
                dataMember->getType()->getFullConstructedName(),
                deepCopyCall);
        auto initExpression =
            BinaryExpression::create(Operator::Assignment,
                                     NamedEntityExpression::create(memberName),
                                     rhs);
        tree.addStatement(initExpression);
    }

    // Generate the following code:
    //
    // memberArray.appendAll(other.memberArray)
    //
    void generateArrayAppendAllCall(const Identifier& memberName, Tree& tree) {
        auto appendAllCall =
            MethodCallExpression::create(BuiltInTypes::arrayAppendAllMethodName);
        appendAllCall->addArgument(
            MemberSelectorExpression::create(CommonNames::otherVariableName,
                                             memberName));
        tree.addStatement(
            MemberSelectorExpression::create(memberName, appendAllCall));
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
    void generateArrayForEachLoop(
        const Identifier& memberName,
        Type* arrayElementType,
        Tree& tree) {

        // Generate:
        // other.memberArray.each |element| {
        auto eachCall =
            MethodCallExpression::create(BuiltInTypes::arrayEachMethodName);
        auto lambdaBody = tree.startBlock();
        auto lambda = LambdaExpression::create(lambdaBody);
        auto lambdaArgument =
            VariableDeclarationStatement::create(Type::create(Type::Implicit),
                                                 elementVariableName,
                                                 nullptr,
                                                 Location());
        lambda->addArgument(lambdaArgument);

        auto appendCall =
            MethodCallExpression::create(BuiltInTypes::arrayAppendMethodName);
        if (arrayElementType->isEnumeration()) {
            // Generate:
            // memberArray.append(MemberType._deepCopy(element))
            auto deepCopyCall =
                MethodCallExpression::create(CommonNames::deepCopyMethodName);
            deepCopyCall->addArgument(elementVariableName);
            appendCall->addArgument(
                MemberSelectorExpression::create(
                    arrayElementType->getFullConstructedName(),
                    deepCopyCall));
        } else {
            // Generate:
            // memberArray.append((ArrayType) element._clone)
            auto cloneCall =
                TypeCastExpression::create(arrayElementType,
                                           MemberSelectorExpression::create(
                                               elementVariableName,
                                               CommonNames::cloneMethodName));
            appendCall->addArgument(cloneCall);
        }
        tree.addStatement(MemberSelectorExpression::create(memberName,
                                                           appendCall));

        tree.finishBlock();
        eachCall->setLambda(lambda);

        auto eachCallMemberSelector =
            MemberSelectorExpression::create(
                NamedEntityExpression::create(CommonNames::otherVariableName),
                MemberSelectorExpression::create(
                    NamedEntityExpression::create(memberName),
                    eachCall));
        tree.addStatement(eachCallMemberSelector);
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
    void generateArrayMemberInit(
        const DataMemberDefinition* dataMember,
        Tree& tree) {

        const Identifier& memberName = dataMember->getName();
        auto dataMemberType = dataMember->getType();

        // Generate:
        // memberArray = new MemberArrayType[other.memberArray.size]
        auto allocation =
            ArrayAllocationExpression::create(
               dataMemberType->clone(),
               MemberSelectorExpression::create(
                   NamedEntityExpression::create(CommonNames::otherVariableName),
                   MemberSelectorExpression::create(
                       memberName,
                       BuiltInTypes::arraySizeMethodName)));
        auto memberArrayInit =
            BinaryExpression::create(Operator::Assignment,
                                     NamedEntityExpression::create(memberName),
                                     allocation);
        tree.addStatement(memberArrayInit);

        auto arrayElementType = Type::createArrayElementType(dataMemberType);
        if (arrayElementType->isPrimitive()) {
            // Generate:
            // memberArray.appendAll(other.memberArray)
            generateArrayAppendAllCall(memberName, tree);
        } else {
            // Generate:
            // other.memberArray.each |element| {
            //     memberArray.append((ArrayType) element._clone)
            // }
            checkNonPrimitiveMember(dataMember);
            generateArrayForEachLoop(memberName, arrayElementType, tree);
        }
    }

    // Generate the following expression:
    //
    // member = (MemberType) other.member._clone
    //
    void generateReferenceMemberInit(
        const DataMemberDefinition* dataMember,
        Tree& tree) {

        checkNonPrimitiveMember(dataMember);

        const Identifier& memberName = dataMember->getName();
        auto clonedMember =
            MemberSelectorExpression::create(
                NamedEntityExpression::create(CommonNames::otherVariableName),
                MemberSelectorExpression::create(memberName,
                                                 CommonNames::cloneMethodName));
        auto rhs = TypeCastExpression::create(dataMember->getType()->clone(),
                                              clonedMember);
        auto initExpression =
            BinaryExpression::create(Operator::Assignment,
                                     NamedEntityExpression::create(memberName),
                                     rhs);
        tree.addStatement(initExpression);
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
    void generateCopyConstructor(ClassDefinition* inputClass, Tree& tree) {
        // An empty copy constructor was created for the message class when the
        // class was created. We will now generate the method body.
        auto copyConstructor = inputClass->getCopyConstructor();
        tree.setCurrentBlock(copyConstructor->getBody());

        generateBaseClassConstructorCall(inputClass, tree);

        for (auto dataMember: inputClass->getDataMembers()) {
            if (dataMember->isStatic()) {
                continue;
            }

            const auto dataMemberType = dataMember->getType();
            if (dataMemberType->isArray()) {
                generateArrayMemberInit(dataMember, tree);
            } else if (dataMemberType->isPrimitive()) {
                generatePrimitiveMemberInit(dataMember, tree);
            } else if (dataMemberType->isEnumeration()) {
                generateEnumMemberInit(dataMember, tree);
            } else {
                generateReferenceMemberInit(dataMember, tree);
            }
        }

        tree.finishBlock();
    }
}

void CloneGenerator::generate(ClassDefinition* inputClass, Tree& tree) {
    tree.reopenClass(inputClass);

    generateCopyConstructor(inputClass, tree);
    generateCloneMethod(inputClass, tree);

    tree.finishClass();
}

// Generate the following method:
//
// object _clone() {}
//
void CloneGenerator::generateEmptyCloneMethod(ClassDefinition *classDef) {
    auto cloneMethod =
        MethodDefinition::create(CommonNames::cloneMethodName,
                                 Type::create(Type::Object),
                                 false,
                                 classDef);
    classDef->appendMember(cloneMethod);
}
