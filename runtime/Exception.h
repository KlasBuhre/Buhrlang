#ifndef Exception_h
#define Exception_h

#include <stdio.h>
#include <stdlib.h>
#include <string>

namespace Exception {
    inline void indexOutOfBounds() {
        printf("\nIndexOutOfBoundsException\n");
        exit(0);
    }

    inline void nullPointer() {
        printf("\nNullPointerException\n");
        exit(0);
    }

    inline void io(const std::string& info) {
        printf("\nIoException: %s\n", info.c_str());
        exit(0);
    }

    inline void numberFormat(const std::string& info) {
        printf("\nNumberFormatException: %s\n", info.c_str());
        exit(0);
    }
}

#endif

