#ifndef Utils_h
#define Utils_h

#include <Runtime.h>
#include <System.h>
#include <string.h>

namespace Utils {
    inline Pointer<string> makeString(const char* buf, size_t length) {
        char* chars = new char[length];
        memcpy(chars, buf, length);
        Pointer<Array<char> > charArrayPtr(new Array<char>(chars, length));
        Pointer<string> str(new string(charArrayPtr));
        return str;
    }

    inline Pointer<string> makeStringNoCopy(char* buf, size_t length) {
        Pointer<Array<char> > charArrayPtr(new Array<char>(buf, length));
        Pointer<string> str(new string(charArrayPtr));
        return str;
    }
}

#endif
