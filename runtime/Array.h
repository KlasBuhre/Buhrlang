#ifndef Array_h
#define Array_h

#include "Exception.h"

template<class T>
class Array: public object {
public:
    Array() : len(0), cap(5), elements(new T[cap]) {}
    Array(unsigned c) : len(0), cap(c), elements(new T[c]) {}
    Array(T* e, unsigned l) : len(l), cap(l), elements(e) {}

    ~Array() {
        delete [] elements;
    }

    int length() const {
        return len;
    }

    int size() const {
        return len;
    }

    int capacity() const {
        return cap;
    }

    T& at(unsigned index) {
        if (index >= len) {
            throw IndexOutOfBoundsException();
        }
        return elements[index];
    }

    const T* data() const {
        return elements;
    }

    void append(T element) {
        if (len == cap) {
            reserve(cap * 2);
        }
        elements[len++] = element;
    }

    void appendAll(Pointer<Array<T> > array) {
        unsigned combinedLength = len + array->len;
        if (combinedLength > cap) {
            reserve(combinedLength * 2);
        }
        copy(elements + len, array->elements, array->len);
        len = combinedLength;
    }

    Pointer<Array<T> > concat(Pointer<Array<T> > array) {
        unsigned combinedLength = len + array->len;
        T* combinedElements = new T[combinedLength];
        copy(combinedElements, elements, len);
        copy(combinedElements + len, array->elements, array->len);
        return Pointer<Array<T> >(new Array<T>(combinedElements,
                                               combinedLength));
    }

    Pointer<Array<T> > slice(unsigned begin, unsigned end) {
        if (begin >= len || end >= len || begin > end) {
            throw IndexOutOfBoundsException();
        }
        unsigned sliceLength = end - begin + 1;
        T* sliceElements = new T[sliceLength];
        copy(sliceElements, elements + begin, sliceLength);
        return Pointer<Array<T> >(new Array<T>(sliceElements, sliceLength));
    }

private:
    void reserve(unsigned newCapacity) {
        cap = newCapacity;
        T* newElements = new T[newCapacity];
        copy(newElements, elements, len);
        delete [] elements;
        elements = newElements;
    }

    static void copy(T* destination, T* source, unsigned length) {
        T* end = source + length;
        while (source < end) {
            *destination++ = *source++;
        }
    }

    unsigned len;
    unsigned cap;
    T* elements;
};

#endif
