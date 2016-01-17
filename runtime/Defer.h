#ifndef Defer_h
#define Defer_h

class fun_void__: public virtual object
{
public:
    virtual ~fun_void__() {}

    virtual void call() = 0;
};

class Defer {
public:
    Defer() : closures() {}

    ~Defer() {
        for (int i = closures.size() - 1; i >= 0; i--) {
            closures.at(i)->call();
        }
    }

    void addClosure(Pointer<fun_void__> closure) {
        closures.append(closure);
    }

private:
    Array<Pointer<fun_void__> > closures;
};

#endif
