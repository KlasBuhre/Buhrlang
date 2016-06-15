#ifndef EnumGenerator_h
#define EnumGenerator_h

#include "Definition.h"

class Tree;
class BlockStatement;
class MatchCase;

class EnumGenerator {
public:
    EnumGenerator(
        const Identifier& name,
        bool isMessage,
        const GenericTypeParameterList& genericTypeParameters,
        const Location& location,
        Tree& t);
    EnumGenerator(ClassDefinition* e, Tree& t);

    void generateVariant(
        const Identifier& variantName,
        const ArgumentList& variantData,
        const Location& location);
    void generateEmptyDeepCopyMethod();
    void generateDeepCopyMethod();
    ClassDefinition* getConvertableEnum();
    ClassDefinition* getEnum();

private:
    struct GenericNoDataVariant {
        Identifier name;
        int tag;
        Location location;
    };

    using GenericNoDataVariantList = std::vector<GenericNoDataVariant>;

    EnumGenerator(
        Type* type,
        bool isMessage,
        const Location& location,
        Tree& t);
    void startClassGeneration(
        const Identifier& name,
        bool isMessage,
        const GenericTypeParameterList& genericTypeParameters,
        const Location& location);
    void generateVariantStaticTag(
        const Identifier& variantName,
        int tagValue,
        const Location& location);
    void generateVariantData(
        const Identifier& variantName,
        const ArgumentList& variantData);
    void generateVariantConstructor(
        const Identifier& variantName,
        const ArgumentList& variantData,
        const Location& location);
    void generateInitializations(
        const Identifier& variantDataName,
        const ArgumentList& variantData,
        const Location& location);
    MethodDefinition* generateVariantConstructorSignature(
        const Identifier& variantName,
        const ArgumentList& variantData,
        BlockStatement* body,
        const Location& location);
    MethodDefinition* generateDeepCopyMethodSignature(
        BlockStatement* body,
        const Location& location);
    MatchCase* generateVariantMatchCase(MethodDefinition* variantConstructor);
    ClassDefinition* generateConvertableEnum();
    void generateImplicitConversion();

    Type* fullEnumType;
    Tree& tree;
    int numberOfVariants;
    GenericNoDataVariantList genericNoDataVariantList;
    ClassDefinition* enumClass;
};

#endif
