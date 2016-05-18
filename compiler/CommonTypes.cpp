
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#include "CommonTypes.h"
#include "Type.h"
#include "File.h"

namespace {
    const Identifier closureTypeName("$Closure");
}

const std::string Keyword::classString("class");
const std::string Keyword::interfaceString("interface");
const std::string Keyword::processString("process");
const std::string Keyword::namedString("named");
const std::string Keyword::messageString("message");
const std::string Keyword::initString("init");
const std::string Keyword::objectString("object");
const std::string Keyword::privateString("private");
const std::string Keyword::staticString("static");
const std::string Keyword::virtualString("virtual");
const std::string Keyword::argString("arg");
const std::string Keyword::byteString("byte");
const std::string Keyword::charString("char");
const std::string Keyword::intString("int");
const std::string Keyword::longString("long");
const std::string Keyword::floatString("float");
const std::string Keyword::stringString("string");
const std::string Keyword::enumString("enum");
const std::string Keyword::funString("fun");
const std::string Keyword::ifString("if");
const std::string Keyword::elseString("else");
const std::string Keyword::boolString("bool");
const std::string Keyword::trueString("true");
const std::string Keyword::falseString("false");
const std::string Keyword::whileString("while");
const std::string Keyword::forString("for");
const std::string Keyword::breakString("break");
const std::string Keyword::continueString("continue");
const std::string Keyword::varString("var");
const std::string Keyword::letString("let");
const std::string Keyword::returnString("return");
const std::string Keyword::newString("new");
const std::string Keyword::thisString("this");
const std::string Keyword::importString("import");
const std::string Keyword::useString("use");
const std::string Keyword::nativeString("native");
const std::string Keyword::yieldString("yield");
const std::string Keyword::matchString("match");
const std::string Keyword::deferString("defer");
const std::string Keyword::jumpString("__jump");

const std::string BuiltInTypes::objectEqualsMethodName("equals");
const std::string BuiltInTypes::objectHashMethodName("hash");
const std::string BuiltInTypes::arrayTypeName("array");
const std::string BuiltInTypes::arrayEachMethodName("each");
const std::string BuiltInTypes::arrayLengthMethodName("length");
const std::string BuiltInTypes::arraySizeMethodName("size");
const std::string BuiltInTypes::arrayCapacityMethodName("capacity");
const std::string BuiltInTypes::arrayAppendMethodName("append");
const std::string BuiltInTypes::arrayAppendAllMethodName("appendAll");
const std::string BuiltInTypes::arrayConcatMethodName("concat");
const std::string BuiltInTypes::arraySliceMethodName("slice");
const std::string BuiltInTypes::processWaitMethodName("wait");
const std::string BuiltInTypes::boxTypeName("Box");

const Identifier CommonNames::cloneableTypeName("_Cloneable");
const Identifier CommonNames::cloneMethodName("_clone");
const Identifier CommonNames::deepCopyMethodName("_deepCopy");
const Identifier CommonNames::messageHandlerTypeName("MessageHandler");
const Identifier CommonNames::matchSubjectName("__match_subject");
const Identifier CommonNames::enumTagVariableName("$tag");
const Identifier CommonNames::otherVariableName("other");
const Identifier CommonNames::callMethodName("call");
const Identifier CommonNames::deferTypeName("Defer");
const Identifier CommonNames::addClosureMethodName("addClosure");

bool Keyword::isType(Kind keyword) {
    switch (keyword) {
        case Object:
        case Byte:
        case Char:
        case Int:
        case Long:
        case Float:
        case String:
        case Bool:
        case Let:
        case Var:
            return true;
        default:
            return false;
    }
}

Type* Keyword::toType(Kind keyword) {
    Type::BuiltInType builtInType;
    switch (keyword) {
        case Keyword::Let:
        case Keyword::Var:
            builtInType = Type::Implicit;
            break;
        case Keyword::Byte:
            builtInType = Type::Byte;
            break;
        case Keyword::Char:
            builtInType = Type::Char;
            break;
        case Keyword::Int:
            builtInType = Type::Integer;
            break;
        case Keyword::Long:
            builtInType = Type::Long;
            break;
        case Keyword::Float:
            builtInType = Type::Float;
            break;
        case Keyword::Bool:
            builtInType = Type::Boolean;
            break;
        case Keyword::String:
            builtInType = Type::String;
            break;
        case Keyword::Object:
            builtInType = Type::Object;
            break;
        default:
            return nullptr;
    }
    return new Type(builtInType);
}

bool Operator::isCompoundAssignment(Kind operatorType) {
    return getDecomposedArithmeticOperator(operatorType) != None;
}

Operator::Kind Operator::getDecomposedArithmeticOperator(
    Kind operatorType) {

    switch (operatorType) {
        case AdditionAssignment:
            return Addition;
        case SubtractionAssignment:
            return Subtraction;
        case MultiplicationAssignment:
            return Multiplication;
        case DivisionAssignment:
            return Division;
        default:
            return None;
    }
}

Operator::Precedence Operator::precedence(Kind operatorType) {
    switch (operatorType) {
        case Multiplication:
        case Division:
        case Modulo:
            return MultiplyDivision;
        case Addition:
        case Subtraction:
            return AddSubtract;
        case Range:
            return OpenClosedRange;
        case LeftShift:
        case RightShift:
            return LeftRightShift;
        case Greater:
        case Less:
        case GreaterOrEqual:
        case LessOrEqual:
            return GreaterLess;
        case Equal:
        case NotEqual:
            return EqualNotEqual;
        case BitwiseAnd:
            return BitwiseAndP;
        case BitwiseXor:
            return BitwiseXorP;
        case BitwiseOr:
            return BitwiseOrP;
        case LogicalAnd:
            return AndAnd;
        case LogicalOr:
            return OrOr;
        case Assignment:
        case AssignmentExpression:
        case AdditionAssignment:
        case SubtractionAssignment:
        case MultiplicationAssignment:
        case DivisionAssignment:
            return AssignmentTo;
        default:
            return NoPrecedence;
    }
}

Location::Location() : filename(), ptr(nullptr), line(1), column(1) {}

Location::Location(const std::string& fname) :
    filename(fname),
    ptr(nullptr),
    line(1),
    column(1) {}

void Location::stepColumn() {
    ptr++;
    column++;
}

void Location::stepLine() {
    ptr++;
    line++;
    column = 1;
}

FunctionSignature::FunctionSignature(Type* rt) :
    arguments(),
    returnType(rt ? rt : new Type(Type::Void)) {}

FunctionSignature::FunctionSignature(const FunctionSignature& other) :
    arguments(),
    returnType(other.returnType->clone()) {

    Utils::cloneList(arguments, other.arguments);
}

FunctionSignature* FunctionSignature::clone() const {
    return new FunctionSignature(*this);
}

bool FunctionSignature::equals(const FunctionSignature& other) const {
    if (!Type::areEqualNoConstCheck(returnType, other.returnType)) {
        return false;
    }

    const TypeList& otherArguments = other.arguments;
    if (arguments.size() != other.arguments.size()) {
        return false;
    }

    auto i = arguments.cbegin();
    auto j = otherArguments.cbegin();
    while (i != arguments.cend()) {
        const Type* type = *i;
        const Type* otherType = *j;
        if (!Type::areEqualNoConstCheck(type, otherType)) {
            return false;
        }
    }

    return true;
}

VariableDeclaration::VariableDeclaration(
    Type* t,
    const Identifier& i,
    const Location& l) :
    Node(l),
    type(t),
    identifier(i),
    isMember(false) {}

VariableDeclaration::VariableDeclaration(Type* t, const Identifier& i) :
    VariableDeclaration(t, i, Location()) {}

VariableDeclaration::VariableDeclaration(const VariableDeclaration& other) :
    Node(other),
    type(other.type ? other.type->clone() : nullptr),
    identifier(other.identifier),
    isMember(other.isMember) {}

VariableDeclaration* VariableDeclaration::clone() const {
    return new VariableDeclaration(*this);
}

bool VariableDeclaration::operator==(const VariableDeclaration& other) const
{
    return *type == *other.type && identifier.compare(other.identifier) == 0 &&
           isMember == other.isMember;
}

std::string VariableDeclaration::toString() const {
    return type->toString() + " " + identifier;
}

Identifier Symbol::makeUnique(
    const Identifier& name,
    const Identifier& className,
    const Identifier& methodName) {

    return "_" + className + "_" + methodName + "_" + name;
}

Identifier Symbol::makeTemp(int index) {
    std::stringstream out;
    out << index;
    return "$" + out.str();
}

Identifier Symbol::makeEnumVariantTagName(const Identifier& variantName) {
    return "$" + variantName + "Tag";
}

Identifier Symbol::makeEnumVariantDataName(const Identifier& variantName) {
    return "$" + variantName;
}

Identifier Symbol::makeEnumVariantClassName(const Identifier& variantName) {
    return "$" + variantName + "Data";
}

Identifier Symbol::makeConvertableEnumName(const Identifier& enumName) {
    return enumName + "<$>";
}

Identifier Symbol::makeClosureClassName(
    const Identifier& userClassName,
    const Identifier& userMethodName,
    const Location& location) {

    std::stringstream out;
    out << location.line;
    out << location.column;
    return closureTypeName + "$" + userClassName + "$" + userMethodName + "$" +
           out.str();
}

void Trace::error(const std::string& message, const Location& location) {
    std::string arrow(location.column - 1, ' ');
    arrow.append("^");
    printf("%s:%d:%d: Error: %s\n%s\n%s\n",
           location.filename.c_str(),
           location.line,
           location.column,
           message.c_str(),
           FileCache::getLine(location.filename, location.line).c_str(),
           arrow.c_str());
    exit(0);
}

void Trace::error(const std::string& message, const Node* node) {
    const Location& location = node->getLocation();
    error(message, location);
}

void Trace::error(
    const std::string& message,
    const Type* lhs,
    const Type* rhs,
    const Node* node) {

    error(message + " Left-hand side: '" +
          lhs->toString() + 
          "'. Right-hand side: '" +
          rhs->toString() + "'.", 
          node);
}

void Trace::internalError(const std::string& where) {
    printf("Tree internal error in %s\n", where.c_str());
    exit(0);
}
