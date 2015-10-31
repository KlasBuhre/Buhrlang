#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <memory>
#include <sstream>
#include <unistd.h>
#include <limits.h>

#include "File.h"

namespace {
    typedef std::map<std::string, std::string> FileMap;

    FileMap fileMap;
    std::string selfPath;
}

File::File(const std::string& fname, const std::string& fmode) 
  : name(fname), 
    mode(fmode),
    file(nullptr) {}

File::~File() {
    if (file != nullptr) {
        fclose(file);
    }
}

bool File::open() {
    file = fopen(name.c_str(), mode.c_str());
    if (file == nullptr) {
        return false;
    }
    return true;
}

size_t File::getSize() {
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    return size;
}

size_t File::read(char *buf, size_t length) {
    return fread(buf, 1, length, file);
}

size_t File::write(const char *buf, size_t length) {
    return fwrite(buf, 1, length, file);
}

void File::writeToFile(const std::string& text, const std::string& fileName) {
    File file(fileName, "wb");
    if (!file.open()) {
        fprintf(stderr, "Could not open '%s'.\n", fileName.c_str());
        exit(0);
    }
    size_t textSize = text.size();
    if (file.write(text.data(), textSize) != textSize) {
        fprintf(stderr, "Could not write to file '%s'.\n", fileName.c_str());
        exit(0);
    }
}

bool File::exists(const std::string& fname) {
    File file(fname, "r");
    return file.open();
}

const std::string& File::getSelfPath() {
    if (selfPath.empty()) {
        char buf[PATH_MAX];
        ssize_t length = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (length == -1) {
            fprintf(stderr, "Could not get path of executable.\n");
            exit(0);
        }
        buf[length] = 0;
        std::string path(buf);
        size_t position = path.find_last_of('/') + 1;
        selfPath = path.erase(position);
    }
    return selfPath;
}

bool File::isStdlib(const std::string& fname) {
    return fname.find("stdlib/") != std::string::npos;
}

std::string File::getFilename(const std::string& fullPath) {
    size_t position = fullPath.find_last_of('/');
    if (position == std::string::npos) {
        return fullPath;
    } else {
        return fullPath.substr(position + 1);
    }
}

const std::string& FileCache::getFile(const std::string& fname) {
    FileMap::const_iterator i = fileMap.find(fname);
    if (i != fileMap.end()) {
        return i->second;
    } else {
        File file(fname, "rb");
        if (!file.open()) {
            fprintf(stderr, "Could not open '%s'.\n", fname.c_str());
            exit(0);
        }
        size_t codeSize = file.getSize();
        std::unique_ptr<char> buf(new char[codeSize + 1]);
        if (file.read(buf.get(), codeSize) != codeSize) {
            fprintf(stderr, "Could not read from '%s'.\n", fname.c_str());
            exit(0);
        }
        buf.get()[codeSize] = 0;
        return fileMap.insert(
            make_pair(fname,
                      std::string(buf.get(), codeSize + 1))).first->second;
    }
}

std::string FileCache::getLine(const std::string& fname, int lineNumber) {
    FileMap::const_iterator i = fileMap.find(fname);
    if (i != fileMap.end()) {
        std::stringstream stream(i->second);
        std::string line;
        for (int i = 0; i < lineNumber; i++) {
            std::getline(stream, line, '\n');
        }
        return line;
    }
    return std::string();
}
