#ifndef Token_h
#define Token_h

#include <string>

#include "CommonTypes.h"

class Token {
public:

    enum TokenType
    {
        Invalid,
        Eof,
        Newline,
        Keyword,
        Operator,
        Identifier,
        Char,
        Integer,
        Float,
        String
    };

    explicit Token(TokenType t);
    explicit Token(Operator::OperatorType o);
    Token(TokenType t, char c);
    Token(TokenType t, const std::string& val);
    Token(TokenType t, const char* valStart, const char* valEnd);
    Token(Keyword::KeywordType k, const std::string& val);

    void print(FILE* file) const;

    void setLocation(const Location& l) {
        location = l;
    }

    const Location& getLocation() const {
        return location;
    }

    TokenType getType() const {
        return type;
    }

    Keyword::KeywordType getKeyword() const {
        return keyword;
    }

    bool isKeyword(Keyword::KeywordType k) const {
        return keyword == k;
    }

    Operator::OperatorType getOperator() const {
        return op;
    }

    bool isOperator(Operator::OperatorType o) const {
        return op == o;
    }

    char getCharacter() const {
        return character;
    }

    const std::string& getValue() const {
        return value;
    }

    bool isKeyword() const {
        return type == Keyword;
    }

    bool isIdentifier() const {
        return type == Identifier;
    }

    bool isOperator() const {
        return type == Operator;
    }

    bool isNewline() const {
        return type == Newline;
    }

    bool isEof() const {
        return type == Eof;
    }

    bool isInvalid() const {
        return type == Invalid;
    }

private:
    void replace(const std::string& what, char with);

    TokenType type;
    Keyword::KeywordType keyword;
    Operator::OperatorType op;
    std::string value;
    char character;
    Location location;
};

#endif
