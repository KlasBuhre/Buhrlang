#ifndef Token_h
#define Token_h

#include <string>

#include "CommonTypes.h"

class Token {
public:

    enum Kind
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

    explicit Token(Kind k);
    explicit Token(Operator::Kind o);
    Token(Kind k, char c);
    Token(Kind k, const std::string& val);
    Token(Kind k, const char* valStart, const char* valEnd);
    Token(Keyword::Kind k, const std::string& val);

    void print(FILE* file) const;

    void setLocation(const Location& l) {
        location = l;
    }

    const Location& getLocation() const {
        return location;
    }

    Kind getKind() const {
        return kind;
    }

    Keyword::Kind getKeyword() const {
        return keyword;
    }

    bool isKeyword(Keyword::Kind k) const {
        return kind == Keyword && keyword == k;
    }

    Operator::Kind getOperator() const {
        return op;
    }

    bool isOperator(Operator::Kind o) const {
        return kind == Operator && op == o;
    }

    char getCharacter() const {
        return character;
    }

    const std::string& getValue() const {
        return value;
    }

    bool isKeyword() const {
        return kind == Keyword;
    }

    bool isIdentifier() const {
        return kind == Identifier;
    }

    bool isOperator() const {
        return kind == Operator;
    }

    bool isNewline() const {
        return kind == Newline;
    }

    bool isEof() const {
        return kind == Eof;
    }

    bool isInvalid() const {
        return kind == Invalid;
    }

private:
    void replace(const std::string& what, char with);

    Kind kind;
    Keyword::Kind keyword;
    Operator::Kind op;
    std::string value;
    char character;
    Location location;
};

#endif
