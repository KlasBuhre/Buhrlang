#ifndef Expression_h
#define Expression_h

#include "CommonTypes.h"
#include "Type.h"
#include "Statement.h"

class Expression: public Statement {
public:

    enum Kind {              // Example:
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
        Lambda,              // |args| { ... }
        Yield,               // yield(...)
        AnonymousFunction,   // |args| { ... }
        Match,               // match ... { a -> b ... }
        ClassDecomposition,  // A { b: c, ... }
        Typed,               // A a
        Placeholder,         // _
        Wildcard,            // ..
        Temporary,
        WrappedStatement
    };

    virtual Expression* clone() const = 0;
    virtual Expression* transform(Context&);
    virtual bool isVariable() const;
    virtual Identifier generateVariableName() const;
    virtual Kind getRightmostExpressionKind() const;

    Kind getKind() const {
        return kind;
    }

    Type* getType() const {
        return type;
    }

    void setType(Type* t) {
        type = t;
    }

    bool isNamedEntity() const {
        return kind == NamedEntity;
    }

    bool isWildcard() const {
        return kind == Wildcard;
    }

    bool isPlaceholder() const {
        return kind == Placeholder;
    }

    bool isClassDecomposition() const {
        return kind == ClassDecomposition;
    }

    static Expression* generateDefaultInitialization(
        Type* type,
        const Location& location);

protected:
    Expression(Kind k, const Location& l);
    Expression(const Expression& other);

    Type* type;

private:
    Kind kind;
};

using Value = std::string;

class LiteralExpression: public Expression {
public:

    enum Kind {
        Character,
        Integer,
        Float,
        String,
        Boolean,
        Array
    };

    static Expression* generateDefault(Type* type, const Location& location);

    const Type* getType() const {
        return type;
    }

    Type* typeCheck(Context&) override {
        return type;
    }

    Kind getKind() const {
        return kind;
    }

protected:
    LiteralExpression(Kind k, Type* t, const Location& loc);

private:
    Kind kind;
};

class CharacterLiteralExpression: public LiteralExpression {
public:
    static CharacterLiteralExpression* create(char c, const Location& loc);

    Expression* clone() const override;

    char getValue() const {
        return value;
    }

private:
    CharacterLiteralExpression(char c, const Location& loc);

    char value;
};

class IntegerLiteralExpression: public LiteralExpression {
public:
    static IntegerLiteralExpression* create(int i, const Location& loc);
    static IntegerLiteralExpression* create(int i);

    Expression* clone() const override;

    int getValue() const {
        return value;
    }

private:
    IntegerLiteralExpression(int i, const Location& loc);

    int value;
};

class FloatLiteralExpression: public LiteralExpression {
public:
    static FloatLiteralExpression* create(float f, const Location& loc);

    Expression* clone() const override;

    float getValue() const {
        return value;
    }

private:
    FloatLiteralExpression(float f, const Location& loc);

    float value;
};

class StringLiteralExpression: public LiteralExpression {
public:
    static StringLiteralExpression* create(
        const std::string& s,
        const Location& loc);

    Expression* clone() const override;
    Expression* transform(Context& context) override;

    const std::string& getValue() const {
        return value;
    }

private:
    StringLiteralExpression(const std::string& s, const Location& loc);
    Expression* createCharArrayExpression(Context& context);

    std::string value;
};

class BooleanLiteralExpression: public LiteralExpression {
public:
    static BooleanLiteralExpression* create(bool b, const Location& loc);

    Expression* clone() const override;

    bool getValue() const {
        return value;
    }

private:
    BooleanLiteralExpression(bool b, const Location& loc);

    bool value;
};

class ArrayLiteralExpression: public LiteralExpression {
public:
    static ArrayLiteralExpression* create(const Location& loc);
    static ArrayLiteralExpression* create(Type* t, const Location& loc);

    ArrayLiteralExpression* clone() const override;
    Expression* transform(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

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
    ArrayLiteralExpression(Type* t, const Location& loc);
    ArrayLiteralExpression(const ArrayLiteralExpression& other);

    void checkElements(Context& context);

    ExpressionList elements;
};

class NamedEntityExpression: public Expression {
public:
    static NamedEntityExpression* create(const Identifier& i);
    static NamedEntityExpression* create(
        const Identifier& i,
        const Location& loc);

    NamedEntityExpression* clone() const override;
    Type* typeCheck(Context& context) override;
    Expression* transform(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    bool resolve(Context& context);
    bool isReferencingStaticDataMember(Context& context);
    bool isReferencingName(const Expression* name);
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
    NamedEntityExpression(const Identifier& i, const Location& loc);

    Identifier identifier;
    Binding* binding;
};

class LocalVariableExpression: public Expression {
public:
    static LocalVariableExpression* create(
        Type* t,
        const Identifier& i,
        const Location& loc);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    bool isVariable() const override;
    Identifier generateVariableName() const override;

    const Identifier& getName() const {
        return identifier;
    }

private:
    LocalVariableExpression(Type* t, const Identifier& i, const Location& loc);
    LocalVariableExpression(const LocalVariableExpression& other);

    Identifier identifier;
    bool hasTransformed;
};

class ClassNameExpression: public Expression {
public:
    static ClassNameExpression* create(ClassDefinition* c, const Location& loc);

    Expression* clone() const override;
    Expression* transform(Context& context) override;

    virtual Type* typeCheck(Context&) {
        return type;
    }

    ClassDefinition* getClassDefinition() const {
        return classDefinition;
    }

private:
    ClassNameExpression(ClassDefinition* c, const Location& loc);
    ClassNameExpression(const ClassNameExpression& other);

    ClassDefinition* classDefinition;
    bool hasTransformed;
};

class WrappedStatementExpression;
class TemporaryExpression;

class MemberSelectorExpression: public Expression {
public:
    static MemberSelectorExpression* create(
        Expression* l,
        Expression* r,
        const Location& loc);
    static MemberSelectorExpression* create(Expression* l, Expression* r);
    static MemberSelectorExpression* create(const Identifier& l, Expression* r);
    static MemberSelectorExpression* create(
        const Identifier& l,
        const Identifier& r);
    static MemberSelectorExpression* create(
        const Identifier& l,
        const Identifier& r,
        const Location& loc);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;
    Identifier generateVariableName() const override;
    Kind getRightmostExpressionKind() const override;

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
    MemberSelectorExpression(Expression* l, Expression* r, const Location& loc);

    NameBindings* bindingScopeOfLeft(Context& context);
    WrappedStatementExpression* transformIntoBlockStatement(
        WrappedStatementExpression* wrappedBlockStatement);
    TemporaryExpression* transformIntoTemporaryExpression(
        TemporaryExpression* temporary);
    void generateThisPointerDeclaration(
        BlockStatement* block, 
        const Identifier& thisPointerIdentifier);
    Expression* transformPrimitiveTypeMethodCall(Context& context);

    Expression* left;
    Expression* right;
};

class BinaryExpression: public Expression {
public:
    static BinaryExpression* create(
        Operator::Kind oper,
        Expression* l,
        Expression* r,
        const Location& loc);
    static BinaryExpression* create(
        Operator::Kind oper,
        Expression* l,
        Expression* r);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;
    Expression* transform(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    Operator::Kind getOperator() const {
        return op;
    }

    Expression* getLeft() const {
        return left;
    }

    Expression* getRight() const {
        return right;
    }

private:
    BinaryExpression(
        Operator::Kind oper,
        Expression* l,
        Expression* r,
        const Location& loc);

    void checkAssignment(
        const Type* leftType,
        const Type* rightType,
        const Context& context);
    Type* resultingType(Type* leftType);
    void inferTypes(const Context& context);
    Type* inferTypeFromOtherSide(
        const Expression* implicitlyTypedExpr,
        const Type* otherSideType,
        const Context& context);
    MemberSelectorExpression* createStringOperation(Context& context);
    MemberSelectorExpression* createArrayOperation(Context& context);
    BinaryExpression* decomposeCompoundAssignment();
    bool leftIsMemberConstant();

    Operator::Kind op;
    Expression* left;
    Expression* right;
};

class UnaryExpression: public Expression {
public:
    static UnaryExpression* create(
        Operator::Kind oper,
        Expression* o,
        bool p,
        const Location& loc);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    Operator::Kind getOperator() const {
        return op;
    }

    Expression* getOperand() const {
        return operand;
    }

    bool isPrefix() const {
        return prefix;
    }

private:
    UnaryExpression(
        Operator::Kind oper,
        Expression* o,
        bool p,
        const Location& loc);

    Operator::Kind op;
    Expression* operand;
    bool prefix;
};

class LambdaExpression: public Expression {
public:
    static LambdaExpression* create(BlockStatement* b, const Location& l);
    static LambdaExpression* create(BlockStatement* b);

    LambdaExpression* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void addArgument(VariableDeclarationStatement* argument);

    const VariableDeclarationStatementList& getArguments() const {
        return arguments;
    }

    BlockStatement* getBlock() const {
        return block;
    }

    void setLambdaSignature(FunctionSignature* s) {
        signature = s;
    }

    FunctionSignature* getLambdaSignature() const {
        return signature;
    }

private:
    LambdaExpression(BlockStatement* b, const Location& l);
    LambdaExpression(const LambdaExpression& other);

    VariableDeclarationStatementList arguments;
    BlockStatement* block;
    FunctionSignature* signature;
};

class YieldExpression: public Expression {
public:
    static YieldExpression* create(const Location& l);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    ExpressionList& getArguments() {
        return arguments;
    }

private:
    explicit YieldExpression(const Location& l);
    YieldExpression(const YieldExpression& other);

    LocalVariableExpression* inlineLambdaExpressionWithReturnValue(
        LambdaExpression* lambdaExpression,
        Context& context);
    BlockStatement* inlineLambdaExpression(
        LambdaExpression* lambdaExpression,
        Context& context);

    ExpressionList arguments;
};

class AnonymousFunctionExpression: public Expression {
public:
    static AnonymousFunctionExpression* create(
        BlockStatement* b,
        const Location& l);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;
    Expression* transform(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void addArgument(VariableDeclaration* argument);
    void inferArgumentTypes(
        const Binding::MethodList& candidates,
        unsigned int anonymousFunctionArgumentIndex);

    BlockStatement* getBody() const {
        return body;
    }

    const ArgumentList& getArgumentList() const {
        return argumentList;
    }

private:
    AnonymousFunctionExpression(BlockStatement* b, const Location& l);
    AnonymousFunctionExpression(const AnonymousFunctionExpression& other);

    void copyArgumentTypes(const ArgumentList& from);

    ArgumentList argumentList;
    BlockStatement* body;
};

class ClassMemberDefinition;

class MemberExpression: public Expression {
public:

    enum Kind {
        DataMember,
        MethodCall
    };

    Kind getKind() const {
        return kind;
    }

protected:
    MemberExpression(Kind k, ClassMemberDefinition* m, const Location& loc);
    MemberExpression(const MemberExpression& other);

    Expression* transformIntoMemberSelector(Context& context);
    void accessCheck(Context& context);

    ClassMemberDefinition* memberDefinition;

private:
    Kind kind;
    bool hasTransformedIntoMemberSelector;
    bool hasCheckedAccess;
};

class DataMemberExpression: public MemberExpression {
public:
    static DataMemberExpression* create(
        DataMemberDefinition* d,
        const Location& loc);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    bool isVariable() const override;
    Identifier generateVariableName() const override;

    const Identifier& getName() const;

private:
    DataMemberExpression(DataMemberDefinition* d, const Location& loc);
    DataMemberExpression(const DataMemberExpression& other);

    Expression* transformIntoConstructorArgumentReference(Context& context);
};

class MethodCallExpression: public MemberExpression {
public:
    static MethodCallExpression* create(const Identifier& n, const Location& l);
    static MethodCallExpression* create(const Identifier& n);

    MethodCallExpression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void setIsConstructorCall();
    void setConstructorCallName(const Type* allocatedObjectType);
    void addArgument(const Identifier& argument);
    MethodDefinition* getEnumCtorMethodDefinition() const;
    void tryResolveEnumConstructor(const Context& context);

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
    MethodCallExpression(const Identifier& n, const Location& l);
    MethodCallExpression(const MethodCallExpression& other);

    void resolve(Context& context);
    void resolveByInferringConcreteClass(
        const MethodDefinition* candidate,
        const TypeList& argumentTypes,
        Context& context);
    Type* inferConcreteType(
        const MethodDefinition* candidate,
        const TypeList& argumentTypes,
        Context& context);
    const Binding::MethodList& resolveCandidates(const Context& context);
    void findCompatibleMethod(
        const Binding::MethodList& candidates,
        const TypeList& argumentTypes);
    void resolveArgumentTypes(
        TypeList& typeList,
        const Binding::MethodList& candidates,
        Context& context);
    bool isBuiltInArrayMethod();
    void checkBuiltInArrayMethodPlaceholderTypes(const Context& context);
    void checkArrayAppend(const Type* arrayType);
    void checkArrayConcatenation(const Type* arrayType);
    void reportError(
        const TypeList& argumentTypes,
        const Binding::MethodList& candidates);
    Expression* transformDueToLambda(
        const MethodDefinition* methodDefinition,
        Context& context);
    Expression* inlineCalledMethod(Context& context);
    void addArgumentsToInlinedMethodBody(BlockStatement* clonedBody);
    TemporaryExpression* inlineMethodWithReturnValue(
        BlockStatement* clonedMethodBody,
        MethodDefinition* calledMethod,
        Context& context);
    WrappedStatementExpression* transformIntoForStatement(Context& context);
    BlockStatement* addLamdaArgumentsToLambdaBlock(
        BlockStatement* whileBlock,
        const Identifier& indexVariableName,
        const Identifier& arrayName);
    bool resolvesToClosure(const Context& context);
    Expression* transformIntoClosureCallMethod(Context& context);

    Identifier name;
    ExpressionList arguments;
    LambdaExpression* lambda;
    bool isCtorCall;
    Type* inferredConcreteType;
};

class HeapAllocationExpression: public Expression {
public:
    static HeapAllocationExpression* create(MethodCallExpression* m);
    static HeapAllocationExpression* create(Type* t, MethodCallExpression* m);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void lookupType(const Context& context);

    MethodCallExpression* getConstructorCall() const {
        return constructorCall;
    }

    void setProcessName(Expression* e) {
        processName = e;
    }

private:
    HeapAllocationExpression(Type* t, MethodCallExpression* m);
    HeapAllocationExpression(const HeapAllocationExpression& other);

    ClassDefinition* lookupClass(Context& context);

    Type* allocatedObjectType;
    ClassDefinition* classDefinition;
    MethodCallExpression* constructorCall;
    Expression* processName;
};

class ArrayAllocationExpression: public Expression {
public:
    static ArrayAllocationExpression* create(
        Type* t,
        Expression* c,
        const Location& l);
    static ArrayAllocationExpression* create(Type* t, Expression* c);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void lookupType(const Context& context);

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
    ArrayAllocationExpression(Type* t, Expression* c, const Location& l);
    ArrayAllocationExpression(const ArrayAllocationExpression& other);

    Type* arrayType;
    Expression* arrayCapacityExpression;
    ArrayLiteralExpression* initExpression;
};

class ArraySubscriptExpression: public Expression {
public:
    static ArraySubscriptExpression* create(Expression* n, Expression* i);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    Expression* getArrayNameExpression() const {
        return arrayNameExpression;
    }

    Expression* getIndexExpression() const {
        return indexExpression;
    }

private:
    ArraySubscriptExpression(Expression* n, Expression* i);

    MemberSelectorExpression* createSliceMethodCall(
        BinaryExpression* range,
        Context& context);

    Expression* arrayNameExpression;
    Expression* indexExpression;
}; 

class TypeCastExpression: public Expression {
public:
    static TypeCastExpression* create(
        Type* target,
        Expression* o,
        const Location& l);
    static TypeCastExpression* create(Type* target, Expression* o);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void lookupTargetType(const Context& context);

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
    TypeCastExpression(Type* target, Expression* o,  const Location& l);
    TypeCastExpression(const TypeCastExpression& other);

    bool isCastBetweenObjectAndInterface(const Type* fromType);

    Type* targetType;
    Expression* operand;
    bool staticCast;
    bool isGenerated;
};

class Pattern;
class MatchCoverage;

using PatternList = std::vector<Pattern*>;

class MatchCase: public Node {
public:
    static MatchCase* create(const Location& loc);
    static MatchCase* create();

    Traverse::Result traverse(Visitor& visitor) override;

    MatchCase* clone() const;    
    void addPatternExpression(Expression* e);
    void buildPatterns(Context& context);
    bool isMatchExhaustive(
        const Expression* subject,
        MatchCoverage& coverage,
        Context& context);
    Type* generateCaseBlock(
        BlockStatement* caseBlock,
        Context& context,
        const Expression* subject,
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
    explicit MatchCase(const Location& loc);
    MatchCase(const MatchCase& other);

    BinaryExpression* generateComparisonExpression(
        const Expression* subject,
        Context& context);
    Type* generateCaseResultBlock(
        BlockStatement* block,
        Context& context,
        const Identifier& matchResultTmpName,
        const Identifier& matchEndLabelName);
    BlockStatement* chooseCaseResultBlock(
        BlockStatement* outerBlock,
        Context& context);
    bool generateTemporariesCreatedByPatterns(BlockStatement* block);
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
    static MatchExpression* create(Expression* s, const Location& loc);
    static MatchExpression* create(Expression* s);

    Expression* clone() const override;
    Expression* transform(Context& context) override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    void addCase(MatchCase* c) {
        cases.push_back(c);
    }

    void setGenerated(bool g) {
        isGenerated = g;
    }

private:
    using CaseList = std::vector<MatchCase*>;

    MatchExpression(Expression* s, const Location& loc);
    MatchExpression(const MatchExpression& other);

    BlockStatement* generateMatchLogic(
        Context& context,
        const Identifier& resultTmpName);
    Expression* generateSubjectTemporary(BlockStatement* matchLogicBlock);
    Identifier generateMatchEndLabelName();
    void buildCasePatterns(Context& context);
    void checkResultType(
        const Type* caseResultType,
        const MatchCase* matchCase);
    void checkCases(Context& context);

    Expression* subject;
    CaseList cases;
    bool isGenerated;
};

class ClassDecompositionExpression: public Expression {
public:

    struct Member {
        Expression* nameExpr {nullptr};
        Expression* patternExpr {nullptr};
    };

    using MemberList = std::vector<Member>;

    static ClassDecompositionExpression* create(Type* t, const Location& l);

    ClassDecompositionExpression* clone() const override;
    Type* typeCheck(Context&) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void addMember(Expression* nameExpr, Expression* patternExpr);
    void lookupType(const Context& context);

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
    ClassDecompositionExpression(Type* t, const Location& l);
    ClassDecompositionExpression(const ClassDecompositionExpression& other);

    MemberList members;
    Identifier enumVariantName;
};

class TypedExpression: public Expression {
public:
    static TypedExpression* create(
        Type* targetType,
        Expression* n,
        const Location& l);

    TypedExpression* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void lookupType(const Context& context);

    Expression* getResultName() const {
        return resultName;
    }

private:
    TypedExpression(Type* targetType, Expression* n, const Location& l);
    TypedExpression(const TypedExpression& other);

    Expression* resultName;
};

class PlaceholderExpression: public Expression {
public:
    static PlaceholderExpression* create(const Location& l);
    static PlaceholderExpression* create();

    Expression* clone() const override;

    Type* typeCheck(Context&) override {
        return &Type::voidType();
    }

private:
    explicit PlaceholderExpression(const Location& l);
    PlaceholderExpression();
};

class WildcardExpression: public Expression {
public:
    static WildcardExpression* create(const Location& l);

    Expression* clone() const override;

    Type* typeCheck(Context&) override {
        return &Type::voidType();
    }

private:
    explicit WildcardExpression(const Location& l);
};

class NullExpression: public Expression {
public:
    static NullExpression* create(const Location& l);

    Expression* clone() const override;

    Type* typeCheck(Context&) override {
        return &Type::nullType();
    }

private:
    explicit NullExpression(const Location& l);
};

class ThisExpression: public Expression {
public:
    static ThisExpression* create();
    static ThisExpression* create(const Location& l);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;

private:
    explicit ThisExpression(const Location& l);
};

class TemporaryExpression: public Expression {
public:
    static TemporaryExpression* create(
        VariableDeclaration* d,
        const Location& l);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;

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
    TemporaryExpression(VariableDeclaration* d, const Location& l);

    VariableDeclaration* declaration;
    BlockStatement* nonStaticInlinedMethodBody;
};

class WrappedStatementExpression: public Expression {
public:
    static WrappedStatementExpression* create(Statement* s, const Location& l);

    Expression* clone() const override;
    Type* typeCheck(Context& context) override;

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
    WrappedStatementExpression(Statement* s, const Location& l);
    WrappedStatementExpression(const WrappedStatementExpression& other);

    Statement* statement;
    bool inlinedNonStaticMethod;
    bool inlinedArrayForEach;
    bool disallowYieldTransformation;
};

#endif
