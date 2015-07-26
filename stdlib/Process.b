import "Message"
import "System"

native class Process {

    // Spawn a new process and return the PID.
    static int spawn(MessageHandlerFactory factory)

    // Spawn a new process with given name and return the PID. If a process of
    // that name is already running then the PID of that process is returned and
    // no new process is spawned.
    static int spawn(MessageHandlerFactory factory, string name)

    // Register a message handler for the current process.
    static int registerMessageHandler(MessageHandler messageHandler)

    // Send a message to a process.
    static send(int destinationPid, Message message)

    // Receive a message.
    static Message receive()

    // Receive a method result message that matches the given ID.
    static Message receiveMethodResult(int messageId)

    // Receive a message that matches the given type and ID.
    static Message receive(MessageType messageType, int messageId)

    // Return the PID of the current process.
    static int getPid()

    // Terminate the current process.
    static terminate()

    // Wait for a given process to terminate.
    static wait(int pid)

    // Block the exection of the current process for the specified milliseconds.
    static sleep(int milliseconds)
}
