
class __string {
    private var char[] buf

    init(char[] value) {
        buf = value
    }

    bool notEquals(string other) {
        if equals(other) {
            return false
        }
        return true        
    }

    bool equals(string other) {
        if buf.length != other.buf.length {
            return false
        }
        let length = buf.length
        let otherBuf = other.buf
        for var i = 0; i < length; i++ {
            if buf[i] != otherBuf[i] {
                return false
            }
        }
        return true
    }

    append(string other) {
        buf.appendAll(other.buf)
    }

    string concat(string other) {
        return new string(buf.concat(other.buf))
    }

    int length() {
        return buf.length
    }

    char at(int index) {
        return buf[index]
    }

    char[] characters() {
        return buf
    }

    // Creates a new string with leading and trailing space characters removed.
    string trim() {
        let length = buf.length
        var start = 0
        while start < length {
            if buf[start] != ' ' {
                break
            }
            start++
        }
        var end = length - 1
        while end > 0 {
            if buf[end] != ' ' {
                break
            }
            end--            
        }
        return new string(buf[start...end])
    }

    // Splits the string into an array of substrings/tokens. The delimiter is
    // the space character.
    string[] split() {
        var result = new string[]
        let length = buf.length
        if length == 0 {
            return result
        }
        var tokenStart = 0
        for var i = 0; i < length; i++ {
            if buf[i] == ' ' && i > 0 {
                let tokenChars = buf[tokenStart ... i - 1]
                result.append(new string(tokenChars))
                tokenStart = i + 1
            }
        }
        let tokenChars = buf[tokenStart ... length - 1]
        result.append(new string(tokenChars))
        return result
    }
}
