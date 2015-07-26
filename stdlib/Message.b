
enum MessageType {
    MethodCall,
    MethodResult,
    Terminate,
    ChildTerminated
}

class Message {

    // Type of message.
    MessageType type

    // Unique message ID. Or child PID in case of a ChildTerminated message.
    var int id

    // ID of the destination message handler in the receiving process.
    int messageHandlerId

    // ID of the interface of the destination message handler.
    int interfaceId

    // Data carried by the message.
    var object data

    // Create a message.
    init(MessageType msgType) {
        type = msgType
    }

    // Create a method call message.
    init(object methodCallData) {
        type = MessageType.MethodCall
        data = methodCallData
    }

    // Create a method call message.
    init(int handlerId, int ifId, object methodCallData) {
        type = MessageType.MethodCall
        messageHandlerId = handlerId
        interfaceId = ifId
        data = methodCallData
    }

    // Create a method result message from this message.
    Message createMethodResult(object result) {
        let methodResult = new Message(MessageType.MethodResult)
        methodResult.id = id
        methodResult.data = result
        return methodResult
    }
}

interface MessageHandler {

    // Handle a message.
    handleMessage(Message message)
}

interface MessageHandlerFactory {

    // Create a message handler.
    MessageHandler createMessageHandler()
}
