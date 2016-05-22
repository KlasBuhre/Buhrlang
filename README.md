# Buhrlang
Buhrlang is an object-oriented, functional and concurrent programming language.

﻿-------------------------------------------------------------------------------
Contents
-------------------------------------------------------------------------------
 - Motivation for a new language
 - Some features
 - Type system
 - Object-oriented features
 - Generic types
 - Functional features
 - Enumerations
 - Concurrency

-------------------------------------------------------------------------------
Motivation for a new language
-------------------------------------------------------------------------------
Many existing languages used in the server application domain have limitations 
that are hard to overcome without introducing non-backward compatible changes. 
Such limitations include:
 - Data is mutable by default in C++, Java, C# etc.
 - Null pointers.
 - Specific to C++: memory management is error-prone and there are implicit 
   conversions between unrelated types.
 - Race conditions in concurrent applications is a common problem.
 - Many languages have grown to be very complicated.

Most of these limitations can be addressed from the beginning in a new language. 
Therefore, the goal was to design a simple, safe and concurrent language used 
for developing large server applications. Also, the language should look
familiar to most developers and it should be easy to learn.

-------------------------------------------------------------------------------
Some features
-------------------------------------------------------------------------------
 - Data is immutable by default
 - All pointers point to a valid object. There are no null pointers
 - Light-weight processes that act as objects
 - Type inference
 - Pattern matching
 - Classes and interfaces
 - Tagged unions (enumerations)
 - Generics

-------------------------------------------------------------------------------
The basics – hello world
-------------------------------------------------------------------------------
    import "Trace"
    
    main() {
        println("Hello World!")
    }

-------------------------------------------------------------------------------
Type System – Type inference
-------------------------------------------------------------------------------
The compiler can infer the type of variables that are initialized when 
declared.

Example:

    typeInferenceExample() {
        let i = 42
        let pi = 3.1415
        var b = true
        b = false  // OK: b is not a constant.
        let hello = ”Hello”
        let characters = [’a’, ’b’, ’c’]
        let book = new Book
        let u      // Error: Implicitly typed variables must be initialized. 
    }

-------------------------------------------------------------------------------
Type System – Static typing
-------------------------------------------------------------------------------
The types of all variables/objects must be known at compile-time.
All data is immutable (write-protected) by default.
 - Write-protection can be overriden.

Example:

    staticTypesExample() {
        let int i = 42
        let bool b = true
        b = false    // Error: Cannot change the value of a constant.
        var bool v = true
        v = false    // OK: v is not a constant.
    }

-------------------------------------------------------------------------------
Type System – Memory safety
-------------------------------------------------------------------------------
Local variables of value type are stored on the stack while objects of 
reference type are stored on the heap.
 - Heap objects are disposed automatically (currently by automatic reference 
   counting, later by garbage collector).
 - Automatic bounds checking at run-time for arrays.

Example:

    memorySafetyExample() {
        let i = 42                    // On the stack.
        let book = new Book           // On the heap.
        let letters = [’a’, ’b’, ’c’] // On the heap.
        let c = letters[3]            // Run-time error!
    }

-------------------------------------------------------------------------------
Object-Oriented Features - classes
-------------------------------------------------------------------------------
Classes are reference types. Constructors are defined using the 'init' keyword.
 - All members are public by default but can be made private.
 - No protected members.

Example:

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

Using compact constructors, the previous example could have been written:

    class Book(string author, string title, float price) {
        let pages = new Vector<string>

        string toString() {
            return author + ": " + title + ". Price: " + Convert.toStr(price)
        }
    }

-------------------------------------------------------------------------------
Object-Oriented Features - Inheritance
-------------------------------------------------------------------------------
A class can only inherit from one concrete class. In the following example, 
note the 'arg' keyword, which indicates that the 'mass' property is not a 
member of Truck. It is only a constructor argument which is sent to the 
constructor of Vehicle.

Example:

    class Vehicle(int mass)

    class Truck(int power, arg int mass): Vehicle(mass) {
        let wheels = new List<Wheel>
    }

-------------------------------------------------------------------------------
Object-Oriented Features - Interfaces
-------------------------------------------------------------------------------
A class can implement many interfaces.

Example:

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

        handleCallback(Result result) {}
    }

-------------------------------------------------------------------------------
Generic types
-------------------------------------------------------------------------------
Generic types are classes that take type variables at compile-time.
The value of a type variable can be any type.

Example:

    class Vector<T> {
        var T[] data

        ...

        add(T element) {
            ...
        }
    }

    let books = new Vector<Book>
    books.add(new Book("Stroustrup", "The C++ Programming Language", 399.50))
    books.add(new Book("Buhre", "Programming for Monkeys", 599.50))
    books.add(new Book("Kurzweil", "How to Create a Mind", 299.50))

-------------------------------------------------------------------------------
Functional Features - Lambda expressions
-------------------------------------------------------------------------------
Lambda expressions are functions that do not have names.
A lambda expression can be passed to other functions as arguments.

Example:

    books.each |book| {
        println(book.toString)
    }
    // Prints:
    // Stroustrup: The C++ Programming Language. Price: 399.500000
    // Buhre: Programming for Monkeys. Price: 599.500000
    // Kurzweil: How to Create a Mind. Price: 299.500000

    let cheapBooks = books.filter(|book| { book.price < 400.0 })

    cheapBooks.each |book| {
        println(book.toString)
    }
    // Prints:
    // Stroustrup: The C++ Programming Language. Price: 399.500000
    // Kurzweil: How to Create a Mind. Price: 299.500000

Usages of lambda expressions 
 - Iterating over collections
 - Automatic resource management
 - Higher-order functions

Example:

    let vector = new Vector<string>
    vector.add("element 1")
    ...
    vector.eachWithIndex |element, index| {
        if index == 2 {
            break
        }
        println(element)
    }

    File.open("file_test", "r") |file| {
        print(file.readLine)
        ...
    } // File is closed.

-------------------------------------------------------------------------------
Functional Features - Pattern matching
-------------------------------------------------------------------------------
Matching operands with sequences of patterns can be a powerful alternative to 
if/else statements. One way of applying pattern matching in this language is to
use a match expression. A match expression looks a little bit like a switch 
statement, however there are a couple of differences. For example, a match 
expression can return a value. In the following example, 'y' will be the 
string value returned from the match expression:

    let x = 4
    let y = match x {
        1     -> "One",
        2     -> "Two",
        3 | 4 -> "Three or four", // Match.
        _     -> "Don’t care"
    }

You can match against arrays ('..' means any number of elements) : 

    let a = [1, 2, 3, 4]
    match a {
        [1, second, _]  -> println(second),
        [1, second, ..] -> println(second),  // Match.    
        _ ->   // Do nothing.
    }

Patterns can also match classes if the classes have compact constructors.

Example:

    class Point(int x, int y)

    testConstructorMatching() {
        let p = new Point(2, 5)    
        match p {
            Point(2, 3) -> println("x is 2 and y is 3"),
            Point(2, y) -> println("y is " + Convert.toStr(y)),
            Point(5)    -> println("y is 5"),
            Point(x, y) -> println(x + y)
        }
    }

It is possible to match with different types.

Example:

    interface Expression
    class X: Expression
    class Const(int value): Expression
    class Add(Expression left, Expression right): Expression
    class Mult(Expression left, Expression right): Expression
    class Neg(Expression expr): Expression

    int eval(Expression expression, int xValue) {
        return match expression {
            X            -> xValue,
            Const(value) -> value,
            Add(l, r)    -> eval(l, xValue) + eval(r, xValue),
            Mult(l, r)   -> eval(l, xValue) * eval(r, xValue),
            Neg(expr)    -> -eval(expr, xValue),
            _            -> 0
        }
    }

    testSubclassMatching() {   
        // 1 + 2 * x * x
        let expr = new Add(new Const(1), 
                           new Mult(new Const(2), new Mult(new X, new X)))
        println(eval(expr, 3)) // Prints: 19
    }

Since downcasting are unsafe operations which fail if the type is not the 
requested one, downcasts have to be done using pattern matching.

Example:

    let fruit = (Fruit) new Apple  // Upcast.
    println(fruit.name)

    let Fruit someFruit = new Orange
    if let Orange orange = someFruit { // Downcast.
        println(orange.name)
    }

    match fruit {
        Apple apple   -> apple.bite,
        Orange orange -> orange.peel,
        Peach _       -> println("Some peach"),
        _             -> {}
    }

-------------------------------------------------------------------------------
Enumerations
-------------------------------------------------------------------------------
Enumeration variants can have data. Such data can only be accessed through 
pattern matching. Unlike class objects which are of reference type, enums are 
value type objects. Enumerations are implemented as tagged unions. 

Example:

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

    let area = rectangularArea(Shape.Rectangle(2, 4)) 

Enumerations can take generic type parameters. This allows us to create an enum
that can represent a value or the absence of a value.

Example:

    enum Option<T> {
        Some(T),
        None
    }

    // Expose the contents of the Option namespace.
    use Option

The option type can then be used instead of null pointers. In this language, 
all pointers are valid. There is no support for null pointers.

Example:

    Option<HttpRequest> parseHttpRequest(string requestLine) {
        return match requestLine.split {
            [method, url, "HTTP/1.1"] -> Some(new HttpRequest(method, url)),
            _ -> None
        }
    }

    testOption() {
        match parseHttpRequest("GET /dir/index.html HTTP/1.1") {
            Some(request) -> println(request.method + " " + request.url),
            None          -> println("Could not parse request")
        }
    }

Instead of using a match expression when checking for the presence of a value,
we can use an 'if let' statement.

Example:

    if let Some(request) = parseHttpRequest("GET /dir/index.html HTTP/1.1") {
        println(request.method + " " + request.url)
    }

-------------------------------------------------------------------------------
Concurrency
-------------------------------------------------------------------------------
Concurrency is implemented using light-weight processes (referred to as just 
”process”). These processes are not native OS processes.
 - Many processes execute within one native OS process.
 - No data is shared between processes. Communications between processes is 
   done by remote method calls. 
 - Remote method calls are implemented by message passing.
 - Message passing code is generated automatically.
 - Arguments are cloned in most cases since data cannot be shared between 
   processes.

Process types are classes
 - Instantiating an object of process type spawns a new process.
 - When one process calls a remote method then a message is sent to the other 
   process.
 - Calling a remote method that returns a value results in a blocking call 
   because the return value is sent to the calling process as a message.

Example:

    process EchoProcess {
        string echo(string str) {
            return str
        }
    }

    main() {
        let server = new EchoProcess // Spawns a process of type EchoProcess.
        println(server.echo("test")) // Blocks until we receive a message.
    }

Calling a remote method that does not return a value results in a non-blocking 
call.

Example:

    process HttpWorker {
        handleConnection(TcpSocket socket) {
            let request = HttpRequest.receive(socket)
            let response = match request.method {
                "GET"  -> handleGet(request),
                _      -> new HttpResponse("405", "Not Allowed")
            }
            response.send(socket)
        }

        HttpResponse handleGet(HttpRequest request) { ... }
    }

    main() {
        let listenerSocket = TcpSocket.createListener(8080)
        while {
            let worker = new HttpWorker
            worker.handleConnection(listenerSocket.accept)
        }
    }

A process instance can have a name.
 - The name must be unique.
 - Other processes can use the name to lookup the named process.

Example:

    namedProcessTest() {
        let server = new EchoServer named "TestServer"
        println(server.echo("echo test"))
        let sameServer = new EchoServer named "TestServer"
        println(sameServer.echo("echo test"))
        server.sleep(10)
        server.wait
    }

-------------------------------------------------------------------------------
Concurrency – process interfaces
-------------------------------------------------------------------------------
A process can implement process interfaces. When an object of process type or 
process interface type is sent as an argument in a remote method call between 
processes then the argument is not cloned as in the case of normal objects.
 - Instead a handle to the process object is sent.
 - A handle acts as a reference to a remote process object.
 - A process can pass ‘this’ as a process handle.
 - This allows processes to communicate asynchronously through callbacks.

Example:

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
            println(Convert.toStr(n) + " * 2 = " + Convert.toStr(result))
        }
    }

A regular class can also implement process interfaces.

Example:

    class DoubleClientHelper: DoubleResultHandler {
        run() {
            let server = new DoubleServer
            server.doubleNumber(4, this)
        }

        handleResult(int n, int result) {
            println(Convert.toStr(n) + " * 2 is " + Convert.toStr(result))
            Process.terminate
        }
    }

    process DoubleClientWithHelper {
        run() {
            let helper = new DoubleClientHelper
            helper.run
        }
    }

-------------------------------------------------------------------------------


