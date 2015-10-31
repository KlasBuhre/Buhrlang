#include <stdio.h>
#include <list>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>

#include "Process.h"
#include "Utils.h"
#include "Exception.h"

class ProcessControlBlock {
public:
    ProcessControlBlock(int id, int parent, const std::string& n);
    ~ProcessControlBlock();

    void start(MessageHandlerFactory* factory);
    void run(MessageHandlerFactory* factory);
    void terminate();
    int registerMessageHandler(Pointer<MessageHandler> messageHandler);
    void addMessage(Message* message);
    Message* getMessage();
    Message* getMessage(MessageType messageType, int messageId);

    int getPid() const {
        return pid;
    }

    const std::string& getName() const {
        return name;
    }

private:
    typedef std::list<Message*> MessageQueue;
    typedef std::vector<Pointer<MessageHandler> > MessageHandlerVector;

    int pid;
    int parentPid;
    std::string name;
    Pointer<MessageHandler> defaultMessageHandler;
    MessageHandlerVector messageHandlerVector;
    int messageHandlerIdCounter;
    std::thread thread;
    MessageQueue mailbox;
    std::mutex mutex;
    std::condition_variable condition;
};

class Kernel {
public:
    Kernel();
    ~Kernel();

    int spawnProcess(MessageHandlerFactory* factory, const std::string& name);
    void sendMessage(int destinationPid, Message* message);
    void waitForProcessTermination(int pid);
    void removeProcess(int pid);

private:
    bool isProcessAlive(int pid);

    typedef std::map<int, ProcessControlBlock*> PidToProcessMap;
    typedef std::map<std::string, ProcessControlBlock*> NameToProcessMap;

    PidToProcessMap processMap;
    NameToProcessMap nameToProcessMap;
    int pidCounter;
    int messageIdCounter;
    std::mutex mutex;
};

void _main_();

namespace {
    thread_local ProcessControlBlock* currentProcess;
    Kernel kernel;

    void processEntryPoint(
        ProcessControlBlock* process,
        MessageHandlerFactory* factory) {

        currentProcess = process;

        try {
            if (factory != nullptr) {
                process->run(factory);
            } else {
                _main_();
            }
        } catch (const IndexOutOfBoundsException&) {
            printf("\nIndexOutOfBoundsException\n");
        } catch (const IoException& exception) {
            printf("\nIoException: %s\n", exception.what());
        } catch (const NumberFormatException& exception) {
            printf("\nNumberFormatException: %s\n", exception.what());
        }

        process->terminate();
    }
}

int main() {
    processEntryPoint(currentProcess, nullptr);
    return 0;
}

ProcessControlBlock::ProcessControlBlock(
    int id,
    int parent,
    const std::string& n) :
    pid(id),
    parentPid(parent),
    name(n),
    defaultMessageHandler(nullptr),
    messageHandlerVector(),
    messageHandlerIdCounter(0),
    thread(),
    mailbox(),
    mutex(),
    condition() {}

ProcessControlBlock::~ProcessControlBlock() {
    for (MessageQueue::iterator i = mailbox.begin(); i != mailbox.end(); i++) {
        delete *i;
    }
}

void ProcessControlBlock::start(MessageHandlerFactory* factory) {
    thread = std::thread(processEntryPoint, this, factory);
    thread.detach();
}

void ProcessControlBlock::run(MessageHandlerFactory* factory) {
    Pointer<MessageHandlerFactory> factoryPtr(factory);
    factoryPtr.decreasereferenceCount();
    defaultMessageHandler = factoryPtr->createMessageHandler();

    while (true) {
        Message* message = currentProcess->getMessage();
        MessageType messageType = message->type;
        if (ENUM_IS(messageType, MessageType, MethodCall)) {
            Pointer<Message> messagePtr(message);
            messagePtr.decreasereferenceCount();
            int messageHandlerId = message->messageHandlerId;
            if (messageHandlerId == 0) {
                // Route the message to the default message handler (process
                // object).
                defaultMessageHandler->handleMessage(messagePtr);
            } else {
                // Route the message to the indicated message handler.
                int index = messageHandlerId - 1;
                if (index < messageHandlerVector.size()) {
                    messageHandlerVector[index]->handleMessage(messagePtr);
                }
            }
        } else if (ENUM_IS(messageType, MessageType, Terminate)) {
            delete message;
            break;
        } else {
            delete message;
        }
    }
}

void ProcessControlBlock::terminate() {
    Message* parentNotification = nullptr;
    int parent = 0;
    if (pid != 0) {
        parentNotification = new Message(MessageType::ChildTerminated());
        parentNotification->id = pid;
        parent = parentPid;
    }

    // This will delete this ProcessControlBlock object, so we cannot access any
    // members after this.
    kernel.removeProcess(pid);

    if (parentNotification) {
        kernel.sendMessage(parent, parentNotification);
    }
}

int ProcessControlBlock::registerMessageHandler(
    Pointer<MessageHandler> messageHandler) {

    if (messageHandler == defaultMessageHandler) {
        return 0;
    }
    messageHandlerVector.push_back(messageHandler);
    return ++messageHandlerIdCounter;
}

void ProcessControlBlock::addMessage(Message* message) {
    std::unique_lock<std::mutex> lock(mutex);
    mailbox.push_back(message);
    condition.notify_one();
}

Message* ProcessControlBlock::getMessage() {
    std::unique_lock<std::mutex> lock(mutex);

    // Loop to handle spurious wakeups.
    while (mailbox.empty()) {
        condition.wait(lock);
    }
    Message* message = mailbox.front();
    mailbox.pop_front();    
    return message;
}

Message* ProcessControlBlock::getMessage(
    MessageType messageType,
    int messageId) {

    Message* matchingMessage = nullptr;
    std::unique_lock<std::mutex> lock(mutex);

    while (true) {
        // Loop to handle spurious wakeups.
        while (mailbox.empty()) {
            condition.wait(lock);
        }
        for (MessageQueue::iterator i = mailbox.begin();
             i != mailbox.end();
             i++) {
            Message* message = *i;
            if (ENUM_EQUALS(message->type, messageType) &&
                message->id == messageId) {
                mailbox.erase(i);
                matchingMessage = message;
                break;
            }
        }
        if (matchingMessage != nullptr) {
            break;
        }
        condition.wait(lock);
    }
    return matchingMessage;
}

Kernel::Kernel() :
    processMap(),
    nameToProcessMap(),
    pidCounter(0),
    messageIdCounter(0),
    mutex() {

    ProcessControlBlock* rootProcess = new ProcessControlBlock(0, 0, "root");
    processMap.insert(std::make_pair(0, rootProcess));
    currentProcess = rootProcess;
}

Kernel::~Kernel() {
    for (PidToProcessMap::iterator i = processMap.begin();
         i != processMap.end();
         i++) {
        delete i->second;
    }
}

int Kernel::spawnProcess(
    MessageHandlerFactory* factory,
    const std::string& name) {

    ProcessControlBlock* process = nullptr;
    int pid = 0;

    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!name.empty()) {
            NameToProcessMap::const_iterator i = nameToProcessMap.find(name);
            if (i != nameToProcessMap.end()) {
                ProcessControlBlock* existingProcess = i->second;
                return existingProcess->getPid();
            }
        }
        pid = ++pidCounter;
        process = new ProcessControlBlock(pid, currentProcess->getPid(), name);
        processMap.insert(std::make_pair(pid, process));
        if (!name.empty()) {
            nameToProcessMap.insert(std::make_pair(name, process));
        }
    }

    process->start(factory);
    return pid;
}

void Kernel::sendMessage(int destinationPid, Message* message) {
    std::lock_guard<std::mutex> lock(mutex);

    PidToProcessMap::const_iterator i = processMap.find(destinationPid);
    if (i != processMap.end()) {
        ProcessControlBlock* process = i->second;
        MessageType messageType = message->type;
        if (ENUM_IS(messageType, MessageType, MethodCall) ||
            ENUM_IS(messageType, MessageType, Terminate)) {
            message->id = messageIdCounter++;
        }
        process->addMessage(message);
    }
}

void Kernel::removeProcess(int pid) {
    std::lock_guard<std::mutex> lock(mutex);

    PidToProcessMap::iterator i = processMap.find(pid);
    if (i != processMap.end()) {
        ProcessControlBlock* process = i->second;
        const std::string& processName = process->getName();
        if (!processName.empty()) {
            nameToProcessMap.erase(processName);
        }
        delete process;
        processMap.erase(i);
    }
}

void Kernel::waitForProcessTermination(int childPid) {
    if (isProcessAlive(childPid)) {
        Message* message =
            currentProcess->getMessage(MessageType::ChildTerminated(),
                                       childPid);
        delete message;
    }
}

bool Kernel::isProcessAlive(int pid) {
    bool result = false;
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (processMap.find(pid) != processMap.end()) {
            result = true;
        }
    }
    return result;
}

int Process::spawn(Pointer<MessageHandlerFactory> factory) {
    MessageHandlerFactory* factoryObj = factory.get();

    factory.increasereferenceCount();
    std::string name;
    return kernel.spawnProcess(factoryObj, name);
}

int Process::spawn(
    Pointer<MessageHandlerFactory> factory,
    Pointer<string> name) {

    MessageHandlerFactory* factoryObj = factory.get();
    
    factory.increasereferenceCount();
    std::string nameStr;
    if (name != nullptr) {
        nameStr = std::string(name->buf->data(), name->buf->length());
    }
    return kernel.spawnProcess(factoryObj, nameStr);
}

int Process::registerMessageHandler(Pointer<MessageHandler> messageHandler) {
    return currentProcess->registerMessageHandler(messageHandler);
}

void Process::send(int destinationPid, Pointer<Message> message) {
    // Should clone the message here so that the sending and receiving process
    // don't share the message.
    Message* messageObj = message.get();
    message.increasereferenceCount();
    kernel.sendMessage(destinationPid, messageObj);
}

Pointer<Message> Process::receive() {
    Message* messageObj = currentProcess->getMessage();
    Pointer<Message> messagePtr(messageObj);
    messagePtr.decreasereferenceCount();
    return messagePtr;
}

Pointer<Message> Process::receiveMethodResult(int messageId) {
    return receive(MessageType::MethodResult(), messageId);
}

Pointer<Message> Process::receive(MessageType messageType, int messageId) {
    Message* messageObj = currentProcess->getMessage(messageType, messageId);
    Pointer<Message> messagePtr(messageObj);
    messagePtr.decreasereferenceCount();
    return messagePtr;
}

int Process::getPid() {
    return currentProcess->getPid();
}

void Process::terminate() {
    Message* message = new Message(MessageType::Terminate());
    currentProcess->addMessage(message);
}

void Process::wait(int pid) {
    kernel.waitForProcessTermination(pid);
}

void Process::sleep(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
