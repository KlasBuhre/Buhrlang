#include "ProcessGenerator.h"
#include "Expression.h"

namespace {

    const Identifier messageTypeName("Message");
    const Identifier messageTypeTypeName("MessageType");
    const Identifier processTypeName("Process");
    const Identifier messageHandlerFactoryTypeName("MessageHandlerFactory");

    const Identifier processInstanceVariableName("processInstance");
    const Identifier sourcePidVariableName("sourcePid");
    const Identifier pidVariableName("pid");
    const Identifier messageVariableName("message");
    const Identifier dataVariableName("data");
    const Identifier retvalVariableName("retval");
    const Identifier nameVariableName("name");
    const Identifier valueVariableName("value");
    const Identifier idVariableName("id");
    const Identifier argVariableName("arg");
    const Identifier messageHandlerIdVariableName("messageHandlerId");
    const Identifier interfaceIdVariableName("interfaceId");

    const Identifier methodCallConstantName("MethodCall");

    const Identifier callMethodName("call");
    const Identifier createMethodResultMethodName("createMethodResult");
    const Identifier handleMessageMethodName("handleMessage");
    const Identifier createMessageHandlerMethodName("createMessageHandler");
    const Identifier registerMessageHandlerMethodName("registerMessageHandler");
    const Identifier sendMethodName("send");
    const Identifier spawnMethodName("spawn");
    const Identifier getPidMethodName("getPid");
    const Identifier receiveMethodResultMethodName("receiveMethodResult");
    const Identifier waitMethodName("wait");

    MethodDefinition* createWaitMethodSignature(
        ClassDefinition* classDef, 
        BlockStatement* body) {

        auto methodSignature =
            MethodDefinition::create(BuiltInTypes::processWaitMethodName,
                                     nullptr,
                                     classDef);
        methodSignature->setBody(body);
        return methodSignature;
    }

    MethodDefinition* createGetProxyMethodSignature(
        ClassDefinition* classDef,
        BlockStatement* body,
        const Identifier& processInterfaceName) {

        auto methodSignature =
            MethodDefinition::create("get" + processInterfaceName + "_Proxy",
                                     Type::create(processInterfaceName),
                                     classDef);
        methodSignature->setBody(body);
        return methodSignature;
    }

    void checkRemoteMethodSignature(const MethodDefinition* method) {
        auto returnType = method->getReturnType();
        if (!returnType->isVoid() &&
            (!returnType->isMessageOrPrimitive() &&
             !returnType->getClass()->isProcess())) {
            Trace::error("Remote methods with return value must return a "
                         "message or process.",
                         method);
        }

        for (auto argument: method->getArgumentList()) {
            auto argumentType = argument->getType();
            if (!argumentType->isMessageOrPrimitive() &&
                !argumentType->getClass()->isProcess()) {
                Trace::error("Remote method arguments must be of type message "
                             "or process.",
                             argument);
            }
        }
    }
}

ProcessGenerator::ProcessGenerator(ClassDefinition* classDef, Tree& t) :
    inputClass(classDef),
    tree(t),
    remoteMethodSignatures(),
    inputClassName(inputClass->getName()),
    callInterfaceName(inputClassName + "_Call"),
    interfaceIdClassName(inputClassName + "_InterfaceId"),
    messageHandlerClassName(inputClassName + "_" +
                            CommonNames::messageHandlerTypeName),
    factoryClassName(inputClassName + "_" + messageHandlerFactoryTypeName),
    proxyClassName(inputClassName + "_Proxy") {}

void ProcessGenerator::generateProcessClasses() {
    ClassDefinition* generatedProcessInterface = inputClass;
    inputClass = inputClass->clone();

    transformIntoGeneratedProcessInterface(generatedProcessInterface);
    generateProxyClass();
    generateCallInterface();
    generateCallClasses();
    generateInterfaceIdClass();
    generateMessageHandlerClass();
    generateMessageHandlerFactoryClass();
}

void ProcessGenerator::generateProcessInterfaceClasses() {
    fillRemoteMethodSignaturesList(inputClass);

    generateGetProcessInterfaceProxyMethodSignature();
    generateProxyClass();
    generateCallInterface();
    generateCallClasses();
}

void ProcessGenerator::addMessageHandlerAbilityToRegularClass() {
    tree.reopenClass(inputClass);

    generateInterfaceIdClass(true);
    tree.addClassDataMember(Type::Integer, messageHandlerIdVariableName);
    updateRegularClassConstructor();
    generateRegularClassMessageHandlerMethod();
    generateMessageHandlerGetProxyMethods();

    tree.finishClass();
}

void ProcessGenerator::fillRemoteMethodSignaturesList(
    ClassDefinition* fromClass) {

    for (auto method: fromClass->getMethods()) {
        if (!method->isConstructor() && !method->isPrivate()) {
            remoteMethodSignatures.push_back(method);
            checkRemoteMethodSignature(method);
        }
    }
}

void ProcessGenerator::transformIntoGeneratedProcessInterface(
    ClassDefinition* processClass) {

    processClass->transformIntoInterface();
    fillRemoteMethodSignaturesList(processClass);
    MethodDefinition* waitMethod = createWaitMethodSignature(processClass, 
                                                             nullptr);
    processClass->appendMember(waitMethod);

    MethodDefinition* getProxyMethod =
        createGetProxyMethodSignature(processClass,
                                      nullptr,
                                      inputClassName);
    processClass->appendMember(getProxyMethod);
}

// Generate the following interface:
//
// message interface [ProcessType]_Call {
//     call(Message message, [ProcessType] processInstance)
// }
//
void ProcessGenerator::generateCallInterface() {
    ClassDefinition::Properties properties;
    properties.isInterface = true;
    properties.isMessage = true;
    tree.startGeneratedClass(callInterfaceName, properties);

    tree.addClassMember(generateCallMethodSignature(nullptr));

    finishClass();
}

// Generate the following method signature:
//
// call(Message message, [ProcessType] processInstance)
//
MethodDefinition* ProcessGenerator::generateCallMethodSignature(
    BlockStatement* body) {

    auto methodSignature =
        MethodDefinition::create(callMethodName,
                                 nullptr,
                                 tree.getCurrentClass());
    methodSignature->setBody(body);
    methodSignature->addArgument(messageTypeName, messageVariableName);
    methodSignature->addArgument(inputClassName, processInstanceVariableName);
    return methodSignature;
}

void ProcessGenerator::generateCallClasses() {
    for (auto remoteMethodSignature: remoteMethodSignatures) {
        generateCallClass(remoteMethodSignature);
    }
}

// Generate the following class:
//
// class [ProcessType]_[callType]_Call: [ProcessType]_Call {
//     int sourcePid
//
//     // If argument is not a process or process interface:
//     [Argument1Type] argument1
//
//     // If argument is a process or process interface:
//     [Argument2Type]_Proxy argument2
//     ...
//
//     init(int pid, [Argument1Type] arg1, [Argument2Type] arg2) {
//         sourcePid = pid
//         argument1 = arg1
//         argument2 = ([Argument2Type]_Proxy) arg2
//         ...
//     }
//
//     call(Message message, [ProcessType] processInstance) {
//         let retval = new Box<[ReturnType]>(processInstance.[callType](arg))
//         Process.send(sourcePid, message.createMethodResult(retval))
//     }
// }
//
void ProcessGenerator::generateCallClass(
    MethodDefinition* remoteMethodSignature) {

    Identifier callClassName(inputClassName + "_" +
                             remoteMethodSignature->getName() + "_Call");
    IdentifierList parents;
    parents.push_back(callInterfaceName);
    ClassDefinition::Properties properties;
    tree.startGeneratedClass(callClassName, properties, parents);

    if (!remoteMethodSignature->getReturnType()->isVoid()) {
        tree.addClassDataMember(Type::Integer, sourcePidVariableName);
    }

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        auto argumentType = argument->getType();
        if (argumentType->getClass()->isProcess()) {
            argumentType = Type::create(argumentType->getName() + "_Proxy");
        } else {
            argumentType->clone();
        }
        tree.addClassDataMember(argumentType, argument->getIdentifier());
    }

    generateCallConstructor(remoteMethodSignature);
    generateCallMethod(remoteMethodSignature);

    finishClass();
}

// Generate the following method:
//
// init(int pid, [Argument1Type] arg1, [Argument2Type] arg2) {
//     sourcePid = pid
//     argument1 = arg1
//     argument2 = ([Argument2Type]_Proxy) arg2
//     ...
// }
//
void ProcessGenerator::generateCallConstructor(
    MethodDefinition* remoteMethodSignature) {

    auto callConstructor =
        generateCallConstructorSignature(tree.startBlock(),
                                         remoteMethodSignature);

    if (!remoteMethodSignature->getReturnType()->isVoid()) {
        tree.addStatement(new BinaryExpression(Operator::Assignment,
                                               new NamedEntityExpression(
                                                   sourcePidVariableName),
                                               new NamedEntityExpression(
                                                   pidVariableName)));
    }

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        auto lhs = new NamedEntityExpression(argument->getIdentifier());
        Expression* rhs = NULL;
        auto argumentType = argument->getType();
        if (argumentType->getClass()->isProcess()) {
            rhs = new TypeCastExpression(
                      Type::create(argumentType->getName() + "_Proxy"),
                      new NamedEntityExpression(
                          argument->getIdentifier() + "_Arg"));
        } else {
            rhs = new NamedEntityExpression(argument->getIdentifier() + "_Arg");
        }
        tree.addStatement(new BinaryExpression(Operator::Assignment, lhs, rhs));
    }

    finishNonAbstractMethod(callConstructor);
}

// Generate the following method signature:
//
// init(int pid, [Argument1Type] arg1, [Argument2Type] arg2) {
//
MethodDefinition* ProcessGenerator::generateCallConstructorSignature(
    BlockStatement* body,
    MethodDefinition* remoteMethodSignature) {

    auto methodSignature =
        MethodDefinition::create(Keyword::initString,
                                 nullptr,
                                 tree.getCurrentClass());
    methodSignature->setBody(body);

    if (!remoteMethodSignature->getReturnType()->isVoid()) {
        methodSignature->addArgument(Type::Integer, pidVariableName);
    }

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        methodSignature->addArgument(argument->getType()->clone(),
                                     argument->getIdentifier() + "_Arg");
    }
    return methodSignature;
}

// Generate the following method:
//
// call(Message message, [ProcessType] processInstance) {
//     let retval = new Box<[ReturnType]>(processInstance.[callType](arg))
//     Process.send(sourcePid, message.createMethodResult(retval))
// }
//
void ProcessGenerator::generateCallMethod(
    MethodDefinition* remoteMethodSignature) {

    MethodDefinition* callMethod = generateCallMethodSignature(
        tree.startBlock());

    MemberSelectorExpression* processMethodCall =
        generateProcessMethodCall(remoteMethodSignature);
    Type* remoteCallReturnType = remoteMethodSignature->getReturnType();
    if (remoteCallReturnType->isVoid()) {
        tree.addStatement(processMethodCall);
    } else {
        tree.addStatement(generateRetValDeclaration(remoteCallReturnType,
                                                    processMethodCall));
        tree.addStatement(generateSendMethodResult());
    }

    finishNonAbstractMethod(callMethod);
}

// Generate the following method call expression wrapped in a member selector
// expression:
//
// processInstance.[callType](args .... ))
//
MemberSelectorExpression* ProcessGenerator::generateProcessMethodCall(
    MethodDefinition* remoteMethodSignature) {

    auto processMethodCall =
        new MethodCallExpression(remoteMethodSignature->getName());
    for (auto argument: remoteMethodSignature->getArgumentList()) {
        processMethodCall->addArgument(argument->getIdentifier());
    }
    return new MemberSelectorExpression(processInstanceVariableName,
                                        processMethodCall);
}

// Generate the following variable declaration statement:
//
// let retval = new Box<[ReturnType]>(processInstance.[callType](arg))
//
VariableDeclarationStatement* ProcessGenerator::generateRetValDeclaration(
    Type* remoteCallReturnType,
    MemberSelectorExpression* processMethodCall) {

    Expression* initExpression = nullptr;
    if (remoteCallReturnType->isReference()) {
        initExpression = processMethodCall;
    } else {
        auto boxType = Type::create(BuiltInTypes::boxTypeName);
        boxType->addGenericTypeParameter(remoteCallReturnType->clone());
        auto boxConstructorCall =
            new MethodCallExpression(BuiltInTypes::boxTypeName);
        boxConstructorCall->addArgument(processMethodCall);
        initExpression = new HeapAllocationExpression(boxType,
                                                      boxConstructorCall);
    }

    return VariableDeclarationStatement::create(retvalVariableName,
                                                initExpression);
}

// Generate the following expression:
//
// Process.send(sourcePid, message.createMethodResult(retval))
//
MemberSelectorExpression* ProcessGenerator::generateSendMethodResult() {
    MethodCallExpression* createMethodResult =
        new MethodCallExpression(createMethodResultMethodName);
    createMethodResult->addArgument(retvalVariableName);
    MethodCallExpression* send =
        new MethodCallExpression(sendMethodName);
    send->addArgument(sourcePidVariableName);
    send->addArgument(new MemberSelectorExpression(messageVariableName,
                                                   createMethodResult));
    return new MemberSelectorExpression(processTypeName, send);
}

// Generate the following class:
//
// class [ProcessType]_InterfaceId {
//     static int [ProcessType]Id = 0
//     static int [ProcessInterfaceType1]Id = 1
//     static int [ProcessInterfaceType2]Id = 2
//     ...
// }
//
void ProcessGenerator::generateInterfaceIdClass(bool generatedAsNestedClass) {
    ClassDefinition::Properties properties;
    tree.startGeneratedClass(interfaceIdClassName, properties);

    int id = 0;
    if (inputClass->isProcess()) {
        generateInterfaceId(inputClassName + "Id", id++);
    }
    if (inputClass->isInheritingFromProcessInterface()) {
        for (auto parent: inputClass->getParentClasses()) {
            if (parent->isProcess() && parent->isInterface()) {
                generateInterfaceId(parent->getName() + "Id", id++);
            }
        }
    }

    if (generatedAsNestedClass) {
        tree.addClassMember(tree.finishClass());
    } else {
        finishClass();
    }
}

// Generate the following data member:
//
// static int [ProcessInterfaceType[id]]Id = [id]
//
void ProcessGenerator::generateInterfaceId(const Identifier& name, int id) {
    Location loc;
    auto dataMember =
        DataMemberDefinition::create(name,
                                     Type::create(Type::Integer),
                                     AccessLevel::Public,
                                     true,
                                     false,
                                     loc);
    dataMember->setExpression(new IntegerLiteralExpression(id));
    tree.addClassMember(dataMember);
}

// Generate the following class:
//
// class [ProcessType]_MessageHandler: [ProcessType], MessageHandler {
//     [original process class data member]*
//
//     handleMessage(Message message) {
//         (([ProcessType]_Call) message.data).call(message, this)
//     }
//
//     // Or, if implementing process interfaces:
//     handleMessage(Message message) {
//         match message.interfaceId {
//             [ProcessType]_InterfaceId.[ProcessType]Id ->
//                 (([ProcessType]_Call) message.data).call(message, this),
//             [ProcessType]_InterfaceId.[ProcessInterfaceType]Id ->
//                 (([ProcessInterfaceType]_Call) message.data).call(message,
//                                                                   this),
//             _ -> {}
//                 // Do nothing.
//         }
//     }
// 
//     [original process class method]*
// 
//     // If implementing process interfaces:
//     [ProcessInterfaceType] get[ProcessInterfaceType]_Proxy() {
//         return new [ProcessInterfaceType]_Proxy(
//             Process.getPid,
//             0,
//             [ProcessType]_InterfaceId.[ProcessInterfaceType]Id)
//     }
//
//     [ProcessType] get[ProcessType]_Proxy() {
//         return new [ProcessType]_Proxy(Process.getPid)
//     }
// 
//     wait() {
//     }
// }
// 
void ProcessGenerator::generateMessageHandlerClass() {
    IdentifierList parents;
    parents.push_back(inputClassName);
    parents.push_back(CommonNames::messageHandlerTypeName);
    ClassDefinition::Properties properties;
    tree.startGeneratedClass(messageHandlerClassName, properties, parents);

    tree.getCurrentClass()->copyMembers(inputClass->getMembers());    
    generateHandleMessageMethod();
    generateMessageHandlerGetProxyMethods();
    generateEmptyWaitMethod();

    finishClass();
}

// Generate the following method:
// 
// handleMessage(Message message) {
//     (([ProcessType]_Call) message.data).call(message, this)
//
//     // Or, if implementing process interfaces:
//     match message.interfaceId {
//         [ProcessType]_InterfaceId.[ProcessType]Id ->
//             (([ProcessType]_Call) message.data).call(message, this),
//         [ProcessType]_InterfaceId.[ProcessInterfaceType]Id ->
//             (([ProcessInterfaceType]_Call) message.data).call(message, this),
//         _ -> {}
//             // Do nothing.
//     }
// }
// 
void ProcessGenerator::generateHandleMessageMethod() {
    MethodDefinition* handleMessageMethod = 
        generateHandleMessageMethodSignature(tree.startBlock());

    Expression* processCall = generateCastAndCall(inputClassName);
    if (inputClass->isInheritingFromProcessInterface()) {
        MatchExpression* match = new MatchExpression(
            new MemberSelectorExpression(messageVariableName,
                                         interfaceIdVariableName));
        MatchCase* processCase = new MatchCase();
        processCase->addPatternExpression(
            new MemberSelectorExpression(interfaceIdClassName,
                                         inputClassName + "Id"));
        processCase->setResultExpression(processCall,
                                         tree.getCurrentClass(),
                                         tree.getCurrentBlock());
        match->addCase(processCase);
        generateInterfaceMatchCases(match);
        tree.addStatement(match);
    } else {
        tree.addStatement(processCall);
    }

    finishNonAbstractMethod(handleMessageMethod);
}

// Generate the following match cases:
//
// [ProcessType]_InterfaceId.[ProcessInterfaceType]Id ->
//     (([ProcessInterfaceType]_Call) message.data).call(message, this),
// _ -> {}
//     // Do nothing.
//
void ProcessGenerator::generateInterfaceMatchCases(MatchExpression* match) {
    for (auto parent: inputClass->getParentClasses()) {
        if (parent->isProcess() && parent->isInterface()) {
            auto interfaceCase = new MatchCase();
            const Identifier& interfaceName = parent->getName();
            interfaceCase->addPatternExpression(
                new MemberSelectorExpression(interfaceIdClassName,
                                             interfaceName + "Id"));
            interfaceCase->setResultExpression(
                generateCastAndCall(interfaceName),
                tree.getCurrentClass(),
                tree.getCurrentBlock());
            match->addCase(interfaceCase);
        }
    }

    auto unknownCase = new MatchCase();
    unknownCase->addPatternExpression(new PlaceholderExpression());
    match->addCase(unknownCase);
}

// Generate the following method signature:
//
// handleMessage(Message message)
//
MethodDefinition* ProcessGenerator::generateHandleMessageMethodSignature(
    BlockStatement* body) {

    auto methodSignature =
        MethodDefinition::create(handleMessageMethodName,
                                 nullptr,
                                 tree.getCurrentClass());
    methodSignature->setBody(body);
    methodSignature->addArgument(messageTypeName, messageVariableName);
    return methodSignature;
}

// Generate the following expression:
//
// (([ProcessType]_Call) message.data).call(message, this)
//
MemberSelectorExpression* ProcessGenerator::generateCastAndCall(
    const Identifier& processType) {

    auto call = new MethodCallExpression(callMethodName);
    call->addArgument(messageVariableName);
    call->addArgument(new ThisExpression());
    auto typeCast =
        new TypeCastExpression(Type::create(processType + "_Call"),
                               new MemberSelectorExpression(messageVariableName,
                                                            dataVariableName));
    return new MemberSelectorExpression(typeCast, call);
}

void ProcessGenerator::generateMessageHandlerGetProxyMethods() {
    if (inputClass->isInheritingFromProcessInterface()) {
        for (auto parent: inputClass->getParentClasses()) {
            if (parent->isProcess() && parent->isInterface()) {
                generateMessageHandlerGetInterfaceProxyMethod(
                    parent->getName());
            }
        }
    }

    if (inputClass->isProcess()) {
        generateMessageHandlerGetProcessProxyMethod();
    }
}

// Generate the following method:
//
// [ProcessInterfaceType] get[ProcessInterfaceType]_Proxy() {
//     return new [ProcessInterfaceType]_Proxy(
//         Process.getPid,
//         0, // Or "messageHandlerId" if input class is not a process.
//         [ProcessType]_InterfaceId.[ProcessInterfaceType]Id)
// }
//
void ProcessGenerator::generateMessageHandlerGetInterfaceProxyMethod(
    const Identifier& interfaceName) {

    MethodDefinition* getProxyMethod = createGetProxyMethodSignature(
        tree.getCurrentClass(),
        tree.startBlock(),
        interfaceName);

    MethodCallExpression* constructorCall =
        new MethodCallExpression(interfaceName + "_Proxy");
    constructorCall->addArgument(
        new MemberSelectorExpression(processTypeName, getPidMethodName));
    if (inputClass->isProcess()) {
        constructorCall->addArgument(new IntegerLiteralExpression(0));
    } else {
        constructorCall->addArgument(messageHandlerIdVariableName);
    }
    constructorCall->addArgument(
        new MemberSelectorExpression(interfaceIdClassName,
                                     interfaceName + "Id"));
    tree.addStatement(
        ReturnStatement::create(new HeapAllocationExpression(constructorCall)));

    finishNonAbstractMethod(getProxyMethod);
}

// Generate the following method:
//
// [ProcessType] get[ProcessType]_Proxy() {
//     return new [ProcessType]_Proxy(Process.getPid)
// }
//
void ProcessGenerator::generateMessageHandlerGetProcessProxyMethod() {
    MethodDefinition* getProxyMethod =
        createGetProxyMethodSignature(tree.getCurrentClass(),
                                      tree.startBlock(),
                                      inputClassName);

    MethodCallExpression* constructorCall =
        new MethodCallExpression(inputClassName + "_Proxy");
    constructorCall->addArgument(
        new MemberSelectorExpression(processTypeName, getPidMethodName));
    tree.addStatement(
        ReturnStatement::create(new HeapAllocationExpression(constructorCall)));

    finishNonAbstractMethod(getProxyMethod);
}

void ProcessGenerator::generateEmptyWaitMethod() {
    MethodDefinition* waitMethod = 
        createWaitMethodSignature(tree.getCurrentClass(), tree.startBlock());
    finishNonAbstractMethod(waitMethod);
}

// Generate the following class:
//
// class [ProcessType]_MessageHandlerFactory: MessageHandlerFactory {
//     MessageHandler createMessageHandler() {
//         return new [ProcessType]_MessageHandler
//     }
// }
//
void ProcessGenerator::generateMessageHandlerFactoryClass() {
    IdentifierList parents;
    parents.push_back(messageHandlerFactoryTypeName);
    ClassDefinition::Properties properties;
    tree.startGeneratedClass(factoryClassName, properties, parents);

    auto createMessageHandlerMethod =
        MethodDefinition::create(
            createMessageHandlerMethodName,
            Type::create(CommonNames::messageHandlerTypeName),
            tree.getCurrentClass());
    createMessageHandlerMethod->setBody(tree.startBlock());
    HeapAllocationExpression* allocation =
        new HeapAllocationExpression(
            new MethodCallExpression(messageHandlerClassName));
    tree.addStatement(ReturnStatement::create(allocation));
    finishNonAbstractMethod(createMessageHandlerMethod);

    finishClass();
}

// Generate the following class:
//
// message class [ProcessType]_Proxy: [ProcessType] {
//     int pid
//
//     // If [ProcessType] is a process interface:
//     int messageHandlerId
//     int interfaceId
//
//     // If [ProcessType] is NOT a process interface:
//     init(string name) {
//         pid = Process.spawn(new [ProcessType]_MessageHandlerFactory, name)
//     }
//
//     // If [ProcessType] is NOT a process interface:
//     init(int arg) {
//         pid = arg
//     }
//
//     // If [ProcessType] is a process interface:
//     init(int processId, int handlerId, int ifId) {
//         pid = processId
//         messageHandlerId = handlerId
//         interfaceId = ifId
//     }
//
//     [returnType] [callType]([argType] arg) {
//         let message = new Message(
//             MessageType.MethodCall,
//             new [ProcessType]_[callType]_Call(Process.getPid, arg))
//         Process.send(pid, message)
//         return ((Box<[returnType]>)
//                     Process.receiveMethodResult(message.id).data).value
//     }
//
//     // If [ProcessType] inherits from a process interface:
//     [ProcessIntefaceType] get[ProcessIntefaceType]_Proxy() {
//         return this
//     }
//
//     [ProcessType] get[ProcessType]_Proxy() {
//         return this
//     }
//
//     wait() {
//         Process.wait(pid)
//     }
// }
//
void ProcessGenerator::generateProxyClass() {
    IdentifierList parents;
    parents.push_back(inputClassName);
    ClassDefinition::Properties properties;
    properties.isMessage = true;
    tree.startGeneratedClass(proxyClassName, properties, parents);

    tree.addClassDataMember(Type::Integer, pidVariableName);
    if (inputClass->isInterface()) {
        tree.addClassDataMember(Type::Integer, messageHandlerIdVariableName);
        tree.addClassDataMember(Type::Integer, interfaceIdVariableName);
        tree.getCurrentClass()->generateConstructor();
    } else {
        generateProxyConstructor(false);
        generateProxyConstructor(true);
    }
    generateProxyConstructorWithPid();

    for (auto remoteMethodSignature: remoteMethodSignatures) {
        generateProxyRemoteMethod(remoteMethodSignature);
    }

    if (inputClass->isInheritingFromProcessInterface()) {
        for (auto parent: inputClass->getParentClasses()) {
            if (parent->isProcess() && parent->isInterface()) {
                generateProxyGetProxyMethod(parent->getName());
            }
        }
    }

    generateProxyGetProxyMethod(inputClassName);
    generateProxyWaitMethod();

    finishClass();
}

// Generate the following method:
//
// init(string name) {
//     pid = Process.spawn(new [ProcessType]_MessageHandlerFactory, name)
// }
//
void ProcessGenerator::generateProxyConstructor(bool includeProcessName) {
    MethodDefinition* proxyConstructorMethod =
        generateProxyConstructorMethodSignature(tree.startBlock(),
                                                includeProcessName);

    MethodCallExpression* spawn = new MethodCallExpression(spawnMethodName);
    spawn->addArgument(new HeapAllocationExpression(new MethodCallExpression(
        factoryClassName)));
    if (includeProcessName) {
        spawn->addArgument(nameVariableName);
    }
    tree.addStatement(
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(pidVariableName),
                             new MemberSelectorExpression(processTypeName,
                                                          spawn)));

    finishNonAbstractMethod(proxyConstructorMethod);
}

// Generate the following method signature:
//
// init(string name)
//
MethodDefinition* ProcessGenerator::generateProxyConstructorMethodSignature(
    BlockStatement* body,
    bool includeProcessName) {

    auto methodSignature =
        MethodDefinition::create(Keyword::initString,
                                 nullptr,
                                 tree.getCurrentClass());
    methodSignature->setBody(body);
    if (includeProcessName) {
        methodSignature->addArgument(Type::String, nameVariableName);
    }

    return methodSignature;
}

// Generate the following method:
//
// init(int arg) {
//     pid = arg
// }
void ProcessGenerator::generateProxyConstructorWithPid() {
    MethodDefinition* proxyConstructorMethod =
        generateProxyConstructorMethodSignatureWithPid(tree.startBlock());

    tree.addStatement(
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(pidVariableName),
                             new NamedEntityExpression(argVariableName)));

    finishNonAbstractMethod(proxyConstructorMethod);
}

// Generate the following method signature:
//
// init(int arg)
//
MethodDefinition*
ProcessGenerator::generateProxyConstructorMethodSignatureWithPid(
    BlockStatement* body) {

    auto methodSignature =
        MethodDefinition::create(Keyword::initString,
                                 nullptr,
                                 tree.getCurrentClass());
    methodSignature->setBody(body);
    methodSignature->addArgument(Type::Integer, argVariableName);

    return methodSignature;
}

// Generate the following method:
//
// [returnType] [callType]([argType] arg) {
//     let message = new Message(
//         MessageType.MethodCall,
//         new [ProcessType]_[callType]_Call(Process.getPid, arg))
//     Process.send(pid, message)
//     return ((Box<[returnType]>)
//                 Process.receiveMethodResult(message.id).data).value
// }
//
void ProcessGenerator::generateProxyRemoteMethod(
    MethodDefinition* remoteMethodSignature) {

    MethodDefinition* proxyMethod =
        generateProxyRemoteMethodSignature(remoteMethodSignature,
                                           tree.startBlock());

    tree.addStatement(generateMessageDeclaration(remoteMethodSignature));
    MethodCallExpression* send = new MethodCallExpression(sendMethodName);
    send->addArgument(pidVariableName);
    send->addArgument(messageVariableName);
    tree.addStatement(new MemberSelectorExpression(processTypeName, send));
    Type* remoteCallReturnType = remoteMethodSignature->getReturnType();
    if (!remoteCallReturnType->isVoid()) {
        tree.addStatement(
            generateMethodResultReturnStatement(remoteCallReturnType));
    }

    finishNonAbstractMethod(proxyMethod);
}

// Generate the following method signature:
//
// [returnType] [callType]([argType] arg1, ... )
//
MethodDefinition* ProcessGenerator::generateProxyRemoteMethodSignature(
    MethodDefinition* remoteMethodSignature,
    BlockStatement* body) {

    auto methodSignature =
        MethodDefinition::create(remoteMethodSignature->getName(),
                                 remoteMethodSignature->getReturnType()->clone(),
                                 tree.getCurrentClass());
    methodSignature->setBody(body);

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        methodSignature->addArgument(argument->getType()->clone(),
                                     argument->getIdentifier());
    }
    return methodSignature;
}

// Generate the following statement:
//
// let message = new Message(
//     MessageType.MethodCall,
//     new [ProcessType]_[callType]_Call(Process.getPid, arg))
//
// Or, if the input class is a process interface:
//
// let message = new Message(
//     messageHandlerId,
//     interfaceId,
//     new [ProcessType]_[callType]_Call(Process.getPid, arg))
//
Statement* ProcessGenerator::generateMessageDeclaration(
    MethodDefinition* remoteMethodSignature) {

    auto callClassConstructorCall =
        new MethodCallExpression(inputClassName + "_" +
                                 remoteMethodSignature->getName() + "_Call");
    if (!remoteMethodSignature->getReturnType()->isVoid()) {
        callClassConstructorCall->addArgument(
            new MemberSelectorExpression(processTypeName, getPidMethodName));
    }

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        callClassConstructorCall->addArgument(
            generateCallClassConstructorCallArgument(argument));
    }

    auto messageClassConstructorCall =
        new MethodCallExpression(messageTypeName);
    if (inputClass->isInterface()) {
        messageClassConstructorCall->addArgument(messageHandlerIdVariableName);
        messageClassConstructorCall->addArgument(interfaceIdVariableName);
    } else {
        messageClassConstructorCall->addArgument(
            new MemberSelectorExpression(messageTypeTypeName,
                                         methodCallConstantName));
    }
    messageClassConstructorCall->addArgument(
        new HeapAllocationExpression(callClassConstructorCall));
    return VariableDeclarationStatement::create(
        messageVariableName,
        new HeapAllocationExpression(messageClassConstructorCall));
}

// Generate the following expression:
//
// arg
// Or, if the argument is of process type:
// arg.get[ProcessType]_Proxy
//
Expression* ProcessGenerator::generateCallClassConstructorCallArgument(
    const VariableDeclaration* argument) {

    Type* argumentType = argument->getType();
    Definition* definition = argumentType->getDefinition();
    if (definition->isClass() &&
        definition->cast<ClassDefinition>()->isProcess()) {
        return new MemberSelectorExpression(
            argument->getIdentifier(),
            "get" + argumentType->getName() + "_Proxy");
    }
    return new NamedEntityExpression(argument->getIdentifier());
}

// Generate the following statement:
//
// return ((Box<[returnType]>)
//             Process.receiveMethodResult(message.id).data).value
//
Statement* ProcessGenerator::generateMethodResultReturnStatement(
    Type* remoteCallReturnType) {

    Type* returnType = nullptr;
    if (remoteCallReturnType->isReference()) {
        returnType = remoteCallReturnType->clone();
    } else {
        returnType = Type::create(BuiltInTypes::boxTypeName);
        returnType->addGenericTypeParameter(remoteCallReturnType->clone());
    }
    MethodCallExpression* receiveMethodResult =
        new MethodCallExpression(receiveMethodResultMethodName);
    receiveMethodResult->addArgument(
        new MemberSelectorExpression(messageVariableName, idVariableName));
    MemberSelectorExpression* selector =
        new MemberSelectorExpression(
            processTypeName,
            new MemberSelectorExpression(
                receiveMethodResult,
                new NamedEntityExpression(dataVariableName)));
    TypeCastExpression* typeCast = new TypeCastExpression(returnType, selector);
    Expression* returnedExpression = nullptr;
    if (remoteCallReturnType->isReference()) {
        returnedExpression = typeCast;
    } else {
        returnedExpression = new MemberSelectorExpression(
            typeCast,
            new NamedEntityExpression(valueVariableName));
    }

    return ReturnStatement::create(returnedExpression);
}

// Generate the following method:
//
// [ProcessType] get[ProcessType]_Proxy() {
//     return this
// }
//
void ProcessGenerator::generateProxyGetProxyMethod(
    const Identifier& processOrInterfaceName) {

    auto getProxyMethod = createGetProxyMethodSignature(
        tree.getCurrentClass(),
        tree.startBlock(),
        processOrInterfaceName);

    tree.addStatement(ReturnStatement::create(new ThisExpression()));

    finishNonAbstractMethod(getProxyMethod);
}

// Generate the following method:
//
// wait() {
//     Process.wait(pid)
// }
//
void ProcessGenerator::generateProxyWaitMethod() {
    MethodDefinition* waitMethod =
        createWaitMethodSignature(tree.getCurrentClass(), tree.startBlock());

    MethodCallExpression* wait = new MethodCallExpression(waitMethodName);
    wait->addArgument(pidVariableName);
    tree.addStatement(new MemberSelectorExpression(processTypeName, wait));

    finishNonAbstractMethod(waitMethod);
}

// Generate the following method signature:
//
// [ProcessType] get[ProcessType]_Proxy()
//
void ProcessGenerator::generateGetProcessInterfaceProxyMethodSignature() {
    MethodDefinition* getProxyMethod =
        createGetProxyMethodSignature(inputClass,
                                      nullptr,
                                      inputClassName);
    inputClass->appendMember(getProxyMethod);
}

// Update the regular class constructor with the following expression:
//
// messageHandlerId = Process.registerMessageHandler(this)
//
void ProcessGenerator::updateRegularClassConstructor() {
    inputClass->generateDefaultConstructorIfNeeded();
    MethodDefinition* constructor = inputClass->getDefaultConstructor();

    MethodCallExpression* registerHandler =
        new MethodCallExpression(registerMessageHandlerMethodName);
    registerHandler->addArgument(new ThisExpression());
    BinaryExpression* assignment =
        new BinaryExpression(Operator::Assignment,
                             new NamedEntityExpression(
                                 messageHandlerIdVariableName),
                             new MemberSelectorExpression(processTypeName,
                                                          registerHandler));
    constructor->getBody()->addStatement(assignment);
}

// Generate the following method:
//
// handleMessage(Message message) {
//     match message.interfaceId {
//         [ProcessType]_InterfaceId.[ProcessInterfaceType1]Id ->
//             (([ProcessInterfaceType1]_Call) message.data).call(message, this),
//         [ProcessType]_InterfaceId.[ProcessInterfaceType2]Id ->
//             (([ProcessInterfaceType2]_Call) message.data).call(message, this),
//         _ -> {}
//             // Do nothing.
//     }
// }
//
void ProcessGenerator::generateRegularClassMessageHandlerMethod() {
    MethodDefinition* handleMessageMethod =
        generateHandleMessageMethodSignature(tree.startBlock());

    MatchExpression* match = new MatchExpression(
        new MemberSelectorExpression(messageVariableName,
                                     interfaceIdVariableName));
    generateInterfaceMatchCases(match);
    tree.addStatement(match);

    finishNonAbstractMethod(handleMessageMethod);
}

void ProcessGenerator::finishNonAbstractMethod(MethodDefinition* method) {
    tree.finishBlock();
    tree.addClassMember(method);
}

void ProcessGenerator::finishClass() {
    ClassDefinition* classDef = tree.finishClass();
    if (inputClass->isImported()) {
        classDef->setIsImported(true);
    }
    tree.addGlobalDefinition(classDef);
}
