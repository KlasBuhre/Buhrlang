#ifndef EnumGenerator_h
#define EnumGenerator_h

#include "Definition.h"

class Tree;
class BlockStatement;

class EnumGenerator {
public:
    EnumGenerator(
        const Identifier& name,
        const GenericTypeParameterList& genericTypeParameters,
        const Location& location,
        Tree& t);

    void generateVariant(
        const Identifier& variantName,
        const ArgumentList& variantData,
        const Location& location);
    ClassDefinition* getConvertableEnum();
    ClassDefinition* getEnum();

private:
    struct GenericNoDataVariant {
        Identifier name;
        int tag;
        Location location;
    };

    typedef std::vector<GenericNoDataVariant> GenericNoDataVariantList;

    EnumGenerator(Type* type, const Location& location, Tree& t);
    void startClassGeneration(
        const Identifier& name,
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
    ClassDefinition* generateConvertableEnum();
    void generateImplicitConversion();

    Type* fullEnumType;
    Tree& tree;
    int numberOfVariants;
    GenericNoDataVariantList genericNoDataVariantList;
    ClassDefinition* enumClass;
};

#endif
