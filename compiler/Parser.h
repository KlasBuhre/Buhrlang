#ifndef Parser_h
#define Parser_h

#include <string>

#include "Lexer.h"
#include "Tree.h"
#include "CommonTypes.h"

class ClassDefinition;
class MethodDefinition;
class BlockStatement;
class Expression;
class MethodCallExpression;
class LambdaExpression;
class AnonymousFunctionExpression;
class MatchExpression;
class NamedEntityExpression;
class Module;
class MatchCase;
class ConstructorCallStatement;
class VariableDeclarationStatement;
class EnumGenerator;

class Parser {
public:
    Parser(const std::string& filename, Tree& ast, Module* module);

    void importDefaultModules();
    void parse();
    void error(const std::string& message, const Token& token);
    void setLookaheadMode();
    void setNormalMode();

    Lexer& getLexer() {
        return lexer;
    }

private:
    void addDefinition(Definition* definition);
    ClassDefinition* parseClass(bool isMessage = false);
    void parseClassWithPrimaryConstructor(
        const Identifier& className,
        bool isMessage,
        const GenericTypeParameterList& genericTypeParameters,
        const Location& location);
    void parseParentClassNameList(IdentifierList& parents);
    void parseGenericTypeParametersDeclaration(
        GenericTypeParameterList& genericTypeParameters);
    void parseClassMembers(bool classIsNative);
    void parseClassMember(AccessLevel::Kind access, bool classIsNative);
    MethodDefinition* parseMethod(
        const Token& name,
        Type* type,
        AccessLevel::Kind access,
        bool isStatic,
        bool isVirtual,
        bool parseBody);
    void parseMethodArgumentList(MethodDefinition* method);
    DataMemberDefinition* parseDataMember(
        const Token& name,
        Type* type,
        AccessLevel::Kind access,
        bool isStatic);
    void parseArgumentList(ArgumentList& argumentList);
    FunctionSignature* parseFunctionSignature();
    ClassDefinition* parseInterface(
        bool isProcessInterface = false,
        bool isMessage = false);
    void parseInterfaceMember();
    void parseProcessOrProcessInterface();
    void parseEnumeration(bool isMessage = false);
    void parseEnumerationVariant(EnumGenerator& enumGenerator);
    void parseMessage();
    MethodDefinition* parseFunction();
    BlockStatement* parseBlock(
        bool startBlock = true,
        bool allowCommaToEndSingleLineBlock = false);
    void parseStatement();
    bool variableDeclarationStartsHere();
    void parseVariableDeclarationStatement();
    Type* parseType();
    Type* parseTypeName();
    void parseGenericTypeParameters(Type* type);
    void parseArrayType(Type* type);
    void parseExpressionStatement();
    Expression* parseExpression(
        bool rangeAllowed = false,
        bool patternAllowed = false,
        Operator::Precedence precedence = Operator::NormalPrecedence);
    Expression* parseSubexpression(bool patternAllowed = false);
    Expression* parseSimpleExpression(bool patternAllowed);
    Expression* parseUnaryExpression(
        const Token& operatorToken,
        Expression* operand);
    Expression* parseBooleanLiteralExpression(const Token& consumedToken);
    Expression* parseArrayLiteralExpression();
    Expression* parseParenthesesOrTypeCastExpression();
    bool typeCastExpressionStartsHere();
    Expression* parseTypeCastExpression();
    Expression* parseMethodCall(const Token& name);
    bool lambdaExpressionStartsHere();
    void parseLambdaExpression(MethodCallExpression* methodCall);
    void parseLambdaArguments(
        LambdaExpression* lambda,
        AnonymousFunctionExpression* anonymousFunction);
    AnonymousFunctionExpression* parseAnonymousFunctionExpression();
    Expression* parseUnknownExpression(
        const Token& previousToken,
        bool patternAllowed);
    Expression* parseNewExpression();
    Expression* parseArrayIndexExpression(bool isIndexOptional = false);
    Expression* parseArraySubscriptExpression(Expression* arrayNameExpression);
    NamedEntityExpression* parseNamedEntityExpression();
    Expression* parseYieldExpression(const Location& location);
    Expression* parseMatchExpression(const Location& location);
    MatchCase* parseMatchCase();
    void parseMatchCasePatterns(MatchCase* matchCase);
    Expression* parseClassDecompositionExpression(
        const Identifier& typeName,
        const Location& loc);
    bool typedExpressionStartsHere();
    Expression* parseTypedExpression();
    ConstructorCallStatement* parseConstructorCall();
    void parseIfStatement();
    void parseOptionalBinding(const Location& location);
    void parseWhileStatement();
    void parseForStatement();
    void parseBreakStatement();
    void parseContinueStatement();
    void parseReturnStatement();
    void parseDeferStatement();
    void parseJumpStatement();
    void parseLabelStatement();
    void parseUse();
    void parseImport();
    void importModule(const std::string& moduleName);
    const Location& location();
    void consumeSingleSemicolon();
    void parseExpressionList(
        ExpressionList& expressions,
        Operator::Kind firstDelimiter,
        Operator::Kind lastDelimiter);
    void parseIdentifierList(IdentifierList& identifierList);
    void expectNewline();
    void expectCloseBraceOrNewline();

    Lexer lexer;
    Tree& tree;
    Module* module;
    bool allowError;
    bool anyErrors;
};

#endif
