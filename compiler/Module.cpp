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
    if (filename.find("stdlib") == std::string::npos) {
        // For non-stdlib modules, import the default modules.
        parser.importDefaultModules();
    }

    // Parse the code and produce the abstract syntax tree (AST).
    parser.parse();

    // Process the AST. This includes doing various transformations and type
    // checks.
    tree.process();

    if (!native) {
        // Generate C++ code from the transformed AST.
        backEnd.generate(dependencies);
    }
}
