import "CStandardLib"

class Convert {

    // Convert integer to string.
    static string toStr(int i) {
        return CStandardLib.toString(i)
    }

    // Convert byte to string.
    static string toStr(byte b) {
        return CStandardLib.toString(b)
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

    // Convert string to int.
    static int toInt(string s) {
        return CStandardLib.toInt(s)
    }

    // Convert string to int.
    static float toFloat(string s) {
        return CStandardLib.toFloat(s)
    }
}
