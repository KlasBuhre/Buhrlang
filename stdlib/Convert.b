import "CStandardLib"

class Convert {

    // Convert integer to string.
    static string toStr(int i) {
        return CStandardLib.toString(i)
    }

    // Convert float to string.
    static string toStr(float f) {
        return CStandardLib.toString(f)
    }

    // Convert boolean to string.
    static string toStr(bool b) {
        if b == true {
            return "true"
        }
        return "false"
    }
}
