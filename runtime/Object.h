#ifndef Object_h
#define Object_h

class object {
public:
    object() : referenceCount(0) {}

    virtual ~object() {}

    virtual bool equals(Pointer<object> obj) {
        return this == obj.get();
    }

    virtual int hash() {
        return static_cast<int>(reinterpret_cast<long>(this));
    }

    int referenceCount;
};

#endif
