
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sstream>
#include <memory>

#include "BackEnd.h"
#include "Definition.h"
#include "Statement.h"
#include "Expression.h"

namespace {

    const int inlineThreshold = 0;
    const int indentSize = 4;

    const char space(' ');
    const char newline('\n');
    const char openBrace('{');
    const char closeBrace('}');
    const char openParentheses('(');
    const char closeParentheses(')');
    const char openBracket('[');
    const char closeBracket(']');
    const char colon(':');
    const char semicolon(';');
    const char comma(',');
    const char quote('"');
    const char apostrophe('\'');
    const char backslash('\\');
    const char operatorPlus('+');
    const char operatorMinus('-');
    const char operatorMultiplication('*');
    const char operatorDivision('/');
    const char operatorAssignment('=');
    const char operatorLogicalNegation('!');
    const char operatorGreater('>');
    const char operatorLess('<');
    const char operatorBitwiseNegation('~');
    const char operatorDot('.');

    const std::string semicolonAndNewLine(";\n");
    const std::string operatorEqual("==");
    const std::string operatorNotEqual("!=");
    const std::string operatorArrow("->");
    const std::string operatorScope("::");
    const std::string operatorIncrement("++");
    const std::string operatorDecrement("--");
    const std::string operatorLogicalAnd("&&");
    const std::string operatorLogicalOr("||");
    const std::string operatorGreaterOrEqual(">=");
    const std::string operatorLessOrEqual("<=");

    const std::string keywordChar("char");
    const std::string keywordUnsignedChar("unsigned char");
    const std::string keywordInt("int");
    const std::string keywordFloat("float");
    const std::string keywordIf("if");
    const std::string keywordElse("else");
    const std::string keywordClass("class");
    const std::string keywordStatic("static");
    const std::string keywordPublic("public");
    const std::string keywordVoid("void");
    const std::string keywordBool("bool");
    const std::string keywordTrue("true");
    const std::string keywordFalse("false");
    const std::string keywordWhile("while");
    const std::string keywordBreak("break");
    const std::string keywordContinue("continue");
    const std::string keywordReturn("return");
    const std::string keywordNew("new");
    const std::string keywordVirtual("virtual");
    const std::string keywordEnum("enum");
    const std::string keywordThis("this");
    const std::string keywordExplicit("explicit");
    const std::string keywordThreadLocal("thread_local");
    const std::string keywordStaticCast("static_cast");
    const std::string keywordGoto("goto");

    const std::string null("NULL");
    const std::string ifNotDef("#ifndef ");
    const std::string endIf("#endif\n");
    const std::string define("#define ");
    const std::string include("#include ");
    const std::string includeRuntime("#include <Runtime.h>\n");

    const std::string pointerClassName("Pointer");
    const std::string arrayClassName("Array");
    const std::string stringName("string");
    const std::string objectName("object");
    const std::string dynamicPointerCastName("dynamicPointerCast");
    const std::string staticPointerCastName("staticPointerCast");
    const std::string arrayAtName("at");

    void replace(Identifier& value, const Identifier& what, char with) {
        while (true) {
            size_t position = value.find(what);
            if (position != std::string::npos) {
                value.replace(position, what.length(), 1, with);
            } else {
                break;
            }
        }
    }

    Identifier mangle(const Identifier& symbol) {
        Identifier mangledSymbol(symbol);
        replace(mangledSymbol, ",", '_');
        replace(mangledSymbol, "<", '_');
        replace(mangledSymbol, ">", '_');
        replace(mangledSymbol, "$", '_');
        return mangledSymbol;
    }

    Identifier eraseInitFromConstructorName(const Identifier& name) {
        Identifier retval(name);
        Identifier toBeErased("_" + Keyword::initString);
        size_t toBeErasedSize = toBeErased.size();
        size_t position = retval.find_last_of(toBeErased) - toBeErasedSize + 1;
        return retval.erase(position, toBeErasedSize);
    }
}

CppBackEnd::CppBackEnd(Tree& t, const std::string& mName) :
    tree(t), 
    moduleName(mName),
    implementationMode(false), 
    headerOutput(), 
    implementationOutput(),
    output(nullptr) {

    setHeaderMode();
}

void CppBackEnd::generate(const std::vector<std::string>& dependencies) {
    generateIncludeGuardBegin();
    generateIncludes(dependencies);

    MethodDefinition* mainMethod = tree.getMainMethod();
    if (mainMethod != nullptr) {
        mainMethod->setName("_" + mainMethod->getName() + "_");
    }

    generateDefinitions(tree.getGlobalDefinitions());

    generateIncludeGuardEnd();

    if (mainMethod != nullptr) {
        generateMainMethod(mainMethod);
    }
}

void CppBackEnd::generateIncludeGuardBegin() {
    setHeaderMode();
    std::string guard(moduleName);
    std::size_t slashPosition = guard.find('/');
    if (slashPosition != std::string::npos) {
        guard.replace(slashPosition, 1, 1 , '_');
    }
    guard += "_h\n";
    generateCpp(ifNotDef);
    generateCpp(guard);
    generateCpp(define);
    generateCpp(guard);
    generateNewline();
}

void CppBackEnd::generateIncludeGuardEnd() {
    setHeaderMode();
    generateCpp(endIf);
    generateNewline();
}

void CppBackEnd::generateIncludes(
    const std::vector<std::string>& dependencies) {

    setHeaderMode();
    generateCpp(includeRuntime);
    generateNewline();

    for (std::vector<std::string>::const_iterator i = dependencies.begin();
         i != dependencies.end();
         i++) {
        const std::string& dependency = *i;
        generateInclude(dependency);
    }

    generateNewline();
    setImplementationMode();
    generateInclude(moduleName);
    generateNewline();
    setHeaderMode();
}

void CppBackEnd::generateInclude(const std::string& fname) {
    generateCpp(include);
    generateCpp(operatorLess);
    generateCpp(fname + ".h");
    generateCpp(operatorGreater);
    generateNewline();
}

void CppBackEnd::generateMainMethod(const MethodDefinition* mainMethod) {
    setImplementationMode();
    generateNewline();
    generateCpp("int main()\n{");
    increaseIndent();
    generateNewline();
    if (!mainMethod->isFunction()) {
        const Identifier& className =
            mainMethod->getEnclosingClass()->getName();
        generateCpp(mangle(className));
        generateCpp(operatorScope);
    }
    generateCpp(mainMethod->getName());
    generateCpp(openParentheses);
    generateCpp(closeParentheses);
    generateSemicolonAndNewline();
    generateCpp(keywordReturn);
    generateCpp(space);
    generateCpp('0');
    generateCpp(semicolon);
    decreaseIndent();
    generateNewline();
    generateCpp(closeBrace);
    generateNewline();
}

void CppBackEnd::generateForwardDeclaration(
    const ForwardDeclarationDefinition* forwardDeclaration) {

    setHeaderMode();
    generateCpp(keywordClass);
    generateCpp(space);
    generateCpp(mangle(forwardDeclaration->getName()));
    generateSemicolonAndNewline();
    generateNewline();
}

void CppBackEnd::generateDefinitions(const DefinitionList& definitions) {
    for (DefinitionList::const_iterator i = definitions.begin();
         i != definitions.end();
         i++) {
        const Definition* definition = *i;
        if (definition->isImported()) {
            // Don't generate code from imported definitions.
            continue;
        }
        switch (definition->getDefinitionType()) {
            case Definition::Class:
                generateClass(definition->cast<ClassDefinition>());
                break;
            case Definition::Member:
                generateMethod(definition->cast<ClassMemberDefinition>()->
                               cast<MethodDefinition>());
                break;
            case Definition::ForwardDeclaration:
                generateForwardDeclaration(
                    definition->cast<ForwardDeclarationDefinition>());
                break;
            default:
                internalError("generateDefinitions");
                break;
        }
    }
}

void CppBackEnd::generateClass(const ClassDefinition* classDef) {
    if (classDef->isGeneric()) {
        return;
    }
    setHeaderMode();
    generateCpp(keywordClass);
    generateCpp(space);
    generateCpp(mangle(classDef->getName()));
    if (!classDef->isEnumeration() && !classDef->isEnumerationVariant()) {
        generateClassParentList(classDef);
    }
    generateNewline();
    generateCpp(openBrace);
    generateNewline();
    generateCpp(keywordPublic);
    generateCpp(colon);
    increaseIndent();
    generateNewline();
    if (classDef->isInterface()) {
        generateVirtualDestructor(classDef);
    }
    generateClassMembers(classDef);
    decreaseIndent();
    eraseLastChars(indentSize);
    generateCpp(closeBrace);
    generateSemicolonAndNewline();
    generateNewline();
}

void CppBackEnd::generateClassParentList(const ClassDefinition* classDef) {
    generateCpp(colon);
    const ClassList& parents = classDef->getParentClasses();
    if (parents.empty()) {
        generateClassParent(objectName);
    } else {
        for (ClassList::const_iterator i = parents.begin();
             i != parents.end(); ) {
            const ClassDefinition* parent = *i;
            generateClassParent(mangle(parent->getName()));
            if (++i != parents.end()) {
                generateCpp(comma);
            }
        }
    }
}

void CppBackEnd::generateClassParent(const Identifier& parentName) {
    generateCpp(space);
    generateCpp(keywordPublic);
    generateCpp(space);
    if (parentName.compare(objectName) == 0) {
        generateCpp(keywordVirtual);
        generateCpp(space);
    }
    generateCpp(parentName);
}

void CppBackEnd::generateClassMembers(const ClassDefinition* classDef) {
    const DefinitionList& memberDefinitions = classDef->getMembers();
    for (DefinitionList::const_iterator i = memberDefinitions.begin();
         i != memberDefinitions.end();
         i++) {
        const Definition* definition = *i;
        switch (definition->getDefinitionType()) {
            case Definition::Member:
                generateClassMember(definition->cast<ClassMemberDefinition>());
                break;
            case Definition::Class:
                generateClass(definition->cast<ClassDefinition>());
                break;
            case Definition::ForwardDeclaration:
                generateForwardDeclaration(
                    definition->cast<ForwardDeclarationDefinition>());
                break;
            default:
                internalError("generateClassMembers");
                break;
        }
    }
}

void CppBackEnd::generateVirtualDestructor(const ClassDefinition* classDef) {
    generateCpp(keywordVirtual);
    generateCpp(space);
    generateCpp(operatorBitwiseNegation);
    generateCpp(mangle(classDef->getName()));
    generateCpp(openParentheses);
    generateCpp(closeParentheses);
    generateCpp(space);
    generateCpp(openBrace);
    generateCpp(closeBrace);
    generateNewline();
}

void CppBackEnd::generateClassMember(const ClassMemberDefinition* member) {
    switch (member->getMemberType()) {
        case ClassMemberDefinition::Method:          
            generateMethod(member->cast<MethodDefinition>());
            break;
        case ClassMemberDefinition::DataMember:    
            generateDataMember(member->cast<DataMemberDefinition>());
            break;
        default:
            internalError("generateClassMember");
    }
}

void CppBackEnd::generateMethod(const MethodDefinition* method) {
    if (method->getLambdaSignature() != nullptr) {
        // Do not need to generate method since it will only be inlined in the
        // calling methods.
        return;
    }
    generateMethodSignature(method);
    if (method->isAbstract()) {
        return;
    }
    BlockStatement* body = method->getBody();
    if (body->getStatements().size() >= inlineThreshold ||
        method->isFunction()) {
        generateSemicolonAndNewline();
        setImplementationMode();
        generateMethodSignature(method);
    }
    generateCpp(space);
    if (method->isConstructor()) {
        BlockStatement::StatementList& statements = body->getStatements();
        if (!statements.empty()) {
            const Statement* firstStatement = *statements.begin();
            if (firstStatement->getStatementType() ==
                Statement::ConstructorCall) {
                const ConstructorCallStatement* constructorCall =
                    firstStatement->cast<ConstructorCallStatement>();
                generateCpp(colon);
                generateCpp(space);
                generateMethodCall(constructorCall->getMethodCallExpression());
                generateCpp(space);
                statements.pop_front();
            }
        }
    }
    generateNewline();
    generateBlock(body);
    generateNewline();
    setHeaderMode();
}

void CppBackEnd::generateMethodSignature(const MethodDefinition* method) {
    generateNewline();
    if (method->isStatic() && !method->isFunction() && !implementationMode) {
        generateCpp(keywordStatic);
        generateCpp(space);
    }
    if (method->isAbstract()) {
        generateCpp(keywordVirtual);
        generateCpp(space);    
    }
    if (method->isConstructor()) {
        if (!implementationMode && method->getArgumentList().size() == 1 &&
            !method->getClass()->isEnumeration()) {
            generateCpp(keywordExplicit);
            generateCpp(space);
        }
    } else {
        generateType(method->getReturnType());
    }
    if (implementationMode && !method->isFunction()) {
        generateScope(method->getEnclosingDefinition());
    }
    Identifier name = method->getName();
    if (method->isConstructor()) {
        name = eraseInitFromConstructorName(name);
    }
    generateCpp(mangle(name));
    generateArgumentList(method->getArgumentList());
    if (method->isAbstract()) {
        generateCpp(space);    
        generateCpp(operatorAssignment);
        generateCpp(space);    
        generateCpp("0");    
        generateSemicolonAndNewline();
    }
}

void CppBackEnd::generateScope(const Definition* enclosing) {
    std::string scope;
    while (enclosing != nullptr) {
        std::string subscope(mangle(enclosing->getName()));
        subscope += colon;
        subscope += colon;
        scope = subscope + scope;
        enclosing = enclosing->getEnclosingDefinition();
    }
    generateCpp(scope);
}

void CppBackEnd::generateArgumentList(const ArgumentList& arguments) {
    generateCpp(openParentheses);
    for (ArgumentList::const_iterator i = arguments.begin();
         i != arguments.end(); ) {
        const VariableDeclaration* argument = *i;
        const Type* type = argument->getType();
        if (type->isLambda()) {
            break;
        }
        generateType(type); 
        generateCpp(mangle(argument->getIdentifier()));
        if (++i != arguments.end()) {
            generateCpp(comma);
            generateCpp(space);
        }
    }
    generateCpp(closeParentheses);
}

void CppBackEnd::generateDataMember(const DataMemberDefinition* dataMember) {
    generateNewline();
    if (dataMember->isStatic()) {
        generateCpp(keywordStatic);
        generateCpp(space);
        generateThreadLocal(dataMember->getType());
    }
    const Type* type = dataMember->getType();
    assert(type);
    generateType(type);
    generateCpp(mangle(dataMember->getName()));
    generateSemicolonAndNewline();
    if (dataMember->isStatic()) {
        setImplementationMode();
        generateThreadLocal(dataMember->getType());
        generateType(type);
        generateScope(dataMember->getEnclosingDefinition());
        generateCpp(mangle(dataMember->getName()));
        Expression* init = dataMember->getExpression();
        if (init) {
            generateCpp(space);
            generateCpp(operatorAssignment);
            generateCpp(space);
            generateExpression(init);
        }
        generateSemicolonAndNewline();
        setHeaderMode();
    }
}

void CppBackEnd::generateThreadLocal(const Type* type) {
    if (type->isConstant() && type->isPrimitive()) {
        return;
    }
    generateCpp(keywordThreadLocal);
    generateCpp(space);
}

void CppBackEnd::generateBlock(const BlockStatement* block) {
    generateCpp(openBrace);
    increaseIndent();
    generateNewline();

    const Statement* statement = nullptr;
    const BlockStatement::StatementList& statements = block->getStatements();
    for (BlockStatement::StatementList::const_iterator i = statements.begin(); 
         i != statements.end(); 
         i++) {
        statement = *i;
        generateStatement(statement);
    }

    if (statement != nullptr &&
        statement->getStatementType() != Statement::Block) {
        eraseLastChars(indentSize);
    } 
    generateCpp(closeBrace);
    decreaseIndent();
}

void CppBackEnd::generateStatement(const Statement* statement) {
    switch (statement->getStatementType()) {
        case Statement::Block:
            generateBlock(statement->cast<BlockStatement>());
            generateNewline();
            break;
        case Statement::VarDeclaration:
            generateVariableDeclaration(
                statement->cast<VariableDeclarationStatement>());
            break;
        case Statement::ExpressionStatement:
            generateExpressionStatement(statement->cast<Expression>());
            break;
        case Statement::If:
            generateIfStatement(statement->cast<IfStatement>());
            break;
        case Statement::While:
            generateWhileStatement(statement->cast<WhileStatement>());
            break;
        case Statement::Break:
            generateBreakStatement();
            break;
        case Statement::Continue:
            generateContinueStatement();
            break;
        case Statement::Return:
            generateReturnStatement(statement->cast<ReturnStatement>());
            break;
        case Statement::Label:
            generateLabelStatement(statement->cast<LabelStatement>());
            break;
        case Statement::Jump:
            generateJumpStatement(statement->cast<JumpStatement>());
            break;
        default:
            internalError("generateStatement");
    }
}

void CppBackEnd::generateVariableDeclaration(
    const VariableDeclarationStatement* varDecl) {

    generateType(varDecl->getType()); 
    generateCpp(mangle(varDecl->getIdentifier()));
    const Expression* init = varDecl->getInitExpression();
    if (init) {
        generateCpp(space);
        generateCpp(operatorAssignment);
        generateCpp(space);
        generateExpression(init);
    }
    generateSemicolonAndNewline();
}

//
// A a = new A()
// C++:
// Pointer<A> a = new A();
//
// A[] a = new A[3]
// C++:
// Pointer<Array<Pointer<A>>> a =
//     Pointer<Array<Pointer<A>>>(new Array<Pointer<A>>(3));
//
void CppBackEnd::generateType(const Type* type, bool generatePointer) {
    if (type->isReference()) {
        if (generatePointer) {
            generateCpp(pointerClassName);
            generateCpp(operatorLess);
        }
        if (type->isArray()) {
            generateArrayType(type);
            generateCpp(space);
        } else {
            generateTypeName(type);
        }
        if (generatePointer) {
            generateCpp(operatorGreater);
        }
    } else {
        generateTypeName(type);    
    }
    generateCpp(space);
}

void CppBackEnd::generateArrayType(const Type* type) {
    generateCpp(arrayClassName);
    generateCpp(operatorLess);
    std::unique_ptr<Type> elementType(Type::createArrayElementType(type));
    generateType(elementType.get());
    generateCpp(operatorGreater);
}

void CppBackEnd::generateTypeName(const Type* type) {
    switch (type->getBuiltInType()) {
        case Type::NotBuiltIn:
        case Type::Enumeration:
            generateNonBuiltInTypeName(type);
            break;
        case Type::Void:
            generateCpp(keywordVoid);
            break;
        case Type::Byte:
            generateCpp(keywordUnsignedChar);
            break;
        case Type::Char:
            generateCpp(keywordChar);
            break;
        case Type::Integer:
            generateCpp(keywordInt);
            break;
        case Type::Float:
            generateCpp(keywordFloat);
            break;
        case Type::Boolean:
            generateCpp(keywordBool);
            break;
        case Type::String:
            generateCpp(stringName);
            break; 
        case Type::Object:
            generateCpp(objectName);
            break;
        default:
            internalError("generateTypeName"); 
    } 
}

void CppBackEnd::generateNonBuiltInTypeName(const Type* type) {
    generateScope(type->getDefinition()->getEnclosingDefinition());

    if (type->hasGenericTypeParameters()) {
        generateCpp(mangle(type->getFullConstructedName()));
    } else {
        generateCpp(mangle(type->getName()));
    }
}

void CppBackEnd::generateExpressionStatement(const Expression* expression) {
    generateExpression(expression); 
    generateSemicolonAndNewline();
}      

void CppBackEnd::generateExpression(
    const Expression* expression,
    bool generateParentheses) {

    switch (expression->getExpressionType()) {
        case Expression::Literal:          
            generateLiteral(expression->cast<LiteralExpression>());
            break;
        case Expression::Binary:
            generateBinaryExpression(expression->cast<BinaryExpression>(),
                                     generateParentheses);
            break;
        case Expression::Unary:
            generateUnaryExpression(expression->cast<UnaryExpression>());
            break;
        case Expression::Member:
            generateMemberExpression(expression->cast<MemberExpression>());
            break;
        case Expression::MemberSelector:
            generateMemberSelectorExpression(
                expression->cast<MemberSelectorExpression>());
            break;
        case Expression::LocalVariable:
            generateLocalVariableExpression(
                expression->cast<LocalVariableExpression>());
            break;
        case Expression::ClassName:
            generateClassNameExpression(
                expression->cast<ClassNameExpression>());
            break;
        case Expression::HeapAllocation:
            generateHeapAllocationExpression(
                expression->cast<HeapAllocationExpression>());
            break;
        case Expression::ArrayAllocation:
            generateArrayAllocationExpression(
                expression->cast<ArrayAllocationExpression>());
            break;
        case Expression::ArraySubscript:
            generateArraySubscriptExpression(
                expression->cast<ArraySubscriptExpression>());
            break;            
        case Expression::TypeCast:
            generateTypeCastExpression(expression->cast<TypeCastExpression>());
            break;
        case Expression::Null:
            generateNullExpression();
            break;
        case Expression::This:
            generateThisExpression();
            break;
        case Expression::Temporary:
            generateTemporaryExpression(
                expression->cast<TemporaryExpression>());
            break;
        case Expression::WrappedStatement:
            generateStatement(
                expression->cast<WrappedStatementExpression>()->getStatement());
            break;
        default:
            internalError("generateExpression");
    }
}      

void CppBackEnd::generateLiteral(const LiteralExpression* lit) {
    switch (lit->getLiteralType()) {
        case LiteralExpression::Character:
            generateCpp(apostrophe);
            generateChar(lit->cast<CharacterLiteralExpression>()->getValue());
            generateCpp(apostrophe);
            break;
        case LiteralExpression::Integer: {
            std::stringstream out;
            out << lit->cast<IntegerLiteralExpression>()->getValue();
            generateCpp(out.str());
            break;
        }
        case LiteralExpression::Float: {
            std::stringstream out;
            out << lit->cast<FloatLiteralExpression>()->getValue();
            generateCpp(out.str());
            break;
        }
        case LiteralExpression::Boolean:        
            if (lit->cast<BooleanLiteralExpression>()->getValue()) {
                generateCpp(keywordTrue);
            } else {
                generateCpp(keywordFalse);
            }
            break;
        default:
            internalError("generateLiteral");
    }
}

void CppBackEnd::generateChar(char c) {
    switch (c) {
        case '\r':
            generateCpp(backslash);
            generateCpp('r');
            break;
        case '\n':
            generateCpp(backslash);
            generateCpp('n');
            break;
        case '\'':
            generateCpp(backslash);
            generateCpp('\'');
            break;
        case '\\':
            generateCpp(backslash);
            generateCpp(backslash);
            break;
        default:
            generateCpp(c);
            break;
    }
}

void CppBackEnd::generateArrayLiteral(
    const ArrayLiteralExpression* arrayLiteral) {

    generateCpp(keywordNew);
    generateCpp(space);
    generateType(Type::createArrayElementType(arrayLiteral->getType()));
    generateCpp(openBracket);
    const ExpressionList& elements = arrayLiteral->getElements();
    std::stringstream arraySizeStr;
    arraySizeStr << elements.size();
    generateCpp(arraySizeStr.str());
    generateCpp(closeBracket);
    generateCpp(space);
    generateCpp(openBrace);
    for (ExpressionList::const_iterator i = elements.begin();
         i != elements.end(); ) {
        const Expression* element = *i;
        generateExpression(element);
        if (++i != elements.end()) {
            generateCpp(comma);
            generateCpp(space);
        }
    }
    generateCpp(closeBrace);
}

void CppBackEnd::generateBinaryExpression(
    const BinaryExpression* expression,
    bool generateParentheses) {

    if (generateParentheses) {
        generateCpp(openParentheses);
    }
    generateExpression(expression->getLeft(), true);
    generateCpp(space);
    generateExpressionOperator(expression->getOperator());
    generateCpp(space);
    generateExpression(expression->getRight(), true);
    if (generateParentheses) {
        generateCpp(closeParentheses);
    }
}

void CppBackEnd::generateUnaryExpression(const UnaryExpression* expression) {
    if (expression->isPrefix()) {
        generateExpressionOperator(expression->getOperator());
        generateExpression(expression->getOperand());
    } else {
        // Postfix.
        generateExpression(expression->getOperand());
        generateExpressionOperator(expression->getOperator());
    }
}

void CppBackEnd::generateExpressionOperator(Operator::OperatorType op) {
    switch (op) {
        case Operator::Addition:
            generateCpp(operatorPlus);
            break;
        case Operator::Subtraction:
            generateCpp(operatorMinus);
            break;
        case Operator::Multiplication:
            generateCpp(operatorMultiplication);
            break;
        case Operator::Division:
            generateCpp(operatorDivision);
            break;
        case Operator::Increment:
            generateCpp(operatorIncrement);
            break;
        case Operator::Decrement:
            generateCpp(operatorDecrement);
            break;
        case Operator::Greater:
            generateCpp(operatorGreater);
            break;
        case Operator::Less:
            generateCpp(operatorLess);
            break;
        case Operator::GreaterOrEqual:
            generateCpp(operatorGreaterOrEqual);
            break;
        case Operator::LessOrEqual:
            generateCpp(operatorLessOrEqual);
            break;
        case Operator::Assignment:
        case Operator::AssignmentExpression:
            generateCpp(operatorAssignment);
            break;
        case Operator::Equal:
            generateCpp(operatorEqual);
            break;
        case Operator::NotEqual:
            generateCpp(operatorNotEqual);
            break;
        case Operator::LogicalNegation:
            generateCpp(operatorLogicalNegation);
            break;
        case Operator::LogicalAnd:
            generateCpp(operatorLogicalAnd);
            break;
        case Operator::LogicalOr:
            generateCpp(operatorLogicalOr);
            break;
        default:
            internalError("generateExpressionOperator");
    }
}

void CppBackEnd::generateHeapAllocationExpression(
    const HeapAllocationExpression* allocExpression) {

    generateType(allocExpression->getType());
    generateCpp(openParentheses);
    generateCpp(keywordNew);
    generateCpp(space);
    generateMethodCall(allocExpression->getConstructorCall());
    generateCpp(closeParentheses);
}

//
// A[] a = new A[3]
// C++:
// Pointer<Array<Pointer<A>>> a =
//     Pointer<Array<Pointer<A>>>(new Array<Pointer<A>>(3));
//
void CppBackEnd::generateArrayAllocationExpression(
    const ArrayAllocationExpression* allocExpression) {

    const Type* arrayType = allocExpression->getType();
    generateType(arrayType);
    generateCpp(openParentheses);
    generateCpp(keywordNew);
    generateCpp(space);
    generateType(allocExpression->getType(), false);
    generateCpp(openParentheses);
    const Expression* capacityExpression =
        allocExpression->getArrayCapacityExpression();
    if (capacityExpression != nullptr) {
        if (allocExpression->getInitExpression() != nullptr) {
            generateArrayLiteral(allocExpression->getInitExpression());
            generateCpp(comma);
            generateCpp(space);
        }
        generateExpression(capacityExpression);
    }
    generateCpp(closeParentheses);
    generateCpp(closeParentheses);
}

void CppBackEnd::generateArraySubscriptExpression(
    const ArraySubscriptExpression* arraySubscriptExpression) {

    generateExpression(arraySubscriptExpression->getArrayNameExpression());
    generateCpp(operatorArrow);
    generateCpp(arrayAtName);
    generateCpp(openParentheses);
    generateExpression(arraySubscriptExpression->getIndexExpression());
    generateCpp(closeParentheses);
}

//
// (A) b
// C++:
// <A>dynamicPointerCast(b)
//
void CppBackEnd::generateTypeCastExpression(
    const TypeCastExpression* typeCastExpression) {

    const Type* type = typeCastExpression->getType();
    if (typeCastExpression->isStaticCast()) {
        if (type->isReference()) {
            generateCpp(staticPointerCastName);
        } else {
            generateCpp(keywordStaticCast);
        }
    } else {
        generateCpp(dynamicPointerCastName);
    }
    generateCpp(operatorLess);
    generateTypeName(type);
    generateCpp(operatorGreater);
    generateCpp(openParentheses);
    generateExpression(typeCastExpression->getOperand());
    generateCpp(closeParentheses);
}

void CppBackEnd::generateMemberExpression(
    const MemberExpression* memberExpression) {

    switch (memberExpression->getMemberExpressionType()) {
        case MemberExpression::DataMember:          
            generateDataMemberExpression(
                memberExpression->cast<DataMemberExpression>());
            break;
		case MemberExpression::MethodCall:
            generateMethodCall(memberExpression->cast<MethodCallExpression>());
            break;
        default:
            internalError("generateMemberExpression");
    }
}

void CppBackEnd::generateDataMemberExpression(
    const DataMemberExpression* dataMemberExpression) {

    generateCpp(mangle(dataMemberExpression->getName()));
}

void CppBackEnd::generateMethodCall(const MethodCallExpression* methodCall) {
    Identifier name = methodCall->getName();
    if (methodCall->isConstructorCall()) {
        name = eraseInitFromConstructorName(name);
    }
    generateCpp(mangle(name));
    generateCpp(openParentheses);
    const ExpressionList& arguments = methodCall->getArguments();
    for (ExpressionList::const_iterator i = arguments.begin();
         i != arguments.end(); ) {
        const Expression* argument = *i;
        generateExpression(argument);
        if (++i != arguments.end()) {
            generateCpp(comma);
            generateCpp(space);
        }
    }
    generateCpp(closeParentheses);
}

void CppBackEnd::generateMemberSelectorExpression(
    const MemberSelectorExpression* memberSelector) {

    Expression* left = memberSelector->getLeft();
    generateExpression(left);
    if (left->getRightmostExpressionType() == Expression::ClassName) {
        generateCpp(operatorScope);
    } else if (left->getType()->isReference()) {
        generateCpp(operatorArrow);
    } else {
        generateCpp(operatorDot);
    }
    generateExpression(memberSelector->getRight());
}

void CppBackEnd::generateLocalVariableExpression(
    const LocalVariableExpression* localVarExpression) {

    generateCpp(mangle(localVarExpression->getName()));
}

void CppBackEnd::generateClassNameExpression(
    const ClassNameExpression* classNameExpression) {

    generateCpp(mangle(classNameExpression->getClassDefinition()->getName()));
}

void CppBackEnd::generateNullExpression() {
    generateCpp(null);
}

void CppBackEnd::generateThisExpression() {
    generateCpp(keywordThis);
}

void CppBackEnd::generateTemporaryExpression(
    const TemporaryExpression* temporary) {

    generateCpp(mangle(temporary->getDeclaration()->getIdentifier()));
}

void CppBackEnd::generateIfStatement(const IfStatement* ifStatement) {
    generateCpp(keywordIf);
    generateCpp(space);
    generateCpp(openParentheses);
    generateExpression(ifStatement->getExpression()); 
    generateCpp(closeParentheses);
    generateCpp(space);
    generateBlock(ifStatement->getBlock());
    const BlockStatement* elseBlock = ifStatement->getElseBlock();
    if (elseBlock) {
        generateCpp(space);
        generateCpp(keywordElse);        
        generateCpp(space);
        generateBlock(elseBlock);
    }
    generateNewline();
}

void CppBackEnd::generateWhileStatement(const WhileStatement* whileStatement) {
    generateCpp(keywordWhile);
    generateCpp(space);
    generateCpp(openParentheses);
    generateExpression(whileStatement->getExpression()); 
    generateCpp(closeParentheses);
    generateCpp(space);
    generateBlock(whileStatement->getBlock());
    generateNewline();
}

void CppBackEnd::generateBreakStatement() {
    generateCpp(keywordBreak);
    generateSemicolonAndNewline();
}

void CppBackEnd::generateContinueStatement() {
    generateCpp(keywordContinue);
    generateSemicolonAndNewline();
}

void CppBackEnd::generateReturnStatement(
    const ReturnStatement* returnStatement) {

    generateCpp(keywordReturn);
    const Expression* returnExpression = returnStatement->getExpression();
    if (returnExpression != nullptr) {
        generateCpp(space);
        generateExpression(returnStatement->getExpression());
    }
    generateSemicolonAndNewline();
}

void CppBackEnd::generateLabelStatement(const LabelStatement* labelStatement) {
    generateCpp(mangle(labelStatement->getName()));
    generateCpp(colon);
    generateSemicolonAndNewline();
}

void CppBackEnd::generateJumpStatement(const JumpStatement* jumpStatement) {
    generateCpp(keywordGoto);
    generateCpp(space);
    generateCpp(mangle(jumpStatement->getLabelName()));
    generateSemicolonAndNewline();
}

void CppBackEnd::increaseIndent() {
    output->indent += indentSize;
}

void CppBackEnd::decreaseIndent() {
    if (output->indent >= indentSize) {
        output->indent -= indentSize;
    }
}

void CppBackEnd::generateNewline() {
    generateCpp(newline);
    output->str.append(output->indent, space);
}

void CppBackEnd::generateSemicolonAndNewline() {
    generateCpp(semicolon);
    generateNewline();
}

void CppBackEnd::setHeaderMode() {
    implementationMode = false;
    output = &headerOutput;
}

void CppBackEnd::setImplementationMode() {
    implementationMode = true;
    output = &implementationOutput;
}

void CppBackEnd::generateCpp(const std::string& element) {
    output->str.append(element);
}

void CppBackEnd::generateCpp(char element) {
    output->str.push_back(element);
}

void CppBackEnd::eraseLastChars(size_t nChars) {
    size_t size = output->str.size();
    output->str.erase(size - nChars, nChars);
}

void CppBackEnd::internalError(const std::string& where) {
    printf("BackEnd: When generating code for module %s: Internal error in "
           "%s.\n",
           moduleName.c_str(),
           where.c_str());
    exit(0);
}
