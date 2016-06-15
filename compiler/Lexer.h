#ifndef Lexer_h
#define Lexer_h

#include <string>
#include <map>
#include <list>

#include "Token.h"

class Lexer {
public:
    explicit Lexer(const std::string& filename);

    const Token& consumeToken();
    const Token& peekToken() const;
    const Token& previousToken() const;
    void stepBack();
    bool previousTokenWasNewline() const;

    const Token* getCurrentTokenPtr() const {
        return &*currentToken;
    }

    const Token& getCurrentToken() const {
        return *currentToken;
    }

    void storePosition() {
        storedPosition = currentToken;
    }

    void restorePosition() {
        currentToken = storedPosition;
    }

private:    
    void initKeywordMap();
    void readFile(const std::string& filename);
    void tokenize();
    void storeToken(Token::Kind kind);
    void storeToken(Token& token, const Location& loc);
    void storePreviousToken();
    void makeIdentifierOrKeywordToken(const char*, const char*);
    void makeOperatorToken(const Location& startLocation);
    void makeStringLiteral(const char*);
    void makeCharLiteral();
    bool isNextChar(char character);
    bool isNextChar(char character, const char* buf);
    void skipUntilNewline();
    void skipMultilineComment();

    enum State {
        Idle,
        GettingIdentifier,
        GettingIntegerNumber,
        GettingFloatingPointNumber
    };

    using TokenList = std::list<Token>;
    using KeywordMap = std::map<std::string, Keyword::Kind>;

    TokenList tokenList;
    State state;
    TokenList::const_iterator currentToken;
    TokenList::const_iterator storedPosition;
    KeywordMap keywordMap;
    Location location;
    Location start;
    const char* eof;
};

#endif

