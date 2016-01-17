#ifndef Exception_h
#define Exception_h

#include <exception>

class IndexOutOfBoundsException: public std::exception {};

class IoException: public std::exception {
public:
    IoException(const char* i) : info(i) {}

    const char* what () const throw () {
        return info;
    }

private:
    const char* info;
};

class NumberFormatException: public std::exception {
public:
    NumberFormatException(const char* i) : info(i) {}

    const char* what () const throw () {
        return info;
    }

private:
    const char* info;
};

#endif
