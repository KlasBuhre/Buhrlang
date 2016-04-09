import "Trace"
import "CStandardLib"
import "Console"

process PointProcess {
    var int x
    var int y

    setPoint(int inX, int inY) {
        x = inX
        y = inY
    }

    int getX() {
        return x
    }

    int getY() {
        return y
    }
}

process PointConsumer {
    init() {
        Process.sleep(CStandardLib.rand % 1000)
    }

    run() {
        let pp = new PointProcess named "PointServer"
        let xx = pp.getX
        let yy = pp.getY
        println("Point(" + Convert.toStr(xx) + ", " +  Convert.toStr(yy) + ")")
    }
}

main() {
    let pp = new PointProcess named "PointServer"
    pp.setPoint(2, 3)
    var i = 0
    while i++ < 7 {
        let pc = new PointConsumer
        pp.setPoint(2 - i, 3 + i)
        pc.run
    }
    Console.readLine
}
