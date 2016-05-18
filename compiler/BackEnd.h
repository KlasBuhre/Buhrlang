#ifndef BackEnd_h
#define BackEnd_h

#include <map>

#include "Tree.h"
#include "Definition.h"
#include "Statement.h"
#include "Expression.h"

class CppBackEnd {
public:

    CppBackEnd(Tree& t, const std::string& mName);

    void generate(const std::vector<std::string>& dependencies);

    const std::string& getHeaderOutput() const {
        return headerOutput.str;
    }

    const std::string& getImplementationOutput() const {
        return implementationOutput.str;
    }

private:
    void generateIncludeGuardBegin();
    void generateIncludeGuardEnd();
    void generateIncludes(const std::vector<std::string>& dependencies);
    void generateInclude(const std::string& fname);
    void generateForwardDeclaration(
        const ForwardDeclarationDefinition* forwardDeclaration);
    void generateDefinitions(const DefinitionList& definitions);
    void generateClass(const ClassDefinition* classDef);
    void generateClassParentList(const ClassDefinition* classDef);
    void generateClassParent(const Identifier& parentName);
    void generateVirtualDestructor(const ClassDefinition* classDef);
    void generateClassMembers(const ClassDefinition* classDef);
    void generateClassMember(const ClassMemberDefinition* member);
    void generateMethod(const MethodDefinition* method);
    void generateMethodSignature(const MethodDefinition* method);
    void generateScope(const Definition* enclosing);
    void generateArgumentList(const ArgumentList& arguments);
    void generateDataMember(const DataMemberDefinition* dataMember);
    void generateThreadLocal(const Type* type);
    void generateBlock(const BlockStatement* block);
    void generateStatement(const Statement* statement);
    void generateVariableDeclaration(const VariableDeclarationStatement* node);
    void generateType(const Type* type, bool generatePointer = true);
    void generateArrayType(const Type* type);
    void generateTypeName(const Type* type);
    void generateNonBuiltInTypeName(const Type* type);
    void generateExpression(
        const Expression* node,
        bool generateParentheses = false);
    void generateLiteral(const LiteralExpression* node);
    void generateArrayLiteral(const ArrayLiteralExpression* arrayLiteral);
    void generateChar(char c);
    void generateExpressionStatement(const Expression* expression);
    void generateBinaryExpression(
        const BinaryExpression* expression,
        bool generateParentheses);
    void generateUnaryExpression(const UnaryExpression* expression);
    void generateExpressionOperator(Operator::Kind op);
    void generateHeapAllocationExpression(
        const HeapAllocationExpression* allocExpression);
    void generateArrayAllocationExpression(
        const ArrayAllocationExpression* allocExpression);
    void generateArraySubscriptExpression(
        const ArraySubscriptExpression* arraySubscriptExpression);
    void generateTypeCastExpression(
        const TypeCastExpression* typeCastExpression);
    void generateMemberExpression(const MemberExpression* memberExpression);
    void generateDataMemberExpression(
        const DataMemberExpression* dataMemberExpression);
    void generateMethodCall(const MethodCallExpression* methodCall);
    void generateMemberSelectorExpression(
        const MemberSelectorExpression* memberAccess);
    void generateLocalVariableExpression(
        const LocalVariableExpression* localVarExpression);
    void generateClassNameExpression(
        const ClassNameExpression* classNameExpression);
    void generateNullExpression();
    void generateThisExpression(const ThisExpression* thisExpression);
    void generateTemporaryExpression(const TemporaryExpression* temporary);
    void generateIfStatement(const IfStatement* ifStatement);
    void generateWhileStatement(const WhileStatement* whileStatement);
    void generateForStatement(const ForStatement* forStatement);
    void generateBreakStatement();
    void generateContinueStatement();
    void generateReturnStatement(const ReturnStatement* returnStatement);
    void generateLabelStatement(const LabelStatement* labelStatement);
    void generateJumpStatement(const JumpStatement* jumpStatement);

    void setHeaderMode();
    void setImplementationMode();
    void generateCpp(const std::string& element);
    void generateCpp(char element);
    void eraseLastChars(size_t nChars);
    void generateNewline();
    void generateSemicolonAndNewline();
    void increaseIndent();
    void decreaseIndent();

    void internalError(const std::string& where);

    struct Output {
        Output() : indent(0) {
            str.reserve(100000);
        }

        std::string str;
        int indent;
    };

    Tree& tree;
    std::string moduleName;
    bool implementationMode;
    Output headerOutput;
    Output implementationOutput;
    Output* output;
};

#endif
