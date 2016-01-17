import "System"
import "CStandardIo"
import "Convert"

// Print a string to stdout.
print(string s) {
    CStandardIo.fputs(s, CStandardIo.getStdOut)
}

// Print a character to stdout.
print(char c) {
    CStandardIo.fputc(c, CStandardIo.getStdOut)
}

// Print a byte to stdout.
print(byte b) {
    CStandardIo.fputs(Convert.toStr(b), CStandardIo.getStdOut)
}

// Print an integer to stdout.
print(int i) {
    CStandardIo.fputs(Convert.toStr(i), CStandardIo.getStdOut)
}

// Print a float to stdout.
print(float f) {
    CStandardIo.fputs(Convert.toStr(f), CStandardIo.getStdOut)
}

// Print a boolean to stdout.
print(bool b) {
    CStandardIo.fputs(Convert.toStr(b), CStandardIo.getStdOut)
}

// Print a string followed by newline to stdout.
println(string s) {
    let stdOut = CStandardIo.getStdOut
    CStandardIo.fputs(s, stdOut)
    CStandardIo.fputc('\n', stdOut)
}

// Print a character followed by newline to stdout.
println(char c) {
    let stdOut = CStandardIo.getStdOut
    CStandardIo.fputc(c, stdOut)
    CStandardIo.fputc('\n', stdOut)
}

// Print a byte followed by newline to stdout.
println(byte b) {
    let stdOut = CStandardIo.getStdOut
    CStandardIo.fputs(Convert.toStr(b), stdOut)
    CStandardIo.fputc('\n', stdOut)
}

// Print an integer followed by newline to stdout.
println(int i) {
    let stdOut = CStandardIo.getStdOut
    CStandardIo.fputs(Convert.toStr(i), stdOut)
    CStandardIo.fputc('\n', stdOut)
}

// Print a float followed by newline to stdout.
println(float f) {
    let stdOut = CStandardIo.getStdOut
    CStandardIo.fputs(Convert.toStr(f), stdOut)
    CStandardIo.fputc('\n', stdOut)
}

// Print a boolean followed by newline to stdout.
println(bool b) {
    let stdOut = CStandardIo.getStdOut
    CStandardIo.fputs(Convert.toStr(b), stdOut)
    CStandardIo.fputc('\n', stdOut)
}

// Print a newline to stdout.
println() {
    CStandardIo.fputc('\n', CStandardIo.getStdOut)
}
