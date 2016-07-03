#include "CStandardLib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "Utils.h"

Pointer<string> CStandardLib::toString(int i) {
    char buf[64];
    snprintf(buf, 64, "%d", i);
    return Utils::makeString(buf, strlen(buf));
}

Pointer<string> CStandardLib::toString(long long l) {
    char buf[64];
    snprintf(buf, 64, "%lld", l);
    return Utils::makeString(buf, strlen(buf));
}

Pointer<string> CStandardLib::toString(float f) {
    char buf[64];
    snprintf(buf, 64, "%f", f);
    return Utils::makeString(buf, strlen(buf));
}

Pointer<string> CStandardLib::toString(unsigned char b) {
    char buf[64];
    snprintf(buf, 64, "%u", b);
    return Utils::makeString(buf, strlen(buf));
}

int CStandardLib::toInt(Pointer<string> s) {
    std::string str(s->buf->data(), s->buf->length());
    int i;

    if (sscanf(str.c_str(), "%d", &i) == EOF) {
        throw NumberFormatException("CStandardLib::toInt");
    }

    return i;
}

int CStandardLib::toFloat(Pointer<string> s) {
    std::string str(s->buf->data(), s->buf->length());
    float f;

    if (sscanf(str.c_str(), "%f", &f) == EOF) {
        throw NumberFormatException("CStandardLib::toFloat");
    }

    return f;
}

int CStandardLib::rand() {
    return ::rand();
}
