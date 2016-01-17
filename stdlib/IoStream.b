import "System"
import "CStandardIo"

message class IoStream(FileHandle outStream, FileHandle inStream) {

    // Create a bidirectional I/O stream that uses two native streams.
    init(int fileDescriptor) : init(CStandardIo.fdopen(fileDescriptor, "w"),
                                    CStandardIo.fdopen(fileDescriptor, "r")) {}

    // Read one line.
    string readLine() {
        return CStandardIo.fgets(inStream)
    }

    // Read the specified number of bytes or until EOF.
    string read(int numBytes) {
        return CStandardIo.fread(numBytes, inStream)
    }

    // Write string.
    write(string str) {
        CStandardIo.fwrite(str, outStream)
        CStandardIo.fflush(outStream)
    }

    // Close the stream.
    close() {
        if outStream != inStream {
            CStandardIo.fclose(outStream)
            CStandardIo.fclose(inStream)
        } else {
            CStandardIo.fclose(outStream)
        }
    }

    // Use the stream in the lambda and then close it.
    using() () {
        yield()
        close
    }
}
