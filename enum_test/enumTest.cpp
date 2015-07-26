#include <stdio.h>
#include <stdlib.h>

#include "TPointer.h"
#include "Pointer.h"

// ----------------------------------------------------------------------------
// Shape
// ----------------------------------------------------------------------------

struct Shape {
    static int _SquareTag;
    static int _RectangleTag;
    static int _CircleTag;
    static int _PointTag;

    struct _SquareData { // $SquareData
        int _0; // $Square.$0
    };

    struct _RectangleData { // $RectangleData
        int _0; // $Rectangle.$0
        int _1; // $Rectangle.$1
    };

    struct _CircleData { // $CircleData
        int _0;  // $Circle.$0
    };

    int _tag; // $tag
    union {
        _SquareData _Square; // $Square
        _RectangleData _Rectangle;
        _CircleData _Circle;
    };

    static Shape Square(int _0) {
        Shape s;
        s._tag = _SquareTag;
        s._Square._0 = _0;
        return s;
    }

    static Shape Rectangle(int _0, int _1) {
        Shape s;
        s._tag = _RectangleTag;
        s._Rectangle._0 = _0;
        s._Rectangle._1 = _1;
        return s;
    }

    static Shape Circle(int _0) {
        Shape s;
        s._tag = _CircleTag;
        s._Circle._0 = _0;
        return s;
    }

    static Shape Point() {
        Shape s;
        s._tag = _PointTag;
        return s;
    }
};

int Shape::_SquareTag = 0;
int Shape::_RectangleTag = 1;
int Shape::_CircleTag = 2;
int Shape::_PointTag = 3;

// ----------------------------------------------------------------------------
// Option<T>
// ----------------------------------------------------------------------------

struct Option___ { // Option<$>

    static int _NoneTag;

    int _tag;

    static Option___ None() {
        Option___ s;
        s._tag = _NoneTag;
        return s;
    }
};

int Option___::_NoneTag = 1;

// ----------------------------------------------------------------------------
// Option<int>
// ----------------------------------------------------------------------------

struct Option_int_ { // Option<int>

    static int _SomeTag;
    static int _NoneTag;

    struct _SomeData  { // $SomeData
        int _0;
    };

    int _tag;
    union {
        _SomeData _Some;
    };

    static Option_int_ Some(int _0) {
        Option_int_ s;
        s._tag = _SomeTag;
        s._Some._0 = _0;
        return s;
    }

    Option_int_() {}

    Option_int_(Option___ other) {
        _tag = other._tag;
    }
};

int Option_int_::_SomeTag = 0;
int Option_int_::_NoneTag = 1;

// ----------------------------------------------------------------------------
// Option<Foo>
// ----------------------------------------------------------------------------

class Foo: public virtual object {
public:
    Foo(int a) : object(), j(a) {}
    ~Foo() { printf("~Foo()\n"); }
    int j;
};

struct Option_Foo_ {
    static int _SomeTag;
    static int _NoneTag;

    struct _SomeData { // $SomeData
        TPointer<Foo> _0;
    };

    int _tag;
    union {
        _SomeData _Some;
    };

    static Option_Foo_ Some(Pointer<Foo> _0) {
        Option_Foo_ s;
        s._tag = _SomeTag;
        TPointer<Foo>::init(s._Some._0, _0);
        return s;
    }

    static Option_Foo_ None() {
        Option_Foo_ s;
        s._tag = _NoneTag;
        return s;
    }

    static void Some_destructor(Option_Foo_& _this) {
        TPointer<Foo>::destroy(_this._Some._0);
    }

    typedef void (*VariantDestructor)(Option_Foo_& _this);
    static VariantDestructor destructors[2];

    ~Option_Foo_() {
        // Use $tag as an index in a vtable to lookup the variant destructor.
        VariantDestructor d = destructors[_tag];
        if (d != NULL) {
            d(*this);
        }
    }
};

int Option_Foo_::_SomeTag = 0;
int Option_Foo_::_NoneTag = 1;
Option_Foo_::VariantDestructor Option_Foo_::destructors[2] = { Some_destructor, NULL };

int main() {
    Shape square = Shape::Square(3);
    Shape rectangle = Shape::Rectangle(2, 4);

    printf("sizeof(Shape)=%d\n", sizeof(Shape));
    printf("square._tag=%d square._Square._0=%lu\n", square._tag, square._Square._0);
    printf("rectangle._tag=%d rectangle._Rectangle._0=%d rectangle._Rectangle._1=%d\n",
           rectangle._tag,
           rectangle._Rectangle._0,
           rectangle._Rectangle._1);

    Option_int_ someInt = Option_int_::Some(7);
    Option_int_ noInt = Option___::None();

    printf("sizeof(Option_int_)=%lu\n", sizeof(Option_int_));
    printf("someInt._tag=%d someInt._Some._0=%d\n", someInt._tag, someInt._Some._0);
    printf("noInt._tag=%d \n", noInt._tag);

    Option_Foo_ someFoo = Option_Foo_::Some(Pointer<Foo>(new Foo(13)));
    Option_Foo_ noFoo = Option_Foo_::None();

    printf("sizeof(Option_Foo_)=%lu\n", sizeof(Option_Foo_));
    printf("someFoo._tag=%d someFoo._Some._0.j=%d\n", someFoo._tag, someFoo._Some._0->j);
    printf("noFoo._tag=%d \n", noFoo._tag);

    printf("sizeof(TPointer<Foo>)=%lu\n", sizeof(TPointer<Foo>));
    printf("sizeof(Foo*)=%lu\n", sizeof(Foo*));
    printf("sizeof(int)=%lu\n", sizeof(int));

    return 0;
}


