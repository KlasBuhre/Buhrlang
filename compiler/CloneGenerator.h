#ifndef CloneGenerator_h
#define CloneGenerator_h

#include "Definition.h"
#include "Tree.h"

class CloneGenerator {
public:
    CloneGenerator(ClassDefinition* classDef, Tree& t);

    void generate();

    static void generateEmptyCloneMethod(ClassDefinition *classDef);

private:
    void generateCloneMethod();
    void generateCopyConstructor();
    void generateBaseClassConstructorCall();
    void generatePrimitiveMemberInit(const DataMemberDefinition* dataMember);
    void generateEnumMemberInit(const DataMemberDefinition* dataMember);
    void generateArrayMemberInit(const DataMemberDefinition* dataMember);
    void generateArrayAppendAllCall(const Identifier& memberName);
    void generateArrayForEachLoop(
        const Identifier& memberName,
        Type* arrayElementType);
    void generateReferenceMemberInit(const DataMemberDefinition* dataMember);

    ClassDefinition* inputClass;
    Tree& tree;
};

#endif
