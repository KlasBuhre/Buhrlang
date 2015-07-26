#ifndef Object_h
#define Object_h

class object {
public:
    object() : referenceCount(0) {}

    virtual ~object() {}

    int referenceCount;
};

#endif

