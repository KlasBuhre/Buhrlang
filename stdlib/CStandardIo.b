import "System"

native class FileHandle

native class CStandardIo {

    // Open a file stream.
    static FileHandle fopen(string filename, string mode)

    // Open a file stream from a file descriptor.
    static FileHandle fdopen(int fileDescriptor, string mode)

    // Close a file stream.
    static fclose(FileHandle fileHandle)

    // Buffered data is written to the native stream.
    static fflush(FileHandle fileHandle)

    // Write a string to a file stream.
    static fputs(string str, FileHandle fileHandle)

    // Write a character to a file stream.
    static fputc(char character, FileHandle fileHandle)

    // Read line from a file stream.
    static string fgets(FileHandle fileHandle)

    // Write buffer to file stream.
    // TODO: This method should take a byte array.
    static fwrite(string buf, FileHandle fileHandle)

    // Read the specified number of bytes from file stream, or until EOF.
    // TODO: This method should take a byte array.
    static string fread(int numBytes, FileHandle fileHandle)

    // Get the size of the underlying file of a file stream.
    static int fileSize(FileHandle fileHandle)

    // Check if a file exists.
    static bool fileExists(string filename)

    // Gets the stdout stream.
    static FileHandle getStdOut()

    // Gets the stdin stream.
    static FileHandle getStdIn()
}
