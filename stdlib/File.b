import "CStandardIo"
import "IoStream"

class File(FileHandle handle): IoStream(handle) {

    // Open a file, give it to a lambda and then close it.
    static open(string filename, string mode) (File) {
        let f = new File(CStandardIo.fopen(filename, mode))
        yield(f)
        f.close
    }

    // Check if a file exists.
    static bool exists(string filename) {
        return CStandardIo.fileExists(filename)
    }

    // Get the size of the underlying file of a file stream.
    int size() {
        return CStandardIo.fileSize(handle)
    }

    // Read the entire file.
    string readAll() {
        return read(size)
    }
}
