#ifndef Expression_h
#define Expression_h

#include "CommonTypes.h"
#include "Type.h"
#include "Statement.h"

class Expression: public Statement {
public:

    enum ExpressionType {    // Example:
        Literal,             // 5 or "text"
        NamedEntity,         // a
        Binary,              // a operator b
        Unary,               // operator a
        MemberSelector,      // a.b
        LocalVariable,       // a
        ClassName,           // A
        Member,              // b
        HeapAllocation,      // new A
        ArrayAllocation,     // new A[...]
        TypeCast,            // (A) b
        ArraySubscript,      // a[...]
        Null,                // null
        This,                // this
        Lambda,              // { |args| ... }
        Yield,               // yield(...)
        Match,               // match ... { a -> b ... }
        ClassDecomposition,  // A { b: c, ... }
        Typed,               // A a
        Placeholder,         // _
        Wildcard,            // ..
        Temporary,
        WrappedStatement
    };

    Expression(ExpressionType t, const Location& l);
    Expression(const Expression& other);

    virtual Expression* clone() const = 0;
    virtual Expression* transform(Context&);
    virtual bool isVariable() const;
    virtual Identifier generateVariableName() const;
    virtual ExpressionType getRightmostExpressionType() const;

    ExpressionType getExpressionType() const {
        return expressionType;
    }

    Type* getType() const {
        return type;
    }

    bool isNamedEntity() const {
        return expressionType == NamedEntity;
    }

    bool isWildcard() const {
        return expressionType == Wildcard;
    }

    bool isPlaceholder() const {
        return expressionType == Placeholder;
    }

    bool isClassDecomposition() const {
        return expressionType == ClassDecomposition;
    }

    static Expression* generateDefaultInitialization(
        Type* type,
        const Location& location);

protected:
    Type* type;

private:
    ExpressionType expressionType;
};

typedef std::string Value;

class LiteralExpression: public Expression {
public:

    enum LiteralType {
        Character,
        Integer,
        Float,
        String,
        Boolean,
        Array
    };

    LiteralExpression(LiteralType l, Type* t, const Location& loc);

    static Expression* generateDefault(Type* type, const Location& location);

    const Type* getType() const {
        return type;
    }

    virtual Type* typeCheck(Context&) {
        return type;
    }

    LiteralType getLiteralType() const {
        return literalType;
    }

private:
    LiteralType literalType;
};

class CharacterLiteralExpression: public LiteralExpression {
public:
    CharacterLiteralExpression(char c, const Location& loc);

    virtual Expression* clone() const;

    char getValue() const {
        return value;
    }

private:
    char value;
};

class IntegerLiteralExpression: public LiteralExpression {
public:
    IntegerLiteralExpression(int i, const Location& loc);
    explicit IntegerLiteralExpression(int i);

    virtual Expression* clone() const;

    int getValue() const {
        return value;
    }

private:
    int value;
};

class FloatLiteralExpression: public LiteralExpression {
public:
    FloatLiteralExpression(float f, const Location& loc);
    explicit FloatLiteralExpression(float i);

    virtual Expression* clone() const;

    float getValue() const {
        return value;
    }

private:
    float value;
};

class StringLiteralExpression: public LiteralExpression {
public:
    StringLiteralExpression(const std::string& s, const Location& loc);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);

    const std::string& getValue() const {
        return value;
    }

private:
    Expression* createCharArrayExpression(Context& context);

    std::string value;
};

class BooleanLiteralExpression: public LiteralExpression {
public:
    BooleanLiteralExpression(bool b, const Location& loc);

    virtual Expression* clone() const;

    bool getValue() const {
        return value;
    }

private:
    bool value;
};

class ArrayLiteralExpression: public LiteralExpression {
public:
    explicit ArrayLiteralExpression(const Location& loc);
    ArrayLiteralExpression(Type* t, const Location& loc);
    ArrayLiteralExpression(const ArrayLiteralExpression& other);

    virtual ArrayLiteralExpression* clone() const;
    virtual Expression* transform(Context& context);

    void addElement(Expression* element) {
        elements.push_back(element);
    }

    const ExpressionList& getElements() const {
        return elements;
    }

    ExpressionList& getElements() {
        return elements;
    }

private:
    void checkElements(Context& context);

    ExpressionList elements;
};

class NamedEntityExpression: public Expression {
public:
    explicit NamedEntityExpression(const Identifier& i);
    NamedEntityExpression(const Identifier& i, const Location& loc);

    virtual NamedEntityExpression* clone() const;
    virtual Type* typeCheck(Context& context);
    virtual Expression* transform(Context& context);

    bool resolve(Context& context);
    bool isReferencingStaticDataMember(Context& context);
    bool isReferencingName(Expression* name);
    MethodCallExpression* getCall(
        Context& context,
        bool allowUnknownIdentifier);

    const Identifier& getIdentifier() const {
        return identifier;
    }

    Binding* getBinding() const {
        return binding;
    }

private:
    Identifier identifier;
    Binding* binding;
};

class LocalVariableExpression: public Expression {
public:
    LocalVariableExpression(Type* t, const Identifier& i, const Location& loc);
    LocalVariableExpression(const LocalVariableExpression& other);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);
    virtual bool isVariable() const;
    virtual Identifier generateVariableName() const;

    const Identifier& getName() const {
        return identifier;
    }

private:
    Identifier identifier;
    bool hasTransformed;
};

class ClassNameExpression: public Expression {
public:
    ClassNameExpression(ClassDefinition* c, const Location& loc);
    ClassNameExpression(const ClassNameExpression& other);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);

    virtual Type* typeCheck(Context&) {
        return type;
    }

    ClassDefinition* getClassDefinition() const {
        return classDefinition;
    }

private:
    ClassDefinition* classDefinition;
    bool hasTransformed;
};

class WrappedStatementExpression;
class TemporaryExpression;

class MemberSelectorExpression: public Expression {
public:
    MemberSelectorExpression(Expression* l, Expression* r, const Location& loc);
    MemberSelectorExpression(Expression* l, Expression* r);
    MemberSelectorExpression(const Identifier& l, Expression* r);
    MemberSelectorExpression(const Identifier& l, const Identifier& r);
    MemberSelectorExpression(
        const Identifier& l,
        const Identifier& r,
        const Location& loc);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);
    virtual Identifier generateVariableName() const;
    virtual ExpressionType getRightmostExpressionType() const;

    MethodCallExpression* getRhsCall(Context& context);

    static MemberSelectorExpression* transformMemberSelector(
        MemberSelectorExpression* memberSelector,
        Context& context);

    Expression* getLeft() const {
        return left;
    }

    Expression* getRight() const {
        return right;
    }

private:
    NameBindings* bindingScopeOfLeft(Context& context);
    WrappedStatementExpression* transformIntoBlockStatement(
        WrappedStatementExpression* wrappedBlockStatement);
    TemporaryExpression* transformIntoTemporaryExpression(
        TemporaryExpression* temporary);
    void generateThisPointerDeclaration(
        BlockStatement* block, 
        const Identifier& thisPointerIdentifier);

    Expression* left;
    Expression* right;
};

class BinaryExpression: public Expression {
public:
    BinaryExpression(
        Operator::OperatorType oper,
        Expression* l,
        Expression* r,
        const Location& loc);
    BinaryExpression(Operator::OperatorType oper, Expression* l, Expression* r);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);
    virtual Expression* transform(Context& context);

    Operator::OperatorType getOperator() const {
        return op;
    }

    Expression* getLeft() const {
        return left;
    }

    Expression* getRight() const {
        return right;
    }

private:
    void checkAssignment(
        const Type* leftType,
        const Type* rightType,
        const Context& context);
    Type* resultingType(Type* leftType);
    MemberSelectorExpression* createStringOperation(Context& context);
    MemberSelectorExpression* createArrayOperation(Context& context);
    BinaryExpression* decomposeCompoundAssignment();
    bool leftIsMemberConstant();

    Operator::OperatorType op;
    Expression* left;
    Expression* right;
};

class UnaryExpression: public Expression {
public:
    UnaryExpression(
        Operator::OperatorType oper,
        Expression* o,
        bool p,
        const Location& loc);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);

    Operator::OperatorType getOperator() const {
        return op;
    }

    Expression* getOperand() const {
        return operand;
    }

    bool isPrefix() const {
        return prefix;
    }

private:
    Operator::OperatorType op;
    Expression* operand;
    bool prefix;
};

class LambdaExpression: public Expression {
public:
    LambdaExpression(BlockStatement* b, const Location& l);
    LambdaExpression(const LambdaExpression& other);

    virtual LambdaExpression* clone() const;
    virtual Type* typeCheck(Context& context);

    void addArgument(VariableDeclarationStatement* argument);

    const VariableDeclarationStatementList& getArguments() const {
        return arguments;
    }

    BlockStatement* getBlock() const {
        return block;
    }

    void setLambdaSignature(LambdaSignature* s) {
        signature = s;
    }

    LambdaSignature* getLambdaSignature() const {
        return signature;
    }

private:
    VariableDeclarationStatementList arguments;
    BlockStatement* block;
    LambdaSignature* signature;
};

class YieldExpression: public Expression {
public:
    explicit YieldExpression(const Location& l);
    YieldExpression(const YieldExpression& other);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);

    ExpressionList& getArguments() {
        return arguments;
    }

private:
    LocalVariableExpression* inlineLambdaExpressionWithReturnValue(
        LambdaExpression* lambdaExpression,
        Context& context);
    BlockStatement* inlineLambdaExpression(
        LambdaExpression* lambdaExpression,
        Context& context);

    ExpressionList arguments;
};

class ClassMemberDefinition;

class MemberExpression: public Expression {
public:

    enum MemberExpressionType {
        DataMember, 
        MethodCall        
    };

    MemberExpression(
        MemberExpressionType t,
        ClassMemberDefinition* m,
        const Location& loc);
    MemberExpression(const MemberExpression& other);

    MemberExpressionType getMemberExpressionType() const {
        return memberExpressionType;
    }

protected:
    Expression* transformIntoMemberSelector(Context& context);
    void accessCheck(Context& context);

    ClassMemberDefinition* memberDefinition;

private:
    MemberExpressionType memberExpressionType;
    bool hasTransformedIntoMemberSelector;
};

class DataMemberExpression: public MemberExpression {
public:
    DataMemberExpression(DataMemberDefinition* d, const Location& loc);
    DataMemberExpression(const DataMemberExpression& other);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);
    virtual bool isVariable() const;
    virtual Identifier generateVariableName() const;

    const Identifier& getName() const;

private:
    Expression* transformIntoConstructorArgumentReference(Context& context);
};

class MethodCallExpression: public MemberExpression {
public:
    MethodCallExpression(const Identifier& n, const Location& l);
    explicit MethodCallExpression(const Identifier& n);
    MethodCallExpression(const MethodCallExpression& other);

    virtual MethodCallExpression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);

    void setIsConstructorCall();
    void setConstructorCallName(Type* allocatedObjectType);
    void addArgument(const Identifier& argument);
    MethodDefinition* getEnumCtorMethodDefinition() const;
    void tryResolveEnumConstructor(Context& context);

    static MethodCallExpression* transformMethodCall(
        MethodCallExpression* methodCall,
        Context& context);

    const Identifier& getName() const {
        return name;
    }

    bool isConstructorCall() const {
        return isCtorCall;
    }

    Type* getInferredConcreteType() const {
        return inferredConcreteType;
    }

    void setName(const Identifier& n) {
        name = n;
    }

    void addArgument(Expression* argument) {
        arguments.push_back(argument);
    }

    ExpressionList& getArguments() {
        return arguments;
    }

    const ExpressionList& getArguments() const {
        return arguments;
    }

    LambdaExpression* getLambda() const {
        return lambda;
    }

    void setLambda(LambdaExpression* e) {
        lambda = e;
    }

private:
    void resolve(Context& context);
    void resolveByInferringConcreteClass(
        const MethodDefinition* candidate,
        const TypeList& argumentTypes,
        Context& context);
    Type* inferConcreteType(
        const MethodDefinition* candidate,
        const TypeList& argumentTypes,
        Context& context);
    const Binding::MethodList& resolveCandidates(Context& context);
    void findCompatibleMethod(
        const Binding::MethodList& candidates,
        const TypeList& argumentTypes);
    void resolveArgumentTypes(TypeList& typeList, Context& context);
    bool isBuiltInArrayMethod();
    void checkBuiltInArrayMethodPlaceholderTypes(Context& context);
    void checkArrayAppend(const Type* arrayType);
    void checkArrayConcatenation(const Type* arrayType);
    void reportError(
        const TypeList& argumentTypes,
        const Binding::MethodList& candidates);
    Expression* inlineCalledMethod(Context& context);
    void addArgumentsToInlinedMethodBody(BlockStatement* clonedBody);
    TemporaryExpression* inlineMethodWithReturnValue(
        BlockStatement* clonedMethodBody,
        MethodDefinition* calledMethod,
        Context& context);
    WrappedStatementExpression* transformIntoWhileStatement(Context& context);
    BlockStatement* addLamdaArgumentsToLambdaBlock(
        BlockStatement* whileBlock,
        const Identifier& indexVariableName,
        const Identifier& arrayName);

    Identifier name;
    ExpressionList arguments;
    LambdaExpression* lambda;
    bool isCtorCall;
    Type* inferredConcreteType;
};

class HeapAllocationExpression: public Expression {
public:
    explicit HeapAllocationExpression(MethodCallExpression* m);
    HeapAllocationExpression(Type* t, MethodCallExpression* m);
    HeapAllocationExpression(const HeapAllocationExpression& other);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);

    MethodCallExpression* getConstructorCall() const {
        return constructorCall;
    }

    void setProcessName(Expression* e) {
        processName = e;
    }

private:
    ClassDefinition* lookupClass(Context& context);

    Type* allocatedObjectType;
    ClassDefinition* classDefinition;
    MethodCallExpression* constructorCall;
    Expression* processName;
};

class ArrayAllocationExpression: public Expression {
public:
    ArrayAllocationExpression(Type* t, Expression* c, const Location& l);
    ArrayAllocationExpression(const ArrayAllocationExpression& other);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);

    void setInitExpression(ArrayLiteralExpression* init) {
        initExpression = init;
    }

    ArrayLiteralExpression* getInitExpression() const {
        return initExpression;
    }

    Expression* getArrayCapacityExpression() const {
        return arrayCapacityExpression;
    }

private:
    Type* arrayType;
    Expression* arrayCapacityExpression;
    ArrayLiteralExpression* initExpression;
};

class ArraySubscriptExpression: public Expression {
public:
    ArraySubscriptExpression(Expression* n, Expression* i);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);

    Expression* getArrayNameExpression() const {
        return arrayNameExpression;
    }

    Expression* getIndexExpression() const {
        return indexExpression;
    }

private:
    MemberSelectorExpression* createSliceMethodCall(
        BinaryExpression* range,
        Context& context);

    Expression* arrayNameExpression;
    Expression* indexExpression;
}; 

class TypeCastExpression: public Expression {
public:
    TypeCastExpression(Type* target, Expression* o,  const Location& l);
    TypeCastExpression(Type* target, Expression* o);
    TypeCastExpression(const TypeCastExpression& other);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);

    void setGenerated(bool g) {
        isGenerated = g;
    }

    Expression* getOperand() const {
        return operand;
    }

    bool isStaticCast() const {
        return staticCast;
    }

private:
    bool isCastBetweenObjectAndInterface(const Type* fromType);

    Type* targetType;
    Expression* operand;
    bool staticCast;
    bool isGenerated;
};

class Pattern;
class MatchCoverage;

typedef std::vector<Pattern*> PatternList;

class MatchCase: public Node {
public:
    explicit MatchCase(const Location& loc);
    MatchCase();
    MatchCase(const MatchCase& other);

    MatchCase* clone() const;
    void addPatternExpression(Expression* e);
    void buildPatterns(Context& context);
    bool isMatchExhaustive(
        Expression* subject,
        MatchCoverage& coverage,
        Context& context);
    Type* generateCaseBlock(
        BlockStatement* caseBlock,
        Context& context,
        Expression* subject,
        const Identifier& matchResultTmpName,
        const Identifier& matchEndLabelName);
    void setResultExpression(
        Expression* resultExpr,
        ClassDefinition* currentClass,
        BlockStatement* currentBlock);

    void setResultBlock(BlockStatement* r) {
        resultBlock = r;
    }

    const BlockStatement* getResultBlock() const {
        return resultBlock;
    }

    void setPatternGuard(Expression* g) {
        patternGuard = g;
    }

private:
    BinaryExpression* generateComparisonExpression(
        Expression* subject,
        Context& context);
    Type* generateCaseResultBlock(
        BlockStatement* block,
        Context& context,
        const Identifier& matchResultTmpName,
        const Identifier& matchEndLabelName);
    BlockStatement* chooseCaseResultBlock(
        BlockStatement* outerBlock,
        Context& context);
    void generateTemporariesCreatedByPatterns(BlockStatement* block);
    void generateVariablesCreatedByPatterns(BlockStatement* block);
    void checkVariablesCreatedByPatterns();

    ExpressionList patternExpressions;
    PatternList patterns;
    Expression* patternGuard;
    BlockStatement* resultBlock;
    bool isExhaustive;
};

class MatchExpression: public Expression {
public:
    MatchExpression(Expression* s, const Location& loc);
    explicit MatchExpression(Expression* s);
    MatchExpression(const MatchExpression& other);

    virtual Expression* clone() const;
    virtual Expression* transform(Context& context);
    virtual Type* typeCheck(Context& context);

    void addCase(MatchCase* c) {
        cases.push_back(c);
    }

    void setGenerated(bool g) {
        isGenerated = g;
    }

private:
    typedef std::vector<MatchCase*> CaseList;

    BlockStatement* generateMatchLogic(
        Context& context,
        const Identifier& resultTmpName);
    Expression* generateSubjectTemporary(BlockStatement* matchLogicBlock);
    Identifier generateMatchEndLabelName();
    void buildCasePatterns(Context& context);
    void checkResultType(Type* caseResultType, const MatchCase* matchCase);
    void checkCases(Context& context);

    Expression* subject;
    CaseList cases;
    bool isGenerated;
};

class ClassDecompositionExpression: public Expression {
public:

    struct Member {
        Member() : nameExpr(nullptr), patternExpr(nullptr) { }

        Expression* nameExpr;
        Expression* patternExpr;
    };

    typedef std::vector<Member> MemberList;

    ClassDecompositionExpression(Type* t, const Location& l);
    ClassDecompositionExpression(const ClassDecompositionExpression& other);

    virtual ClassDecompositionExpression* clone() const;
    virtual Type* typeCheck(Context&);

    void addMember(Expression* nameExpr, Expression* patternExpr);

    const MemberList& getMembers() const {
        return members;
    }

    void setEnumVariantName(const Identifier& e) {
        enumVariantName = e;
    }

    const Identifier& getEnumVariantName() const {
        return enumVariantName;
    }

private:
    MemberList members;
    Identifier enumVariantName;
};

class TypedExpression: public Expression {
public:
    TypedExpression(Type* targetType, Expression* n,  const Location& l);
    TypedExpression(const TypedExpression& other);

    virtual TypedExpression* clone() const;
    virtual Type* typeCheck(Context& context);

    Expression* getResultName() const {
        return resultName;
    }

private:
    Expression* resultName;
};

class PlaceholderExpression: public Expression {
public:
    explicit PlaceholderExpression(const Location& l);
    PlaceholderExpression();

    virtual Expression* clone() const;

    virtual Type* typeCheck(Context&) {
        return &Type::voidType();
    }
};

class WildcardExpression: public Expression {
public:
    explicit WildcardExpression(const Location& l);

    virtual Expression* clone() const;

    virtual Type* typeCheck(Context&) {
        return &Type::voidType();
    }
};

class NullExpression: public Expression {
public:
    explicit NullExpression(const Location& l);

    virtual Expression* clone() const;

    virtual Type* typeCheck(Context&) {
        return &Type::nullType();
    }
};

class ThisExpression: public Expression {
public:
    ThisExpression();
    explicit ThisExpression(const Location& l);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);
};

class TemporaryExpression: public Expression {
public:
    TemporaryExpression(VariableDeclaration* d, const Location& l);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);

    void setNonStaticInlinedMethodBody(BlockStatement* b) {
        nonStaticInlinedMethodBody = b;
    }

    BlockStatement* getNonStaticInlinedMethodBody() const {
        return nonStaticInlinedMethodBody;
    }

    VariableDeclaration* getDeclaration() const {
        return declaration;
    }

private:
    VariableDeclaration* declaration;
    BlockStatement* nonStaticInlinedMethodBody;
};

class WrappedStatementExpression: public Expression {
public:
    WrappedStatementExpression(Statement* s, const Location& l);
    WrappedStatementExpression(const WrappedStatementExpression& other);

    virtual Expression* clone() const;
    virtual Type* typeCheck(Context& context);

    Statement* getStatement() const {
        return statement;
    }

    void setInlinedNonStaticMethod(bool i) {
        inlinedNonStaticMethod = i;
    }

    bool isInlinedNonStaticMethod() const {
        return inlinedNonStaticMethod;
    }

    void setInlinedArrayForEach(bool i) {
        inlinedArrayForEach = i;
    }

    bool isInlinedArrayForEach() const {
        return inlinedArrayForEach;
    }

    void setDisallowYieldTransformation(bool d) {
        disallowYieldTransformation = d;
    }

private:
    Statement* statement;
    bool inlinedNonStaticMethod;
    bool inlinedArrayForEach;
    bool disallowYieldTransformation;
};

#endif
