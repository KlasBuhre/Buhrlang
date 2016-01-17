import "NativeSocket"
import "IoStream"

class TcpSocket(int fileDescriptor): IoStream(fileDescriptor) {

    // Accept incomming connection.
    TcpSocket accept() {
        let acceptedFd = NativeSocket.accept(fileDescriptor)
        return new TcpSocket(acceptedFd)
    }

    // Connect to a host.
    bool connect(string host, int port) {
        return NativeSocket.connect(fileDescriptor, host, port)
    }

    // Create socket.
    static TcpSocket create() {
        return new TcpSocket(NativeSocket.socket)
    }

    // Create a listener socket.
    static TcpSocket createListener(int port) {
        let listenerSocketFd = NativeSocket.socket
        NativeSocket.bind(listenerSocketFd, port)
        NativeSocket.listen(listenerSocketFd)
        return new TcpSocket(listenerSocketFd)
    }

    // Listen on a port and accept incomming connections.
    static acceptLoop(int port) (TcpSocket) {
        let listenerSocket = createListener(port)
        while true {
            let acceptedSocket = listenerSocket.accept
            yield(acceptedSocket)
            acceptedSocket.close
        }
    }

    // Open a socket, give it to a lambda and then close it.
    static open(string host, int port) (TcpSocket) {
        let openSocket = create
        openSocket.connect(host, port)
        yield(openSocket)
        openSocket.close
    }
}
