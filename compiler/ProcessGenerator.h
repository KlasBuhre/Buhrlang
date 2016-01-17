#ifndef ProcessGenerator_h
#define ProcessGenerator_h

#include "Definition.h"
#include "Tree.h"

class MemberSelectorExpression;
class VariableDeclarationStatement;
class MatchExpression;

class ProcessGenerator {
public:
    ProcessGenerator(ClassDefinition* classDef, Tree& t);

    void generateProcessClasses();
    void generateProcessInterfaceClasses();
    void addMessageHandlerAbilityToRegularClass();

private:
    void fillRemoteMethodSignaturesList(ClassDefinition* fromClass);
    void transformIntoGeneratedProcessInterface(ClassDefinition* processClass);
    void generateCallInterface();
    MethodDefinition* generateCallMethodSignature(BlockStatement* body);
    void generateCallClasses();
    void generateCallClass(MethodDefinition* remoteMethodSignature);
    void generateCallConstructor(MethodDefinition* remoteMethodSignature);
    MethodDefinition* generateCallConstructorSignature(
        BlockStatement* body,
        MethodDefinition* remoteMethodSignature);
    void generateCallMethod(MethodDefinition* remoteMethodSignature);
    MemberSelectorExpression* generateProcessMethodCall(
        MethodDefinition* remoteMethodSignature);
    VariableDeclarationStatement* generateRetValDeclaration(
        Type* remoteCallReturnType,
        MemberSelectorExpression* processMethodCall);
    MemberSelectorExpression* generateSendMethodResult();
    void generateInterfaceIdClass(bool generatedAsNestedClass = false);
    void generateInterfaceId(const Identifier& name, int id);
    void generateMessageHandlerClass();
    void generateHandleMessageMethod();
    MethodDefinition* generateHandleMessageMethodSignature(
        BlockStatement* body);
    MemberSelectorExpression* generateCastAndCall(
        const Identifier& processType);
    void generateInterfaceMatchCases(MatchExpression* match);
    void generateMessageHandlerGetProxyMethods();
    void generateMessageHandlerGetInterfaceProxyMethod(
        const Identifier& interfaceName);
    void generateMessageHandlerGetProcessProxyMethod();
    void generateEmptyWaitMethod();
    void generateMessageHandlerFactoryClass();
    void generateProxyClass();
    void generateProxyConstructor(bool includeProcessName);
    MethodDefinition* generateProxyConstructorMethodSignature(
        BlockStatement* body,
        bool includeProcessName);
    void generateProxyConstructorWithPid();
    MethodDefinition* generateProxyConstructorMethodSignatureWithPid(
        BlockStatement* body);
    void generateProxyRemoteMethod(MethodDefinition* remoteMethodSignature);
    MethodDefinition* generateProxyRemoteMethodSignature(
        MethodDefinition* remoteMethodSignature,
        BlockStatement* body);
    Statement* generateMessageDeclaration(
        MethodDefinition* remoteMethodSignature);
    Expression* generateCallClassConstructorCallArgument(
        const VariableDeclaration* argument);
    Statement* generateMethodResultReturnStatement(Type* remoteCallReturnType);
    void generateProxyGetProxyMethod(const Identifier& processOrInterfaceName);
    void generateProxyWaitMethod();
    void generateGetProcessInterfaceProxyMethodSignature();
    void updateRegularClassConstructor();
    void generateRegularClassMessageHandlerMethod();
    void finishNonAbstractMethod(MethodDefinition* method);
    void finishClass();

    ClassDefinition* inputClass;
    Tree& tree;
    MemberMethodList remoteMethodSignatures;
    Identifier inputClassName;
    Identifier callInterfaceName;
    Identifier interfaceIdClassName;
    Identifier messageHandlerClassName;
    Identifier factoryClassName;
    Identifier proxyClassName;
};

#endif
