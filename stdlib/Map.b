import "Option"

class Node<K, V>(K key, var V value) {
    var Option<Node<K, V> > next = None
}

class HashTable<K, V> {
    init(int numBuckets) {
        buckets = new Option<Node<K, V> >[numBuckets]
        for var i = 0; i < numBuckets; i++ {
            buckets.append(None)
        }
    }

    insert(K key, V value) {
        let index = key.hash % buckets.size
        match buckets[index] {
            Some(node) -> insertWithChaining(node, key, value),
            None -> {
                numEntries++
                buckets[index] = Some(new Node(key, value))
            }
        }
    }

    Option<V> find(K key) {
        var node = buckets[key.hash % buckets.size]
        while {
            match node {
                Some(n) -> {
                    if n.key.equals(key) {
                        return Some(n.value)
                    }
                    node = n.next
                },
                None -> return None
            }
        }
    }

    remove(K key) {
        let index = key.hash % buckets.size
        var node = buckets[index]
        var Option<Node<K, V> > prev = None
        while {
            match node {
                Some(n) -> {
                    if n.key.equals(key) {
                        unlink(n, prev, index)
                        numEntries--
                        return
                    }
                    prev = node
                    node = n.next
                },
                None -> return
            }
        }
    }

    int size() {
        return numEntries
    }

    float loadFactor() {
        return (float) numEntries / (float) buckets.size
    }

    Option<Node<K, V> >[] getBuckets() {
        return buckets
    }

private:
    insertWithChaining(Node<K, V> existingNode, K key, V value) {
        var node = existingNode
        while {
            if node.key.equals(key) {
                node.value = value
                break
            }

            if let Some(next) = node.next {
                node = next
            } else {
                node.next = Some(new Node(key, value))
                numEntries++
                break
            }
        }
    }

    unlink(Node<K, V> node, Option<Node<K, V> > prev, int index) {
        if let Some(prevNode) = prev {
            if let None = node.next {
                prevNode.next = None
            } else {
                prevNode.next = node.next
            }
        } else {
            if let None = node.next {
                buckets[index] = None
            } else {
                buckets[index] = node.next
            }
        }
    }

    var Option<Node<K, V> >[] buckets
    var numEntries = 0
}

class Map<K, V> {

    // Create a map.
    init() {
        hashTable = new HashTable<K, V>(8)
    }

    // Insert a key-value pair into the map.
    insert(K key, V value) {
        if hashTable.loadFactor > 0.75 {
            resize
        }
        hashTable.insert(key, value)
    }

    // Get a value from the map.
    Option<V> find(K key) {
        return hashTable.find(key)
    }

    // Remove a value from the map.
    remove(K key) {
        hashTable.remove(key)
    }

    // Iterate over the map.
    each() (K, V) {
        let buckets = hashTable.getBuckets
        var node = buckets[0]
        let numBuckets = buckets.size
        var index = 0
        while {
            if let Some(n) = node {
                node = n.next
                yield(n.key, n.value)
            } else {
                if ++index == numBuckets {
                    break
                }
                node = buckets[index]
            }
        }
    }

    // Return the size of the map.
    int size() {
        return hashTable.size
    }

private:
    resize() {
        let newTable = new HashTable<K, V>(hashTable.size * 2)
        each |key, value| {
            newTable.insert(key, value)
        }
        hashTable = newTable
    }

    var HashTable<K, V> hashTable
}
