#include <stdio.h>

#include "Token.h"

Token::Token(TokenType t) : 
    type(t), 
    keyword(Keyword::None), 
    op(Operator::None), 
    value(), 
    character(0), 
    location() {}

Token::Token(TokenType t, char c) : 
    type(t), 
    keyword(Keyword::None), 
    op(Operator::None), 
    value(), 
    character(c), 
    location() {}

Token::Token(TokenType t, const std::string& val) : 
    type(t), 
    keyword(Keyword::None), 
    op(Operator::None), 
    value(val), 
    character(0), 
    location() {}

Token::Token(TokenType t, const char* valStart, const char* valEnd) : 
    type(t), 
    keyword(Keyword::None), 
    op(Operator::None), 
    value(valStart, valEnd), 
    character(0), 
    location() {

    if (type == String) {
        replace("\\n", '\n');
        replace("\\r", '\r');
    }
}

Token::Token(Operator::OperatorType o) : 
    type(Operator),
    keyword(Keyword::None), 
    op(o), 
    value(), 
    character(0), 
    location() {}

Token::Token(Keyword::KeywordType k, const std::string& val) : 
    type(Keyword), 
    keyword(k), 
    op(Operator::None), 
    value(val),
    character(0), 
    location() {}

void Token::replace(const std::string& what, char with) {
    while (true) {
        size_t position = value.find(what);
        if (position != std::string::npos) {
            value.replace(position, what.length(), 1, with);
        } else {
            break;
        }
    }
}

void Token::print(FILE* file) const {
    fprintf(file, "Line=%d column=%d ", location.line, location.column);
    fprintf(file, "type=");
    switch (type) {
        case Invalid:
            fprintf(file, "Invalid");
            break;
        case Eof:
            fprintf(file, "Eof");
            break;
        case Newline:
            fprintf(file, "Newline");
            break;
        case Keyword:
            fprintf(file, "Keyword");
            fprintf(file, " keyword=");
            switch (keyword) {
                case Keyword::Class:
                    fprintf(file, Keyword::classString.c_str());
                    break;
                case Keyword::Interface:
                    fprintf(file, Keyword::interfaceString.c_str());
                    break;
                case Keyword::Private:
                    fprintf(file, Keyword::privateString.c_str());
                    break;
                case Keyword::Static:
                    fprintf(file, Keyword::staticString.c_str());
                    break;
                case Keyword::Byte:
                    fprintf(file, Keyword::byteString.c_str());
                    break;
                case Keyword::Char:
                    fprintf(file, Keyword::charString.c_str());
                    break;
                case Keyword::Int:
                    fprintf(file, Keyword::intString.c_str());
                    break;
                case Keyword::String:
                    fprintf(file, Keyword::stringString.c_str());
                    break;
                case Keyword::If:
                    fprintf(file, Keyword::ifString.c_str());
                    break;
                case Keyword::Else:
                    fprintf(file, Keyword::elseString.c_str());
                    break;
                case Keyword::Bool:
                    fprintf(file, Keyword::boolString.c_str());
                    break;
                case Keyword::True:
                    fprintf(file, Keyword::trueString.c_str());
                    break;
                case Keyword::False:
                    fprintf(file, Keyword::falseString.c_str());
                    break;
                case Keyword::While:
                    fprintf(file, Keyword::whileString.c_str());
                    break;
                case Keyword::For:
                    fprintf(file, Keyword::forString.c_str());
                    break;
                case Keyword::Break:
                    fprintf(file, Keyword::breakString.c_str());
                    break;
                case Keyword::Var:
                    fprintf(file, Keyword::varString.c_str());
                    break;
                case Keyword::Return:
                    fprintf(file, Keyword::returnString.c_str());
                    break;
                case Keyword::New:
                    fprintf(file, Keyword::newString.c_str());
                    break;
                case Keyword::Object:
                    fprintf(file, Keyword::objectString.c_str());
                    break;
                case Keyword::Let:
                    fprintf(file, Keyword::letString.c_str());
                    break;
                case Keyword::Import:
                    fprintf(file, Keyword::importString.c_str());
                    break;
                case Keyword::Use:
                    fprintf(file, Keyword::useString.c_str());
                    break;
                case Keyword::Native:
                    fprintf(file, Keyword::nativeString.c_str());
                    break;
                case Keyword::Yield:
                    fprintf(file, Keyword::yieldString.c_str());
                    break;
                default:
                    break;
            }
            break;
        case Operator:
            fprintf(file, "Operator");
            fprintf(file, " operator='");
            switch (op) {
                case Operator::Addition:
                    fprintf(file, "+");
                    break;
                case Operator::Subtraction:
                    fprintf(file, "-");
                    break;
                case Operator::Multiplication:
                    fprintf(file, "*");
                    break;
                case Operator::Division:
                    fprintf(file, "/");
                    break;
                case Operator::Assignment:
                    fprintf(file, "=");
                    break;
                case Operator::Dot:
                    fprintf(file, ".");
                    break;
                case Operator::Comma:
                    fprintf(file, ",");
                    break;
                case Operator::OpenParentheses:
                    fprintf(file, "(");
                    break;
                case Operator::CloseParentheses:
                    fprintf(file, ")");
                    break;
                case Operator::OpenBrace: 
                    fprintf(file, "{");
                    break;
                case Operator::CloseBrace:
                    fprintf(file, "}");
                    break;
                case Operator::OpenBracket: 
                    fprintf(file, "[");
                    break;
                case Operator::CloseBracket:
                    fprintf(file, "]");
                    break;
                case Operator::Equal:
                    fprintf(file, "==");
                    break;
                case Operator::NotEqual:
                    fprintf(file, "!=");
                    break;
                case Operator::Colon:
                    fprintf(file, ":");
                    break;
                case Operator::Greater:
                    fprintf(file, ">");
                    break;
                case Operator::Less:
                    fprintf(file, "<");
                    break;
                case Operator::Question:
                    fprintf(file, "?");
                    break;
                case Operator::VerticalBar:
                    fprintf(file, "|");
                    break;
                case Operator::Increment:
                    fprintf(file, "++");
                    break;
                case Operator::Decrement:
                    fprintf(file, "--");
                    break;
                case Operator::LogicalNegation:
                    fprintf(file, "!");
                    break;
                case Operator::LogicalAnd:
                    fprintf(file, "&&");
                    break;
                case Operator::LogicalOr:
                    fprintf(file, "||");
                    break;
                case Operator::Semicolon:
                    fprintf(file, ";");
                    break;
                case Operator::Arrow:
                    fprintf(file, "->");
                    break;
                default:
                    break;
            }
            fprintf(file, "'");
            break;
        case Identifier:
            fprintf(file, "Identifier");
            fprintf(file, " value='%s'", value.c_str());
            break;
        case Char:
            fprintf(file, "Char");
            fprintf(file, " character='%c'", character);
            break;
        case Integer:
            fprintf(file, "Integer");
            fprintf(file, " value='%s'", value.c_str());
            break;
        case Float:
            fprintf(file, "Float");
            fprintf(file, " value='%s'", value.c_str());
            break;
        case String:
            fprintf(file, "String");
            fprintf(file, " value='%s'", value.c_str());
            break;
        default:
            break;
    }
    fprintf(file, "\n");
}
