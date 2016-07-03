#include "Process.h"

#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>
#include <memory>

#include "Utils.h"
#include "Exception.h"

void _main_();

namespace {
    template<typename T, typename... Ts>
    std::unique_ptr<T> make_unique(Ts&&... params)
    {
        return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
    }

    class ProcessControlBlock {
    public:
        ProcessControlBlock(int id, int parent, const std::string& n);

        void start(MessageHandlerFactory* factory);
        void run(MessageHandlerFactory* factory);
        void terminate();
        int registerMessageHandler(Pointer<MessageHandler> messageHandler);
        void addMessage(std::unique_ptr<Message> message);
        std::unique_ptr<Message> getMessage();
        std::unique_ptr<Message> getMessage(int messageType, int messageId);

        int getPid() const {
            return pid;
        }

        const std::string& getName() const {
            return name;
        }

    private:
        using MessageQueue = std::list<std::unique_ptr<Message>>;
        using MessageHandlerVector = std::vector<Pointer<MessageHandler>>;

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

        int spawnProcess(
            MessageHandlerFactory* factory,
            const std::string& name);
        int sendMessage(int destinationPid, std::unique_ptr<Message> message);
        void waitForProcessTermination(int pid);
        void removeProcess(int pid);

    private:
        bool isProcessAlive(int pid);

        using PidToProcessMap =
            std::map<int, std::unique_ptr<ProcessControlBlock>>;
        using NameToProcessMap = std::map<std::string, ProcessControlBlock*>;

        PidToProcessMap processMap;
        NameToProcessMap nameToProcessMap;
        int pidCounter;
        int messageIdCounter;
        std::mutex mutex;
    };

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
    time_t t;
    srand(time(&t));

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

void ProcessControlBlock::start(MessageHandlerFactory* factory) {
    thread = std::thread(processEntryPoint, this, factory);
    thread.detach();
}

void ProcessControlBlock::run(MessageHandlerFactory* factory) {
    Pointer<MessageHandlerFactory> factoryPtr(factory);
    defaultMessageHandler = factoryPtr->createMessageHandler();

    while (true) {
        auto message = std::move(currentProcess->getMessage());
        int messageType = message->type;
        if (messageType == MessageType::MethodCall) {
            Pointer<Message> messagePtr(message.release());
            int messageHandlerId = messagePtr->messageHandlerId;
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
        } else if (messageType == MessageType::Terminate) {
            break;
        }
    }
}

void ProcessControlBlock::terminate() {
    std::unique_ptr<Message> parentNotification;
    int parent = 0;
    if (pid != 0) {
        parentNotification = make_unique<Message>(MessageType::ChildTerminated);
        parentNotification->id = pid;
        parent = parentPid;
    }

    // This will delete this ProcessControlBlock object, so we cannot access any
    // members after this.
    kernel.removeProcess(pid);

    if (parentNotification) {
        kernel.sendMessage(parent, std::move(parentNotification));
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

void ProcessControlBlock::addMessage(std::unique_ptr<Message> message) {
    std::unique_lock<std::mutex> lock(mutex);
    mailbox.push_back(std::move(message));
    condition.notify_one();
}

std::unique_ptr<Message> ProcessControlBlock::getMessage() {
    std::unique_lock<std::mutex> lock(mutex);

    // Loop to handle spurious wakeups.
    while (mailbox.empty()) {
        condition.wait(lock);
    }
    auto message = std::move(mailbox.front());
    mailbox.pop_front();    
    return message;
}

std::unique_ptr<Message> ProcessControlBlock::getMessage(
    int messageType,
    int messageId) {

    std::unique_ptr<Message> matchingMessage;
    std::unique_lock<std::mutex> lock(mutex);

    while (true) {
        // Loop to handle spurious wakeups.
        while (mailbox.empty()) {
            condition.wait(lock);
        }

        for (auto i = mailbox.begin(); i != mailbox.end(); i++) {
            auto message = i->get();
            if (message->type == messageType && message->id == messageId) {
                matchingMessage = std::move(*i);
                mailbox.erase(i);
                break;
            }
        }
        if (matchingMessage) {
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

    auto rootProcess = make_unique<ProcessControlBlock>(0, 0, "root");
    currentProcess = rootProcess.get();
    processMap.insert(std::make_pair(0, std::move(rootProcess)));
}

int Kernel::spawnProcess(
    MessageHandlerFactory* factory,
    const std::string& name) {

    ProcessControlBlock* process = nullptr;
    int pid = 0;

    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!name.empty()) {
            auto i = nameToProcessMap.find(name);
            if (i != nameToProcessMap.end()) {
                ProcessControlBlock* existingProcess = i->second;
                return existingProcess->getPid();
            }
        }
        pid = ++pidCounter;
        process = new ProcessControlBlock(pid, currentProcess->getPid(), name);
        processMap.insert(
            std::make_pair(pid, std::unique_ptr<ProcessControlBlock>(process)));
        if (!name.empty()) {
            nameToProcessMap.insert(std::make_pair(name, process));
        }
    }

    process->start(factory);
    return pid;
}

int Kernel::sendMessage(int destinationPid, std::unique_ptr<Message> message) {
    std::lock_guard<std::mutex> lock(mutex);

    PidToProcessMap::const_iterator i = processMap.find(destinationPid);
    if (i != processMap.end()) {
        ProcessControlBlock* process = i->second.get();
        int messageType = message->type;
        if (messageType == MessageType::MethodCall ||
            messageType == MessageType::Terminate) {
            message->id = messageIdCounter++;
        }
        int messageId = message->id;
        process->addMessage(std::move(message));
        return messageId;
    }
    return 0;
}

void Kernel::removeProcess(int pid) {
    std::lock_guard<std::mutex> lock(mutex);

    auto i = processMap.find(pid);
    if (i != processMap.end()) {
        ProcessControlBlock* process = i->second.get();
        const std::string& processName = process->getName();
        if (!processName.empty()) {
            nameToProcessMap.erase(processName);
        }
        processMap.erase(i);
    }
}

void Kernel::waitForProcessTermination(int childPid) {
    if (isProcessAlive(childPid)) {
        auto message =
            currentProcess->getMessage(MessageType::ChildTerminated, childPid);
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
    return spawn(factory, nullptr);
}

int Process::spawn(
    Pointer<MessageHandlerFactory> factory,
    Pointer<string> name) {

    // Clone the factory so that the sending and receiving process don't share
    // the factory.
    Pointer<MessageHandlerFactory> clonedFactory =
        dynamicPointerCast<MessageHandlerFactory>(factory->_clone());

    // Release the cloned factory from the smart pointer so that the cloned
    // factory is not deleted once this method returns.
    MessageHandlerFactory* clonedFactoryRawPtr = clonedFactory.release();

    std::string nameStr;
    if (name.get() != nullptr) {
        nameStr = std::string(name->buf->data(), name->buf->length());
    }
    return kernel.spawnProcess(clonedFactoryRawPtr, nameStr);
}

int Process::registerMessageHandler(Pointer<MessageHandler> messageHandler) {
    return currentProcess->registerMessageHandler(messageHandler);
}

void Process::send(int destinationPid, Pointer<Message> message) {
    // Clone the message so that the sending and receiving process don't share
    // the message.
    Pointer<Message> clonedMsg = dynamicPointerCast<Message>(message->_clone());

    // Release the cloned message from the ref counted pointer so that the
    // cloned message is not deleted once this method returns. The reference
    // count for messages in the kernel is zero.
    std::unique_ptr<Message> clonedMsgPtr(clonedMsg.release());

    // Send the message.
    // Set the message ID filled in by the kernel so that the generated code can
    // receive a result message based on the message id in the request message.
    message->id = kernel.sendMessage(destinationPid, std::move(clonedMsgPtr));
}

Pointer<Message> Process::receive() {
    Pointer<Message> message(currentProcess->getMessage().release());
    return message;
}

Pointer<Message> Process::receiveMethodResult(int messageId) {
    return receive(MessageType::MethodResult, messageId);
}

Pointer<Message> Process::receive(int messageType, int messageId) {
    Pointer<Message> message(currentProcess->getMessage(messageType,
                                                        messageId).release());
    return message;
}

int Process::getPid() {
    return currentProcess->getPid();
}

void Process::terminate() {
    currentProcess->addMessage(make_unique<Message>(MessageType::Terminate));
}

void Process::wait(int pid) {
    kernel.waitForProcessTermination(pid);
}

void Process::sleep(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
