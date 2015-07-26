#include "EnumGenerator.h"
#include "Tree.h"
#include "Expression.h"
#include "Statement.h"

namespace {
    const Identifier retvalVariableName("retval");
    const Identifier otherVariableName("other");
}

EnumGenerator::EnumGenerator(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    const Location& location,
    Tree& t) :
    fullEnumType(Type::create(name)),
    tree(t),
    numberOfVariants(0),
    genericNoDataVariantList(),
    enumClass(nullptr) {

    for (GenericTypeParameterList::const_iterator i =
             genericTypeParameters.begin();
         i != genericTypeParameters.end();
         i++) {
        GenericTypeParameterDefinition* genericTypeDef = *i;
        fullEnumType->addGenericTypeParameter(
            Type::create(genericTypeDef->getName()));
    }

    startClassGeneration(name, genericTypeParameters, location);
}

EnumGenerator::EnumGenerator(Type* type, const Location& location, Tree& t) :
    fullEnumType(type),
    tree(t),
    numberOfVariants(0),
    genericNoDataVariantList(),
    enumClass(nullptr) {

    startClassGeneration(fullEnumType->getFullConstructedName(),
                         GenericTypeParameterList(),
                         location);
}

void EnumGenerator::startClassGeneration(
    const Identifier& name,
    const GenericTypeParameterList& genericTypeParameters,
    const Location& location) {

    IdentifierList parents;
    ClassDefinition::Properties properties;
    properties.isEnumeration = true;
    properties.isGenerated = true;
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

    for (ArgumentList::const_iterator i = variantData.begin();
         i != variantData.end();
         i++) {
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

    EnumGenerator
        enumGenerator(convertableEnumType, enumClass->getLocation(), tree);

    for (GenericNoDataVariantList::const_iterator i =
             genericNoDataVariantList.begin();
         i != genericNoDataVariantList.end();
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
