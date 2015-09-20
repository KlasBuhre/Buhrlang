import "CStandardIo"
import "Convert"

class Console {

    // Read a line from stdio.
    static string readLine() {
        return CStandardIo.fgets(CStandardIo.getStdIn)
    }

    // Read an integer from stdio.
    static int readInt() {
        return Convert.toInt(readLine)
    }

    // Read a float from stdio.
    static float readFloat() {
        return Convert.toFloat(readLine)
    }
}
