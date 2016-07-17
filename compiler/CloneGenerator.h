#ifndef CloneGenerator_h
#define CloneGenerator_h

#include "Definition.h"
#include "Tree.h"

namespace CloneGenerator {
    void generate(ClassDefinition* classDef, Tree& tree);
    void generateEmptyCloneMethod(ClassDefinition *classDef);
}

#endif
