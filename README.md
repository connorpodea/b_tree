C++ Template B-Tree Implementation. Supports Map or Set.

Overview: A high-performance, self-balancing B-Tree implemmentation supporting both Set and Map data stuctures. This data structure is designed for efficient disk-style memory where minimimum retrieval is necessary.

Features: 
- The Set works with any data type K and the Map works with any pair K,V that supports comparison operators.
- Users can define the minimum degree b_count at initialization, which dictates the minimum and maximum capacity of each node.
- Splits full nodes during insertion to prevent overflow.
- Rebalances the tree during deletion by borrowing from siblings or merging nodes to maintain the minimum fill factor (b-1).
- Utilizes std::vector with pre-allocated capacity for keys and child pointers to minimize dynamic reallocations.
- Includes logic for massive random data generation and execution timing for insertion, search, and deletion.

B-Tree Set Interface:
- insert(K key): Inserts a new key. If the root is full, it splits the root and increases tree heigh.
- remove(K key): Deletes a key from the tree. Handles internal node deletions and leaf rebalancing.
- search(K key): Prints confirmation of key's existance within the tree.
- in_tree(K key): Returns a boolean of key's existance within the tree.

Node ("Block") Management - Utilizes a privated nested Block class with attributes defined below:
- int b_count: the order of the tree.
- int min_keys: the minimum keys required for a Block to exist independently.
- int max_keys: the maximum keys allowed within a Block before splitting.
- int min_children: the minimum children pointers allowed for a Block.
- int max_children: the maximum children pointers allowed for a Block.
- std::vector<K> keys: a vector containing all keys associated with a block.
- std::vector<Block *> children: a vector containing the children Blocks of a given Block

B-Tree Map Interface: 
- insert(K key, V value): Inserts the key-value pair. If the key already exists, the value is updated.
- remove(K key): Removes the key-value pair associated with the provided key.
- search(K key): Prints confirmation of key's existance within the tree.
- at(K key): Returns the value associated with the key. Returns a std::out_of_range for cases where the tree is empty or when the key was not found.
- in_tree(K key): Returns a boolean of key's existance within the tree.

Node ("Block") Management - Utilizes a privated nested Block class with attributes defined below:
- int b_count: the order of the tree.
- int min_kv_pairs: the minimum keys required for a Block to exist independently.
- int max_kv_pairs: the maximum keys allowed within a Block before splitting.
- int min_children: the minimum children pointers allowed for a Block.
- int max_children: the maximum children pointers allowed for a Block.
- std::vector<std::pair<K,V>> kv_pairs: a vector containing all kv pairs associated with a block.
- std::vector<Block *> children: a vector containing the children Blocks of a given Block

