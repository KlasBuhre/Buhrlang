#include <stdio.h>

#include "Utils.h"
#include "CStandardLib.h"

Pointer<string> CStandardLib::toString(int i) {
    char buf[64];
    snprintf(buf, 64, "%d", i);
    return Utils::makeString(buf, strlen(buf));
}

Pointer<string> CStandardLib::toString(float f) {
    char buf[32];
    snprintf(buf, 64, "%f", f);
    return Utils::makeString(buf, strlen(buf));
}

int CStandardLib::toInt(Pointer<string> s) {
    const char* str = s->buf->data();
    int i;

    if (sscanf(str, "%d", &i) == EOF) {
        throw NumberFormatException("CStandardLib::toInt");
    }

    return i;
}

int CStandardLib::toFloat(Pointer<string> s) {
    const char* str = s->buf->data();
    float f;

    if (sscanf(str, "%f", &f) == EOF) {
        throw NumberFormatException("CStandardLib::toFloat");
    }

    return f;
}
