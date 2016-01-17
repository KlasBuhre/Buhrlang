#include "EnumGenerator.h"
#include "Tree.h"
#include "Expression.h"
#include "Statement.h"

namespace {
    const Identifier retvalVariableName("retval");
    const Identifier otherVariableName("other");
    const Identifier otherTagVariableName("otherTag");

    void checkNonPrimitiveVariantDataMember(
        VariableDeclaration* variantDataMember) {

        if (!variantDataMember->getType()->isMessageOrPrimitive()) {
            Trace::error("Non-primitive members in a message enum must be of "
                         "type message.",
                         variantDataMember);
        }
    }

    // Generate the following expression:
    //
    // // If $0 is of primitive type:
    // other.$[Variant0Name].$0
    //
    // // If $0 is of reference type:
    // ([$0Type]) other.$[Variant0Name].$0._clone
    //
    // // If $0 is of enum type:
    // [$0Type]._deepCopy(other.$[Variant0Name].$0)
    //
    Expression* generateVariantDataMemberInitRhs(
        VariableDeclaration* variantDataMember,
        const Identifier& enumVariantDataName) {

        MemberSelectorExpression* variantDataMemberSelector =
            new MemberSelectorExpression(
                new NamedEntityExpression(otherVariableName),
                new MemberSelectorExpression(
                    enumVariantDataName,
                    variantDataMember->getIdentifier()));
        Type* variantDataType = variantDataMember->getType();
        if (variantDataType->isPrimitive()) {
            return variantDataMemberSelector;
        } else {
            checkNonPrimitiveVariantDataMember(variantDataMember);
            if (variantDataType->isReference()) {
                return new TypeCastExpression(
                    variantDataType->clone(),
                    new MemberSelectorExpression(variantDataMemberSelector,
                        new NamedEntityExpression(
                            CommonNames::cloneMethodName)));
            } else {
                MethodCallExpression* deepCopyCall =
                    new MethodCallExpression(CommonNames::deepCopyMethodName);
                deepCopyCall->addArgument(variantDataMemberSelector);
                return new MemberSelectorExpression(
                    variantDataType->getFullConstructedName(),
                    deepCopyCall);
            }
        }
    }

    MethodDefinition* getDeepCopyMethod(const ClassDefinition* enumClass) {
        const MemberMethodList& methods = enumClass->getMethods();
        for (auto i = methods.cbegin(); i != methods.cend(); i++) {
            MethodDefinition* method = *i;
            if (method->getName().compare(
                    CommonNames::deepCopyMethodName) == 0) {
                return method;
            }
        }
        return nullptr;
    }
}

EnumGenerator::EnumGenerator(
    const Identifier& name,
    bool isMessage,
    const GenericTypeParameterList& genericTypeParameters,
    const Location& location,
    Tree& t) :
    fullEnumType(Type::create(name)),
    tree(t),
    numberOfVariants(0),
    genericNoDataVariantList(),
    enumClass(nullptr) {

    for (auto i = genericTypeParameters.cbegin();
         i != genericTypeParameters.cend();
         i++) {
        GenericTypeParameterDefinition* genericTypeDef = *i;
        fullEnumType->addGenericTypeParameter(
            Type::create(genericTypeDef->getName()));
    }

    startClassGeneration(name, isMessage, genericTypeParameters, location);
}

EnumGenerator::EnumGenerator(ClassDefinition* e, Tree& t) :
    fullEnumType(Type::create(e->getName())),
    tree(t),
    numberOfVariants(0),
    genericNoDataVariantList(),
    enumClass(e) {}

EnumGenerator::EnumGenerator(
    Type* type,
    bool isMessage,
    const Location& location,
    Tree& t) :
    fullEnumType(type),
    tree(t),
    numberOfVariants(0),
    genericNoDataVariantList(),
    enumClass(nullptr) {

    startClassGeneration(fullEnumType->getFullConstructedName(),
                         isMessage,
                         GenericTypeParameterList(),
                         location);
}

void EnumGenerator::startClassGeneration(
    const Identifier& name,
    bool isMessage,
    const GenericTypeParameterList& genericTypeParameters,
    const Location& location) {

    IdentifierList parents;
    ClassDefinition::Properties properties;
    properties.isEnumeration = true;
    properties.isGenerated = true;
    properties.isMessage = isMessage;
    tree.startClass(name,
                    genericTypeParameters,
                    parents,
                    properties,
                    location);

    Type* tagVariableType = new Type(Type::Integer);
    tagVariableType->setConstant(false);
    tree.addClassDataMember(tagVariableType, CommonNames::enumTagVariableName);
}

ClassDefinition* EnumGenerator::getConvertableEnum() {
    enumClass = tree.finishClass();

    if (genericNoDataVariantList.empty()) {
        return nullptr;
    }

    return generateConvertableEnum();
}

ClassDefinition* EnumGenerator::getEnum() {
    if (genericNoDataVariantList.empty()) {
        return enumClass;
    }

    enumClass->generateDefaultConstructor();
    tree.reopenClass(enumClass);
    generateImplicitConversion();
    return tree.finishClass();
}

// Generate the following members:
//
// // Variant static tag.
// static int $[VariantName]Tag = [TagValue]
//
// // Variant data class.
// class $[VariantName]Data {
//     [VariantDataMemberType0] $0
//     [VariantDataMemberType1] $1
//     ...
// }
//
// // Variant data members.
// $[VariantName]Data $[VariantName]
//
// // Variant constructor.
// static [EnumName] [VariantName]([VariantDataMemberType0] $0, ...) {
//     [EnumName] retval
//     retval.$tag = $[VariantName]Tag
//     retval.$[VariantName].$0 = $0
//     retval.$[VariantName].$1 = $1
//     ...
//     return retval
// }
//
void EnumGenerator::generateVariant(
    const Identifier& variantName,
    const ArgumentList& variantData,
    const Location& location) {

    generateVariantStaticTag(variantName, numberOfVariants, location);

    if (!variantData.empty()) {
        generateVariantData(variantName, variantData);
    }

    if (fullEnumType->hasGenericTypeParameters() && variantData.empty()) {
        // The enumeration has type parameters (it is a generic) and the variant
        // constructor takes no data. This means that it is not trivial to infer
        // the concrete enumeration type when calling the variant constructor.
        // Therefore, place the variant constructor in a concrete class
        // [EnumName]<_>. The class [EnumName]<_> is implicitly convertable to
        // [EnumName].
        GenericNoDataVariant variant;
        variant.name = variantName;
        variant.tag = numberOfVariants;
        variant.location = location;
        genericNoDataVariantList.push_back(variant);
    }

    // TODO: Should generate empty constructor if the enumeration is a generic
    // and the variant constructor takes no data.
    generateVariantConstructor(variantName, variantData, location);
    numberOfVariants++;
}

// Generate the following data member:
//
// static int $[VariantName]Tag = [TagValue]
//
void EnumGenerator::generateVariantStaticTag(
    const Identifier& variantName,
    int tagValue,
    const Location& location) {

    DataMemberDefinition* staticTag =
        new DataMemberDefinition(Symbol::makeEnumVariantTagName(variantName),
                                 new Type(Type::Integer),
                                 AccessLevel::Public,
                                 true,
                                 false,
                                 location);
    staticTag->setExpression(new IntegerLiteralExpression(tagValue, location));
    tree.addClassMember(staticTag);
}

// Generate the following class and data member:
//
// class $[VariantName]Data {
//     [VariantDataMemberType0] $0
//     [VariantDataMemberType1] $1
//     ...
// }
//
// $[VariantName]Data $[VariantName]
//
void EnumGenerator::generateVariantData(
    const Identifier& variantName,
    const ArgumentList& variantData) {

    Identifier variantClassName(Symbol::makeEnumVariantClassName(variantName));
    ClassDefinition::Properties properties;
    properties.isEnumerationVariant = true;
    tree.startGeneratedClass(variantClassName, properties);
    tree.getCurrentClass()->addPrimaryCtorArgsAsDataMembers(variantData);
    tree.addClassMember(tree.finishClass());

    tree.addClassDataMember(Type::create(variantClassName),
                            Symbol::makeEnumVariantDataName(variantName));
}

// Generate the following enum variant constructor:
//
// static [EnumName] [VariantName]([VariantDataMemberType0] $0, ...) {
//     [EnumName] retval
//     retval.$tag = $[VariantName]Tag
//     retval.$[VariantName].$0 = $0
//     retval.$[VariantName].$1 = $1
//     ...
//     return retval
// }
//
void EnumGenerator::generateVariantConstructor(
    const Identifier& variantName,
    const ArgumentList& variantData,
    const Location& location) {

    MethodDefinition* variantConstructor =
        generateVariantConstructorSignature(variantName,
                                            variantData,
                                            tree.startBlock(),
                                            location);
    tree.addStatement(new VariableDeclarationStatement(fullEnumType->clone(),
                                                       retvalVariableName,
                                                       nullptr,
                                                       location));
    tree.addStatement(
        new BinaryExpression(Operator::Assignment,
                             new MemberSelectorExpression(
                                 retvalVariableName,
                                 CommonNames::enumTagVariableName,
                                 location),
                             new NamedEntityExpression(
                                 Symbol::makeEnumVariantTagName(variantName),
                                 location),
                             location));

    generateInitializations(Symbol::makeEnumVariantDataName(variantName),
                            variantData,
                            location);
    tree.addStatement(
        new ReturnStatement(new NamedEntityExpression(retvalVariableName,
                                                      location),
                            location));
    tree.finishBlock();
    tree.addClassMember(variantConstructor);
}

// Generate the following enum variant data initializations:
//
//     retval.$[VariantName].$0 = $0
//     retval.$[VariantName].$1 = $1
//     ...
//
void EnumGenerator::generateInitializations(
    const Identifier& variantDataName,
    const ArgumentList& variantData,
    const Location& location) {

    for (auto i = variantData.cbegin(); i != variantData.cend(); i++) {
        VariableDeclaration* variantDataMember = *i;
        MemberSelectorExpression* lhs =
            new MemberSelectorExpression(
                new NamedEntityExpression(retvalVariableName, location),
                new MemberSelectorExpression(
                    new NamedEntityExpression(variantDataName, location),
                    new NamedEntityExpression(
                        variantDataMember->getIdentifier(),
                        location),
                    location));
        tree.addStatement(
            new BinaryExpression(Operator::Assignment,
                                 lhs,
                                 new NamedEntityExpression(
                                     variantDataMember->getIdentifier(),
                                     location),
                                 location));
    }
}

// Generate the following enum variant constructor signature:
//
// static [EnumName] [VariantName]([VariantDataMemberType0] $0, ...)
//
MethodDefinition* EnumGenerator::generateVariantConstructorSignature(
    const Identifier& variantName,
    const ArgumentList& variantData,
    BlockStatement* body,
    const Location& location) {

    MethodDefinition* constructorSignature =
        new MethodDefinition(variantName,
                             fullEnumType->clone(),
                             AccessLevel::Public,
                             true,
                             tree.getCurrentClass(),
                             location);
    constructorSignature->setBody(body);
    constructorSignature->addArguments(variantData);
    constructorSignature->setIsEnumConstructor(true);
    return constructorSignature;
}

// Generate the following method:
//
// static [EnumName] _deepCopy([EnumName] other) {
//     [EnumName] retval
// }
//
void EnumGenerator::generateEmptyDeepCopyMethod() {
    const Location& location = tree.getCurrentClass()->getLocation();

    MethodDefinition* deepCopyMethod =
        generateDeepCopyMethodSignature(tree.startBlock(), location);
    tree.addStatement(new VariableDeclarationStatement(fullEnumType->clone(),
                                                       retvalVariableName,
                                                       nullptr,
                                                       location));
    tree.finishBlock();
    tree.addClassMember(deepCopyMethod);
}

// Generate the following method:
//
// static [EnumName] _deepCopy([EnumName] other) {
//     [EnumName] retval
//     int otherTag = other.$tag
//     retval.$tag = otherTag
//     match otherTag {
//          $[Variant0Name]Tag -> {
//              // If $0 is of primitive type:
//              retval.$[Variant0Name].$0 = other.$[Variant0Name].$0
//
//              // If $0 is of reference type:
//              retval.$[Variant0Name].$0 =
//                  ([$0Type]) other.$[Variant0Name].$0._clone
//
//              // If $0 is of enum type:
//              retval.$[Variant0Name].$0 =
//                  [$0Type]._deepCopy(other.$[Variant0Name].$0)
//              ...
//          }
//          $[Variant1Name]Tag ->
//              ...
//          _ -> {}
//     }
//     return retval
// }
//
void EnumGenerator::generateDeepCopyMethod() {
    // An empty deepCopy method was created for the message enum when it was
    // created. We will now generate the method body.
    MethodDefinition* deepCopyMethod = getDeepCopyMethod(enumClass);
    if (deepCopyMethod == nullptr) {
        // No deepCopy method means that this is a convertable enum. Convertable
        // enums don't have a deepCopy method.
        return;
    }

    tree.reopenClass(enumClass);
    tree.setCurrentBlock(deepCopyMethod->getBody());

    const Location& location = tree.getCurrentClass()->getLocation();
    MemberSelectorExpression* otherTagSelector =
        new MemberSelectorExpression(otherVariableName,
                                     CommonNames::enumTagVariableName);
    tree.addStatement(new VariableDeclarationStatement(new Type(Type::Integer),
                                                       otherTagVariableName,
                                                       otherTagSelector,
                                                       location));
    tree.addStatement(
        new BinaryExpression(Operator::Assignment,
                             new MemberSelectorExpression(
                                 retvalVariableName,
                                 CommonNames::enumTagVariableName),
                             new NamedEntityExpression(otherTagVariableName)));

    MatchExpression* match =
        new MatchExpression(new NamedEntityExpression(otherTagVariableName));
    const DefinitionList& members = tree.getCurrentClass()->getMembers();
    for (auto i = members.cbegin(); i != members.cend(); i++) {
        Definition* member = *i;
        if (MethodDefinition* method = member->dynCast<MethodDefinition>()) {
            if (method->isEnumConstructor()) {
                match->addCase(generateVariantMatchCase(method));
            }
        }
    }
    MatchCase* unknownCase = new MatchCase();
    unknownCase->addPatternExpression(new PlaceholderExpression());
    match->addCase(unknownCase);
    tree.addStatement(match);

    tree.addStatement(new ReturnStatement(new NamedEntityExpression(
                                              retvalVariableName)));
    tree.finishBlock();
    tree.finishClass();
}

// Generate the following method signature:
//
// static [EnumName] _deepCopy([EnumName] other)
//
MethodDefinition* EnumGenerator::generateDeepCopyMethodSignature(
    BlockStatement* body,
    const Location& location) {

    MethodDefinition* signature =
        new MethodDefinition(CommonNames::deepCopyMethodName,
                             fullEnumType->clone(),
                             AccessLevel::Public,
                             true,
                             tree.getCurrentClass(),
                             location);
    signature->setBody(body);
    signature->addArgument(fullEnumType->clone(), otherVariableName);
    signature->setIsEnumCopyConstructor(true);
    return signature;
}

// Generate the following match case:
//
// $[Variant0Name]Tag -> {
//     // If $0 is of primitive type:
//     retval.$[Variant0Name].$0 = other.$[Variant0Name].$0
//
//     // If $0 is of reference type:
//     retval.$[Variant0Name].$0 = ([$0Type]) other.$[Variant0Name].$0._clone
//
//     // If $0 is of enum type:
//     retval.$[Variant0Name].$0 = [$0Type]._deepCopy(other.$[Variant0Name].$0)
//     ...
//  }
//
MatchCase* EnumGenerator::generateVariantMatchCase(
    MethodDefinition* variantConstructor) {

    MatchCase* matchCase = new MatchCase();
    const Identifier& variantName = variantConstructor->getName();
    matchCase->addPatternExpression(
        new NamedEntityExpression(Symbol::makeEnumVariantTagName(variantName)));
    matchCase->setResultBlock(tree.startBlock());

    Identifier enumVariantDataName(
        Symbol::makeEnumVariantDataName(variantName));
    const ArgumentList& variantData = variantConstructor->getArgumentList();
    for (auto i = variantData.cbegin(); i != variantData.cend(); i++) {
        VariableDeclaration* variantDataMember = *i;
        MemberSelectorExpression* lhs =
            new MemberSelectorExpression(
                new NamedEntityExpression(retvalVariableName),
                new MemberSelectorExpression(
                    enumVariantDataName,
                    variantDataMember->getIdentifier()));
        Expression* rhs = generateVariantDataMemberInitRhs(variantDataMember,
                                                           enumVariantDataName);
        tree.addStatement(new BinaryExpression(Operator::Assignment, lhs, rhs));
    }

    tree.finishBlock();
    return matchCase;
}

// Generate the following class:
//
// class [EnumName]<_> {
//     int $tag
//
//     // Variant static tags.
//     static int $[VariantName]Tag = [TagValue]
//     ...
//
//     // Variant constructors.
//     static [EnumName]<_> [VariantName]() {
//         [EnumName] retval
//         retval.$tag = $[VariantName]Tag
//         return retval
//     }
//     ..
// }
//
ClassDefinition* EnumGenerator::generateConvertableEnum() {
    Type* convertableEnumType = Type::create(fullEnumType->getName());
    convertableEnumType->addGenericTypeParameter(new Type(Type::Placeholder));

    EnumGenerator enumGenerator(convertableEnumType,
                                enumClass->isMessage(),
                                enumClass->getLocation(),
                                tree);

    for (auto i = genericNoDataVariantList.cbegin();
         i != genericNoDataVariantList.cend();
         i++) {
        const GenericNoDataVariant& variant = *i;
        const Identifier& variantName = variant.name;
        const Location& variantLocation = variant.location;
        enumGenerator.generateVariantStaticTag(variantName,
                                               variant.tag,
                                               variantLocation);
        enumGenerator.generateVariantConstructor(variantName,
                                                 ArgumentList(),
                                                 variantLocation);
    }

    return tree.finishClass();
}

// Generate the following conversion constructor in the [EnumName] class:
//
// init([EnumName]<_> other) {
//     $tag = other.$tag
// }
//
void EnumGenerator::generateImplicitConversion() {
    const Location& location = tree.getCurrentClass()->getLocation();

    MethodDefinition* method =
       new MethodDefinition(Keyword::initString,
                            nullptr,
                            tree.getCurrentClass());
    method->setBody(tree.startBlock());

    Type* convertableEnumType = Type::create(fullEnumType->getName());
    convertableEnumType->addGenericTypeParameter(new Type(Type::Placeholder));
    VariableDeclaration* argument =
        new VariableDeclaration(convertableEnumType,
                                otherVariableName,
                                location);
    method->addArgument(argument);

    tree.addStatement(
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(
                                 CommonNames::enumTagVariableName,
                                 location),
                             new MemberSelectorExpression(
                                 otherVariableName,
                                 CommonNames::enumTagVariableName,
                                 location),
                             location));

    tree.finishBlock();
    tree.addClassMember(method);
}
