#ifndef Process_h
#define Process_h

#include <Runtime.h>
#include <Message.h>
#include <System.h>

class Process: public object {
public:
    static int spawn(Pointer<MessageHandlerFactory> factory);
    static int spawn(
        Pointer<MessageHandlerFactory> factory,
        Pointer<string> name);
    static int registerMessageHandler(Pointer<MessageHandler> messageHandler);
    static void send(int destinationPid, Pointer<Message> message);
    static Pointer<Message> receive();
    static Pointer<Message> receiveMethodResult(int messageId);
    static Pointer<Message> receive(int messageType, int messageId);
    static int getPid();
    static void terminate();
    static void wait(int pid);
    static void sleep(int milliseconds);
};

#endif
