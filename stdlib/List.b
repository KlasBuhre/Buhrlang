import "Option"

class List<T> {

    // Add an element at the end of the list.
    add(T data) {
        match back {
            Some(b) -> {
                let node = Some(new Node(data, None))
                b.next = node
                back = node
            },
            None -> {
                front = Some(new Node(data, None))
                back = front
            }
        }
    }

    // Add an element at the front of the list.
    addFront(T data) {
        match front {
            Some(_) -> {
                let node = Some(new Node(data, front))
                front = node
            },
            None -> {
                front = Some(new Node(data, None))
                back = front
            }
        }
    }

    // Get the front element.
    Option<T> getFront() {
        return getData(front)
    }

    // Get the back element.
    Option<T> getBack() {
        return getData(back)
    }

    // Iterate over the list.
    each() (T) {
        var node = front
        while {
            match node {
                None -> break,
                Some(n) -> {
                    yield(n.data)
                    node = n.next
                }
            }
        }
    }

private:
    class Node(T data, var Option<Node> next)

    var Option<Node> front = None
    var Option<Node> back = None

    Option<T> getData(Option<Node> node) {
        return match node {
            Some(n) -> Some(n.data),
            None    -> None
        }
    }
}
