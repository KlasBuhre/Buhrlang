#ifndef CStandardLib_h
#define CStandardLib_h

#include <Runtime.h>
#include <System.h>

class CStandardLib: public object {
public:
    static Pointer<string> toString(int i);
    static Pointer<string> toString(long long l);
    static Pointer<string> toString(float f);
    static Pointer<string> toString(unsigned char b);
    static int toInt(Pointer<string> s);
    static int toFloat(Pointer<string> s);
    static int rand();
};

#endif
