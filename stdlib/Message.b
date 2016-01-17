import "System"

class MessageType {
    static int MethodCall      = 0
    static int MethodResult    = 1
    static int Terminate       = 2
    static int ChildTerminated = 3
}

message class Message {

    // Type of message.
    int type

    // Unique message ID. Or child PID in case of a ChildTerminated message.
    var int id

    // ID of the destination message handler in the receiving process.
    int messageHandlerId

    // ID of the interface of the destination message handler.
    int interfaceId

    // Data carried by the message.
    var _Cloneable data

    // Create a message.
    init(int msgType) {
        type = msgType
    }

    // Create a message with data.
    init(int msgType, _Cloneable msgData) {
        type = msgType
        data = msgData
    }

    // Create a method call message.
    init(int handlerId, int ifId, _Cloneable methodCallData) {
        type = MessageType.MethodCall
        messageHandlerId = handlerId
        interfaceId = ifId
        data = methodCallData
    }

    // Create a method result message from this message.
    Message createMethodResult(_Cloneable result) {
        let methodResult = new Message(MessageType.MethodResult)
        methodResult.id = id
        methodResult.data = result
        return methodResult
    }
}

interface MessageHandler {

    // Handle a message.
    handleMessage(Message msg)
}

message interface MessageHandlerFactory {

    // Create a message handler.
    MessageHandler createMessageHandler()
}
