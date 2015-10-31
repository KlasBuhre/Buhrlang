#ifndef File_h
#define File_h

#include <string>

class File {
public:
    File(const std::string& fname, const std::string& fmode);
    ~File();

    bool open();
    size_t getSize();
    size_t read(char* buf, size_t length);
    size_t write(const char* buf, size_t length); 

    static void writeToFile(
        const std::string& text,
        const std::string& fileName);
    static bool exists(const std::string& fname);
    static const std::string& getSelfPath();
    static bool isStdlib(const std::string& fname);
    static std::string getFilename(const std::string& fullPath);

private:
    std::string name;
    std::string mode;
    FILE*       file;
};

namespace FileCache {
    const std::string& getFile(const std::string& fname);
    std::string getLine(const std::string& fname, int lineNumber);
}

#endif
