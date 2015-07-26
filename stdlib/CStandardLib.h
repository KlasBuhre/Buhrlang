#ifndef CStandardLib_h
#define CStandardLib_h

#include <Runtime.h>
#include <System.h>

class CStandardLib: public object {
public:
    static Pointer<string> toString(int i);
    static Pointer<string> toString(float f);
};

#endif
