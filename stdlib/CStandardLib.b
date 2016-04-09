import "System"

native class CStandardLib {

    // Convert byte to string.
    static string toString(byte b)

    // Convert int to string.
    static string toString(int i)

    // Convert long to string.
    static string toString(long l)

    // Convert float to string.
    static string toString(float f)

    // Convert string to int.
    static int toInt(string s)

    // Convert string to float.
    static float toFloat(string s)

    // Generate a random number.
    static int rand()
}
