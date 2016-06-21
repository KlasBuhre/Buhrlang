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
        tree.addStatement(
            BinaryExpression::create(Operator::Assignment,
                                     NamedEntityExpression::create(
                                         sourcePidVariableName),
                                     NamedEntityExpression::create(
                                         pidVariableName)));
    }

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        auto lhs = NamedEntityExpression::create(argument->getIdentifier());
        Expression* rhs = NULL;
        auto argumentType = argument->getType();
        if (argumentType->getClass()->isProcess()) {
            rhs = TypeCastExpression::create(
                      Type::create(argumentType->getName() + "_Proxy"),
                      NamedEntityExpression::create(
                          argument->getIdentifier() + "_Arg"));
        } else {
            rhs = NamedEntityExpression::create(
                argument->getIdentifier() + "_Arg");
        }
        tree.addStatement(
            BinaryExpression::create(Operator::Assignment, lhs, rhs));
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
        MethodCallExpression::create(remoteMethodSignature->getName());
    for (auto argument: remoteMethodSignature->getArgumentList()) {
        processMethodCall->addArgument(argument->getIdentifier());
    }
    return MemberSelectorExpression::create(processInstanceVariableName,
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
            MethodCallExpression::create(BuiltInTypes::boxTypeName);
        boxConstructorCall->addArgument(processMethodCall);
        initExpression = HeapAllocationExpression::create(boxType,
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
    auto createMethodResult =
        MethodCallExpression::create(createMethodResultMethodName);
    createMethodResult->addArgument(retvalVariableName);
    auto send = MethodCallExpression::create(sendMethodName);
    send->addArgument(sourcePidVariableName);
    send->addArgument(MemberSelectorExpression::create(messageVariableName,
                                                       createMethodResult));
    return MemberSelectorExpression::create(processTypeName, send);
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
    dataMember->setExpression(IntegerLiteralExpression::create(id));
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
    auto handleMessageMethod =
        generateHandleMessageMethodSignature(tree.startBlock());

    auto processCall = generateCastAndCall(inputClassName);
    if (inputClass->isInheritingFromProcessInterface()) {
        auto match = MatchExpression::create(
            MemberSelectorExpression::create(messageVariableName,
                                             interfaceIdVariableName));
        auto processCase = MatchCase::create();
        processCase->addPatternExpression(
            MemberSelectorExpression::create(interfaceIdClassName,
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
            auto interfaceCase = MatchCase::create();
            const Identifier& interfaceName = parent->getName();
            interfaceCase->addPatternExpression(
                MemberSelectorExpression::create(interfaceIdClassName,
                                                 interfaceName + "Id"));
            interfaceCase->setResultExpression(
                generateCastAndCall(interfaceName),
                tree.getCurrentClass(),
                tree.getCurrentBlock());
            match->addCase(interfaceCase);
        }
    }

    auto unknownCase = MatchCase::create();
    unknownCase->addPatternExpression(PlaceholderExpression::create());
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

    auto call = MethodCallExpression::create(callMethodName);
    call->addArgument(messageVariableName);
    call->addArgument(ThisExpression::create());
    auto typeCast =
        TypeCastExpression::create(Type::create(processType + "_Call"),
                                   MemberSelectorExpression::create(
                                       messageVariableName,
                                       dataVariableName));
    return MemberSelectorExpression::create(typeCast, call);
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

    auto getProxyMethod = createGetProxyMethodSignature(
        tree.getCurrentClass(),
        tree.startBlock(),
        interfaceName);

    auto constructorCall =
        MethodCallExpression::create(interfaceName + "_Proxy");
    constructorCall->addArgument(
        MemberSelectorExpression::create(processTypeName, getPidMethodName));
    if (inputClass->isProcess()) {
        constructorCall->addArgument(IntegerLiteralExpression::create(0));
    } else {
        constructorCall->addArgument(messageHandlerIdVariableName);
    }
    constructorCall->addArgument(
        MemberSelectorExpression::create(interfaceIdClassName,
                                         interfaceName + "Id"));
    tree.addStatement(
        ReturnStatement::create(
            HeapAllocationExpression::create(constructorCall)));

    finishNonAbstractMethod(getProxyMethod);
}

// Generate the following method:
//
// [ProcessType] get[ProcessType]_Proxy() {
//     return new [ProcessType]_Proxy(Process.getPid)
// }
//
void ProcessGenerator::generateMessageHandlerGetProcessProxyMethod() {
    auto getProxyMethod =
        createGetProxyMethodSignature(tree.getCurrentClass(),
                                      tree.startBlock(),
                                      inputClassName);

    auto constructorCall =
        MethodCallExpression::create(inputClassName + "_Proxy");
    constructorCall->addArgument(
        MemberSelectorExpression::create(processTypeName, getPidMethodName));
    tree.addStatement(
        ReturnStatement::create(
            HeapAllocationExpression::create(constructorCall)));

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
    auto allocation =
        HeapAllocationExpression::create(
            MethodCallExpression::create(messageHandlerClassName));
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
    auto proxyConstructorMethod =
        generateProxyConstructorMethodSignature(tree.startBlock(),
                                                includeProcessName);

    auto spawn = MethodCallExpression::create(spawnMethodName);
    spawn->addArgument(HeapAllocationExpression::create(
        MethodCallExpression::create(factoryClassName)));
    if (includeProcessName) {
        spawn->addArgument(nameVariableName);
    }
    tree.addStatement(BinaryExpression::create(
        Operator::Assignment,
        NamedEntityExpression::create(pidVariableName),
        MemberSelectorExpression::create(processTypeName, spawn)));

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

    tree.addStatement(BinaryExpression::create(
        Operator::Assignment,
        NamedEntityExpression::create(pidVariableName),
        NamedEntityExpression::create(argVariableName)));

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

    auto proxyMethod =
        generateProxyRemoteMethodSignature(remoteMethodSignature,
                                           tree.startBlock());

    tree.addStatement(generateMessageDeclaration(remoteMethodSignature));
    auto send = MethodCallExpression::create(sendMethodName);
    send->addArgument(pidVariableName);
    send->addArgument(messageVariableName);
    tree.addStatement(MemberSelectorExpression::create(processTypeName, send));
    auto remoteCallReturnType = remoteMethodSignature->getReturnType();
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

    auto callClassConstructorCall = MethodCallExpression::create(
        inputClassName + "_" +
        remoteMethodSignature->getName() + "_Call");
    if (!remoteMethodSignature->getReturnType()->isVoid()) {
        callClassConstructorCall->addArgument(
            MemberSelectorExpression::create(processTypeName,
                                             getPidMethodName));
    }

    for (auto argument: remoteMethodSignature->getArgumentList()) {
        callClassConstructorCall->addArgument(
            generateCallClassConstructorCallArgument(argument));
    }

    auto messageClassConstructorCall =
        MethodCallExpression::create(messageTypeName);
    if (inputClass->isInterface()) {
        messageClassConstructorCall->addArgument(messageHandlerIdVariableName);
        messageClassConstructorCall->addArgument(interfaceIdVariableName);
    } else {
        messageClassConstructorCall->addArgument(
            MemberSelectorExpression::create(messageTypeTypeName,
                                             methodCallConstantName));
    }
    messageClassConstructorCall->addArgument(
        HeapAllocationExpression::create(callClassConstructorCall));
    return VariableDeclarationStatement::create(
        messageVariableName,
        HeapAllocationExpression::create(messageClassConstructorCall));
}

// Generate the following expression:
//
// arg
// Or, if the argument is of process type:
// arg.get[ProcessType]_Proxy
//
Expression* ProcessGenerator::generateCallClassConstructorCallArgument(
    const VariableDeclaration* argument) {

    auto argumentType = argument->getType();
    Definition* definition = argumentType->getDefinition();
    if (definition->isClass() &&
        definition->cast<ClassDefinition>()->isProcess()) {
        return MemberSelectorExpression::create(
            argument->getIdentifier(),
            "get" + argumentType->getName() + "_Proxy");
    }
    return NamedEntityExpression::create(argument->getIdentifier());
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
    auto receiveMethodResult =
        MethodCallExpression::create(receiveMethodResultMethodName);
    receiveMethodResult->addArgument(
        MemberSelectorExpression::create(messageVariableName, idVariableName));
    auto selector =
        MemberSelectorExpression::create(
            processTypeName,
            MemberSelectorExpression::create(
                receiveMethodResult,
                NamedEntityExpression::create(dataVariableName)));
    auto typeCast = TypeCastExpression::create(returnType, selector);
    Expression* returnedExpression = nullptr;
    if (remoteCallReturnType->isReference()) {
        returnedExpression = typeCast;
    } else {
        returnedExpression =
            MemberSelectorExpression::create(
                typeCast,
                NamedEntityExpression::create(valueVariableName));
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

    tree.addStatement(ReturnStatement::create(ThisExpression::create()));

    finishNonAbstractMethod(getProxyMethod);
}

// Generate the following method:
//
// wait() {
//     Process.wait(pid)
// }
//
void ProcessGenerator::generateProxyWaitMethod() {
    auto waitMethod =
        createWaitMethodSignature(tree.getCurrentClass(), tree.startBlock());

    auto wait = MethodCallExpression::create(waitMethodName);
    wait->addArgument(pidVariableName);
    tree.addStatement(MemberSelectorExpression::create(processTypeName, wait));

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
    auto constructor = inputClass->getDefaultConstructor();

    auto registerHandler =
        MethodCallExpression::create(registerMessageHandlerMethodName);
    registerHandler->addArgument(ThisExpression::create());
    auto assignment = BinaryExpression::create(
        Operator::Assignment,
        NamedEntityExpression::create(messageHandlerIdVariableName),
        MemberSelectorExpression::create(processTypeName, registerHandler));
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
    auto handleMessageMethod =
        generateHandleMessageMethodSignature(tree.startBlock());

    auto match = MatchExpression::create(
        MemberSelectorExpression::create(messageVariableName,
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
