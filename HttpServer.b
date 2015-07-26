import "Convert"
import "TcpSocket"
import "File"
import "Trace"

class HttpRequest(string method, string url) {
    static Option<HttpRequest> receive(TcpSocket socket) {
        return match socket.readLine.split {
            [meth, uri, "HTTP/1.1\r\n"] -> Some(new HttpRequest(meth, uri)),
            _                           -> None
        }
    }
}

class HttpResponse(string statusCode, string reasonPhrase) {
    var Option<string> body = None

    string encode() {
        let crlf = "\r\n"
        let bodyContent = match body {
            Some(data) -> data,
            None -> "<html><head><title>" + reasonPhrase +
                    "</title></head><body>" + statusCode + " " + reasonPhrase +
                    "</body></html>"
        }
        return "HTTP/1.1 " + statusCode + " " + reasonPhrase + crlf +
               "Content-Type: text/html; charset=UTF-8" + crlf +
               "Content-Length: " + Convert.toStr(bodyContent.length) + crlf +
               "Connection: close" + crlf + crlf + bodyContent
    }
}

process HttpWorker {
    handleConnection(TcpSocket socket) {
        socket.using { ||
            let request = HttpRequest.receive(socket)
            let response = match request {
                Some(validRequest) -> handleRequest(validRequest),
                None               -> new HttpResponse("400", "Bad Request")
            }
            socket.write(response.encode)
        }
        Process.terminate
    }

    HttpResponse handleRequest(HttpRequest request) {
        return match request.method {
            "GET"  -> handleGet(request),
            "POST" -> handlePost,
            _      -> new HttpResponse("405", "Not Allowed")
        }
    }

    HttpResponse handleGet(HttpRequest request) {
        if !File.exists(request.url) {
            return new HttpResponse("404", "Not Found")
        }
        let response = new HttpResponse("200", "OK")
        File.open(request.url, "r") { |file|
            response.body = Some(file.readAll)
        }
        return response
    }

    HttpResponse handlePost() {
        return new HttpResponse("405", "Not Allowed")
    }
}

main() {
    println("Listening on port 8080...")
    let listenerSocket = TcpSocket.createListener(8080)
    while {
        let acceptedSocket = listenerSocket.accept
        let worker = new HttpWorker
        worker.handleConnection(acceptedSocket)
    }
}
