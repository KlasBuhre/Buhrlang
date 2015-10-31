
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "Module.h"
#include "File.h"

typedef std::vector<Module*> ModuleList;
ModuleList modules;

void compile() {
    for (ModuleList::iterator i = modules.begin(); i != modules.end(); i++) {
        Module* module = *i;
        printf("Compiling %s.b\n", module->getFilename().c_str());
        module->compile();
    }
}

void writeGeneratedCppCodeToDisk() {
    for (ModuleList::iterator i = modules.begin(); i != modules.end(); i++) {
        const Module* module = *i;
        if (module->isNative()) {
            continue;
        }
        const std::string& filename = module->getFilename();
        File::writeToFile(module->getHeaderOutput(), filename + ".h");
        File::writeToFile(module->getImplementationOutput(), filename + ".cpp");
    }
}

void removeGeneratedCppCode() {
    for (ModuleList::iterator i = modules.begin(); i != modules.end(); i++) {
        const Module* module = *i;
        const std::string& filename = module->getFilename();
        std::string cmd = "rm " + filename + ".o ";
        if (!module->isNative()) {
            cmd += filename + ".cpp "  + filename + ".h";
        }
        system(cmd.c_str());
    }
}

void callGcc(const std::string& executableName) {
    std::string objectFiles;
    for (ModuleList::iterator i = modules.begin(); i != modules.end(); i++) {
        const Module* module = *i;
        const std::string& filename = module->getFilename();
        const std::string& compilerPath = File::getSelfPath();

        // Compile C++ file into object file.
        std::string cmd = "g++";
        if (filename.find("stdlib/CStandardIo") == std::string::npos) {
            // All modules except CStandardIo can be compiled with C++11 because
            // of fdopen().
            cmd += " -std=c++11";
        }
        cmd += " -g -c " + filename + ".cpp -o " + filename +
               ".o -I . -I " + compilerPath + "stdlib/ -I " + compilerPath +
               "runtime/ -pthread";

        system(cmd.c_str());
        objectFiles += filename + ".o ";
    }

    // Link object files to create executable.
    std::string cmd = "g++ -o " + executableName + " " + objectFiles +
                      " -pthread";
    system(cmd.c_str());
}

int main(int argc, char** argv) {
    std::string executableName;
    int c;
    opterr = 0;

    while ((c = getopt(argc, argv, "o:")) != -1) {
        switch (c) {
            case 'o':
                executableName = optarg;
                break;
            case '?':
                if (optopt == 'o') {
                    printf("Option -%c requires an argument.\n", optopt);
                } else if (isprint(optopt)) {
                    printf("Unknown option `-%c'.\n", optopt);
                } else {
                    printf("Unknown option character `\\x%x'.\n", optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    for (int index = optind; index < argc; index++) {
        std::string filename(argv[index]);
        filename.resize(filename.size() - 2);
        modules.push_back(new Module(filename));
    }

    compile();
    writeGeneratedCppCodeToDisk();
    callGcc(executableName);
    removeGeneratedCppCode();

    return 0;
}
