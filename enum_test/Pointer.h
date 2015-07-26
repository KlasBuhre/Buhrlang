#ifndef Pointer_h
#define Pointer_h

#include "Exception.h"

template<class T>
class Pointer {
public:
    Pointer() : referenceCountPtr(0), ptr(0) {}

    Pointer(T* p) : ptr(p) {
        if (ptr) {
            referenceCountPtr = &(ptr->referenceCount);
            (*referenceCountPtr)++;
        } 
    }

    Pointer(const Pointer& rhs) :
        ptr(rhs.ptr),
        referenceCountPtr(rhs.referenceCountPtr) {

        if (ptr) {
            (*referenceCountPtr)++;
        } 
    }

    template<class U> 
    Pointer(const Pointer<U>& rhs) :
        ptr(rhs.get()),
        referenceCountPtr(rhs.referenceCountPtr) {

        if (ptr) {
            (*referenceCountPtr)++;
        } 
    }

    ~Pointer() {
        if (ptr && --(*referenceCountPtr) == 0) {
            delete ptr; 
        } 
    }

    Pointer& operator=(const Pointer& rhs) {
        Pointer(rhs).swap(*this); 
        return *this; 
    }

    Pointer& operator=(T* rhs) {
        Pointer(rhs).swap(*this); 
        return *this; 
    }

    template<class U> 
    Pointer& operator=(const Pointer<U>& rhs) {
        Pointer(rhs).swap(*this); 
        return *this; 
    }

    bool operator==(const Pointer& rhs) {
        return ptr == rhs.ptr; 
    }

    bool operator!=(const Pointer& rhs) {
        return ptr != rhs.ptr; 
    }

    void reset() {
        Pointer().swap(*this); 
    }

    void reset(T* rhs) {
        Pointer(rhs).swap(*this); 
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

    void swap(Pointer& rhs) {
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
Pointer<T> staticPointerCast(const Pointer<U>& p) {
    return static_cast<T*>(p.get());
}

template<class T, class U>
Pointer<T> dynamicPointerCast(const Pointer<U>& p) {
    return dynamic_cast<T*>(p.get());
}

#endif
