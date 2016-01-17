#include <stdio.h>
#include <stdlib.h>

#include "Lexer.h"
#include "File.h"

// #define DEBUG

Lexer::Lexer(const std::string& filename) :
    tokenList(),
    state(Idle),
    currentToken(tokenList.end()),
    storedPosition(tokenList.end()),
    keywordMap(),
    location(filename),
    start(filename),
    eof(nullptr) {

    initKeywordMap();
    readFile(filename);
    tokenize();
    for (currentToken = tokenList.begin(); 
         currentToken->isNewline(); 
         currentToken++);
}

const Token& Lexer::consumeToken() {
    const Token& token = *currentToken;
    while ((++currentToken)->isNewline());
    return token;
}

const Token& Lexer::peekToken() const {
    TokenList::const_iterator peekedToken = currentToken;
    while ((++peekedToken)->isNewline());
    return *peekedToken;
}

void Lexer::stepBack() {
    while ((--currentToken)->isNewline());
}

bool Lexer::previousTokenWasNewline() const {
    TokenList::const_iterator token = currentToken;
    return (--token)->isNewline();
}

void Lexer::readFile(const std::string& filename) {
    const std::string& fileBuf = FileCache::getFile(filename);
    const char* buf = fileBuf.data();
    eof = buf + fileBuf.size() - 1;
    location.ptr = buf;
}

void Lexer::tokenize() {
    while (location.ptr < eof) {
        bool newLine = false;
        char c = *location.ptr;
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                storePreviousToken();
                break;
            case '\n':
                newLine = true;
                storePreviousToken();
                storeToken(Token::Newline);
                break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
            case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
            case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
            case 'Y': case 'Z':
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
            case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
            case 's': case 't': case 'u': case 'v': case 'w': case 'x':
            case 'y': case 'z':
            case '_':
                switch (state) {
                    case Idle:
                        state = GettingIdentifier;
                        start = location;
                        break;
                    case GettingIntegerNumber:
                    case GettingFloatingPointNumber:
                        storeToken(Token::Invalid);
                        break;
                    default:
                        break;
                }
                break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                switch (state) {
                    case Idle:
                        state = GettingIntegerNumber;
                        start = location;
                        break;
                    default:
                        break;
                }
                break;
            case '/':
                storePreviousToken();
                if (isNextChar('/', location.ptr)) {
                    skipUntilNewline();
                    continue;
                } else if (isNextChar('*', location.ptr)) {
                    skipMultilineComment();
                    continue;
                } else {
                    makeOperatorToken(location);
                }
                break;
            case '=':
            case '!':     
            case '+':
            case '-':
            case '*':
            case '.':
            case ',':
            case '>':
            case '<':
            case ':':
            case ';':
            case '?':
            case '|':
            case '&':
            case '^':
            case '~':
            case '(': case ')':
            case '{': case '}':
            case '[': case ']':
                if (c == '.' && !isNextChar('.') &&
                    state == GettingIntegerNumber) {
                    state = GettingFloatingPointNumber;
                } else {
                    storePreviousToken();
                    makeOperatorToken(location);
                }
                break;
            case '"':
                storePreviousToken();
                makeStringLiteral(location.ptr);
                continue;
            case '\'':
                storePreviousToken();
                makeCharLiteral();
                continue;
            default:
                storeToken(Token::Invalid);
                break;
        }
        if (newLine) {
            location.stepLine();
        } else {
            location.stepColumn();
        }
    }
    storeToken(Token::Eof);
}

void Lexer::storePreviousToken() {
    switch (state) {
        case GettingIdentifier:
            if (location.ptr - start.ptr == 1 && *(start.ptr) == '_') {
                makeOperatorToken(start);
            } else { 
                makeIdentifierOrKeywordToken(start.ptr, location.ptr);
            }
            break;
        case GettingIntegerNumber: {
            Token token(Token::Integer, start.ptr, location.ptr);
            storeToken(token, start);
            break;
        }
        case GettingFloatingPointNumber: {
            Token token(Token::Float, start.ptr, location.ptr);
            storeToken(token, start);
            break;
        }
        default:
            break;
    }
}

void Lexer::storeToken(Token::Kind kind) {
    Token token(kind);
    storeToken(token, location);
}

void Lexer::storeToken(Token& token, const Location& loc) {
    token.setLocation(loc);
    tokenList.push_back(token);
    state = Idle;

#ifdef DEBUG
    token.print(stdout);
#endif
}

void Lexer::makeIdentifierOrKeywordToken(const char* tStart, const char* tEnd) {
    std::string str(tStart, tEnd);
    KeywordMap::const_iterator it = keywordMap.find(str);
    if (it == keywordMap.end()) {
        Token token(Token::Identifier, str);    
        storeToken(token, start);
    } else {
        Token token(it->second, str);
        storeToken(token, start);
    }
}

void Lexer::makeStringLiteral(const char* tStart) {
    start = location;
    location.stepColumn();
    while (location.ptr < eof) {
        char c = *location.ptr;
        switch (c) {
            case '\n':
                storeToken(Token::Invalid);
                break;
            case '"': {
                Token token(Token::String, tStart + 1, location.ptr);         
                storeToken(token, start);
                location.stepColumn();
                return;
            }
            default:
                break;
        }
        location.stepColumn();
    }
    storeToken(Token::Eof);
}

void Lexer::makeCharLiteral() {
    if (location.ptr + 4 >= eof) {
        storeToken(Token::Eof);
        return;
    }
    location.stepColumn();
    char c = *location.ptr;
    location.stepColumn();
    char d = *location.ptr;
    if (d == '\'') {
        Token token(Token::Char, c);
        storeToken(token, location);
    } else if (c == '\\'){
        switch (d) {
            case 'n':
                c = '\n';
                break;
            default:
                storeToken(Token::Invalid);
                break;
        }
        Token token(Token::Char, c);
        storeToken(token, location);
        location.stepColumn();
    } else {
        storeToken(Token::Invalid);    
    }
    location.stepColumn();
}

void Lexer::makeOperatorToken(const Location& startLocation) {
    Operator::Kind op;
    const char* tStart = startLocation.ptr;
    switch (*tStart) {
        case '=':
            if (isNextChar('=', tStart)) {
                op = Operator::Equal;
                location.stepColumn();
            } else {
                op = Operator::Assignment;
            }
            break;
        case '!':
            if (isNextChar('=', tStart)) {
                op = Operator::NotEqual;
                location.stepColumn();
            } else {
                op = Operator::LogicalNegation;
            }
            break;
        case ':':
            if (isNextChar('=', tStart)) {
                op = Operator::AssignmentExpression;
                location.stepColumn();
            } else {
                op = Operator::Colon;
            }
            break;
        case '>':
            if (isNextChar('=', tStart)) {
                op = Operator::GreaterOrEqual;
                location.stepColumn();
            } else if (isNextChar('>', tStart)) {
                op = Operator::RightShift;
                location.stepColumn();
            } else {
                op = Operator::Greater;
            }
            break;
        case '<':
            if (isNextChar('=', tStart)) {
                op = Operator::LessOrEqual;
                location.stepColumn();
            } else if (isNextChar('<', tStart)) {
                op = Operator::LeftShift;
                location.stepColumn();
            } else {
                op = Operator::Less;
            }
            break;
        case '&':
            if (isNextChar('&', tStart)) {
                op = Operator::LogicalAnd;
                location.stepColumn();
            } else {
                op = Operator::BitwiseAnd;
            }
            break;
        case '|':
            if (isNextChar('|', tStart)) {
                op = Operator::LogicalOr;
                location.stepColumn();
            } else {
                op = Operator::BitwiseOr;
            }
            break;    
        case '+':
            if (isNextChar('+', tStart)) {
                op = Operator::Increment;
                location.stepColumn();
            } else if (isNextChar('=', tStart)) {
                op = Operator::AdditionAssignment;
                location.stepColumn();
            } else {
                op = Operator::Addition;
            }
            break;
        case '-':
            if (isNextChar('-', tStart)) {
                op = Operator::Decrement;
                location.stepColumn();
            } else if (isNextChar('=', tStart)) {
                op = Operator::SubtractionAssignment;
                location.stepColumn();
            } else if (isNextChar('>', tStart)) {
                op = Operator::Arrow;
                location.stepColumn();
            } else {
                op = Operator::Subtraction;
            }
            break;
        case '*':
            if (isNextChar('=', tStart)) {
                op = Operator::MultiplicationAssignment;
                location.stepColumn();
            } else {
                op = Operator::Multiplication;
            }
            break;
        case '/':
            if (isNextChar('=', tStart)) {
                op = Operator::DivisionAssignment;
                location.stepColumn();
            } else {
                op = Operator::Division;
            }
            break;
        case '.':
            if (isNextChar('.', tStart)) {
                if (isNextChar('.', tStart + 1)) {
                    op = Operator::Range;
                    location.stepColumn();
                } else {
                    op = Operator::Wildcard;
                }
                location.stepColumn();
            } else {
                op = Operator::Dot;
            }
            break;
        case ',':
            op = Operator::Comma;
            break;
        case '(':
            op = Operator::OpenParentheses;
            break;
        case ')':
            op = Operator::CloseParentheses;
            break;
        case '{': 
            op = Operator::OpenBrace;
            break;
        case '}':
            op = Operator::CloseBrace;
            break;
        case '[':
            op = Operator::OpenBracket;
            break;
        case ']':
            op = Operator::CloseBracket;
            break;
        case ';':
            op = Operator::Semicolon;
            break;
        case '?':
            op = Operator::Question;
            break;
        case '_':
            op = Operator::Placeholder;
            break;
        case '^':
            op = Operator::BitwiseXor;
            break;
        case '~':
            op = Operator::BitwiseNot;
            break;
        default:
            break;
    }
    Token token(op);
    storeToken(token, startLocation);
}

bool Lexer::isNextChar(char character) {
    return isNextChar(character, location.ptr);
}

bool Lexer::isNextChar(char character, const char* buf) {
    return (buf + 1 < eof && buf[1] == character);
}

void Lexer::skipUntilNewline() {
    while (location.ptr < eof) {
        if (*location.ptr == '\n') {
            break;
        }
        location.stepColumn();
    }
}

void Lexer::skipMultilineComment() {
    location.stepColumn();
    location.stepColumn();
    while (location.ptr < eof) {
        switch (*location.ptr) {
            case '*':
                if (isNextChar('/', location.ptr)) {
                    location.stepColumn();
                    location.stepColumn();
                    return;
                }
                location.stepColumn();
                break;
            case '\n':
                location.stepLine();
                break;
            default:
                location.stepColumn();
                break;
        }
    }
}

void Lexer::initKeywordMap() {
    keywordMap[Keyword::classString] = Keyword::Class;
    keywordMap[Keyword::interfaceString] = Keyword::Interface;
    keywordMap[Keyword::processString] = Keyword::Process;
    keywordMap[Keyword::namedString] = Keyword::Named;
    keywordMap[Keyword::messageString] = Keyword::Message;
    keywordMap[Keyword::initString] = Keyword::Init;
    keywordMap[Keyword::privateString] = Keyword::Private;
    keywordMap[Keyword::staticString] = Keyword::Static;
    keywordMap[Keyword::argString] = Keyword::Arg;
    keywordMap[Keyword::byteString] = Keyword::Byte;
    keywordMap[Keyword::charString] = Keyword::Char;
    keywordMap[Keyword::intString] = Keyword::Int;
    keywordMap[Keyword::floatString] = Keyword::Float;
    keywordMap[Keyword::stringString] = Keyword::String;
    keywordMap[Keyword::enumString] = Keyword::Enum;
    keywordMap[Keyword::funString] = Keyword::Fun;
    keywordMap[Keyword::ifString] = Keyword::If;
    keywordMap[Keyword::elseString] = Keyword::Else;
    keywordMap[Keyword::boolString] = Keyword::Bool;
    keywordMap[Keyword::trueString] = Keyword::True;
    keywordMap[Keyword::falseString] = Keyword::False;
    keywordMap[Keyword::whileString] = Keyword::While;
    keywordMap[Keyword::forString] = Keyword::For;
    keywordMap[Keyword::breakString] = Keyword::Break;
    keywordMap[Keyword::continueString] = Keyword::Continue;
    keywordMap[Keyword::varString] = Keyword::Var;
    keywordMap[Keyword::letString] = Keyword::Let;
    keywordMap[Keyword::returnString] = Keyword::Return;
    keywordMap[Keyword::newString] = Keyword::New;
    keywordMap[Keyword::thisString] = Keyword::This;
    keywordMap[Keyword::importString] = Keyword::Import;
    keywordMap[Keyword::useString] = Keyword::Use;
    keywordMap[Keyword::nativeString] = Keyword::Native;
    keywordMap[Keyword::yieldString] = Keyword::Yield;
    keywordMap[Keyword::matchString] = Keyword::Match;
    keywordMap[Keyword::deferString] = Keyword::Defer;
    keywordMap[Keyword::jumpString] = Keyword::Jump;
}
