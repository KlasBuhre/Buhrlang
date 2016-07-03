#include "CStandardIo.h"

#include <string>
#include <string.h>
#include <unistd.h>

#include "Utils.h"

namespace {
    Pointer<FileHandle> stdOut = NULL;
    Pointer<FileHandle> stdIn = NULL;
}

Pointer<object> FileHandle::_clone() {
    return new FileHandle(*this);
}

Pointer<FileHandle> CStandardIo::fopen(
    Pointer<string> filename,
    Pointer<string> mode) {

    std::string fname(filename->buf->data(), filename->buf->length());
    std::string fmode(mode->buf->data(), mode->buf->length());

    FILE* file = ::fopen(fname.c_str(), fmode.c_str());
    if (file == NULL) {
        throw IoException("CStandardIo.fopen()");
    }

    Pointer<FileHandle> fileHandle(new FileHandle());
    fileHandle->file = file;
    return fileHandle;
}

Pointer<FileHandle> CStandardIo::fdopen(
    int fileDescriptor,
    Pointer<string> mode) {

    std::string fmode(mode->buf->data(), mode->buf->length());

    FILE* file = ::fdopen(fileDescriptor, fmode.c_str());
    if (file == NULL) {
        throw IoException("CStandardIo.fdopen()");
    }

    Pointer<FileHandle> fileHandle(new FileHandle());
    fileHandle->file = file;
    return fileHandle;
}

void CStandardIo::fclose(Pointer<FileHandle> fileHandle) {
    ::fclose(fileHandle->file);
}

void CStandardIo::fflush(Pointer<FileHandle> fileHandle) {
    ::fflush(fileHandle->file);
}

void CStandardIo::fputs(Pointer<string> str, Pointer<FileHandle> fileHandle) {
    std::string data(str->buf->data(), str->buf->length());
    ::fputs(data.c_str(), fileHandle->file);
}

void CStandardIo::fputc(char character, Pointer<FileHandle> fileHandle) {
    ::fputc(character, fileHandle->file);
}

Pointer<string> CStandardIo::fgets(Pointer<FileHandle> fileHandle) {
    const size_t lineBufSize = 2048;
    char buf[lineBufSize];
    if (::fgets(buf, lineBufSize, fileHandle->file) == NULL) {
        if (ferror(fileHandle->file)) {
            throw IoException("CStandardIo.fgets()");
        }
        return Utils::makeString(buf, 0);
    }
    return Utils::makeString(buf, strlen(buf));
}

void CStandardIo::fwrite(Pointer<string> buf, Pointer<FileHandle> fileHandle) {
    size_t bufSize = buf->buf->length();
    if (::fwrite(buf->buf->data(), 1, bufSize, fileHandle->file) != bufSize) {
        throw IoException("CStandardIo.fwrite()");
    }
}

Pointer<string> CStandardIo::fread(
    int numBytes,
    Pointer<FileHandle> fileHandle) {

    char* buf = new char[numBytes];
    size_t numBytesRead = ::fread(buf, 1, numBytes, fileHandle->file);
    if (numBytesRead < numBytes) {
        char* oldBuf = buf;
        buf = new char[numBytesRead];
        memcpy(buf, oldBuf, numBytesRead);
        delete[] oldBuf;
    }
    return Utils::makeStringNoCopy(buf, numBytesRead);
}

int CStandardIo::fileSize(Pointer<FileHandle> fileHandle) {
    FILE* file = fileHandle->file;
    if (fseek(file, 0, SEEK_END) == -1) {
        throw IoException("CStandardIo.fileSize()");
    }
    size_t size = ftell(file);
    if (size == -1) {
        throw IoException("CStandardIo.fileSize()");
    }
    rewind(file);
    return size;
}

bool CStandardIo::fileExists(Pointer<string> filename) {
    std::string fname(filename->buf->data(), filename->buf->length());
    return ::access(fname.c_str(), F_OK) != -1;
}

Pointer<FileHandle> CStandardIo::getStdOut() {
    if (stdOut == NULL) {
        stdOut = new FileHandle();
        stdOut->file = stdout;
    }
    return stdOut;
}

Pointer<FileHandle> CStandardIo::getStdIn() {
    if (stdIn == NULL) {
        stdIn = new FileHandle();
        stdIn->file = stdin;
    }
    return stdIn;
}
