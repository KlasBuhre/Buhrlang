#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "Parser.h"
#include "Definition.h"
#include "Statement.h"
#include "Expression.h"
#include "File.h"
#include "Module.h"
#include "ProcessGenerator.h"
#include "EnumGenerator.h"

namespace  {
    void error(const std::string& message, const Token& token) {
        Trace::error(message, token.getLocation());
    }

    void removeAliasPrefix(Identifier& identifier) {
        if (identifier.length() > 2 &&
            identifier.substr(0, 2).compare("__") == 0) {
            identifier = identifier.substr(2, identifier.length() - 2);
        }    
    }

    class CommaSeparatedListParser {
    public:
        CommaSeparatedListParser(Lexer& l, Operator::OperatorType end) :
            lexer(l),
            endDelimiter(end),
            commaExpected(false) {}

        CommaSeparatedListParser(Lexer& l) :
            CommaSeparatedListParser(l, Operator::None) {}

        bool parseComma() {
            const Token& token = lexer.getCurrentToken();
            if (endDelimiter == Operator::None) {
                if (commaExpected && !token.isOperator(Operator::Comma)) {
                    return false;
                }
            } else {
                if (token.isOperator(endDelimiter)) {
                    lexer.consumeToken();
                    return false;
                }
            }
            if (commaExpected) {
                if (!token.isOperator(Operator::Comma)) {
                    error("Expected ','.", token);
                }
                lexer.consumeToken();
            } else {
                if (token.isOperator(Operator::Comma)) {
                    error("Unexpected ','.", token);
                }
            }
            commaExpected = true;
            return true;
        }
        
    private:
        Lexer& lexer;
        Operator::OperatorType endDelimiter;
        bool commaExpected;
    };
}

Parser::Parser(const std::string& filename, Tree& ast, Module* mod) :
    lexer(filename),
    tree(ast),
    module(mod) {}

void Parser::importDefaultModules() {
    importModule("System.b");
    importModule("Box.b");
    importModule("Process.b");
    importModule("Option.b");
}

void Parser::parse() {
    while (true) {
        const Token& token = lexer.getCurrentToken();
        switch (token.getType()) {
            case Token::Keyword:
                switch (token.getKeyword()) {
                    case Keyword::Native:
                    case Keyword::Class:
                        addDefinition(parseClass());
                        break;
                    case Keyword::Interface:
                        addDefinition(parseInterface());
                        break;
                    case Keyword::Enum:
                        parseEnumeration();
                        break;
                    case Keyword::Process:
                        parseProcessOrProcessInterface();
                        break;
                    case Keyword::Import:
                        parseImport();
                        break;
                    case Keyword::Use:
                        parseUse();
                        break;
                    default:
                        addDefinition(parseFunction());
                        break;
                }
                break;
            case Token::Identifier:
                addDefinition(parseFunction());
                break;
            case Token::Eof:
                return;
            default:
                error("Syntax error.", token);
                break;
        }
        expectNewline();
    }
}

void Parser::addDefinition(Definition* definition) {
    if (module == nullptr) {
        // No module set means we are parsing imported code.
        definition->setIsImported(true);
    }

    tree.addGlobalDefinition(definition);

    if (definition->isClass()) {
        ClassDefinition* classDef = definition->cast<ClassDefinition>();
        if (!classDef->isProcess() &&
            classDef->isInheritingFromProcessInterface()) {
            ProcessGenerator generator(classDef, tree);
            generator.addMessageHandlerAbilityToRegularClass();
        }
    }
}

ClassDefinition* Parser::parseClass() {
    bool native = false;
    const Token& nativeToken = lexer.getCurrentToken();
    if (nativeToken.isKeyword(Keyword::Native)) {
        native = true;
        if (module != nullptr) {
            module->setIsNative(true);
        }
        lexer.consumeToken();
    }

    const Token& classToken = lexer.consumeToken();
    if (!classToken.isKeyword(Keyword::Class)) {
        error("Expected 'class'.", classToken);
    }

    const Token& name = lexer.consumeToken();
    if (!name.isIdentifier()) {
        error("Expected identifier.", name);
    }
    Identifier className(name.getValue());
    removeAliasPrefix(className);

    GenericTypeParameterList genericTypeParameters;
    if (lexer.getCurrentToken().isOperator(Operator::Less)) {
        parseGenericTypeParametersDeclaration(genericTypeParameters);
    }

    if (lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        parseClassWithPrimaryConstructor(className,
                                         genericTypeParameters,
                                         classToken.getLocation());
    } else {
        IdentifierList parents;
        parseParentClassNameList(parents);

        ClassDefinition::Properties properties;
        tree.startClass(className,
                        genericTypeParameters,
                        parents,
                        properties,
                        classToken.getLocation());
        parseClassMembers(native);
    }

    return tree.finishClass();
}

void Parser::parseClassWithPrimaryConstructor(
    const Identifier& className,
    const GenericTypeParameterList& genericTypeParameters,
    const Location& location) {

    ArgumentList constructorArguments;
    parseArgumentList(constructorArguments);

    ConstructorCallStatement* constructorCall = nullptr;
    IdentifierList parents;
    if (lexer.getCurrentToken().isOperator(Operator::Colon)) {
        lexer.consumeToken();
        const Token& token = lexer.getCurrentToken();
        if (token.isKeyword(Keyword::Init) ||
            lexer.peekToken().isOperator(Operator::OpenParentheses)) {
            if (token.isIdentifier()) {
                parents.push_back(token.getValue());
            }
            constructorCall = parseConstructorCall();
            if (lexer.getCurrentToken().isOperator(Operator::Comma)) {
                lexer.consumeToken();
                parseIdentifierList(parents);
            }
        } else {
            parseIdentifierList(parents);
        }
    }

    ClassDefinition::Properties properties;
    tree.startClass(className,
                    genericTypeParameters,
                    parents,
                    properties,
                    location);
    tree.getCurrentClass()->addPrimaryConstructor(constructorArguments,
                                                  constructorCall);
    parseClassMembers(false);
}

void Parser::parseParentClassNameList(IdentifierList& parents) {
    if (lexer.getCurrentToken().isOperator(Operator::Colon)) {
        lexer.consumeToken();
        parseIdentifierList(parents);
    }
}

void Parser::parseGenericTypeParametersDeclaration(
    GenericTypeParameterList& genericTypeParameters) {

    assert(lexer.consumeToken().isOperator(Operator::Less));

    CommaSeparatedListParser listParser(lexer, Operator::Greater);
    while (listParser.parseComma()) {
        const Token& typeParameterName = lexer.consumeToken();
        if (!typeParameterName.isIdentifier()) {
            error("Expected identifier.", typeParameterName);
        }
        GenericTypeParameterDefinition* typeParameter =
            new GenericTypeParameterDefinition(typeParameterName.getValue(),
                                               typeParameterName.getLocation());
        genericTypeParameters.push_back(typeParameter);
    }
}

void Parser::parseClassMembers(bool classIsNative) {
    if (lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
        lexer.consumeToken();
        while (!lexer.getCurrentToken().isOperator(Operator::CloseBrace)) {
            parseClassMember(classIsNative);
            expectCloseBraceOrNewline();
        }
        lexer.consumeToken();
    }
}

void Parser::parseClassMember(bool isClassNative) {
    const Token* token = lexer.getCurrentTokenPtr(); 
    if (token->isKeyword(Keyword::Class)) {
        // Parse a nested class.
        tree.addClassMember(parseClass());
        return;
    }

    AccessLevel::AccessLevelType access = AccessLevel::Public;
    if (token->isKeyword(Keyword::Private)) {
        access = AccessLevel::Private;
        lexer.consumeToken();
        token = lexer.getCurrentTokenPtr(); 
    }

    bool isStatic = false;
    if (token->isKeyword(Keyword::Static)) {
        isStatic = true;
        lexer.consumeToken();
    }

    Type* type = nullptr;
    if (!lexer.peekToken().isOperator(Operator::OpenParentheses)) {
        type = parseType();
    }

    const Token& name = lexer.consumeToken();
    if (!name.isKeyword(Keyword::Init) && !name.isIdentifier()) {
        error("Expected identifier.", name);
    }

    if (lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        // This is a method definition.
        MethodDefinition* method = parseMethod(name,
                                               type,
                                               access,
                                               isStatic,
                                               !isClassNative);
        tree.addClassMember(method);
    } else {
        // This should be a data member declaration.
        DataMemberDefinition* dataMember = parseDataMember(name,
                                                           type,
                                                           access,
                                                           isStatic);
        tree.addClassMember(dataMember);
    } 
}

MethodDefinition* Parser::parseMethod(
    const Token& name,
    Type* type,
    AccessLevel::AccessLevelType access,
    bool isStatic,
    bool parseBody) {

    Identifier methodName(name.getValue());
    removeAliasPrefix(methodName);
    MethodDefinition* method = new MethodDefinition(methodName,
                                                    type,
                                                    access,
                                                    isStatic,
                                                    tree.getCurrentClass(),
                                                    name.getLocation());
    if (parseBody) {
        BlockStatement* body = tree.startBlock(location());
        method->setBody(body);
        parseMethodArgumentList(method);
        if (method->isConstructor() &&
            lexer.getCurrentToken().isOperator(Operator::Colon)) {
            lexer.consumeToken();
            tree.addStatement(parseConstructorCall());
        } else {
            if (!lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
                parseLambdaSignature(method);
            }
        }
        parseBlock(false);
    } else {
        parseMethodArgumentList(method);
    }
    return method;
}

DataMemberDefinition* Parser::parseDataMember(
    const Token& name,
    Type* type,
    AccessLevel::AccessLevelType access,
    bool isStatic) {

    DataMemberDefinition* dataMember =
        new DataMemberDefinition(name.getValue(),
                                 type,
                                 access,
                                 isStatic,
                                 false,
                                 name.getLocation());
    if (lexer.getCurrentToken().isOperator(Operator::Assignment)) {
        lexer.consumeToken();
        dataMember->setExpression(parseExpression());
    }

    return dataMember;
}

void Parser::parseMethodArgumentList(MethodDefinition* method) {
    ArgumentList arguments;
    parseArgumentList(arguments);
    method->addArguments(arguments);
}

void Parser::parseArgumentList(ArgumentList& argumentList) {
    assert(lexer.consumeToken().isOperator(Operator::OpenParentheses));

    CommaSeparatedListParser listParser(lexer, Operator::CloseParentheses);
    while (listParser.parseComma()) {
        bool isDataMember = true;
        if (lexer.getCurrentToken().isKeyword(Keyword::Arg)) {
            lexer.consumeToken();
            isDataMember = false;
        }

        Type* type = parseType();
        const Token& identifier = lexer.consumeToken();
        if (!identifier.isIdentifier()) {
            error("Expected identifier.", identifier);
        }

        VariableDeclaration* argument =
            new VariableDeclaration(type,
                                    identifier.getValue(),
                                    identifier.getLocation());
        argument->setIsDataMember(isDataMember);
        argumentList.push_back(argument);
    }
}

void Parser::parseLambdaSignature(MethodDefinition* method) {
    Type* returnType = nullptr;
    if (!lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        returnType = parseType();
    }

    const Token& openParentheses = lexer.consumeToken(); 
    if (!openParentheses.isOperator(Operator::OpenParentheses)) {
        error("Expected '('.", openParentheses);
    }

    LambdaSignature* lambdaSignature = new LambdaSignature(returnType); 
    CommaSeparatedListParser listParser(lexer, Operator::CloseParentheses);
    while (listParser.parseComma()) {
        lambdaSignature->addArgument(parseType());
    }

    method->setLambdaSignature(lambdaSignature, location());
}

ClassDefinition* Parser::parseInterface(bool isProcessInterface) {
    const Token& interfaceToken = lexer.consumeToken();
    assert(interfaceToken.isKeyword(Keyword::Interface));

    const Token& name = lexer.consumeToken();
    if (!name.isIdentifier()) {
        error("Expected identifier.", name);
    }

    IdentifierList parents;
    parseParentClassNameList(parents);

    GenericTypeParameterList genericTypeParameters;
    ClassDefinition::Properties properties;
    properties.isInterface = true;
    properties.isProcess = isProcessInterface;
    tree.startClass(name.getValue(),
                    genericTypeParameters,
                    parents,
                    properties,
                    interfaceToken.getLocation());

    if (lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
        lexer.consumeToken();
        while (!lexer.getCurrentToken().isOperator(Operator::CloseBrace)) {
            parseInterfaceMember();
            expectCloseBraceOrNewline();
        }
        lexer.consumeToken();
    }

    return tree.finishClass();
}

void Parser::parseInterfaceMember() {
    Type* type = nullptr;
    if (!lexer.peekToken().isOperator(Operator::OpenParentheses)) {
        type = parseType();
    }

    const Token& name = lexer.consumeToken();
    if (!name.isIdentifier()) {
        error("Expected identifier.", name);
    }

    if (lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        // This is an interface method signature.
        MethodDefinition* method = parseMethod(name,
                                               type,
                                               AccessLevel::Public,
                                               false,
                                               false);
        tree.addClassMember(method);
    } else {
        // This should be a static interface data member declaration.
        DataMemberDefinition* dataMember = parseDataMember(name,
                                                           type,
                                                           AccessLevel::Public,
                                                           true);
        tree.addClassMember(dataMember);
    }
}

void Parser::parseProcessOrProcessInterface() {
    const Token& processToken = lexer.consumeToken();
    assert(processToken.isKeyword(Keyword::Process));

    ClassDefinition* classDef = nullptr;
    const Token& token = lexer.getCurrentToken();
    if (token.isKeyword(Keyword::Interface)) {
        classDef = parseInterface(true);
        addDefinition(classDef);

        ProcessGenerator generator(classDef, tree);
        generator.generateProcessInterfaceClasses();
    } else {
        lexer.consumeToken();
        if (!token.isIdentifier()) {
            error("Expected identifier.", token);
        }

        Identifier processName(token.getValue());
        removeAliasPrefix(processName);

        IdentifierList parents;
        parseParentClassNameList(parents);

        GenericTypeParameterList genericTypeParameters;
        ClassDefinition::Properties properties;
        properties.isProcess = true;
        tree.startClass(processName,
                        genericTypeParameters,
                        parents,
                        properties,
                        processToken.getLocation());
        parseClassMembers(false);
        classDef = tree.finishClass();
        addDefinition(classDef);

        ProcessGenerator generator(classDef, tree);
        generator.generateProcessClasses();
    }
}

void Parser::parseEnumeration() {
    const Token& enumToken = lexer.consumeToken();
    assert(enumToken.isKeyword(Keyword::Enum));

    const Token& name = lexer.consumeToken();
    if (!name.isIdentifier()) {
        error("Expected identifier.", name);
    }

    GenericTypeParameterList genericTypeParameters;
    if (lexer.getCurrentToken().isOperator(Operator::Less)) {
        parseGenericTypeParametersDeclaration(genericTypeParameters);
    }

    if (!lexer.consumeToken().isOperator(Operator::OpenBrace)) {
        error("Expected '{'.", lexer.getCurrentToken());
    }

    EnumGenerator enumGenerator(name.getValue(),
                                genericTypeParameters,
                                name.getLocation(),
                                tree);

    CommaSeparatedListParser listParser(lexer, Operator::CloseBrace);
    while (listParser.parseComma()) {
        parseEnumerationVariant(enumGenerator);
    }

    ClassDefinition* convertableEnum = enumGenerator.getConvertableEnum();
    if (convertableEnum != nullptr) {
        addDefinition(convertableEnum);
    }

    addDefinition(enumGenerator.getEnum());
}

void Parser::parseEnumerationVariant(EnumGenerator& enumGenerator) {
    const Token& variantName = lexer.consumeToken();
    if (!variantName.isIdentifier()) {
        error("Expected identifier.", variantName);
    }

    ArgumentList variantData;
    if (lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        lexer.consumeToken();
        CommaSeparatedListParser listParser(lexer, Operator::CloseParentheses);
        int numberOfDataMembers = 0;
        while (listParser.parseComma()) {
            Location loc = location();
            VariableDeclaration* variantDataMember =
                new VariableDeclaration(parseType(),
                                        Symbol::makeTemp(numberOfDataMembers++),
                                        loc);
            variantDataMember->setIsDataMember(true);
            variantData.push_back(variantDataMember);
        }
    }

    enumGenerator.generateVariant(variantName.getValue(),
                                  variantData,
                                  variantName.getLocation());
}

MethodDefinition* Parser::parseFunction() {
    Type* type = nullptr;
    if (!lexer.peekToken().isOperator(Operator::OpenParentheses)) {
        type = parseType();
    }

    const Token& name = lexer.consumeToken();
    if (!name.isIdentifier()) {
        error("Expected identifier.", name);
    }

    const Token& openParentheses = lexer.getCurrentToken();
    if (!openParentheses.isOperator(Operator::OpenParentheses)) {
        error("Expected '('", openParentheses);
    }

    tree.startFunction();
    MethodDefinition* function = parseMethod(name,
                                             type,
                                             AccessLevel::Public,
                                             true,
                                             true);
    tree.finishFunction(function);
    return function;
}

BlockStatement* Parser::parseBlock(
    bool startBlock,
    bool allowCommaToEndSingleLineBlock) {

    if (startBlock) {
        tree.startBlock(location());
    }

    if (lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
        lexer.consumeToken();
        while (!lexer.getCurrentToken().isOperator(Operator::CloseBrace)) {
            parseStatement();
            expectCloseBraceOrNewline();
        }
        lexer.consumeToken();
    } else {
        // One-line block
        parseStatement();
        if (!allowCommaToEndSingleLineBlock) {
            expectCloseBraceOrNewline();
        }
    }

    return tree.finishBlock();
}

void Parser::parseStatement() {
    const Token& token = lexer.getCurrentToken();
    switch (token.getType()) {
        case Token::Keyword:
            switch (token.getKeyword()) {
                case Keyword::Let:
                case Keyword::Var:
                    parseVariableDeclarationStatement();
                    break;
                case Keyword::If:
                    parseIfStatement();
                    break;
                case Keyword::While:
                    parseWhileStatement();
                    break;
                case Keyword::For:
                    parseForStatement();
                    break;
                case Keyword::Break:
                    parseBreakStatement();
                    break;
                case Keyword::Continue:
                    parseContinueStatement();
                    break;
                case Keyword::Return:
                    parseReturnStatement();
                    break;
                case Keyword::Jump:
                    parseJumpStatement();
                    break;
                case Keyword::This:
                case Keyword::Match:
                case Keyword::Yield:
                case Keyword::New:
                case Keyword::True:
                case Keyword::False:
                    parseExpressionStatement();
                    break;
                case Keyword::Use:
                    parseUse();
                    break;
                default:
                    error("Syntax error.", token);
                    break;
            }
            break;
        case Token::Identifier:
            if (lexer.peekToken().isOperator(Operator::Colon)) {
                parseLabelStatement();
            } else {
                parseExpressionStatement();
            }
            break;
        case Token::Operator:
            if (token.isOperator(Operator::OpenBrace)) {
                tree.addStatement(parseBlock());
            } else {
                parseExpressionStatement();
            }
            break;
        case Token::Char:
        case Token::Integer:
        case Token::Float:
        case Token::String:
            parseExpressionStatement();
            break;
        case Token::Eof:
            error("Unexpected end of file.", token);
            break;
        default:
            error("Syntax error.", token);
            break;
    }
}

bool Parser::variableDeclarationStartsHere() {
    switch (lexer.getCurrentToken().getKeyword()) {
        case Keyword::Let:
        case Keyword::Var:
            return true;
        default:
            return false;
    }
}

void Parser::parseVariableDeclarationStatement() {
    Type* type = parseType();

    const Token& identifier = lexer.consumeToken();
    if (!identifier.isIdentifier()) {
        error("Expected identifier.", identifier);
    }

    Expression* patternExpression = nullptr;
    switch (lexer.getCurrentToken().getOperator()) {
        case Operator::OpenBrace:
            patternExpression =
                parseClassDecompositionExpression(identifier.getValue(),
                                                  identifier.getLocation());
            break;
        case Operator::OpenParentheses:
            patternExpression = parseMethodCall(identifier);
            break;
        default:
            break;
    }

    Expression* initExpression = nullptr;
    if (lexer.getCurrentToken().isOperator(Operator::Assignment)) {
        lexer.consumeToken();
        initExpression = parseExpression();
    }

    VariableDeclarationStatement* varDecl = nullptr;
    if (patternExpression != nullptr) {
        varDecl = new VariableDeclarationStatement(type,
                                                   patternExpression,
                                                   initExpression,
                                                   identifier.getLocation());
    } else {
        varDecl = new VariableDeclarationStatement(type,
                                                   identifier.getValue(),
                                                   initExpression,
                                                   identifier.getLocation());
    }
    varDecl->setAddToNameBindingsWhenTypeChecked(true);
    tree.addStatement(varDecl);
}

Type* Parser::parseType() {
    Type* type = nullptr;
    const Token& token = lexer.consumeToken();
    if (token.isIdentifier()) {
        type = new Type(token.getValue());
    } else if (token.isKeyword()) {
        Keyword::KeywordType keyword = token.getKeyword();
        switch (keyword) {
            case Keyword::Let:
            case Keyword::Var: 
                type = parseTypeName();
                if (keyword == Keyword::Var)
                {
                    type->setConstant(false);
                }
                break;
            default:
                type = Keyword::toType(keyword);
                if (type == nullptr) {
                    error("Expected type.", token);
                }
                break;
        }
    } else {
        error("Expected type.", token);    
    }

    if (lexer.getCurrentToken().isOperator(Operator::Less)) {
        parseGenericTypeParameters(type);
    }
    if (lexer.getCurrentToken().isOperator(Operator::OpenBracket)) {
        parseArrayType(type);
    }
    return type;
}

Type* Parser::parseTypeName() {
    Type* type = nullptr;
    const Token& token = lexer.getCurrentToken();
    if (token.isKeyword()) {
        type = Keyword::toType(token.getKeyword());
        if (type == nullptr) {
            error("Expected type.", token);
        }
        lexer.consumeToken();
    } else if (token.isIdentifier()) {
        // Now we must peek one token ahead to figure out if the 
        // identifier is a type name or variable name.
        const Token& secondToken = lexer.peekToken();
        if (secondToken.isIdentifier() ||
            secondToken.isOperator(Operator::OpenBracket) ||
            secondToken.isOperator(Operator::Less)) {
            // The identifer is a type name.
            type = new Type(token.getValue());
            lexer.consumeToken();
        } else {
            // Not a type name. The type is implicit.
            type = new Type(Type::Implicit);
        }
    } else {
        error("Expected type.", token);
    }
    return type;
}

void Parser::parseGenericTypeParameters(Type* type) {
    assert(lexer.consumeToken().isOperator(Operator::Less));

    CommaSeparatedListParser listParser(lexer, Operator::Greater);
    while (listParser.parseComma()) {
        Type* typeParameter = parseType();
        type->addGenericTypeParameter(typeParameter);
    }
}

void Parser::parseArrayType(Type* type) {
    assert(lexer.getCurrentToken().isOperator(Operator::OpenBracket));

    lexer.consumeToken();
    const Token& token = lexer.consumeToken();
    if (!token.isOperator(Operator::CloseBracket))
    {
        error("Expected ']'.", token);
    }
    type->setArray(true);
}

void Parser::parseExpressionStatement() {
    Expression* expression = parseExpression();
    tree.addStatement(expression);
}

Expression* Parser::parseExpression(
    bool rangeAllowed,
    bool patternAllowed,
    Operator::PrecedenceType leftPrecedence) {

    Expression* left = parseSubexpression(patternAllowed);

    while (true) {
        const Token& currentToken = lexer.getCurrentToken();
        if (!currentToken.isOperator()) {
            return left;
        }

        Operator::OperatorType op = currentToken.getOperator();
        if (op == Operator::Range && !rangeAllowed) {
            error("Unexpected operator '...'.", currentToken);
        }

        Operator::PrecedenceType rightPrecedence = Operator::precedence(op);
        if (rightPrecedence == Operator::NoPrecedence) {
            return left;
        }

        if (leftPrecedence >= rightPrecedence) {
            return left;
        }

        lexer.consumeToken();
        Expression* right = parseExpression(false,
                                            patternAllowed,
                                            rightPrecedence);
        left = new BinaryExpression(op,
                                    left,
                                    right,
                                    currentToken.getLocation());
    }
}

Expression* Parser::parseSubexpression(bool patternAllowed) {
    Expression* expression = nullptr;
    const Token& token = lexer.consumeToken();
    const Location& location = token.getLocation();
    const Value& value = token.getValue();

    if (patternAllowed && Keyword::isType(token.getKeyword())) {
        return parseTypedExpression(value, location);
    }

    switch (token.getType()) {
        case Token::Identifier:
            switch (lexer.getCurrentToken().getOperator())
            {
                case Operator::OpenParentheses:
                    expression = parseMethodCall(token);
                    break;
                case Operator::OpenBracket:
                    expression = parseArraySubscriptExpression(
                        new NamedEntityExpression(value, location));
                    break;
                default:
                    expression = parseUnknownExpression(token, patternAllowed);
                    break;
            }
            break;
        case Token::Char:
            expression = new CharacterLiteralExpression(token.getCharacter(),
                                                        location);
            break;
        case Token::Integer:
            expression = new IntegerLiteralExpression(atoi(value.c_str()),
                                                      location);
            break;
        case Token::Float:
            expression = new FloatLiteralExpression(atof(value.c_str()),
                                                    location);
            break;
        case Token::String:
            expression = new StringLiteralExpression(value, location);
            break;
        case Token::Keyword: 
            switch (token.getKeyword()) {
                case Keyword::True:
                case Keyword::False:
                    expression = parseBooleanLiteralExpression(token);
                    break;
                case Keyword::This:
                    expression = new ThisExpression(location);
                    break;
                case Keyword::New:
                    expression = parseNewExpression();
                    break;
                case Keyword::Yield:
                    expression = parseYieldExpression(location);
                    break;
                case Keyword::Match:
                    expression = parseMatchExpression(location);
                    break;
                default:
                    error("Expected expression.", token);
                    break;
            }
            break;
        case Token::Operator:
            switch (token.getOperator()) {
                case Operator::Increment:
                case Operator::Decrement:
                case Operator::Addition:
                case Operator::Subtraction:
                case Operator::LogicalNegation:
                    expression = parseUnaryExpression(token, nullptr);
                    break;
                case Operator::OpenBracket:
                    expression = parseArrayLiteralExpression();
                    break;
                case Operator::OpenParentheses:
                    expression = parseParenthesesOrTypeCastExpression();
                    break;
                case Operator::Placeholder:
                    expression = new PlaceholderExpression(location);
                    break;
                case Operator::Wildcard:
                    expression = new WildcardExpression(location);
                    break;
                default:
                    error("Unexpected token.", token);
                    break;
            }
            break;
        default:
            error("Expected expression.", token);
            break;
    }

    const Token& currentToken = lexer.getCurrentToken();
    switch (currentToken.getOperator()) {
        case Operator::Dot:
            expression = parseMemberSelectorExpression(expression);
            break;
        case Operator::Increment:
        case Operator::Decrement:
        case Operator::LogicalNegation:
            expression = parseUnaryExpression(currentToken, expression);
            break;
        case Operator::OpenBracket:
            expression = parseArraySubscriptExpression(expression);
            break;
        default:
            break;
    }

    return expression;
}

Expression* Parser::parseUnaryExpression(
    const Token& operatorToken,
    Expression* operand) {

    bool prefix;
    if (operand == nullptr) {
        operand = parseSubexpression();
        prefix = true;
    } else {
        prefix = false;
        lexer.consumeToken();
    }
    return new UnaryExpression(operatorToken.getOperator(),
                               operand,
                               prefix,
                               operatorToken.getLocation());
}

Expression* Parser::parseMemberSelectorExpression(Expression* left) {
    const Token& currentToken = lexer.getCurrentToken();
    assert(currentToken.isOperator(Operator::Dot));

    lexer.consumeToken();
    Expression* right = parseSubexpression();
    return new MemberSelectorExpression(left,
                                        right,
                                        currentToken.getLocation());
}

Expression* Parser::parseBooleanLiteralExpression(const Token& consumedToken) {
    bool boolValue = false;
    switch (consumedToken.getKeyword()) {
        case Keyword::True:
            boolValue = true;
            break;
        case Keyword::False:
            boolValue = false;
            break;
        default:
            assert(0);
            break;
    }
    return new BooleanLiteralExpression(boolValue, consumedToken.getLocation());
}

Expression* Parser::parseArrayLiteralExpression() {
    ArrayLiteralExpression* arrayLiteral =
        new ArrayLiteralExpression(location());

    lexer.stepBack();
    parseExpressionList(arrayLiteral->getElements(),
                        Operator::OpenBracket,
                        Operator::CloseBracket);
    return arrayLiteral;
}

bool Parser::typeCastExpressionStartsHere() {
    const Token& token = lexer.getCurrentToken();
    switch (token.getType()) {
        case Token::Keyword:
            return Keyword::isType(token.getKeyword());
        case Token::Identifier:
        {
            const Token& secondToken = lexer.peekToken();
            if (secondToken.isOperator(Operator::Less) ||
                secondToken.isOperator(Operator::CloseParentheses)) {
                return true;
            }
            lexer.consumeToken();
            const Token& thirdToken = lexer.peekToken();
            lexer.stepBack();
            if (secondToken.isOperator(Operator::OpenBracket) &&
                thirdToken.isOperator(Operator::CloseBracket)) {
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

Expression* Parser::parseParenthesesOrTypeCastExpression() {
    Expression* expression = nullptr;
    if (typeCastExpressionStartsHere()) {
        expression = parseTypeCastExpression();
    } else {
        expression = parseExpression();
        const Token& token = lexer.consumeToken();
        if (!token.isOperator(Operator::CloseParentheses)) {
            error("Expected ')'.", token);
        }
    }
    return expression;
}

Expression* Parser::parseTypeCastExpression() {
    Type* type = parseType();

    const Token& token = lexer.consumeToken();
    if (!token.isOperator(Operator::CloseParentheses)) {
        error("Expected ')'.", token);
    }

    Expression* expression = parseSubexpression();
    TypeCastExpression* typeCast = new TypeCastExpression(type,
                                                          expression,
                                                          token.getLocation());
    typeCast->setGenerated(false);
    return typeCast;
}

Expression* Parser::parseMethodCall(const Token& name) {
    MethodCallExpression* methodCall =
        new MethodCallExpression(name.getValue(), name.getLocation());
    parseExpressionList(methodCall->getArguments(),
                        Operator::OpenParentheses,
                        Operator::CloseParentheses);
    if (lambdaExpressionStartsHere()) {
        parseLambdaExpression(methodCall);
    }
    return methodCall;
}

bool Parser::lambdaExpressionStartsHere() {
    if (lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
        const Token& nextToken = lexer.peekToken();
        if (nextToken.isOperator(Operator::VerticalBar) ||
            nextToken.isOperator(Operator::LogicalOr)) {
            return true;
        }
    }
    return false;
}

void Parser::parseLambdaExpression(MethodCallExpression* methodCall) {
    assert(lexer.getCurrentToken().isOperator(Operator::OpenBrace));

    BlockStatement* body = tree.startBlock(location());
    LambdaExpression* lambda = new LambdaExpression(body, location());
    lexer.consumeToken();

    switch (lexer.getCurrentToken().getOperator()) {
        case Operator::VerticalBar:
            parseLambdaArguments(lambda);
            break;
        case Operator::LogicalOr:
            lexer.consumeToken();
            break;
        default:
            error("Expected '|'.", lexer.getCurrentToken());
            break;
    }

    while (!lexer.getCurrentToken().isOperator(Operator::CloseBrace)) {
        parseStatement();
        expectCloseBraceOrNewline();
    }
    lexer.consumeToken();

    tree.finishBlock();
    methodCall->setLambda(lambda);
}

void Parser::parseLambdaArguments(LambdaExpression* lambda) {
    assert(lexer.consumeToken().isOperator(Operator::VerticalBar));

    CommaSeparatedListParser listParser(lexer, Operator::VerticalBar);
    while (listParser.parseComma()) {
        Type* type = nullptr;
        const Token& nextToken = lexer.peekToken();
        if (lexer.getCurrentToken().isIdentifier() && 
            (nextToken.isOperator(Operator::Comma) ||
             nextToken.isOperator(Operator::VerticalBar))) {
            type = new Type(Type::Implicit);            
        } else {
            type = parseType();
        }

        const Token& identifier = lexer.consumeToken();
        if (!identifier.isIdentifier()) {
            error("Expected identifier.", identifier);
        }

        VariableDeclarationStatement* argument =
            new VariableDeclarationStatement(
                type,
                identifier.getValue(),
                nullptr,
                identifier.getLocation());
        lambda->addArgument(argument);
    }
}

Expression* Parser::parseUnknownExpression(
    const Token& previousToken,
    bool patternAllowed) {

    const Identifier& previousTokenValue = previousToken.getValue();
    const Location& loc = previousToken.getLocation();
    const Token& currentToken = lexer.getCurrentToken();

    if (lambdaExpressionStartsHere()) {
        MethodCallExpression* methodCall =
            new MethodCallExpression(previousTokenValue, loc);
        parseLambdaExpression(methodCall);
        return methodCall;
    } else if (currentToken.isOperator(Operator::OpenBrace) && patternAllowed) {
        return parseClassDecompositionExpression(previousTokenValue, loc);
    } else if (couldBeTypedExpressionResultName() && patternAllowed) {
        return parseTypedExpression(previousTokenValue, loc);
    } else {
        return new NamedEntityExpression(previousTokenValue, loc);
    }
}

Expression* Parser::parseNewExpression() {
    Identifier typeName;
    const Token& typeNameToken = lexer.consumeToken();
    const Location& typeNameLocation = typeNameToken.getLocation();
    if (typeNameToken.isIdentifier() || 
        (typeNameToken.isKeyword() &&
         Keyword::isType(typeNameToken.getKeyword()))) {
        typeName = typeNameToken.getValue();
    } else {
        error("Expected type or identifier.", typeNameToken);
    }

    Type* type = Type::create(typeName);
    if (lexer.getCurrentToken().isOperator(Operator::Less)) {
        parseGenericTypeParameters(type);
    }

    HeapAllocationExpression* heapAllocation = nullptr;
    switch (lexer.getCurrentToken().getOperator()) {
        case Operator::OpenParentheses: {
            MethodCallExpression* constructorCall =
                new MethodCallExpression(typeName, typeNameLocation);
            parseExpressionList(constructorCall->getArguments(),
                                Operator::OpenParentheses,
                                Operator::CloseParentheses);
            heapAllocation = new HeapAllocationExpression(type,
                                                          constructorCall);
            break;
        }
        case Operator::OpenBracket: {
            Expression* arrayCapacityExpression =
                parseArrayIndexExpression(true);
            return new ArrayAllocationExpression(type,
                                                 arrayCapacityExpression,
                                                 typeNameLocation);
        }
        default: {
            MethodCallExpression* constructorCall =
                new MethodCallExpression(typeName, typeNameLocation);
            heapAllocation = new HeapAllocationExpression(type,
                                                          constructorCall);
            break;
        }
    }

    if (lexer.getCurrentToken().isKeyword(Keyword::Named)) {
        lexer.consumeToken();
        heapAllocation->setProcessName(parseExpression());
    }

    return heapAllocation;
}

Expression* Parser::parseYieldExpression(const Location& location) {
    YieldExpression* yieldExpression = new YieldExpression(location);
    if (lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        parseExpressionList(yieldExpression->getArguments(),
                            Operator::OpenParentheses,
                            Operator::CloseParentheses);
    }
    return yieldExpression;
}

ConstructorCallStatement* Parser::parseConstructorCall() {
    const Token& baseClassOrInit = lexer.consumeToken();
    if (!baseClassOrInit.isKeyword(Keyword::Init) &&
        !baseClassOrInit.isIdentifier()) {
        error("Expected identifier or 'init'.", baseClassOrInit);
    }

    if (!lexer.getCurrentToken().isOperator(Operator::OpenParentheses)) {
        error("Expected '('.", lexer.getCurrentToken());
    }

    MethodCallExpression* constructorCall =
        new MethodCallExpression(baseClassOrInit.getValue(), location());
    parseExpressionList(constructorCall->getArguments(),
                        Operator::OpenParentheses,
                        Operator::CloseParentheses);
    return new ConstructorCallStatement(constructorCall);
}

Expression* Parser::parseArrayIndexExpression(bool isIndexOptional) {
    assert(lexer.getCurrentToken().isOperator(Operator::OpenBracket));

    lexer.consumeToken();
    if (isIndexOptional &&
        lexer.getCurrentToken().isOperator(Operator::CloseBracket)) {
        lexer.consumeToken();
        return nullptr;
    }

    Expression* expression = parseExpression(true);
    const Token& token = lexer.consumeToken();
    if (!token.isOperator(Operator::CloseBracket)) {
        error("Expected ']'.", token);
    }

    return expression;
}

Expression* Parser::parseArraySubscriptExpression(
    Expression* arrayNameExpression) {

    assert(lexer.getCurrentToken().isOperator(Operator::OpenBracket));

    if (lexer.previousTokenWasNewline()) {
        return arrayNameExpression;
    }

    Expression* arrayIndexExpression = parseArrayIndexExpression();
    return new ArraySubscriptExpression(arrayNameExpression,
                                        arrayIndexExpression);
}

NamedEntityExpression* Parser::parseNamedEntityExpression() {
    const Token& name = lexer.consumeToken();
    if (!name.isIdentifier()) {
        error("Expected identifier.", name);
    }

    return new NamedEntityExpression(name.getValue(), name.getLocation());
}

Expression* Parser::parseMatchExpression(const Location& location) {
    Expression* subject = parseExpression();
    MatchExpression* matchExpression = new MatchExpression(subject, location);

    const Token& openBrace = lexer.consumeToken();
    if (!openBrace.isOperator(Operator::OpenBrace)) {
        error("Expected '{'.", openBrace);
    }

    CommaSeparatedListParser listParser(lexer, Operator::CloseBrace);
    while (listParser.parseComma()) {
        matchExpression->addCase(parseMatchCase());
    }

    return matchExpression;
}

MatchCase* Parser::parseMatchCase() {
    MatchCase* matchCase = new MatchCase(location());
    parseMatchCasePatterns(matchCase);

    const Token& arrowToken = lexer.consumeToken();
    if (!arrowToken.isOperator(Operator::Arrow)) {
        error("Expected '->'.", arrowToken);
    }

    matchCase->setResultBlock(parseBlock(true, true));
    return matchCase;
}

void Parser::parseMatchCasePatterns(MatchCase* matchCase) {
    while (true) {
        matchCase->addPatternExpression(parseSubexpression(true));

        const Token& token = lexer.getCurrentToken();
        if (token.isOperator(Operator::Arrow)) {
            break;
        } else if (token.isKeyword(Keyword::If)) {
            lexer.consumeToken();
            matchCase->setPatternGuard(parseExpression());
            break;
        } else if (token.isOperator(Operator::VerticalBar)) {
            lexer.consumeToken();
        } else {
            error("Expected '|'.", token);
        }
    }
}

Expression* Parser::parseClassDecompositionExpression(
    const Identifier& typeName,
    const Location& loc) {

    assert(lexer.consumeToken().isOperator(Operator::OpenBrace));

    ClassDecompositionExpression* classDecomposition =
        new ClassDecompositionExpression(Type::create(typeName), loc);

    CommaSeparatedListParser listParser(lexer, Operator::CloseBrace);
    while (listParser.parseComma()) {
        NamedEntityExpression* memberName = parseNamedEntityExpression();

        Expression* memberPattern = nullptr;
        if (lexer.getCurrentToken().isOperator(Operator::Colon)) {
            lexer.consumeToken();
            memberPattern = parseExpression(false, true);
        }

        classDecomposition->addMember(memberName, memberPattern);
    }
    return classDecomposition;
}

Expression* Parser::parseTypedExpression(
    const Identifier& typeName,
    const Location& loc) {

    if (!couldBeTypedExpressionResultName()) {
        error("Expected identifier or '_'.", lexer.getCurrentToken());
    }

    Expression* resultName = parseSubexpression();
    return new TypedExpression(Type::create(typeName), resultName, loc);
}

bool Parser::couldBeTypedExpressionResultName()
{
    const Token& currentToken = lexer.getCurrentToken();
    if (currentToken.isIdentifier() ||
        currentToken.isOperator(Operator::Placeholder)) {
        return true;
    }

    return false;
}

void Parser::parseIfStatement() {
    const Token& ifToken = lexer.consumeToken();
    assert(ifToken.isKeyword(Keyword::If));

    if (lexer.getCurrentToken().isKeyword(Keyword::Let)) {
        parseOptionalBinding(ifToken.getLocation());
    } else {
        Expression* expression = parseExpression();
        BlockStatement* block = parseBlock();
        BlockStatement* elseBlock = nullptr;

        const Token& token = lexer.getCurrentToken();
        if (token.isKeyword(Keyword::Else)) {
            lexer.consumeToken();
            elseBlock = parseBlock();
        }

        IfStatement* ifStatement = new IfStatement(expression,
                                                   block,
                                                   elseBlock,
                                                   ifToken.getLocation());
        tree.addStatement(ifStatement);
    }
}

void Parser::parseOptionalBinding(const Location& location) {
    assert(lexer.consumeToken().isKeyword(Keyword::Let));

    Expression* pattern = parseSubexpression(true);

    const Token& assignment = lexer.consumeToken();
    if (!assignment.isOperator(Operator::Assignment)) {
        error("Expected '='.", assignment);
    }

    Expression* subject = parseExpression();
    MatchExpression* matchExpression = new MatchExpression(subject, location);
    matchExpression->setGenerated(true);

    MatchCase* matchCase = new MatchCase(location);
    matchCase->addPatternExpression(pattern);
    matchCase->setResultBlock(parseBlock());
    matchExpression->addCase(matchCase);
    tree.addStatement(matchExpression);
}

void Parser::parseWhileStatement() {
    const Token& whileToken = lexer.consumeToken();
    assert(whileToken.isKeyword(Keyword::While));

    Expression* expression = nullptr;
    if (!lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
        expression = parseExpression();
    }
    BlockStatement* block = parseBlock();

    WhileStatement* whileStatement = 
        new WhileStatement(expression, block, whileToken.getLocation()); 
    tree.addStatement(whileStatement);
}

void Parser::parseForStatement() {
    const Token& forToken = lexer.consumeToken();
    assert(forToken.isKeyword(Keyword::For));

    tree.startBlock(location());
    Expression* conditionExpression = nullptr;
    Expression* loopExpression = nullptr;

    if (!lexer.getCurrentToken().isOperator(Operator::OpenBrace)) {
        if (variableDeclarationStartsHere()) {
            parseVariableDeclarationStatement();
        } else {
            Expression* initExpression = parseExpression();
            tree.addStatement(initExpression);
        }
        consumeSingleSemicolon();
        conditionExpression = parseExpression();
        consumeSingleSemicolon();
        loopExpression = parseExpression();
    }

    BlockStatement* loopBlock = parseBlock();
    if (loopExpression != nullptr) {
        loopBlock->addStatement(loopExpression);
    }

    WhileStatement* whileStatement = 
        new WhileStatement(conditionExpression,
                           loopBlock,
                           forToken.getLocation());
    tree.addStatement(whileStatement);

    tree.addStatement(tree.finishBlock());
}

void Parser::consumeSingleSemicolon() {
    const Token& semicolon = lexer.consumeToken();
    if (!semicolon.isOperator(Operator::Semicolon)) {
        error("Expected ';'.", semicolon);
    }
}

void Parser::parseBreakStatement() {
    const Token& breakToken = lexer.consumeToken();
    assert(breakToken.isKeyword(Keyword::Break));

    BreakStatement* breakStatement =
        new BreakStatement(breakToken.getLocation());
    tree.addStatement(breakStatement);
}

void Parser::parseContinueStatement() {
    const Token& continueToken = lexer.consumeToken();
    assert(continueToken.isKeyword(Keyword::Continue));

    ContinueStatement* continueStatement =
        new ContinueStatement(continueToken.getLocation());
    tree.addStatement(continueStatement);
}

void Parser::parseReturnStatement() {
    const Token& returnToken = lexer.consumeToken();
    assert(returnToken.isKeyword(Keyword::Return));

    Expression* expression = nullptr;
    if (!lexer.previousTokenWasNewline() &&
        !lexer.getCurrentToken().isOperator(Operator::CloseBrace)) {
        expression = parseExpression();
    }

    ReturnStatement* returnStatement = 
        new ReturnStatement(expression, returnToken.getLocation()); 
    tree.addStatement(returnStatement);
}

void Parser::parseJumpStatement() {
    const Token& jumpToken = lexer.consumeToken();
    assert(jumpToken.isKeyword(Keyword::Jump));

    const Token& identifier = lexer.consumeToken();
    if (!identifier.isIdentifier()) {
        error("Expected label identifier.", identifier);
    }

    JumpStatement* jumpStatement =
        new JumpStatement(identifier.getValue(), jumpToken.getLocation());
    tree.addStatement(jumpStatement);
}

void Parser::parseLabelStatement() {
    const Token& identifier = lexer.consumeToken();
    assert(identifier.isIdentifier());
    assert(lexer.consumeToken().isOperator(Operator::Colon));

    LabelStatement* labelStatement =
        new LabelStatement(identifier.getValue(), identifier.getLocation());
    tree.addStatement(labelStatement);
}

void Parser::parseUse() {
    assert(lexer.consumeToken().isKeyword(Keyword::Use));

    const Token& namespaceName = lexer.consumeToken();
    if (!namespaceName.isIdentifier()) {
        error("Expected identifier.", namespaceName);
    }

    tree.useNamespace(namespaceName.getValue(), namespaceName.getLocation());
}

void Parser::parseImport() {
    assert(lexer.consumeToken().isKeyword(Keyword::Import));

    const Token& moduleNameToken = lexer.consumeToken();
    if (moduleNameToken.getType() != Token::String) {
        error("Expected string.", moduleNameToken);
    }

    importModule(moduleNameToken.getValue());
}

void Parser::importModule(const std::string& moduleName) {
    std::string filename(moduleName);
    if (filename.find(".b") == std::string::npos) {
        filename.append(".b");
    }

    if (!tree.isModuleAlreadyImported(filename)) {
        tree.addImportedModule(filename);
        if (module != nullptr) {
            module->addDependency(filename);
        }

        if (!File::exists(filename)) {
            filename = "stdlib/" + filename;
        }

        Parser parser(filename, tree, nullptr);
        parser.parse();
    }
}

void Parser::parseExpressionList(
    ExpressionList& expressions,
    Operator::OperatorType firstDelimiter,
    Operator::OperatorType lastDelimiter) {

    assert(lexer.consumeToken().isOperator(firstDelimiter));

    CommaSeparatedListParser listParser(lexer, lastDelimiter);
    while (listParser.parseComma()) {
        expressions.push_back(parseExpression());
    }
}

void Parser::parseIdentifierList(IdentifierList& identifierList) {
    CommaSeparatedListParser listParser(lexer);
    while (listParser.parseComma()) {
        const Token& identifier = lexer.consumeToken();
        if (!identifier.isIdentifier()) {
            error("Expected identifier.", identifier);
        }
        identifierList.push_back(identifier.getValue());
    }
}

void Parser::expectNewline() {
    if (!lexer.previousTokenWasNewline()) {
        error("Expected newline.", lexer.getCurrentToken());
    }
}

void Parser::expectCloseBraceOrNewline() {
    if (!lexer.getCurrentToken().isOperator(Operator::CloseBrace)) {
        expectNewline();
    }
}

const Location& Parser::location() {
    return lexer.getCurrentToken().getLocation();
}

