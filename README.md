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

tree.set_verbose(false);  // stop insert/remove/search from narrating
tree.validate();          // -> bool, true if still a legal B-Tree
tree.validate(error);     // same, and fills error with the first thing wrong
```

`validate` walks the whole tree and checks that every non-root block holds
between `b - 1` and `2b - 1` keys, that keys within a block are strictly
increasing, that an internal block with `k` keys has exactly `k + 1` children,
that every key sits inside the range its subtree is responsible for, and that
all leaves are at the same depth. It's `O(n)`, so it's a test tool, not
something to call in a hot loop.

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

Both files' `main()` takes an optional benchmark size, defaulting to 200,000:

```bash
./b_tree_set            # tests + benchmarks at n = 200,000
./b_tree_set 2000000    # same, at n = 2,000,000
```

It runs four things, back to back:

- `run_comprehensive_test(b)` at `b = 2` and `b = 4` — a correctness suite
  covering insertion, duplicate/upsert handling, internal-node deletion, and a
  random-deletion stress test that exercises the borrow/merge underflow paths.
  `validate()` is checked after the bulk insert and after every one of the 1,000
  random deletions.
- `run_differential_test(b, ops)` at `b = 2` and `b = 4` — 2,000 randomly
  chosen inserts and removes over a 500-key range, mirrored into a `std::set`
  (or `std::map`). After *every* operation it asserts the tree is still a legal
  B-Tree and that it agrees with the STL container on every key in range — for
  the map, on the associated values too. The engine is seeded with a fixed value
  so a failure is reproducible.
- `run_fanout_sweep(n, trials)` — the same insert/search/remove workload at
  `b` = 2, 4, 8, 16, 32, 64 and 128, best of `trials` runs per degree.
- `run_stl_comparison(n, b, trials)` — that workload against `std::set` (or
  `std::map`), which is a red-black tree. The two containers are run in
  alternating order so allocator warm-up doesn't consistently favour one.

`test_tree(b, n)` is still there — it's the original single-degree timing run,
now superseded by the sweep.

Both timing paths call `set_verbose(false)` first; with the per-operation
printing left on, roughly 40% of the measured time was `iostream` rather than
the tree. Both also assert, outside the timed region, that every key was found,
that `validate()` still passes, and that the container is empty at the end — a
benchmark that silently stopped doing the work would otherwise look fast.

## Benchmarks

Apple M2 Pro, 16 GB, macOS 26.6, Apple clang 17, `-O2`. Total milliseconds for
100% insert then 100% search then 100% delete of `n` shuffled unique ints, best
of 3 trials.

Minimum degree matters far more than anything else. A bigger `b` means a
shallower tree and fewer pointer chases per operation, and the keys inside a
block are contiguous, so scanning one block is cache-friendly in a way that
following child pointers is not:

| `b` | set, n=2M | map, n=2M |
|---|---|---|
| 2 | 12,711 ms | 12,849 ms |
| 4 | 6,103 ms | 6,863 ms |
| 8 | 3,246 ms | 3,948 ms |
| 16 | 2,141 ms | 2,693 ms |
| 32 | 1,765 ms | 2,154 ms |
| 64 | 1,657 ms | 2,001 ms |
| 128 | 1,573 ms | 1,960 ms |

Going from `b = 2` to `b = 64` is a 7.7x speedup on the set and 6.4x on the map.

Against the STL red-black trees at `b = 64`, there's a crossover. Below roughly
half a million keys `std::set` wins; above a million this tree wins, and the
gap widens as `n` grows — which is the cache-locality argument for a B-Tree
showing up in practice:

| n | this B-Tree (b=64) | `std::set` | ratio |
|---|---|---|---|
| 50,000 | 31.8 ms | 20.4 ms | 0.6x |
| 100,000 | 64.7 ms | 43.4 ms | 0.7x |
| 250,000 | 168.9 ms | 123.6 ms | 0.7x |
| 500,000 | 346.7 ms | 317.4 ms | 0.9x |
| 1,000,000 | 804.0 ms | 951.4 ms | 1.2x |
| 2,000,000 | 1,655 ms | 2,634 ms | 1.6x |

The map lands in the same place: 2,010 ms against `std::map`'s 3,229 ms at
n = 2,000,000, a 1.6x margin.

Caveat on the low end: at `b = 2` this tree is much slower than `std::set` at
every size. The interesting numbers all come from raising the fanout.

## Memory

Both variants free every block in the destructor, and the copy constructor and
copy assignment are `= delete`d — the tree owns raw `Block *` pointers, so a
default copy would hand two trees the same blocks and double free them.

Checked with macOS `leaks`, which reports 0 leaks for both binaries, including
the case of building a populated tree and destroying it without removing
anything first:

```bash
leaks -atExit -- ./b_tree_set
```

## Why

Built as a learning exercise to internalize the invariants that make a B-Tree
work — bounded fanout, splitting on overflow, borrow-before-merge on underflow —
by implementing them rather than just reading about them.

## Author

Connor Podea — a personal project built for learning advanced data structures
and algorithms.
