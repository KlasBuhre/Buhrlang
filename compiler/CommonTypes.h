#ifndef CommonTypes_h
#define CommonTypes_h

#include <string>
#include <vector>
#include <list>
#include <assert.h>

class Type;

namespace Keyword {
    enum Kind {
        None,
        Object,
        Class,
        Interface,
        Process,
        Named,
        Init,
        Private,
        Static,
        Arg,
        Byte,
        Char,
        Int,
        Float,
        String,
        Enum,
        If,
        Else,
        Bool,
        True,
        False,
        While,
        For, 
        Break,
        Continue,
        Var,
        Let,
        Return,
        New,
        This,
        Import,
        Use,
        Native,
        Yield,
        Match,
        Jump
    };

    bool isType(Kind keyword);
    Type* toType(Kind keyword);

    extern const std::string classString;
    extern const std::string interfaceString;
    extern const std::string processString;
    extern const std::string namedString;
    extern const std::string initString;
    extern const std::string objectString;
    extern const std::string privateString;
    extern const std::string staticString;
    extern const std::string argString;
    extern const std::string byteString;
    extern const std::string charString;
    extern const std::string intString;
    extern const std::string floatString;
    extern const std::string stringString;
    extern const std::string enumString;
    extern const std::string ifString;
    extern const std::string elseString;
    extern const std::string boolString;
    extern const std::string trueString;
    extern const std::string falseString;
    extern const std::string whileString;
    extern const std::string forString;
    extern const std::string breakString;
    extern const std::string continueString;
    extern const std::string varString;
    extern const std::string letString;
    extern const std::string returnString;
    extern const std::string newString;
    extern const std::string thisString;
    extern const std::string importString;
    extern const std::string useString;
    extern const std::string nativeString;
    extern const std::string yieldString;
    extern const std::string matchString;
    extern const std::string jumpString;
}

namespace AccessLevel {
    enum Kind {
        Public,
        Private
    };
}

namespace Operator {
    enum Kind {
        None,
        Addition,                 // +
        Subtraction,              // -
        Multiplication,           // *
        Division,                 // /
        Increment,                // ++
        Decrement,                // --
        Assignment,               // =
        AssignmentExpression,     // :=
        AdditionAssignment,       // +=
        SubtractionAssignment,    // -=
        MultiplicationAssignment, // *=
        DivisionAssignment,       // /=
        Dot,                      // .
        Comma,                    // ,
        OpenParentheses ,         // (
        CloseParentheses,         // )
        OpenBrace,                // {
        CloseBrace,               // }
        OpenBracket,              // [
        CloseBracket,             // ]
        Equal,                    // ==
        NotEqual,                 // !=
        Greater,                  // >
        Less,                     // <
        GreaterOrEqual,           // >=
        LessOrEqual,              // <=
        LogicalNegation,          // !
        LogicalAnd,               // &&
        LogicalOr,                // ||
        Colon,                    // :
        Semicolon,                // ;
        Question,                 // ?
        VerticalBar,              // |
        Arrow,                    // ->
        Placeholder,              // _
        Wildcard,                 // ..
        Range                     // ...
    };

    enum Precedence {
        NoPrecedence,
        NormalPrecedence,
        AssignmentTo,     // =, :=
        OrOr,             // ||
        AndAnd,           // &&
        EqualNotEqual,    // ==, !=
        GreaterLess,      // >,  <, >=, <=
        OpenClosedRange,  // ...
        AddSubtract,      // +,  -
        MultiplyDivision  // *,  /
    };

    bool isCompoundAssignment(Kind operatorType);
    Kind getDecomposedArithmeticOperator(Kind operatorType);
    Precedence precedence(Kind operatorType);
}

struct Location {
    Location();
    explicit Location(const std::string& fname);

    void stepColumn();
    void stepLine();

    std::string filename;
    const char* ptr;
    int line;
    int column;
};

typedef std::string Identifier;
typedef std::list<Identifier> IdentifierList;

class Node {
public:
    explicit Node(const Location& l) : location(l) {}
    virtual ~Node() {}

    template<class T>
    T* cast() {
        return static_cast<T*>(this);
    }

    template<class T>
    const T* cast() const {
        return static_cast<const T*>(this);
    }

    template<class T>
    T* dynCast() {
        return dynamic_cast<T*>(this);
    }

    template<class T>
    const T* dynCast() const {
        return dynamic_cast<const T*>(this);
    }

    const Location& getLocation() const {
        return location;
    }

private:
    Location location;
};

namespace Utils {
    template<class T>
    void cloneList(T& to, const T& from) {
        for (typename T::const_iterator i = from.begin();
             i != from.end();
             i++) {
            to.push_back((*i)->clone());
        }
    }
}

class VariableDeclaration: public Node {
public:
    VariableDeclaration(Type* t, const Identifier& i, const Location& l);
    VariableDeclaration(Type* t, const Identifier& i);
    VariableDeclaration(const VariableDeclaration& other);

    bool operator==(const VariableDeclaration& other) const;
    std::string toString() const;

    Type* getType() const {
        return type;
    }

    void setType(Type* t) {
        type = t;
    }

    const Identifier& getIdentifier() const {
        return identifier;
    }

    void setIdentifier(const Identifier& i) {
        identifier = i;
    }

    void setIsDataMember(bool m) {
        isMember = m;
    }

    bool isDataMember() const {
        return isMember;
    }

private:
    Type* type;
    Identifier identifier;
    bool isMember;
};

class Expression;

typedef std::list<VariableDeclaration*> ArgumentList;
typedef std::vector<Type*> TypeList;
typedef std::list<Expression*> ExpressionList;

class LambdaSignature {
public:
    explicit LambdaSignature(Type* rt);
    LambdaSignature(const LambdaSignature& other);

    void addArgument(Type* type) {
        arguments.push_back(type);
    }

    TypeList& getArguments() {
        return arguments;
    }

    Type* getReturnType() const {
        return returnType;
    }

    void setReturnType(Type* r) {
        returnType = r;
    }

private:
    TypeList arguments;
    Type* returnType;
};

namespace BuiltInTypes {
    extern const std::string arrayTypeName;
    extern const std::string arrayEachMethodName;
    extern const std::string arrayLengthMethodName;
    extern const std::string arraySizeMethodName;
    extern const std::string arrayCapacityMethodName;
    extern const std::string arrayAppendMethodName;
    extern const std::string arrayAppendAllMethodName;
    extern const std::string arrayConcatMethodName;
    extern const std::string arraySliceMethodName;
    extern const std::string processWaitMethodName;
    extern const std::string boxTypeName;
}

namespace CommonNames {
    extern const Identifier matchSubjectName;
    extern const Identifier enumTagVariableName;
    extern const Identifier messageHandlerTypeName;
}

namespace Symbol {
    Identifier makeUnique(
        const Identifier& name,
        const Identifier& className,
        const Identifier& methodName);
    Identifier makeTemp(int index);
    Identifier makeEnumVariantTagName(const Identifier& variantName);
    Identifier makeEnumVariantDataName(const Identifier& variantName);
    Identifier makeEnumVariantClassName(const Identifier& variantName);
    Identifier makeConvertableEnumName(const Identifier& enumName);
}

namespace Trace {
    void error(const std::string& message, const Location& location);
    void error(const std::string& message, const Node* node); 
    void error(
        const std::string& message,
        const Type* lhs,
        const Type* rhs,
        const Node* node);
    void internalError(const std::string& where);
}

#endif
