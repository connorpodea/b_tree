# B-Tree

A from-scratch, templated B-Tree in C++, written over winter break 2025 to prepare for data structures and algorithms. No external dependencies, just the STL.

## What's here

| File | What it is |
|---|---|
| [b_tree_map.cpp](b_tree_map.cpp) | `B_Tree<K, V>` — an ordered map keyed on `K` |
| [b_tree_set.cpp](b_tree_set.cpp) | `B_Tree<K>` — an ordered set |

Both are self-contained, header-free single files: the tree, its internal `Block`
(node) class, and a small test/benchmark harness all live in one `.cpp`.

## How it's structured

Each `Block` holds up to `2b - 1` keys (or key-value pairs) and up to `2b` child
pointers, where `b` — the minimum degree — is passed into the constructor
(`B_Tree(int b_count)`, defaulting to `b = 2`). Keys inside a block are kept sorted,
and `get_index` binary-searches a block to find where a key belongs or already lives.

- **Insert** walks down to a leaf, inserts in sorted order, and if the leaf
  overflows past `2b - 1` keys, splits it: the middle key moves up into the parent
  and the block is divided into two half-full siblings. Splits can cascade up to
  the root, which is how the tree grows in height.
- **Remove** finds the key (if it's in an internal node, swaps it with its
  in-order predecessor/successor from a leaf first), deletes from a leaf, and if
  that leaf underflows below `b - 1` keys, fixes it by borrowing a key from a
  sibling or, if neither sibling has one to spare, merging with a sibling and
  pulling a key down from the parent. Merges can cascade up, which is how the
  tree shrinks in height.
- **Search** / **`in_tree`** binary-search down the tree in `O(log n)`.

This borrow-before-merge, split/merge-cascades-upward approach is the standard
B-Tree algorithm (as in CLRS) — the point of the project was implementing it
myself rather than reasoning about it abstractly.

## API

Both variants expose:

```cpp
B_Tree<K, V> tree;        // default b = 2
B_Tree<K, V> tree(b);     // explicit minimum degree

tree.insert(key, value);  // set: insert(key) — upsert semantics on the map
tree.remove(key);
tree.search(key);         // prints a found/not-found message
tree.in_tree(key);        // -> bool
```

The map variant additionally has:

```cpp
V &at(K key);              // throws std::out_of_range if missing
```

## Building & running

Each file is standalone and compiles on its own:

```bash
g++ -std=c++17 -O2 b_tree_set.cpp -o b_tree_set
./b_tree_set
```

Both files' `main()` runs the same two things, back to back:

- `run_comprehensive_test(b)` at `b = 2` and `b = 4` — a correctness suite
  covering insertion, duplicate/upsert handling, internal-node deletion, and a
  random-deletion stress test that exercises the borrow/merge underflow paths.
- `test_tree(b, n)` — inserts, searches, and removes 100,000 shuffled integers
  at `b = 2` and prints timing for each phase.

## Known issues

Neither variant frees the tree's nodes on destruction (there's no destructor),
only as a side effect of merges during `remove`.

## Why

Built as a learning exercise to internalize the invariants that make a B-Tree
work — bounded fanout, splitting on overflow, borrow-before-merge on underflow —
by implementing them rather than just reading about them.
