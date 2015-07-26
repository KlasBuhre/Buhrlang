#ifndef Module_h
#define Module_h

#include <string>
#include <vector>

#include "Tree.h"
#include "BackEnd.h"

class Module {
public:
    Module(const std::string& fname);

    void compile();
    void addDependency(const std::string& fname);

    void setIsNative(bool n) {
        native = n;
    }

    bool isNative() const {
        return native;
    }

    const std::string& getFilename() const {
        return filename;
    }

    const std::string& getHeaderOutput() const {
        return backEnd.getHeaderOutput();
    }

    const std::string& getImplementationOutput() const {
        return backEnd.getImplementationOutput();
    }

private:
    std::string filename;
    std::vector<std::string> dependencies;
    Tree tree;
    CppBackEnd backEnd;
    bool native;
};

#endif
