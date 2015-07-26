#ifndef NativeSocket_h
#define NativeSocket_h

#include <Runtime.h>
#include <System.h>

class NativeSocket: public object {
public:
    static int socket();
    static void bind(int socketFd, int port);
    static void listen(int listenerSocketFd);
    static int accept(int listenerSocketFd);
    static bool connect(int socketFd, Pointer<string> host, int port);
};

#endif
