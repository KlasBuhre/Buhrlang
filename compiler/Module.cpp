#include "Module.h"
#include "Parser.h"
#include "File.h"

Module::Module(const std::string& fname) :
    filename(fname),
    dependencies(),
    tree(),
    backEnd(tree, filename),
    native(false) {}

void Module::addDependency(const std::string& fname) {
    std::string dependency(fname);
    dependency.resize(dependency.size() - 2);
    dependencies.push_back(dependency);
}

void Module::compile() {
    tree.setCurrentTree();

    Parser parser(filename + ".b", tree, this);    
    if (!File::isStdlib(filename)) {
        // For non-stdlib modules, import the default modules.
        parser.importDefaultModules();
    }

    // Parse the code and produce the abstract syntax tree (AST).
    parser.parse();

    // Check that methods have a terminating return statement.
    tree.checkReturnStatements();

    // Generate concrete classes from generic classes and make generic types in
    // method signatures and data members concrete.
    tree.makeGenericTypesConcreteInSignatures();

    // Convert closure types in method signatures and data members into
    // references to closure interfaces.
    tree.convertClosureTypesInSigntures();

    // Generate clone methods for message classes.
    tree.generateCloneMethods();

    // Do type checks and transform statements into a more low-level form.
    tree.typeCheckAndTransform();

    if (!native) {
        // Generate C++ code from the transformed AST.
        backEnd.generate(dependencies);
    }
}
