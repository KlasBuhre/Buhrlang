#ifndef Statement_h
#define Statement_h

#include "CommonTypes.h"
#include "NameBindings.h"

class Context;

class Statement: public Node {
public:

    enum Kind {
        VarDeclaration,
        Block,
        ExpressionStatement,
        If,
        While,
        For,
        Break,
        Continue,
        Return,
        Defer,
        ConstructorCall,
        Label,
        Jump
    };

    virtual Statement* clone() const = 0;
    virtual Type* typeCheck(Context& context) = 0;
    virtual bool mayFallThrough() const;
    virtual Traverse::Result traverse(Visitor& visitor);

    Kind getKind() const {
        return kind;
    }

    bool isExpression() const {
        return kind == ExpressionStatement;
    }

protected:
    Statement(Kind k, const Location& l);

private:
    Kind kind;
};

class VariableDeclarationStatement: public Statement {
public:
    static VariableDeclarationStatement* create(
        const Identifier& i,
        Expression* e);
    static VariableDeclarationStatement* create(
        Type* t,
        const Identifier& i,
        Expression* e,
        const Location& l);
    static VariableDeclarationStatement* create(
        Type* t,
        Expression* p,
        Expression* e,
        const Location& l);
    static Identifier generateTemporaryName(const Identifier& name);
    static VariableDeclarationStatement* generateTemporary(
        Type* t,
        const Identifier& name,
        Expression* init,
        const Location& loc);

    VariableDeclarationStatement* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    void lookupType(const Context& context);

    void setInitExpression(Expression* e) {
        initExpression = e;
    }

    void setIsNameUnique(bool u) {
        isNameUnique = u;
    }

    void setAddToNameBindingsWhenTypeChecked(bool a) {
        addToNameBindingsWhenTypeChecked = a;
    }

    Type* getType() const {
        return declaration.getType();
    }

    const Identifier& getIdentifier() const {
        return declaration.getIdentifier();
    }

    VariableDeclaration* getDeclaration() {
        return &declaration;
    }

    Expression* getInitExpression() const {
        return initExpression;
    }

    bool hasPattern() const {
        return patternExpression != nullptr;
    }

    bool getAddToNameBindingsWhenTypeChecked() const {
        return addToNameBindingsWhenTypeChecked;
    }

private:
    VariableDeclarationStatement(
        Type* t,
        const Identifier& i,
        Expression* e,
        const Location& l);
    VariableDeclarationStatement(const VariableDeclarationStatement& other);

    void typeCheckAndTransformInitExpression(Context& context);
    void generateDeclarationsFromPattern(Context& context);
    Expression* generateInitTemporary(Context& context);
    void makeIdentifierUniqueIfTakingLambda(Context& context);

    VariableDeclaration declaration;
    Expression* patternExpression;
    Expression* initExpression;
    bool isNameUnique;
    bool addToNameBindingsWhenTypeChecked;
    bool hasLookedUpType;
};

using VariableDeclarationStatementList = std::vector<VariableDeclarationStatement*>;

class LabelStatement;
class ConstructorCallStatement;

class BlockStatement: public Statement {
public:
    static BlockStatement* create(
        ClassDefinition* classDef,
        BlockStatement* enclosing,
        const Location& l);

    BlockStatement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    void addStatement(Statement* statement);
    void insertStatementAtFront(Statement* statement);
    void insertStatementAfterFront(Statement* statement);
    void insertBeforeCurrentStatement(Statement* statement);
    void copyStatements(const BlockStatement& from);
    void addLocalBinding(VariableDeclaration* localObject);
    void setEnclosingBlock(BlockStatement* b);
    void returnLastExpression(
        VariableDeclarationStatement* retvalTmpDeclaration);
    ConstructorCallStatement* getFirstStatementAsConstructorCall() const;
    void replaceLastStatement(Statement* statement);
    void replaceCurrentStatement(Statement* s);
    Expression* getLastStatementAsExpression() const;
    bool containsDeferDeclaration() const;

    using StatementList = std::list<Statement*>;

    StatementList& getStatements() {
        return statements;
    }

    const StatementList& getStatements() const {
        return statements;
    }

    NameBindings& getNameBindings() {
        return nameBindings;
    }

    BlockStatement* getEnclosingBlock() const {
        return enclosingBlock;
    }

private:
    BlockStatement(
        ClassDefinition* classDef,
        BlockStatement* enclosing,
        const Location& l);
    BlockStatement(const BlockStatement& other);

    void initialStatementCheck(Statement* statement);
    void addLabel(const LabelStatement* label);

    static BlockStatement* cloningBlock;

    NameBindings nameBindings;
    StatementList statements;
    StatementList::iterator iterator;
    BlockStatement* enclosingBlock;
};

class IfStatement: public Statement {
public:
    static IfStatement* create(
        Expression* e,
        BlockStatement* b,
        BlockStatement* eb,
        const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    Expression* getExpression() const {
        return expression;
    }

    BlockStatement* getBlock() const {
        return block;
    }

    BlockStatement* getElseBlock() const {
        return elseBlock;
    }

private:
    IfStatement(
        Expression* e,
        BlockStatement* b,
        BlockStatement* eb,
        const Location& l);

    Expression* expression;
    BlockStatement* block;
    BlockStatement* elseBlock;
};

class WhileStatement: public Statement {
public:
    static WhileStatement* create(
        Expression* e,
        BlockStatement* b,
        const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    Expression* getExpression() const {
        return expression;
    }

    BlockStatement* getBlock() const {
        return block;
    }

private:
    WhileStatement(Expression* e, BlockStatement* b, const Location& l);

    Expression* expression;
    BlockStatement* block;
};

class ForStatement: public Statement {
public:
    static ForStatement* create(
        Expression* cond,
        Expression* iter,
        BlockStatement* b,
        const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    Expression* getConditionExpression() const {
        return conditionExpression;
    }

    Expression* getIterExpression() const {
        return iterExpression;
    }

    BlockStatement* getBlock() const {
        return block;
    }

private:
    ForStatement(
        Expression* cond,
        Expression* iter,
        BlockStatement* b,
        const Location& l);

    Expression* conditionExpression;
    Expression* iterExpression;
    BlockStatement* block;
};

class BreakStatement: public Statement {
public:
    static BreakStatement* create(const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;

private:
    explicit BreakStatement(const Location& l);
};

class ContinueStatement: public Statement {
public:
    static ContinueStatement* create(const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;

private:
    explicit ContinueStatement(const Location& l);
};

class ReturnStatement: public Statement {
public:
    static ReturnStatement* create(Expression* e, const Location& l);
    static ReturnStatement* create(Expression* e);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;
    Traverse::Result traverse(Visitor& visitor) override;

    Expression* getExpression() const {
        return expression;
    }

private:
    ReturnStatement(Expression* e, const Location& l);
    ReturnStatement(const ReturnStatement& other);

    Expression* expression;
    MethodDefinition* originalMethod;
};

class DeferStatement: public Statement {
public:
    static DeferStatement* create(BlockStatement* b, const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

private:
    DeferStatement(BlockStatement* b, const Location& l);
    DeferStatement(const DeferStatement& other);

    BlockStatement* block;
};

class MethodCallExpression;

class ConstructorCallStatement: public Statement {
public:
    static ConstructorCallStatement* create(MethodCallExpression* c);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    Traverse::Result traverse(Visitor& visitor) override;

    MethodCallExpression* getMethodCallExpression() const {
        return constructorCall;
    }

    bool isBaseClassConstructorCall() const {
        return isBaseClassCtorCall;
    }

private:
    explicit ConstructorCallStatement(MethodCallExpression* c);

    MethodCallExpression* constructorCall;
    bool isBaseClassCtorCall;
    bool isTypeChecked;
};

class LabelStatement: public Statement {
public:
    static LabelStatement* create(const Identifier& n, const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context&) override;

    const Identifier& getName() const {
        return name;
    }

private:
    LabelStatement(const Identifier& n, const Location& l);

    Identifier name;
};

class JumpStatement: public Statement {
public:
    static JumpStatement* create(const Identifier& n, const Location& l);

    Statement* clone() const override;
    Type* typeCheck(Context& context) override;
    bool mayFallThrough() const override;

    const Identifier& getLabelName() const {
        return labelName;
    }

private:
    JumpStatement(const Identifier& n, const Location& l);

    Identifier labelName;
};

#endif
