# B-Tree

A templated B-Tree in C++, written from scratch over winter break 2025 while
prepping for DSA. STL only, no dependencies.

## Files

| File | What it is |
|---|---|
| [b_tree_map.cpp](b_tree_map.cpp) | `B_Tree<K, V>`, an ordered map |
| [b_tree_set.cpp](b_tree_set.cpp) | `B_Tree<K>`, an ordered set |

Each file is standalone. The tree, its inner `Block` class, and the test harness
all sit in one `.cpp`.

## How it works

A `Block` holds up to `2b - 1` keys and `2b` child pointers. `b` is the minimum
degree, passed to the constructor, default 2. Keys in a block stay sorted, and
`get_index` binary searches a block for where a key is or where it should go.

**Insert** walks down to a leaf and inserts in sorted order. If the leaf goes
past `2b - 1` keys it splits: the middle key moves up into the parent, and the
rest divides into two half-full blocks. That can push the parent over its own
limit, so splits cascade upward. This is how the tree gets taller.

**Remove** finds the key. If it sits in an internal node, it gets swapped with
the in-order predecessor or successor from a leaf first, so the actual deletion
always happens in a leaf. If that leaf drops below `b - 1` keys, it borrows a key
from a sibling, or merges with one and pulls a key down from the parent when no
sibling can spare anything. Merges cascade too, which is how the tree gets
shorter.

**Search** and **`in_tree`** binary search down the tree.

None of this is novel, it's the CLRS algorithm. I wanted to write it instead of
just reading it.

## API

```cpp
B_Tree<K, V> tree;        // default b = 2
B_Tree<K, V> tree(b);     // explicit minimum degree

tree.insert(key, value);  // set: insert(key). upsert semantics on the map
tree.remove(key);
tree.search(key);         // prints a found/not-found message
tree.in_tree(key);        // -> bool

tree.set_verbose(false);  // stop insert/remove/search from narrating
tree.validate();          // -> bool, true if still a legal B-Tree
tree.validate(error);     // same, and fills error with the first thing wrong
```

The map also has:

```cpp
V &at(K key);             // throws std::out_of_range if missing
```

`validate()` walks the tree and checks the things that should always hold. Every
non-root block holds between `b - 1` and `2b - 1` keys. Keys in a block strictly
increase. An internal block with `k` keys has `k + 1` children. Every key falls
inside the range its subtree covers. All leaves sit at the same depth. It's
`O(n)`, so it's test code only.

## Building

```bash
g++ -std=c++17 -O2 b_tree_set.cpp -o b_tree_set
./b_tree_set
```

`main()` runs the tests, then the benchmarks at whatever size you give it:

```bash
./b_tree_set            # n = 200,000 (default)
./b_tree_set 2000000
```

What it runs:

- `run_comprehensive_test(b)` at `b = 2` and `b = 4`. Insertion, duplicate and
  upsert handling, internal-node deletion, and a random-deletion stress test to
  hit the borrow and merge paths. `validate()` runs after the bulk insert and
  after each of the 1,000 random deletes.
- `run_differential_test(b, ops)` at `b = 2` and `b = 4`. 2,000 random inserts
  and removes over a 500-key range, mirrored into a `std::set` or `std::map`.
  After every operation it checks the tree is still legal and still agrees with
  the STL container on every key in range, values included for the map. Fixed
  seed, so a failure reproduces.
- `run_fanout_sweep(n, trials)`. The same workload at `b` = 2, 4, 8, 16, 32, 64
  and 128.
- `run_stl_comparison(n, b, trials)`. The same workload against `std::set` or
  `std::map`. They run in alternating order so allocator warm-up doesn't keep
  helping the same one.

`test_tree(b, n)` is the original single-degree timing run. The sweep covers it
now.

Both timing paths call `set_verbose(false)` first. With the per-operation
printing left on, about 40% of the measured time was `iostream`. They also check,
outside the timed section, that every key was found and the container ended up
empty.

## Benchmarks

Apple M2 Pro, 16 GB, macOS 26.6, Apple clang 17, `-O2`. Total milliseconds to
insert all `n`, then search all `n`, then delete all `n`, on shuffled unique
ints. Best of 3 runs.

Minimum degree turned out to matter more than anything else. A bigger `b` means a
shallower tree and fewer pointer hops per operation, and the keys in a block are
contiguous, so scanning one block is cheap next to chasing child pointers:

| `b` | set, n=2M | map, n=2M |
|---|---|---|
| 2 | 12,711 ms | 12,849 ms |
| 4 | 6,103 ms | 6,863 ms |
| 8 | 3,246 ms | 3,948 ms |
| 16 | 2,141 ms | 2,693 ms |
| 32 | 1,765 ms | 2,154 ms |
| 64 | 1,657 ms | 2,001 ms |
| 128 | 1,573 ms | 1,960 ms |

Going from `b = 2` to `b = 64` is 7.7x on the set and 6.4x on the map.

At `b = 64` there's a crossover against the STL red-black trees. Under about half
a million keys `std::set` wins. Past a million this one does, and the margin
keeps growing:

| n | this B-Tree (b=64) | `std::set` | ratio |
|---|---|---|---|
| 50,000 | 31.8 ms | 20.4 ms | 0.6x |
| 100,000 | 64.7 ms | 43.4 ms | 0.7x |
| 250,000 | 168.9 ms | 123.6 ms | 0.7x |
| 500,000 | 346.7 ms | 317.4 ms | 0.9x |
| 1,000,000 | 804.0 ms | 951.4 ms | 1.2x |
| 2,000,000 | 1,655 ms | 2,634 ms | 1.6x |

The map is the same story: 2,010 ms against `std::map`'s 3,229 ms at n = 2M.

At `b = 2` it loses to `std::set` at every size I tested. All of the good numbers
come from raising the fanout.

## Memory

Both variants free every block in the destructor. The copy constructor and copy
assignment are deleted, since the tree owns raw `Block *` and a default copy
would hand two trees the same blocks to free.

macOS `leaks` reports 0 for both, including the case of building a full tree and
destroying it without removing anything first:

```bash
leaks -atExit -- ./b_tree_set
```

## Author

Connor Podea. Personal project, built to learn the data structure properly.
