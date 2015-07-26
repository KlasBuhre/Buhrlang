import "System"

native class NativeSocket {

    // Create a native socket. Return value is the file descriptor of the
    // socket.
    static int socket()

    // Bind socket to a port.
    static bind(int socketFd, int port)

    // Start listening for incomming connections.
    static listen(int listenerSocketFd)

    // Accept a connection. Return value is the file descriptor of the acceped
    // socket.
    static int accept(int listenerSocketFd)

    // Connect to a host.
    static bool connect(int socketFd, string host, int port)
}
