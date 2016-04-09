#ifndef Context_h
#define Context_h

#include "CommonTypes.h"
#include "NameBindings.h"

class MethodDefinition;
class ClassDefinition;
class BlockStatement;
class LambdaExpression;

class Context {
public:
    explicit Context(MethodDefinition* m);

    Binding* lookup(const Identifier& name) const;
    Definition* lookupType(const Identifier& name) const;
    Type* lookupConcreteType(Type* type, const Location& location) const;
    void enterBlock(BlockStatement* b);
    void exitBlock();
    void reset();
    ClassDefinition* getClassDefinition() const;

    MethodDefinition* getMethodDefinition() const {
        return methodDefinition;
    }

    NameBindings* getNameBindings() const {
        return bindings;
    }

    BlockStatement* getBlock() const {
        return block;
    }

    NameBindings* getClassLocalNameBindings() const {
        return classLocalNameBindings;
    }

    LambdaExpression* getLambdaExpression() const {
        return lambdaExpression;
    }

    VariableDeclaration* getTemporaryRetvalDeclaration() const {
        return temporaryRetvalDeclaration;
    }

    Type* getArrayType() const {
        return arrayType;
    }

    void setClassLocalNameBindings(NameBindings* classLocals) {
        classLocalNameBindings = classLocals;
    }

    void setLambdaExpression(LambdaExpression* e) {
        lambdaExpression = e;
    }

    void setTemporaryRetvalDeclaration(VariableDeclaration* d) {
        temporaryRetvalDeclaration = d;
    }

    void setArrayType(Type* a) {
        arrayType = a;
    }

    bool isStatic() const {
        return staticContext;
    }

    void setIsStatic(bool s) {
        staticContext = s;
    }

    void setIsStringConstructorCall(bool b) {
        stringConstructorCall = b;
    }

    bool isStringConstructorCall() const {
        return stringConstructorCall;
    }

    void setIsConstructorCallStatement(bool c) {
        constructorCallStatement = c;
    }

    bool isConstructorCallStatement() const {
        return constructorCallStatement;
    }

    void setIsInsideLoop(bool w) {
        insideLoop = w;
    }

    bool isInsideLoop() const {
        return insideLoop;
    }

    class BindingsGuard {
    public:
        explicit BindingsGuard(Context& c);
        BindingsGuard(Context& c, NameBindings* classLocals);
        ~BindingsGuard();

    private:
        Context& context;
    };

private:
    MethodDefinition* methodDefinition;
    BlockStatement* block;
    NameBindings* bindings;
    NameBindings* classLocalNameBindings;
    LambdaExpression* lambdaExpression;
    VariableDeclaration* temporaryRetvalDeclaration;
    Type* arrayType;
    bool staticContext;
    bool stringConstructorCall;
    bool insideLoop;
    bool constructorCallStatement;
};

#endif
