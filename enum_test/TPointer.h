#ifndef TPointer_h
#define TPointer_h

#include "Object.h"
#include "Exception.h"
#include "Pointer.h"

// Pointer class for usage in enum variants. Since C++ does not allow objects 
// with non-trivial constructors/copy constructors/destructors in unions, this 
// pointer class has those methods replaced with static methods. The static 
// methods should be called when the corresponding constructor/destructor 
// would have been called automatically. 
template<class T>
class TPointer {
public:
    // Corresponds to:
    // TPointer()
    static TPointer create() {
        TPointer retval;
        retval.referenceCountPtr = 0;
        retval.ptr = 0;
        return retval;
    }

    // Corresponds to:
    // TPointer(T* p)
    static TPointer create(T* p) { 
        TPointer retval;
        retval.ptr = p;
        if (p) {
            retval.referenceCountPtr = &(p->referenceCount);
            (*retval.referenceCountPtr)++;
        }
        return retval;
    }

    // Corresponds to:
    // TPointer(const TPointer& rhs)
    static TPointer create(const TPointer& rhs) {
        TPointer retval;
        retval.ptr = rhs.ptr;
        retval.referenceCountPtr = rhs.referenceCountPtr;
        if (retval.ptr) { 
            (*retval.referenceCountPtr)++;
        } 
    }

    // Corresponds to:
    // template<class U> 
    // TPointer(const TPointer<U>& rhs)
    template<class U> 
    static TPointer create(const TPointer<U>& rhs) {
        TPointer retval;
        retval.ptr = rhs.get();
        retval.referenceCountPtr = rhs.referenceCountPtr;
        if (retval.ptr) { 
            (*retval.referenceCountPtr)++;
        } 
    }

    // Corresponds to:
    // ~TPointer() 
    static void destroy(TPointer& p) { 
        if (p.ptr && --(*p.referenceCountPtr) == 0) {
            delete p.ptr; 
        } 
    }

    // Corresponds to:
    // TPointer& operator=(const TPointer& rhs) 
    static TPointer& assign(TPointer& lhs, const TPointer& rhs) { 
        TPointer::create(rhs).swap(lhs); 
        return lhs; 
    }

    // Corresponds to:
    // TPointer& operator=(T* rhs) 
    static TPointer& assign(TPointer& lhs, T* rhs) { 
        TPointer::create(rhs).swap(lhs); 
        return lhs; 
    }

    // Corresponds to:
    // template<class U> 
    // TPointer& operator=(const TPointer<U>& rhs) 
    template<class U> 
    static TPointer& assign(TPointer& lhs, const TPointer<U>& rhs) {
        TPointer::create(rhs).swap(lhs); 
        return lhs; 
    }

    static void init(TPointer& lhs, const Pointer<T>& rhs) {
        TPointer retval;
        lhs.ptr = rhs.ptr;
        lhs.referenceCountPtr = rhs.referenceCountPtr;
        if (lhs.ptr) { 
            (*lhs.referenceCountPtr)++;
        } 
    }

    bool operator==(const TPointer& rhs) { 
        return ptr == rhs.ptr; 
    }

    bool operator!=(const TPointer& rhs) { 
        return ptr != rhs.ptr; 
    }

    void reset() { 
        TPointer().swap(*this); 
    }

    void reset(T* rhs) { 
        TPointer(rhs).swap(*this); 
    }

    T* get() const { 
        return ptr; 
    }

    T& operator*() const { 
        return *ptr; 
    }

    T* operator->() const {
        return ptr; 
    }

    void increasereferenceCount() {
        (*referenceCountPtr)++;
    }

    void decreasereferenceCount() {
        (*referenceCountPtr)--;
    }

    int* referenceCountPtr;

private:
    void swap(TPointer& rhs) { 
        T* tmp = ptr; 
        ptr = rhs.ptr; 
        rhs.ptr = tmp; 
        int* countTmp = referenceCountPtr;
        referenceCountPtr = rhs.referenceCountPtr;
        rhs.referenceCountPtr = countTmp;
    }

    T* ptr;
};

template<class T, class U>
TPointer<T> staticTPointerCast(const TPointer<U>& p)
{
    return static_cast<T*>(p.get());
}

template<class T, class U>
TPointer<T> dynamicTPointerCast(const TPointer<U>& p)
{
    return dynamic_cast<T*>(p.get());
}

#endif

