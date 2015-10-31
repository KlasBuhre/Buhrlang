import "Trace"
import "File"
import "TcpSocket"
import "Convert"
import "List"
import "Vector"
import "Console"

// ----------------------------------------------------------------------------
// Examples:
// ----------------------------------------------------------------------------

class Foo

staticTypesExample() {
    let int i = 42
    let float pi = 3.1415
    let bool b = true
    // b = false     // Error: Cannot change the value of a constant.
    let string hello = "Hello"
    let Foo foo = new Foo
    let char[] characters = ['a', 'b', 'c']
}

typeInferenceExample() {
    let i = 42
    let pi = 3.1415
    let b = true
    let hello = "Hello"
    let foo = new Foo
    let characters = ['a', 'b', 'c']
    //let unknown // Error: Implicitly typed variables must be initialized. 
}

memorySafetyExample() {
    let i = 42                    // On the stack.
    let foo = new Foo             // On the heap.
    let letters = ['a', 'b', 'c'] // On the heap.
    // let c = letters[3]         // Run-time error!
}

/*
class Book {
    string author
    string title 
    float price
    let pages = new Vector<string>

    init(string a, string t, float p) {
        author = a
        title = t
        price = p
    }

    string toString() {
        return author + ": " + title + ". Price: " + Convert.toStr(price)
    }
}
*/

class Book(string author, string title, float price) {
    let pages = new Vector<string>

    string toString() {
        return author + ": " + title + ". Price: " + Convert.toStr(price)
    }
}

genericTypesExample() {
    let strings = new List<string>
    strings.add("Hello")
    strings.add("World!")
    strings.each { |s| print(s + " ") }
    strings.addFront("in Buhrlang")
    if let Some(s) = strings.getFront {
        println(s)
    }

    let books = new Vector<Book>
    books.add(new Book("Stroustrup", "The C++ Programming Language", 399.50))
    books.add(new Book("Buhre", "Programming for Monkeys", 599.50))
    books.add(new Book("Kurzweil", "How to Create a Mind", 299.50))

    println("All books:")
    books.each { |book|
        println(book.toString)
    }

    println("Cheap books:")
    let cheapBooks = books.select { |book| book.price < 400.0 }
    cheapBooks.each { |book|
        println(book.toString)
    }
}

enum Result {
    Ok,
    Error
}

class HttpEvent

interface HttpService {
    Result handleEvent(HttpEvent event)
}

interface CallbackListener {
    handleCallback(Result result)
}

class ChatService: HttpService, CallbackListener {
    Result handleEvent(HttpEvent event) {
        return Result.Ok
    }

    handleCallback(Result result) {
    }
}

interfaceExample() {
    let service = new ChatService
    let result = service.handleEvent(new HttpEvent)
    service.handleCallback(Result.Error) 
}

runExamples() {
    staticTypesExample
    typeInferenceExample
    memorySafetyExample
    interfaceExample
    genericTypesExample

}

// ----------------------------------------------------------------------------
// Tests:
// ----------------------------------------------------------------------------

class HttpResponse {
    string statusCode
    string reasonPhrase
    var string body

    init(string status, string reason) {
        statusCode = status
        reasonPhrase = reason
    }
}

class HttpResponse2(string statusCode, string reasonPhrase) {
    var string body
}

class Vehicle(int mass)

class Wheel

class Truck2(int power, arg int mass): Vehicle(mass) {
    let wheels = new List<Wheel>
}

class Rocket: Vehicle {
    string destination
    string paylod

    init(string d, string p, int mass): Vehicle(mass) {
        destination = d
        paylod = p
    }

    init(string d, int mass) : init(d, "empty", mass) {}
}

class Rocket2(string destination, string paylod, arg int mass): Vehicle(mass) {
    init(string d, int mass) : init(d, "empty", mass) {}
}

class Truck: Vehicle {
    int power
    let wheels = new List<Wheel>

    init(int hp, int mass) : Vehicle(mass) {
        power = hp
    }
}

class ConstructorTest {
    run() {
        println("----------[Constructor Test]----------")

        let response = new HttpResponse("200", "OK")
        println(response.statusCode)

        let response2 = new HttpResponse2("200", "OK")
        println(response2.statusCode)

        let truck = new Truck(200, 2200)
        println(truck.mass)
        truck.wheels.add(new Wheel)

        let truck2 = new Truck2(200, 2200)
        println(truck2.mass)

        let rocket = new Rocket("Mars", 100)
        println("Rocket to " + rocket.destination)

        let rocket2 = new Rocket2("Mars", 100)
        println("Rocket to " + rocket2.destination)
    }
}

// --------------------------------------------------------------------------

class SomeType {
    init(int a) {
        member = a
        let flag = true
        let string str = "test"
        print(str)
        let i = 44 + 55
    }

    method() {
        print(member)
    }

    int method2(string str) {
        print(str)
        return 1
    }

    var int member
}

class AnotherType {
    init() {}
}

class ExpressionTest {
    SomeType someType
    var int number

    init() {
        println("----------[Expression Test]----------")
        someType = new SomeType(0)
        number = 1
    }

    run() {
        let int test = 124
        let int i2 = test + 65
        print(test)

        if test == 123 {
            print(test)
        } else if test == 124 {
            print(124)
        } else {
            print(2)
        }

        someType.member = 4
        print(someType.member)
        print(returnSomeType.member)

        //a.b.c
        someType.method()
        someType.method2(returnStr)

        if someType.method2(returnStr) == 5 {
            print(5)
        }

        let string str1 = "test"
        let string str2 = new string("test")
        if str1.equals(str2) {
            println("\nStrings " + str1 + " and " + str2 + " are equal.")
        }

        let j = 3
        let k = j * 5 - 4 / 2
        println("k = " + Convert.toStr(k))
        let q = 2*(9 - 5) / (j - 1) + 2
        println("q = " + (Convert.toStr(q))) 

        let float pi = 3.1415
        let diameter = 2.5
        let circumference = pi * diameter
        println("circumference = " + Convert.toStr(circumference))

        let neg = -20
        println(20 * neg)
        let f = -1.3 * -2.77
        println(f)

        let a1 = 1
        let a2 = 2
        let b4 = 4
        let b5 = 5
        println((a1 < a2) && (b4 < b5))

        var y = 2
        y += 1
        println(y)

        // var m = 0
        // if m = 1 {     // Error: Assignement returns void.
        //     print("m=")
        //     print(m)
        // }

        testBytesAndChars()
        testBooleans()

        this.number = 10
        println(number)
    }

    string returnStr() {
        return "hello"
    }

    SomeType returnSomeType() {
        return new SomeType(0)
    }

    testBytesAndChars() {
        let char c = 'a'
        let byte b = 255
        println("character=" + Convert.toStr(c))
        print("byte=")
        println(b)
        var byte[] buf = new byte[1024]
        // buf[255] = 64
        buf.append(64)
    }

    testBooleans() {
        let b = true
        if b == true {
            println("b is true")
        }
        if b {
            println("b is true")
        }
        if !b {
            println("b is false")
        }

        let c = true
        if b && c {
            println("b and c are true")
        }
        var d = false
        if d || !d {
            println("This should be printed")
        }

        let e = 1
        let f = 2
        if e == 1 && f == 2 {
            println("This should be printed")
        }

        if e == 2 && f != 3 || e == 1 {
            println("This should be printed")
        }

        if !SomeNamespace.returnBool(false) {
            println("This should be printed")
        }
    }
}

class SomeNamespace {
    static bool returnBool(bool b) {
        return b
    }
}

// --------------------------------------------------------------------------

class ConstantAndVariableDeclarationTest {
    var int member
    // static var int staticData

    run() {
        println(
            "----------[Constants and variable declarations Test]----------")

        let int constIntA = 1
        let constIntB = 2      
        var int variableIntA      
        var variableIntB = 3    
        
        variableIntA = constIntA
        // constIntB = 4    // Error: cannot modify constant.

        println(variableIntA)

        var SomeType varObject
        let SomeType constObject

        // staticData = 1
        member = 2
    }

    static staticMethod() {
        // print member     // Error: cannot access non-static member from
    }                       // static method.
}

// ---------------------------------------------------------------------------

class StringArray {
    let array = ["string 1", "string 2"]

    each() (string) {
        for var i = 0; i < array.length; i++ {
            yield(array[i])
        }
    }

    each2() (string) {
        array.each { |s|
            yield(s)
        }
    }
}

class IntArray {
    let array = [1, 2, 3]

    int fold() int(int, int) {
        var acc = 0
        for var i = 0; i < array.length; i++ {
            acc = yield(acc, array[i])
        }
        return acc
    }
}

class DoNotTransform {
    int doNotTransformMember1
}

class LambdaMethods(int member) {
    method() (int) {
        yield(internalMethod)
    }

    int internalMethod() {
        return 1
    }

    int outer(int a) (int) {
        let k = a + member
        return inner(k) { |m|
            let n = m + member
            yield(n)
        }
    }

    int inner(int a) (int) {
        let k = a + member
        let bv = new Vector<bool>
        bv.add(true)
        yield(k)
        return k
    }

    static staticMethod() (int) {
        yield(internalStaticMethod)
    }

    static int internalStaticMethod() {
        return 2
    }
}

class LambdaTest {
    int doNotTransformMember

    run() {
        println("----------[Lambda Test]----------")

        let strings = new StringArray
        strings.each { |str|
            str.characters.each { |character|
                print(character)
            }
            println
        }

        strings.each2 { |str|
            str.characters.each { |character|
                print(character)
            }
            println
        }

        let outerStrings = new StringArray
        outerStrings.each { |outer|
            let innnerStrings = new StringArray
            innnerStrings.each { |inner|
                print(outer)
                print(inner)
            }
            println
        }

        let integers = new IntArray
        let sum = integers.fold { |acc, e| acc + e }
        println("sum=" + Convert.toStr(sum))

        var outer = 2
        method() { |i, str|
            var j = i + 5
            j = outer
            outer = 0
            println(str)
            j
        }

        twoYields() { |i| 
            println("Yield " + Convert.toStr(i))
        }

        let obj = new DoNotTransform
        lambdaMethod(obj.doNotTransformMember1) { |i|
            print(i)
        }

        lambdaMethod(doNotTransformMember) { |i|
            print(i)
        }

        let lambdas = new LambdaMethods(2)
        lambdas.method() { |i|
            print(i)
        }
        lambdas.outer(1) { |a| println(a) }

        let lambdas2 = new LambdaMethods(2)
        lambdas2.outer(1) { |a| println(a) }

        LambdaMethods.staticMethod() { |i|
            print(i)
        }

        let wv = new WrappedVector<byte>
        wv.v.add((byte) 10)
        wv.v.add((byte) 20)
        wv.each { |e| print(e) }

        println
    }

    method() int(int, string) {
        yield(1, "str")
    }

    twoYields() (int) {
        yield(1)
        yield(2)
    }

    lambdaMethod(int a) (int) {
        yield(a)
    }
}

class LambdaMethods2(int member) {
    int outer(int a) (int) {
        let k = a + member
        return inner(k) { |m|
            let n = m + member
            yield(n)
        }
    }

    int inner(int a) (int) {
        let k = a + member
        let bv = new Vector<char>
        bv.add('a')
        yield(k)
        return k
    }
}

class WrappedVector<T> {
    let v = new Vector<T>

    each() (T) {
        v.each { |e|
            yield(e)
        }
    }
}

// ----------------------------------------------------------------------------

interface FastCar {
    driveFast()
}

class Car: FastCar {
    int mass
    int hp

    init(int m, int h) {
        mass = m
        hp = h
    }

    printBasicInfo() {
        println("mass = " + Convert.toStr(mass) + ", hp = " +
                Convert.toStr(hp))
    }

    drive() {
        println("Driving...")
    }
}

class Audi: Car {
    bool quatro

    init(int m, int h, bool q) : Car(m, h) {
        quatro = q
    }

    init() : init(1500, 170, true) {}

    printInfo() {
        printBasicInfo
        println("quatro = " + Convert.toStr(quatro))
    }

    driveFast() {
        println("Driving fast in an Audi!")
    }
}

class Bmw: Car {
    init(int m, int h) : Car(m, h) {}

    driveFast() {
        println("Driving fast in a BMW!")
    }
}

class A {}

class B: A {}

class C: B {}

interface IntegerList {
    add(int element)
    removeLast()
}

class IntegerListImpl: IntegerList {
    add(int element) {
        println("IntegerListImpl.add " + Convert.toStr(element))
    }

    removeLast() {}
}

interface CallbackInterface {
    callback()
}

class CallbackExecutor {
    doCallback(CallbackInterface obj) {
        obj.callback()
    }
}

interface Ia {
    a()
    b()
}

interface Ib {
    c()
    d()
}

class Cc: Ia, Ib {
    e() {}
}

class Cd: Cc {
    a() {}
    b() {}
    c() {}
    d() {}
}

class InheritanceTest: CallbackInterface {
    run() {
        println("----------[Inheritance Test]----------")

        let bmw = new Bmw(1600, 300)
        let a = new A
        let c = new C
        let audi = new Audi(1700, 450, true)
        audi.printInfo
        audi.drive

        let list = new IntegerListImpl
        list.add(7)

        let executor = new CallbackExecutor
        executor.doCallback(this)

        let d = new Cd
    }

    callback() {
        println("Callback")
    }
}

// ----------------------------------------------------------------------------

Vector<float> getFloats() {
    let v = new Vector<float>
    v.add(3.14)
    v.add(2.0)
    return v
}

enum BoolEnum { True, False }

class RecursiveRefType(string data, Option<RecursiveRefType> next)

class RecursiveGenRefType<T>(T data, Option<RecursiveGenRefType<T>> next)

class Outer {
    class Inner

    method() {
        let inner = Some(new Inner)
    }
}

class GenericsTest {
    let memberVector = new Vector<string>

    run() {    
        println("----------[Generics Test]----------")

        let list = new List<int>
        list.add(1)
        list.add(2)
        list.add(3)
        list.addFront(0)
        if let Some(f) = list.getFront {
            print(f)
        }
        if let Some(b) = list.getBack {
            print(b)
        }
        list.each { |e|
            print(e)
        }
        println

        let enumList = new List<BoolEnum>
        enumList.add(BoolEnum.True)
        enumList.add(BoolEnum.False)
        enumList.each { |e|
            match e {
                BoolEnum.True -> print("True"),
                BoolEnum.False -> print("False")
            }
        }
        println

        let recursive = new RecursiveRefType("data1",
                                             Some(new RecursiveRefType("data2",
                                                                       None)))
        var r = recursive
        while {
            println(r.data)
            match r.next {
                Some(next) -> r = next,
                None -> break
            }
        }

        let recursiveGen =
            new RecursiveGenRefType<string>(
                "data1",
                Some(new RecursiveGenRefType("data2", None)))
        var g = recursiveGen
        while {
            println(g.data)
            match g.next {
                Some(next) -> g = next,
                None -> break
            }
        }

        let ints = new Vector<int>
        ints.add(2)
        ints.add(3)
        ints.add(4)
        print("Vector.foldl: ")
        let f = ints.foldl(1) { |acc, elem| acc * elem }
        println(f)

        let largeInts = ints.select { |elem| elem > 2 }
        print("Vector.select: ")
        largeInts.each { |i|
            print(i)
        }
        println

        let cars = new List<Car>
        cars.add(new Bmw(1600, 300))
        cars.add(new Audi(1700, 450, true))
        cars.each { |car|
            car.driveFast
        }

        let vector = new Vector<string>
        vector.add("abc")
        vector.add("def")
        vector.add("ghi")
        vector.each { |s|
            println(s)
        }        
        vector.eachWithIndex { |s, index|
            if index == 2 {
                break
            }
            println(Convert.toStr(index) + " : " + s)
        }

        let numbers = new Vector<int>
        for var j = 0; j < 100; j++ {
            numbers.add(j)
        }
        println(numbers.at(50))
        numbers.each { |n|
            // if n == 50 continue
            print(n)
        }

        println

        let val = new Box<string>("value")
        println(val.value)

        let Box<string> anotherBoxedString = new Box<string>("str")
        println(anotherBoxedString.value)

        let memberVector = new Vector<string>
        memberVector.add("a")
        memberVector.add("b")
        memberVector.each { |s|
            print(s)
        }
        println

        let vectorOfBoxes = new Vector<Box<int>>
        vectorOfBoxes.add(new Box(10))
        vectorOfBoxes.add(new Box(20))
        vectorOfBoxes.each { |box|
            print(box.value)
        }
        println

        let boxedInt = new Box<int>(4)
        println(boxedInt.value)

        let boxedBool = new Box(true)
        println(boxedBool.value)

        let listOfVectors = new List<Vector<string>>
        let v1 = new Vector<string>
        v1.add("v1_1")
        v1.add("v1_2")
        listOfVectors.add(v1)
        let v2 = new Vector<string>
        v2.add("v2_1")
        v2.add("v2_2")
        listOfVectors.add(v2)
        listOfVectors.each { |v|
            v.each { |s|
                print(s)
            }
            println
        }

        let floats = getFloats
    }
}

// ----------------------------------------------------------------------------

class HasString {
    let memberString = "HasString"
}

class HasArray {
    let array = [3, 2, 1]
}

class ArrayTest {
    var int[] memberArray

    run() {
        println("----------[Array Test]----------")

        var array = new int[3]   // Array capacity is 3, size is zero.
        print(array.size)
        print(array.capacity)
        array.append(4)
        array.append(5)
        print(array[1])

        let capacity = 10
        let stringArray = new string[]
        var intArray = new int[capacity]
        var i = 0
        while i < capacity {
            intArray.append(i*i)
            i++
        }

        i = 0
        while i < capacity {
            println(intArray[i])
            i++
        }

        memberArray = new int[1 + 5] 
        memberArray.append(1)

        let numbers = [1, 2, 3]
        numbers.each { |number|
            print(number)
            if number == 2 {
                println("This element equals two")
                println("Testing outer block access from lambda " +
                        Convert.toStr(memberArray[0]))
                break
            }
        }

        let str = "string"
        str.characters.each { |character|
            print(character)
        }

        let hs = new HasString
        hs.memberString.characters.each { |c|
            print(c)
        }

        println

        let strArray = ["str1", "str2"]
        strArray.each { |s|
            print(s)
        }

        println

        var ints = [0, 0, 0]
        for var i = 0; i < ints.length; i++ {
            ints[i]++
        }
        ints.each { |i| print(i) }
        println

        ints.append(4)
        ints.appendAll([1, 2])
        ints.each { |i| print(i) }
        println

        let c = numbers.concat(ints)
        print("c=")
        c.each { |i| print(i) }
        println

        let c2 = numbers + ints
        print("c2=")
        c2.each { |i| print(i) }
        println

        let slice = ints.slice(1, 2)
        print("slice=")
        slice.each { |i| print(i) }
        println

        let hasStrArray = new HasString[3]
        let hasArray = new HasArray
        print(hasArray.array[1])
        println

        [1, 2, 3, 4].each { |i| print(i) }
        println

        let arr = [0, 1, 2, 3, 4]
        let slice2 = arr[1...3]
        print("slice2=")
        slice2.each { |i| print(i) }
        println

        // let q = 1...2
        var defaultCapacity = new int[]
        defaultCapacity.append(0)
        defaultCapacity.append(1)
        defaultCapacity.appendAll(arr)
        defaultCapacity.each { |i| print(i) }
        println

        var d2 = new int[]
        d2.append(0)
        d2.append(1)
        d2 += arr
        d2.each { |i| print(i) }
        println

        println(createIntArray(5)[2])
    }

    int[] createIntArray(int size) {
        var array = new int[size]
        for var i = 0; i < size; i++ {
            array.append(i)
        }
        return array
    }
}

// ----------------------------------------------------------------------------

class WhileTest {
    run() {
        println("----------[While Test]----------")
        
        var i = 0
        while i < 5 {
            ++i
            println(i)
            if i == 5 {
                var str = "i == 5\n"
                print(str)
            } else {
                var i = 0
                print("i in inner block=" + Convert.toStr(i))
            }
        }

        var a = 0
        while {
            print(a)
            if a++ == 3 {
                break
            }
        }

        println("\nFor loop:")

        for var k = 0; k < 3; k++ {
            print("k=" + Convert.toStr(k) + " ")
            if k == 1 {
                break
            }
        }

        var int m
        for m = 3; m < 6; m++ {
            print("m=" + Convert.toStr(m) + " ")
        }

        var b = 0
        for {
            print(b)
            if b++ == 3 {
                break
            }
        }


        // println("Eternal loop for checking garbage collection.")
        // var j = 0
        // while true {
        //     print(j)
        //     j++
        //     allocate()
        // }
        println
    }

    allocate() {
        let c = new Audi
        let buffer = new byte[1000000]
    }
}

// --------------------------------------------------------------------------

class StringTest {
    run() {
        println("----------[String Test]----------")

        let s = "abcdefg"
        println(s)
        print("at(2)=")
        println(s.at(2))
        let s2 = "abcdefg"
        if s.equals(s2) {
            println("s and s2 are equal")
        }

        let untrimmed = "  abc d "
        println("untrimmed=" + untrimmed)
        println("trimmed=" + untrimmed.trim)

        let a = "This is string a. "
        let b = "This is string b."
        a.append(b)
        println("Appended strings=" + a)

        let a2 = "This is string a. "
        a2 += b
        println("Appended strings=" + a2)

        let c = b.concat(" Concatenated part.")
        println("Concatenated string=" + c)
        let d = "abc".concat("defg")
        println("Concatenated string d=" + d)

        let e = "abc" + "defg"
        println("operator '+' e=" + e)

        let f = "ff"
        let g = "gg"
        if f != g {
            println(f + " and " + g + " are not equal.")
        }

        let h = "hh"
        let i = "hh"
        if h == i {
            println(h + " and " + i + " are equal.")
        }

        let tokens = "GET /index.html HTTP/1.1\r\n".split
        tokens.each { |token|
            println("'" + token + "'")
        }

        println("Number is " + Convert.toStr(123))
    }
}

// --------------------------------------------------------------------------

class Obj
{
    int m1
    int m2

    init(int a,
         int b) {
         m1 = a
         m2 = b
    }

    method(
        string arg1, 
        string arg2,
        int arg3)
    {
        method2(arg1,
                arg2,
                arg3)
    }

    method2(
        string arg1, 
        string arg2,
        int arg3)
    {
        for var i = 0;
            i < arg3;
            i++ {
            print(arg1)
        }
    }
}

function(string s, int i) {
    println(s + Convert.toStr(i))
    println(add(1, 2))
}

int add(int a, int b) {
    return a + b
}

loop(int times) (int) {
    for var int i = 0; i < times; i++ {
        yield(i)
    }
}

class CompactClass { method(int x) { println(x) }}
class CompactClass2 {
    method(int x) { println(x)
                    return }}

class CompactClass3 { int x
                      int y }

class ParseTest {
    run() {
        println("----------[Parse Test]----------")

        let o = new Obj(1, 2)
        o.method("arg1", 
                 "arg2", 
                 2)
        println
        function(
            "test", 2)

        println(add(2, 5))

        loop(2) { |j|
            println(j)
        }

        let compact =
            new CompactClass
        compact.
            method(  10 )

        /*
        println("This should not be printed.");
        println("This should not be printed.");
        println("This should not be printed.");
        println("This should not be printed.");
        */
    }
}

// --------------------------------------------------------------------------

testArrayPatterns() {
    let a = [1, 2, 3, 4]
    match a {
        [1, second, _]  -> println(second),
        [1, second, ..] -> println(second),  // Match.
        _ -> {}
    }

    let v = [1, 2, 3]
    let l = match v {
        [.., last] -> last,
        _ -> 0
    }
    println(l)

    match [-3.14, 0.0, 0.0, 3.14] {
        [x, .., y] if x < y -> println(x*y),
        [..] -> {}
    }

    let d = 'a'
    match ['a', 'b', 'c', 'd'] {
        ['a', b, 'c', 'f'] -> println(b),
        ['c', 'b', c, 'd'] -> println(c),
        ['a', _, 'c', d]   -> println(d),
        _ -> println("No match")
    }

    match [3, 2] {
        [1, 2] | [2, 3] -> println("[1, 2] | [2, 3]"),
        [3, 4] | [3, 2] -> println("[3, 4] | [3, 2]"),
        _ -> {}
    }

    let v2 = [1, 2, 3]
    let l3 = match v2 {
        [.., last] if last == 3 -> last,
        [.., last]              -> last * 2,
        _ -> 0
    }
    println(l3)
}

class Point(int x, int y)
class HttpRequest(string method, string url)

testClassDecompositionPatterns() {
    let p = new Point(2, 5)

    let x = match new Point(1, 2) {
        Point { x: xPos } -> xPos
    }
    println(x)

    let y = 2

    match p {
        Point { x: 2, y: 3 }       -> println("x is 2 and y is 3"),
        Point { x: 2, y } if y > 3 -> println("x is 2 and y is > 3"),
        Point { y: 5 }             -> println("y is 5"),
        Point { x, y }             -> println(x + y)
    }

    let request = new HttpRequest("GET", "/image.jpeg")
    match request {
        HttpRequest { method: "GET", url }  -> println("GET " + url),
        HttpRequest { method: "POST", url } -> println("POST " + url),
        _ -> {}
    }

    let x2 = match new Point(2, 3) {
        Point { x: _, y: 4 }    -> 0,
        Point { x: xPos, y: 2 } -> xPos,
        Point {}                -> 0
    }
    println(x2)

    let q = new Point(2, 3)
    let b = match q {
        Point { x: 0, y: 0 }                  -> false,
        Point { x: 1 } | Point { y: 1 }       -> false,
        Point { x, y: 2 } | Point { x, y: 3 } -> true,
        Point { x }                           -> false
    }
    println(b)

    let book = new Book("Adams", "Hitchhiker's Guide to the Galaxy", 199.0)
    let Book { author, title: _, price: pr } = book
    println(author + " " + Convert.toStr(pr))

    {
        let Book { author, title } = new Book("X", "Y", 599.0)
        println(author + ": " + title)

        let Point { x, y } = new Point(3, 5)
        println(x + y)
    }
}

testConstructorPatterns() {
    let p = new Point(2, 5)

    match p {
        Point(2, 3)          -> println("x is 2 and y is 3"),
        Point(2, y) if y > 3 -> println("x is 2 and y is > 3"),
        Point(5, _)          -> println("y is 5"),
        Point(x, y)          -> println(x + y)
    }

    let request = new HttpRequest("GET", "/image.jpeg")
    match request {
        HttpRequest("GET", url)  -> println("GET " + url),
        HttpRequest("POST", url) -> println("POST " + url),
        _ -> {}
    }

    let x = match new Point(1, 2) {
        Point(xPos, _) -> xPos
    }
    println(x)

    println(match new Point(1, 2) { Point(xPos, _) -> xPos })

    let x2 = match new Point(2, 3) {
        Point(_, 4)    -> 0,
        Point(xPos, 2) -> xPos,
        Point(_, _)    -> 0
    }
    println(x2)

    let k = match new Point(9, 8) {
        Point(9, y) ->
            match y {
                7 -> "seven",
                8 -> "eight",
                _ -> "no match"
            },
        Point(_, _) ->
            "no match"
    }
    println("y is " + k)

    let q = new Point(2, 3)
    let b = match q {
        Point(0, 0)               -> false,
        Point(1, _) | Point(_, 1) -> false,
        Point(x, 2) | Point(x, 3) -> true,
        Point(x, _)               -> false
    }
    println(b)

    let book = new Book("Adams", "Hitchhiker's Guide to the Galaxy", 199.0)
    let Book(author, _, pr) = book
    println(author + " " + Convert.toStr(pr))

    {
        let Book(author, title, _) = new Book("X", "Y", 599.0)
        println(author + ": " + title)

        let Point(x, y) = new Point(3, 5)
        println(x + y)
    }
}

interface Expression
class X: Expression
class Const(int value): Expression
class Add(Expression left, Expression right): Expression
class Mult(Expression left, Expression right): Expression
class Neg(Expression expr): Expression

int eval(Expression expression, int xValue) {
    return match expression {
        X {}                 -> xValue,
        Const { value }      -> value,
        Add { left, right }  -> eval(left, xValue) + eval(right, xValue),
        Mult { left, right } -> eval(left, xValue) * eval(right, xValue),
        Neg { expr }         -> -eval(expr, xValue),
        _                    -> 0
    }
}

int eval2(Expression expression, int xValue) {
    return match expression {
        X            -> xValue,
        Const(value) -> value,
        Add(l, r)    -> eval2(l, xValue) + eval2(r, xValue),
        Mult(l, r)   -> eval2(l, xValue) * eval2(r, xValue),
        Neg(expr)    -> -eval2(expr, xValue),
        _            -> 0
    }
}

string eToStr(Expression expression) {
    return match expression {
        X {}                       -> "x",
        Const { value: cst }       -> Convert.toStr(cst),
        Add { left: l, right: r }  -> "(" + eToStr(l) + " + " + eToStr(r) + ")",
        Mult { left: l, right: r } -> eToStr(l) + " * " + eToStr(r),
        Neg { expr: e }            -> "-" + eToStr(e),
        _                          -> ""
    }
}

Expression simplify(Expression expression) {
    return match expression {
        Mult { left: Const { value: 1 }, right } -> right,
        Mult { left, right: Const { value: 1 } } -> left,
        _ -> expression
    }
}

Expression simplify2(Expression expression) {
    return match expression {
        Mult(Const(1), x) -> x,
        Mult(x, Const(1)) -> x,
        _ -> expression
    }
}

testSubclassPatterns() {
    // 1 + 2 * x * x
    let expr = new Add(new Const(1), 
                       new Mult(new Const(2), new Mult(new X, new X)))
    println(eval(expr, 3))
    println(eval2(expr, 3))

    // 1 * (x + x)
    let expr2 = new Mult(new Const(1), new Add(new X, new X))
    println(eToStr(expr2))
    let simpleExpr = simplify(expr2)
    println(eToStr(simpleExpr))
    let simpleExpr2 = simplify2(expr2)
    println(eToStr(simpleExpr))
}

class MatchTest {
    run() {
        println("----------[Match Test]----------")

        let x = 4
        let y = match x {
            1     -> "One",
            2     -> "Two",
            3 | 4 -> "Three or four",
            n     -> "Big"
        }
        println(y)

        let flag = match x {
            1         -> false,
            2 | 3 | 4 -> true,
            _         -> false
        }
        println(flag)

        match 1 {
            0 -> {
                let i = 0
                println(i)
            },
            1 -> {
                let i = 1
                println(i)
            },
            _ -> {} // Do nothing.
        }

        let request = new HttpRequest("GET", "/")
        match request.method {
            "GET"  -> handleGet(request),
            "POST" -> handlePost(request),
            _      -> {} // Do nothing.
        }

        println(match true { true -> "is true", false -> "is false" })
        println(fact(4))

        testArrayPatterns
        testClassDecompositionPatterns
        testConstructorPatterns
        testSubclassPatterns
    }

    int fact(int x) {
        return match x {
            0 -> 1,
            n -> n * fact(n - 1)
        }
    }

    handleGet(HttpRequest request) {
        println("Handling GET")
    }

    handlePost(HttpRequest request) {
        println("Handling POST")
    }
}

// ----------------------------------------------------------------------------

interface Fruit {
    string name()
}

class Apple: Fruit {
    string name() {
        return "Apple"
    }

    bite() {
        println("Biting the apple.")
    }
}

class Orange: Fruit {
    string name() {
        return "Orange"
    }

    peel() {
        println("Peeling the orange.")
    }
}

class Peach: Fruit {
    string name() {
        return "Peach"
    }
}

class TypeCastTest {
    run() {
        println("----------[Type cast Test]----------")

        let object obj = new Audi(1500, 200, false)
        if let Audi audi = obj {
            audi.printInfo
        }

        let object apple = new Apple
        if let Orange orange = apple {
            println("This should not be printed.")
        }

        let fruit = (Fruit) new Apple  // Upcast.
        println(fruit.name)

        let Fruit someFruit = new Orange
        if let Orange orange = someFruit {
            println(orange.name)
        }

        match fruit {
            Apple apple -> apple.bite,
            Orange orange -> orange.peel,
            Peach _ -> println("Some peach"),
            _ -> {}
        }

        if let Apple apple = fruit {
            apple.bite
        }

        let byte b = 255
        let i = (int) b
        println(i)

        let object o = new Vector<int>
        match o {
            Vector<int> v -> println("Vector<int>"),
            _ -> {}
        }
    }
}

// ----------------------------------------------------------------------------

enum Shape {
    Square(int),
    Rectangle(int, int),
    Circle(int),
    Point
}

int rectangularArea(Shape shape) {
    return match shape {
        Shape.Square(side)             -> side * side,
        Shape.Rectangle(width, height) -> width * height,
        Shape.Circle(_)                -> 0,
        Shape.Point                    -> 0
    }
}

enum Animal {
    Dog,
    Cat,
    Dolphin
}

enum Plant {
    Tree,
    Flower
}

enum UsedEnum {
    Variant1(string),
    Variant2(int)
}

testExposedEnum() {
    use UsedEnum
    let e = Variant1("variant1")

    match e {
        Variant1(s) -> println(s),
        Variant2(_) -> {}
    }
}

Option<float> divide(float numerator, float denominator) {
    if denominator == 0.0 {
        return None
    } else {
        return Some(numerator / denominator)
    }
}

class Boat(string name)

takeOptionBoat(Option<Boat> optional) {
    match optional {
        Some(boat) -> println("Got a boat named " + boat.name),
        None       -> println("No boat")
    }

    if let Some(boat) = optional {
        println("Got a boat named " + boat.name)
    }
    // optional.map { |boat| println("Got a boat named " + boat.name) }
}

Option<HttpRequest> parseHttpRequest(string requestLine) {
    return match requestLine.split {
        [method, url, "HTTP/1.1"] -> Some(new HttpRequest(method, url)),
        _ -> None
    }
}

class DbRecord(int key, string data)

Option<DbRecord> getRecord(int key) {
    return match key {
        0 -> None,
        k -> Some(new DbRecord(k, "data"))
    }
}

testOption() {
    match parseHttpRequest("GET /dir/index.html HTTP/1.1") {
        Some(request) -> println(request.method + " " + request.url),
        None          -> println("Could not parse request")
    }

    // The type Option<Boat> can not be inferred from None in declarations. Type
    // needs to be specified.
    let Option<Boat> noBoat = None
    takeOptionBoat(noBoat)

    takeOptionBoat(None)

    // The type Option<Boat> can be inferred from Some(Boat).
    let boat = Some(new Boat("Titanic"))
    takeOptionBoat(boat)

    let result = divide(2.0, 3.0)
    match result {
        Some(x) -> println(x),
        None    -> println("Cannot divide by 0")
    }

    match getRecord(2) {
        Some(record) -> println(record.data),
        None         -> println("No record.")
    }

    let boats = [Some(new Boat("Titanic")), None, Some(new Boat("Unsinkable"))]
    boats.each { |b|
        match b {
            Some(boat) -> println(boat.name),
            None       -> println("No boat")
        }
    }
}

enum Expr {
    X,
    Const(int),
    Add(Box<Expr>, Box<Expr>),
    Mult(Box<Expr>, Box<Expr>),
    Neg(Box<Expr>)
}

int ev(Box<Expr> expression, int xValue) {
    return match expression.value {
        Expr.X            -> xValue,
        Expr.Const(value) -> value,
        Expr.Add(l, r)    -> ev(l, xValue) + ev(r, xValue),
        Expr.Mult(l, r)   -> ev(l, xValue) * ev(r, xValue),
        Expr.Neg(expr)    -> -ev(expr, xValue)
    }
}

testRecursiveEnum() {
    // 1 + 2 * x * x
    let expr = 
        new Box(Expr.Add(new Box(Expr.Const(1)), 
                         new Box(Expr.Mult(new Box(Expr.Const(2)), 
                                           new Box(Expr.Mult(new Box(Expr.X), 
                                                             new Box(Expr.X)
                                                            )
                                                  )
                                          )
                                )
                        )
               )

    println(ev(expr, 3))
}

class EnumTest {
    Animal member = Animal.Dog

    run() {
        println("----------[Enum Test]----------")

        let animal = Animal.Cat
        match animal {
            Animal.Dog -> println("Dog"),
            Animal.Cat -> println("Cat"),
            Animal.Dolphin -> println("Dolphin")
        }

        var dogDolphin = Animal.Dolphin
        dogDolphin = Animal.Dog

        let a = Animal.Cat
        match a {
            Animal.Cat -> println("Cat"),
            Animal.Dolphin -> println("Dolphin"),
            _ -> println("Not Cat or Dolphin")
        }

        if let Animal.Dog = member {
            println("member is Dog")
        }

        let shape = Shape.Square(3)
        match shape {
            Shape.Rectangle(_, 2) -> println("Rectangle height: 2"),
            Shape.Square(side) -> println(side),
            _ -> {}
        }

        match Shape.Rectangle(4, 2) {
            Shape.Rectangle(_, 2) -> println("Rectangle height: 2"),
            Shape.Square(side) -> println(side),
            _ -> {}
        }

        println(rectangularArea(Shape.Rectangle(2, 4)))
        println(rectangularArea(Shape.Point))

        /*
        let plant = Plant.Tree
        match plant {
            Animal.Dog -> println("Dog"),  // Should be an error.
            _ ->
        }
        */

        testExposedEnum
        testOption
        testRecursiveEnum
    }
}

// ----------------------------------------------------------------------------

class IoTest {
    run() {
        println("----------[I/O Test]----------")

        let text = "Some text in a file.\nLine2"
        File.open("file_test", "w+") { |file|
            file.write(text)
        }

        File.open("file_test", "r") { |file|
            print(file.readLine)
        }

        let size = 10000
        File.open("big_file", "w+") { |file|
            file.write(makeBigString(size))
        }

        File.open("big_file", "r") { |file|
            print("Big file size (1)=")
            println(file.read(size + 10).length)
        }

        File.open("big_file", "r") { |f|
            print("Big file size (2)=")
            println(f.readAll.length)
        }

        if File.exists("file_test") {
            println("File exists.")
        }

        let socket = TcpSocket.create
        socket.connect("www.google.com", 80)
        socket.write("GET /index.html HTTP/1.1\r\nHost: www.google.com\r\n\r\n")
        print(socket.readLine)
        socket.close

        TcpSocket.open("www.google.com", 80) { |socket|
            socket.write(
                "GET /index.html HTTP/1.1\r\nHost: www.google.com\r\n\r\n")
            print(socket.readLine)
        }

        /*
        println("readLine:")
        println(Console.readLine)

        println("readInt:")
        println(Console.readInt * 2)

        println("readFloat:")
        println(Console.readFloat * (float) 3.1415)
        */
    }

    string makeBigString(int size) {
        var chars = new char[size]
        for var i = 0; i < size; i++ {
            chars.append('a')
        }
        return new string(chars)
    }
}

// ----------------------------------------------------------------------------

class TraceTest {
    run() {
        println("----------[Trace Test]----------")
        print('c')
        print('\n')
        print(1234)
        print(true)
        println("Hello")
        println('c')
        println(1234)
        println(false)
    }
}

// ----------------------------------------------------------------------------

process EchoServer {
    static var int staticMember = 1
    int nonStaticMember = 3
    
    string echo(string str) {
        println(++staticMember)
        return "From " + Convert.toStr(Process.getPid) + ": " + str
    }

    sleep(int milliseconds) {
        Process.sleep(milliseconds)
        Process.terminate
    }
}

// ------------------------------------

process interface DoubleResultHandler {
    handleResult(int n, int result)
}

process DoubleServer {
    doubleNumber(int n, DoubleResultHandler resultHandler) {
        resultHandler.handleResult(n, n + n)
    }
}

process DoubleClient: DoubleResultHandler {
    run() {
        let server = new DoubleServer
        server.doubleNumber(3, this)
    }

    handleResult(int n, int result) {
        println(Convert.toStr(n) + " times 2 is " + Convert.toStr(result))
        Process.terminate
    }
}

// ------------------------------------

process AsyncProcess {
    doubleNumber(int n, AsyncProcess resultHandler) {
        println(Convert.toStr(Process.getPid) + ": doubleNumber")
        resultHandler.handleResult(n, n + n)
    }

    run() {
        let sibling = new AsyncProcess
        sibling.doubleNumber(9, this)
    }

    handleResult(int n, int result) {
        println(Convert.toStr(Process.getPid) + ": handleResult: " +
                Convert.toStr(n) + " times 2 is " + Convert.toStr(result))
        Process.terminate
    }
}

// ------------------------------------

class DoubleClientHelper: DoubleResultHandler {
    run() {
        let server = new DoubleServer
        server.doubleNumber(4, this)
    }

    handleResult(int n, int result) {
        println(Convert.toStr(n) + " times 2 is " + Convert.toStr(result))
        Process.terminate
    }
}

process DoubleClientWithHelper {
    run() {
        let helper = new DoubleClientHelper
        helper.run
    }
}

// ------------------------------------

process CounterServer {
    var int n

    increaseCounter() {
        n++
        if n == 100 {
            println(Process.getPid)
            Process.terminate
        }
    }
}

process CounterClient {
    run() {
        let server = new CounterServer
        for var int i = 0; i < 100; i++ {
            server.increaseCounter
        }
        server.wait
        Process.terminate
    }
}

// ------------------------------------

class CoreProcessTest {
    performanceTest() {
        println("Performance test start.")
        let numClients = 1
        var clients = new CounterClient[numClients]
        for var i = 0; i < numClients; i++ {
            let client = new CounterClient
            client.run
            clients.append(client)
        }
        clients.each { |client|
            client.wait
        }
        println("Performance test done!")
    }

    synchronousProcessCallTest() {
        let server = new EchoServer
        println(server.echo("echo test1"))
        server.sleep(1000)
        server.wait
    }

    namedProcessTest() {
        let server = new EchoServer named "TestServer"
        println(server.echo("echo test"))
        let sameServer = new EchoServer named "TestServer"
        println(sameServer.echo("echo test"))
        server.sleep(10)
        server.wait
    }

    asynchronousProcessCallTest1() {
        let client = new DoubleClient
        client.run
        client.wait
    }

    asynchronousProcessCallTest2() {
        let asyncProcess = new AsyncProcess
        asyncProcess.run
        asyncProcess.wait
    }

    asynchronousProcessCallTest3() {
        let client = new DoubleClientWithHelper
        client.run
        client.wait
    }

    run() {
        println("----------[Core Process Test]----------")

        synchronousProcessCallTest
        namedProcessTest
        asynchronousProcessCallTest1
        asynchronousProcessCallTest2
        asynchronousProcessCallTest3

        performanceTest
    }
}

// ----------------------------------------------------------------------------
// Stdlib Process Test
// ----------------------------------------------------------------------------
//
// Test 1. Spawn nameless process. Scenario:
//
// process EchoProcess {
//     string echo(string str) {
//         return str
//     }
//
//     sleep(int milliseconds) {
//         Process.sleep(milliseconds)
//         Process.terminate
//     }
// }
//
// class Main {
//     static main() {
//         let server = new EchoProcess
//         println(server.echo("test"))
//         server.sleep(1000)
//         server.wait
//     }
// }
//
// Code that should be generated:
//

interface EchoProcess {
    string echo(string str)
    sleep(int milliseconds)

    // Common process methods.
    wait()
}

interface EchoProcess_Call {
    call(Message message, EchoProcess processInstance)
}

// Class that encapsulates the arguments to the echo method.
class EchoProcess_echo_Call: EchoProcess_Call {
    int sourcePid
    string str

    init(int pid, string arg1) {
        sourcePid = pid
        str = arg1
    }

    call(Message message, EchoProcess processInstance) {
        let retval = processInstance.echo(str)
        Process.send(sourcePid, message.createMethodResult(retval))
    }
}

// Class that encapsulates the arguments to the sleep method.
class EchoProcess_sleep_Call: EchoProcess_Call {
    int milliseconds

    init(int arg1) {
        milliseconds = arg1
    }

    call(Message message, EchoProcess processInstance) {
        processInstance.sleep(milliseconds)
    }
}

// Class that implements the EchoProcess process.
class EchoProcess_MessageHandler: EchoProcess, MessageHandler {

    // The generated code should be like this, but unchecked downcasts are only
    // allowed in the generated code.
    //
    // handleMessage(Message message) {
    //     ((EchoProcess_Call) message.data).call(message, this)
    // }
    handleMessage(Message message) {
        if let EchoProcess_Call epCall = message.data {
             epCall.call(message, this)
        }
    }

    string echo(string str) {
        return str
    }

    sleep(int milliseconds) {
        Process.sleep(milliseconds)
        Process.terminate
    }

    wait() {
    }
}

class EchoProcess_MessageHandlerFactory: MessageHandlerFactory {
    MessageHandler createMessageHandler() {
        return new EchoProcess_MessageHandler
    }
}

// EchoProcess proxy class.
class EchoProcess_Proxy: EchoProcess {
    int pid

    init() {
        pid = Process.spawn(new EchoProcess_MessageHandlerFactory)
    }

    init(string name) {
        pid = Process.spawn(new EchoProcess_MessageHandlerFactory, name)
    }

    // The generated code should be like this, but unchecked downcasts are only
    // allowed in the generated code.
    //
    // string echo(string str) {
    //     let message = new Message(new EchoProcess_echo_Call(Process.getPid,
    //                                                         str))
    //     Process.send(pid, message)
    //     return ((string) Process.receiveMethodResult(message.id).data).value
    // }
    string echo(string str) {
        let message = new Message(new EchoProcess_echo_Call(Process.getPid,
                                                            str))
        Process.send(pid, message)
        return match Process.receiveMethodResult(message.id).data {
            string retval -> retval,
            _ -> ""
        }
    }

    sleep(int milliseconds) {
        let message = new Message(new EchoProcess_sleep_Call(milliseconds))
        Process.send(pid, message)
    }

    wait() {
        Process.wait(pid)
    }
}

// ----------------------------------------------------------------------------
//
// Test 2. Spawn named process. Scenario:
//
// process EchoServer {
//     string echo(string message) {
//         return message
//     }
//
//     sleep(int milliseconds) {
//         Process.sleep(milliseconds)
//         Process.terminate
//     }
// }
//
// class Main {
//     static main() {
//         let server = new EchoServer named "Echo"
//         print(server.echo("Echo this"))
//         server.sleep(30)
//         server.wait
//     }
// }
//
// Same generated code as in Test 1.
//

// ----------------------------------------------------------------------------
//
// Test 3. Passing a process interface reference to another process. Scenario:
//
// process interface SquareResultHandler {
//     handleResult(int n, int result)
// }
//
// process SquareServer {
//     square(int n, SquareResultHandler resultHandler) {
//         resultHandler.handleResult(n, n * n)
//     }
// }
//
// process SquareClient: SquareResultHandler {
//     run() {
//         let server = new SquareServer
//         server.square(3, this)
//     }
//
//     handleResult(int n, int result) {
//         println("Square of " + Convert.toStr(n) + " is " +
//                 Convert.toStr(result))
//         Process.terminate
//     }
// }
//
// class Main {
//     static main() {
//         let client = new SquareClient
//         client.run
//         client.wait
//     }
// }
//
// Code that should be generated:
//

// ------------------------------------
// process interface SquareResultHandler
// ------------------------------------

interface SquareResultHandler {
    handleResult(int n, int result)
    SquareResultHandler getSquareResultHandler_Proxy()
}

interface SquareResultHandler_Call {
    call(Message message, SquareResultHandler processInstance)
}

// Class that encapsulates the arguments to the handleResult method.
class SquareResultHandler_handleResult_Call: SquareResultHandler_Call {
    int n
    int result

    init(int arg1, int arg2) {
        n = arg1
        result = arg2
    }

    call(Message message, SquareResultHandler processInstance) {
        processInstance.handleResult(n, result)
    }
}

// SquareResultHandler proxy class.
class SquareResultHandler_Proxy: SquareResultHandler {
    int pid
    int messageHandlerId
    int interfaceId

    init(int processId, int handlerId, int ifId) {
        pid = processId
        messageHandlerId = handlerId
        interfaceId = ifId
    }

    handleResult(int n, int result) {
        let message = new Message(
            messageHandlerId,
            interfaceId,
            new SquareResultHandler_handleResult_Call(n, result))
        Process.send(pid, message)
    }

    SquareResultHandler getSquareResultHandler_Proxy() {
        return this
    }
}

// ------------------------------------
// process SquareServer
// ------------------------------------

interface SquareServer {
    square(int n, SquareResultHandler resultHandler)

    // Common process methods.
    wait()
}

interface SquareServer_Call {
    call(Message message, SquareServer processInstance)
}

// Class that encapsulates the arguments to the square method.
class SquareServer_square_Call: SquareServer_Call {
    int n
    SquareResultHandler resultHandler

    init(int arg1, SquareResultHandler arg2) {
        n = arg1
        resultHandler = arg2
    }

    call(Message message, SquareServer processInstance) {
        processInstance.square(n, resultHandler)
    }
}

// Class that implements the SquareServer process.
class SquareServer_MessageHandler: SquareServer, MessageHandler {

    // The generated code should be like this, but unchecked downcasts are only
    // allowed in the generated code.
    //
    // handleMessage(Message message) {
    //     ((SquareServer_Call) message.data).call(message, this)
    // }
    handleMessage(Message message) {
        if let SquareServer_Call ssCall = message.data {
             ssCall.call(message, this)
        }
    }

    square(int n, SquareResultHandler resultHandler) {
        resultHandler.handleResult(n, n * n)
    }

    wait() {
    }
}

class SquareServer_MessageHandlerFactory: MessageHandlerFactory {
    MessageHandler createMessageHandler() {
        return new SquareServer_MessageHandler
    }
}

// SquareServer proxy class.
class SquareServer_Proxy: SquareServer {
    int pid

    init() {
        pid = Process.spawn(new SquareServer_MessageHandlerFactory)
    }

    init(string name) {
        pid = Process.spawn(new SquareServer_MessageHandlerFactory, name)
    }

    square(int n, SquareResultHandler resultHandler) {
        let message = new Message(
            new SquareServer_square_Call(
                n,
                resultHandler.getSquareResultHandler_Proxy))
        Process.send(pid, message)
    }

    wait() {
        Process.wait(pid)
    }
}

// ------------------------------------
// process SquareClient: SquareResultHandler
// ------------------------------------

interface SquareClient: SquareResultHandler {
    run()

    // Common process methods.
    wait()
}

interface SquareClient_Call {
    call(Message message, SquareClient processInstance)
}

// Class that encapsulates the arguments to the run method.
class SquareClient_run_Call: SquareClient_Call {
    call(Message message, SquareClient processInstance) {
        processInstance.run
    }
}

class SquareClient_InterfaceId {
    static int SquareClientId = 0
    static int SquareResultHandlerId = 1
}

// Class that implements the SquareClient process.
class SquareClient_MessageHandler: SquareClient, MessageHandler {

    // The generated code should be like this, but unchecked downcasts are only
    // allowed in the generated code.
    //
    // handleMessage(Message message) {
    //     match message.interfaceId {
    //         SquareClient_InterfaceId.SquareClientId ->
    //             ((SquareClient_Call) message.data).call(message, this),
    //         SquareClient_InterfaceId.SquareResultHandlerId ->
    //             ((SquareResultHandler_Call) message.data).call(message, this),
    //         _ ->
    //             {} // Do nothing.
    //     }
    // }
    handleMessage(Message message) {
        match message.interfaceId {
            SquareClient_InterfaceId.SquareClientId -> {
                if let SquareClient_Call scCall = message.data {
                    scCall.call(message, this)
                }
            },
            SquareClient_InterfaceId.SquareResultHandlerId -> {
                if let SquareResultHandler_Call srhCall = message.data {
                    srhCall.call(message, this)
                }
            },
            _ ->
                {} // Do nothing.
        }
    }

    SquareResultHandler getSquareResultHandler_Proxy() {
        return new SquareResultHandler_Proxy(
            Process.getPid,
            0,
            SquareClient_InterfaceId.SquareResultHandlerId)
    }

    wait() {
    }

    run() {
        let server = new SquareServer_Proxy
        server.square(3, this)
    }

    handleResult(int n, int result) {
        println("Square of " + Convert.toStr(n) + " is " +
                Convert.toStr(result))
        Process.terminate
    }
}

class SquareClient_MessageHandlerFactory: MessageHandlerFactory {
    MessageHandler createMessageHandler() {
        return new SquareClient_MessageHandler
    }
}

// SquareClient proxy class.
class SquareClient_Proxy: SquareClient {
    int pid

    init() {
        pid = Process.spawn(new SquareClient_MessageHandlerFactory)
    }

    init(string name) {
        pid = Process.spawn(new SquareClient_MessageHandlerFactory, name)
    }

    run() {
        let message = new Message(new SquareClient_run_Call)
        Process.send(pid, message)
    }

    handleResult(int n, int result) {
        let message = new Message(
            0,
            SquareClient_InterfaceId.SquareResultHandlerId,
            new SquareResultHandler_handleResult_Call(n, result))
        Process.send(pid, message)
    }

    SquareResultHandler getSquareResultHandler_Proxy() {
        return this
    }

    wait() {
        Process.wait(pid)
    }
}

// ----------------------------------------------------------------------------
//
// Test 4. Regular classes implementing process interfaces. Scenario:
//
// process interface SquareResultHandler {
//     handleResult(int n, int result)
// }
//
// process SquareServer {
//     square(int n, SquareResultHandler resultHandler) {
//         resultHandler.handleResult(n, n * n)
//     }
// }
//
// class SquareClientHelper: SquareResultHandler {
//     run() {
//         let server = new SquareServer
//         server.square(4, this)
//     }
//
//     handleResult(int n, int result) {
//         println("Square of " + Convert.toStr(n) + " is " +
//                 Convert.toStr(result))
//         Process.terminate
//     }
// }
//
// process SquareClient4 {
//     run() {
//         let helper = new SquareClientHelper
//         helper.run
//     }
// }
//
// class Main {
//     static main() {
//         let client = new SquareClient4
//         client.run
//         client.wait
//     }
// }
//
// Code that should be generated:
//

// ------------------------------------
// class SquareClientHelper: SquareResultHandler
// ------------------------------------

class SquareClientHelper: SquareResultHandler, MessageHandler {
    int messageHandlerId

    init() {
        messageHandlerId = Process.registerMessageHandler(this)
    }

    // The generated code should be like this, but unchecked downcasts are only
    // allowed in the generated code.
    //
    // handleMessage(Message message) {
    //     ((SquareResultHandler_Call) message.data).call(message, this)
    // }
    handleMessage(Message message) {
        if let SquareResultHandler_Call srhCall = message.data {
             srhCall.call(message, this)
        }
    }

    SquareResultHandler getSquareResultHandler_Proxy() {
        return new SquareResultHandler_Proxy(Process.getPid,
                                             messageHandlerId,
                                             0)
    }

    wait() {
    }

    run() {
        let server = new SquareServer_Proxy
        server.square(4, this)
    }

    handleResult(int n, int result) {
        println("SquareClientHelper: Square of " + Convert.toStr(n) + " is " +
                Convert.toStr(result))
        Process.terminate
    }
}

// ------------------------------------
// process SquareClient4
// ------------------------------------

interface SquareClient4 {
    run()

    // Common process methods.
    wait()
}

interface SquareClient4_Call {
    call(Message message, SquareClient4 processInstance)
}

// Class that encapsulates the arguments to the run method.
class SquareClient4_run_Call: SquareClient4_Call {
    call(Message message, SquareClient4 processInstance) {
        processInstance.run
    }
}

// Class that implements the SquareClient4 process.
class SquareClient4_MessageHandler: SquareClient4, MessageHandler {

    // The generated code should be like this, but unchecked downcasts are only
    // allowed in the generated code.
    //
    // handleMessage(Message message) {
    //     ((SquareClient4_Call) message.data).call(message, this)
    // }
    handleMessage(Message message) {
        if let SquareClient4_Call scCall = message.data {
             scCall.call(message, this)
        }
    }

    run() {
        let helper = new SquareClientHelper
        helper.run
    }

    wait() {
    }
}

class SquareClient4_MessageHandlerFactory: MessageHandlerFactory {
    MessageHandler createMessageHandler() {
        return new SquareClient4_MessageHandler
    }
}

// SquareClient4 proxy class.
class SquareClient4_Proxy: SquareClient4 {
    int pid

    init() {
        pid = Process.spawn(new SquareClient4_MessageHandlerFactory)
    }

    init(string name) {
        pid = Process.spawn(new SquareClient4_MessageHandlerFactory, name)
    }

    run() {
        let message = new Message(new SquareClient4_run_Call)
        Process.send(pid, message)
    }

    wait() {
        Process.wait(pid)
    }
}

// ----------------------------------------------------------------------------

class StdlibProcessTest {
    run() {
        println("----------[Stdlib Process Test]----------")

        test1
        test2
        test3
        test4

        println("done")
    }

    test1() {
        let server = (EchoProcess) new EchoProcess_Proxy
        println(server.echo("echo test1"))
        server.sleep(1000)
        server.wait
    }

    test2() {
        let server = (EchoProcess) new EchoProcess_Proxy("EchoServer")
        println(server.echo("echo test2"))
        server.sleep(30)
        server.wait
    }

    test3() {
        let client = (SquareClient) new SquareClient_Proxy
        client.run
        client.wait
    }

    test4() {
        let client = (SquareClient4) new SquareClient4_Proxy
        client.run
        client.wait
    }
}

// ----------------------------------------------------------------------------

main() {
    runExamples

    //
    // Core language tests.
    //
    let constructorTest = new ConstructorTest
    constructorTest.run

    let inheritanceTest = new InheritanceTest
    inheritanceTest.run

    let genericsTest = new GenericsTest
    genericsTest.run

    let whileTest = new WhileTest
    whileTest.run

    let constTest = new ConstantAndVariableDeclarationTest
    constTest.run

    let expressionTest = new ExpressionTest
    expressionTest.run

    let lambdaTest = new LambdaTest
    lambdaTest.run

    let arrayTest = new ArrayTest
    arrayTest.run

    let parseTest = new ParseTest
    parseTest.run

    let matchTest = new MatchTest
    matchTest.run

    let typeCastTest = new TypeCastTest
    typeCastTest.run

    let enumTest = new EnumTest
    enumTest.run

    let coreProcessTest = new CoreProcessTest
    coreProcessTest.run

    //
    // Stdlib tests.
    //
    let stringTest = new StringTest
    stringTest.run

    let ioTest = new IoTest
    ioTest.run

    let traceTest = new TraceTest
    traceTest.run

    let stdlibProcessTest = new StdlibProcessTest
    stdlibProcessTest.run
}

