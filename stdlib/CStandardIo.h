#ifndef CStandardIo_h
#define CStandardIo_h

#include <stdio.h>
#include <Runtime.h>

#include "System.h"

class FileHandle: public object {
public:
    FILE* file;
};

class CStandardIo: public object {
public:
    static Pointer<FileHandle> fopen(
        Pointer<string> filename,
        Pointer<string> mode);
    static Pointer<FileHandle> fdopen(int fileDescriptor, Pointer<string> mode);
    static void fclose(Pointer<FileHandle> fileHandle);
    static void fflush(Pointer<FileHandle> fileHandle);
    static void fputs(Pointer<string> str, Pointer<FileHandle> fileHandle);
    static void fputc(char character, Pointer<FileHandle> fileHandle);
    static Pointer<string> fgets(Pointer<FileHandle> fileHandle);
    static void fwrite(Pointer<string> buf, Pointer<FileHandle> fileHandle);
    static Pointer<string> fread(int numBytes, Pointer<FileHandle> fileHandle);
    static int fileSize(Pointer<FileHandle> fileHandle);
    static bool fileExists(Pointer<string> filename);
    static Pointer<FileHandle> getStdOut();
};

#endif
