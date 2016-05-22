import "System"

message class Vector<T> {
    private var T[] data

    // Create a vector with default capacity.
    init() {
        data = new T[]
    }

    // Create a vector with the given initial capacity.
    init(int capacity) {
        data = new T[capacity]
    }

    // Return the size of the vector.
    int size() {
        return data.size
    }

    // Add an element at the end of the vector.
    add(T element) {
        data.append(element)
    }

    // Return the element at the given index.
    T at(int index) {
        return data[index]
    }

    // Iterate over each element in the vector.
    each() (T) {
        data.each |element| {
            yield(element)
        }
    }

    // Iterate over each element and index in the vector.
    eachWithIndex() (T, int) {
        let size = data.size
        for var i = 0; i < size; i++ {
            yield(data[i], i)
        }
    }

    // Iterative fold left.
    T foldl(T start, fun T(T, T) f) {
        var T acc = start
        data.each |element| {
            acc = f(acc, element)
        }
        return acc
    }

    // Create a new vector from the old one and select which elements to
    // include.
    Vector<T> filter(fun bool(T) f) {
        let result = new Vector<T>
        data.each |element| {
            if f(element) {
                result.add(element)
            }
        }
        return result
    }
}
