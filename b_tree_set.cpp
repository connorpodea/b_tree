#include <vector>
#include <iostream>
#include <iomanip> // for print formatting
#include <limits>  // access to INT_MIN and INT_MAX

// for the testing data
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstdlib> // std::atoi for the benchmark size argument
#include <set>    // reference container for the differential test
#include <string> // invariant checker error messages

template <typename K>

class B_Tree
{
private:
    class Block
    {
    private:
        int b_count;
        int min_keys;
        int max_keys;
        int min_children;
        int max_children;

        std::vector<K> keys;
        std::vector<Block *> children;

    public:
        Block(int b_count)
        {
            this->b_count = b_count;
            this->min_keys = b_count - 1;
            this->min_children = b_count;
            this->max_keys = 2 * b_count - 1;
            this->max_children = 2 * b_count;

            this->keys.reserve(this->max_keys + 1);
            this->children.reserve(this->max_children + 1);
        }

        std::vector<K> &get_keys() { return this->keys; }
        std::vector<Block *> &get_children() { return this->children; }

        int get_b_count() { return this->b_count; }
        int get_min_keys() { return this->min_keys; }
        int get_max_keys() { return this->max_keys; }
        int get_min_children() { return this->min_children; }
        int get_max_children() { return this->max_children; }
    };

    Block *root;

    // the public methods narrate what they do, which is handy for a demo but
    // ruins a benchmark, so it can be switched off
    bool verbose = true;

    bool is_leaf(Block *block)
    {
        return block->get_children().empty();
    }

    bool is_root(Block *block)
    {
        return block == this->root;
    }

    int get_index(Block *block, K key)
    {
        std::vector<K> &keys = block->get_keys();
        int left = 0;
        int right = keys.size();

        while (left < right)
        {
            int mid = (left + right) / 2;
            if (keys.at(mid) > key)
            {
                right = mid;
            }
            else
            {
                left = mid + 1;
            }
        }
        return left;
    }

    void destroy(Block *block)
    {
        if (block == nullptr)
        {
            return;
        }

        for (Block *child : block->get_children())
        {
            destroy(child);
        }

        delete block;
    }

    // walks the subtree and returns its height, or -1 if any b-tree invariant
    // is broken. lower/upper are the separator keys this subtree sits between
    // (nullptr means unbounded on that side).
    int check_block(Block *block, const K *lower, const K *upper, bool root, std::string &error)
    {
        std::vector<K> &keys = block->get_keys();
        std::vector<Block *> &children = block->get_children();

        // 1. key count bounds. only the root is allowed under the minimum
        if ((int)keys.size() > block->get_max_keys())
        {
            error = "a block holds more than 2b - 1 keys";
            return -1;
        }
        if (!root && (int)keys.size() < block->get_min_keys())
        {
            error = "a non-root block holds fewer than b - 1 keys";
            return -1;
        }
        if (root && keys.empty() && !children.empty())
        {
            error = "the root has children but no keys";
            return -1;
        }

        // 2. keys inside a block are strictly increasing (no duplicates)
        for (size_t i = 1; i < keys.size(); i++)
        {
            if (!(keys.at(i - 1) < keys.at(i)))
            {
                error = "keys inside a block are not in sorted order";
                return -1;
            }
        }

        // 3. every key falls inside the range this subtree is responsible for
        if (!keys.empty())
        {
            if (lower != nullptr && !(*lower < keys.front()))
            {
                error = "a key is not greater than the separator to its left";
                return -1;
            }
            if (upper != nullptr && !(keys.back() < *upper))
            {
                error = "a key is not less than the separator to its right";
                return -1;
            }
        }

        // 4. a leaf has no children, so the walk ends here
        if (children.empty())
        {
            return 0;
        }

        // 5. an internal block with k keys has exactly k + 1 children
        if (children.size() != keys.size() + 1)
        {
            error = "an internal block has the wrong number of children";
            return -1;
        }

        // 6. recurse, narrowing the allowed range for each child, and require
        //    every leaf to sit at the same depth
        int height = -1;
        for (size_t i = 0; i < children.size(); i++)
        {
            const K *child_lower = (i == 0) ? lower : &keys.at(i - 1);
            const K *child_upper = (i == keys.size()) ? upper : &keys.at(i);

            int child_height = check_block(children.at(i), child_lower, child_upper, false, error);

            if (child_height < 0)
            {
                return -1;
            }

            if (height == -1)
            {
                height = child_height;
            }
            else if (child_height != height)
            {
                error = "leaves are not all at the same depth";
                return -1;
            }
        }

        return height + 1;
    }

    int get_child_index(Block *parent, Block *child)
    {
        std::vector<Block *> &children = parent->get_children();

        for (int i = 0; i < children.size(); i++)
        {
            if (children.at(i) == child)
            {
                return i;
            }
        }

        // should never happen
        return INT_MIN;
    }

    void insert_helper(Block *trav, K key, std::vector<Block *> &path)
    {
        // recursively go to leaf
        if (!is_leaf(trav))
        {
            path.push_back(trav);
            int child_index = get_index(trav, key);
            Block *travs_child = trav->get_children().at(child_index);
            insert_helper(travs_child, key, path);
            return;
        }

        // at leaf

        std::vector<K> &keys = trav->get_keys();
        int insert_index = get_index(trav, key);
        keys.insert(keys.begin() + insert_index, key);

        // overflow : needs restructure
        if (keys.size() > trav->get_max_keys())
        {
            insert_restructure(trav, path);
        }
    }

    void insert_restructure(Block *block, std::vector<Block *> &path)
    {
        int b_count = block->get_b_count();

        Block *parent = nullptr;

        // root block has no parent
        if (path.empty())
        {
            Block *new_root = new Block(b_count);
            this->root = new_root;
            parent = new_root;
            parent->get_children().push_back(block);
        }
        else
        {
            parent = path.back();
            path.pop_back();
        }

        // treat the block needing restructure as the "left_half"
        std::vector<K> &keys_to_restructure = block->get_keys();
        std::vector<Block *> &children_to_restructure = block->get_children();

        K key_to_move_up = keys_to_restructure.at(b_count);

        Block *right_half = new Block(b_count);
        std::vector<K> &right_half_keys = right_half->get_keys();
        std::vector<Block *> &right_half_children = right_half->get_children();

        // first b stay in left half [index 0 to b_count - 1]
        // the next key goes into the parent [index b_count]
        // the last b-1 go into right half [index b_count + 1 to size - 1]

        // move keys to right half
        for (int i = b_count + 1; i < keys_to_restructure.size(); i++)
        {
            right_half_keys.push_back(keys_to_restructure.at(i));
        }

        // remove middle key and everything to the right from left block
        keys_to_restructure.erase(keys_to_restructure.begin() + b_count, keys_to_restructure.end());

        // move children if not a leaf
        if (!is_leaf(block))
        {
            for (int i = b_count + 1; i < children_to_restructure.size(); i++)
            {
                right_half_children.push_back(children_to_restructure.at(i));
            }

            children_to_restructure.erase(children_to_restructure.begin() + b_count + 1, children_to_restructure.end());
        }

        std::vector<K> &parent_keys = parent->get_keys();
        std::vector<Block *> &parent_children = parent->get_children();

        int parent_index = get_index(parent, key_to_move_up);

        parent_keys.insert(parent_keys.begin() + parent_index, key_to_move_up);
        parent_children.insert(parent_children.begin() + parent_index + 1, right_half);

        if (parent_keys.size() > parent->get_max_keys())
        {
            insert_restructure(parent, path);
        }
    }

    void search_helper(Block *trav, K target_key, std::vector<Block *> &path)
    {
        path.push_back(trav);
        std::vector<K> &keys = trav->get_keys();
        int index = get_index(trav, target_key);

        // base case : target key exists in current blocks keys
        if (index > 0 && keys.at(index - 1) == target_key)
        {
            return;
        }

        // else, recursively find the block where the key may exist
        std::vector<Block *> &travs_children = trav->get_children();
        if (!travs_children.empty() && travs_children.at(index) != nullptr)
        {
            return search_helper(travs_children.at(index), target_key, path);
        }

        // key is not found
        return;
    }

    void remove_helper(Block *target_block, K key, std::vector<Block *> &path)
    {
        std::vector<K> &target_keys = target_block->get_keys();
        int index = get_index(target_block, key);

        if (is_leaf(target_block))
        {
            if (index > 0 && target_keys.at(index - 1) == key)
            {
                target_keys.erase(target_keys.begin() + index - 1);
            }

            // underflow can only occur when removing a key from a leaf
            if (target_keys.size() < target_block->get_min_keys())
            {
                remove_restructure(target_block, path);
            }
        }
        else
        {
            std::vector<Block *> &children = target_block->get_children();
            Block *replacement_block = nullptr;
            K replacement_key;
            path.push_back(target_block);

            if (index > 0 && children.at(index - 1) != nullptr)
            {
                Block *left_child = children.at(index - 1);
                // find max key of left subtree
                replacement_block = get_replacement(left_child, false, true, path);

                if (replacement_block != nullptr)
                {
                    replacement_key = replacement_block->get_keys().back();
                }
            }

            else if (index < children.size() && children.at(index) != nullptr)
            {
                Block *right_child = children.at(index);
                // find min key of right subtree
                replacement_block = get_replacement(right_child, true, false, path);

                if (replacement_block != nullptr)
                {
                    replacement_key = replacement_block->get_keys().front();
                }
            }

            target_keys.at(index - 1) = replacement_key;
            path.pop_back();
            remove_helper(replacement_block, replacement_key, path);
        }
    }

    void remove_restructure(Block *block, std::vector<Block *> &path)
    {
        // an edge cases arise from merging the only two children of a root
        // edge case 1: root has a child and is now empty.

        if (is_root(block))
        {
            if (!is_leaf(block) && block->get_keys().empty())
            {
                Block *old_root = block;
                this->root = block->get_children().front();
                old_root->get_children().clear();
                delete old_root;
            }
            return;
        }

        // this method is active when block underflowed
        Block *parent = path.back();
        path.pop_back();
        std::vector<K> &parent_keys = parent->get_keys();

        Block *left_sibling = get_sibling(parent, block, true, false);
        Block *right_sibling = get_sibling(parent, block, false, true);

        std::vector<K> &block_keys = block->get_keys();

        // two additional edge cases, however you should always try stealing from a sibling
        // edge case 1: stealing a key from the sibling causes underflow, requiring a merge of the 2 siblings
        // edge case 2: merging requires bridging the gap by pulling down a key from the parent block. if this
        //              causes the parent block to underflow, must recursively restructure the parent block.

        // edge case 2 will be handled within merge

        // always steal from a sibling
        if (right_sibling != nullptr) // slightly more efficient on average, prioritze
        {
            int index_of_parent_key = get_child_index(parent, block);

            std::vector<K> &right_sibling_keys = right_sibling->get_keys();

            // edge case 1 (size - 1 because this checks if after stealing, right will be under min keys)
            if (right_sibling_keys.size() - 1 < right_sibling->get_min_keys())
            {
                merge(parent, right_sibling, block, true, false, path);
                return;
            }

            // push the parent key to the back of block_keys, move up and erase the first key of right_sibling
            block_keys.push_back(parent_keys.at(index_of_parent_key));
            parent_keys.at(index_of_parent_key) = right_sibling_keys.front();
            right_sibling_keys.erase(right_sibling_keys.begin());

            if (!is_leaf(right_sibling))
            {
                block->get_children().push_back(right_sibling->get_children().front());
                right_sibling->get_children().erase(right_sibling->get_children().begin());
            }
        }
        else if (left_sibling != nullptr)
        {
            int index_of_parent_key = get_child_index(parent, block) - 1;
            std::vector<K> &left_sibling_keys = left_sibling->get_keys();

            // edge case 1 (size - 1 because this checks if after stealing, left will be under min keys)
            if (left_sibling_keys.size() - 1 < left_sibling->get_min_keys())
            {
                merge(parent, left_sibling, block, false, true, path);
                return;
            }

            // push the parent key to the front of block_keys, move up and erase the last element of left_sibling
            block_keys.insert(block_keys.begin(), parent_keys.at(index_of_parent_key));
            parent_keys.at(index_of_parent_key) = left_sibling_keys.back();
            left_sibling_keys.pop_back();

            if (!is_leaf(left_sibling))
            {
                block->get_children().insert(block->get_children().begin(), left_sibling->get_children().back());
                left_sibling->get_children().pop_back();
            }
        }
    }

    Block *get_replacement(Block *trav, bool search_min, bool search_max, std::vector<Block *> &path)
    {
        path.push_back(trav);

        if (is_leaf(trav))
        {
            return trav;
        }

        // else, recursively find the next block
        std::vector<Block *> &travs_children = trav->get_children();

        if (search_min)
        {
            return get_replacement(travs_children.front(), true, false, path);
        }

        if (search_max)
        {
            return get_replacement(travs_children.back(), false, true, path);
        }

        // replacement key is not found (should always be found)
        return nullptr;
    }

    Block *get_sibling(Block *parent, Block *target_child, bool left, bool right)
    {
        std::vector<Block *> &children = parent->get_children();
        int index = -1;

        for (int i = 0; i < children.size(); i++)
        {
            if (children.at(i) == target_child)
            {
                index = i;
                break;
            }
        }

        if (left && index > 0)
        {
            // sibling with keys < target_child's keys
            return children.at(index - 1);
        }
        else if (right && index + 1 < children.size())
        {
            // sibling with keys > target_child's keys
            return children.at(index + 1);
        }

        // no valid siblings
        return nullptr;
    }

    void merge(Block *parent, Block *from, Block *to, bool right_to_left, bool left_to_right, std::vector<Block *> &path)
    {
        int to_index = get_child_index(parent, to);
        int parent_key_index;

        if (right_to_left)
        {
            parent_key_index = to_index;
        }
        else if (left_to_right)
        {
            parent_key_index = to_index - 1;
        }

        std::vector<K> &parent_keys = parent->get_keys();
        std::vector<Block *> &parent_children = parent->get_children();

        std::vector<K> &to_keys = to->get_keys();
        std::vector<K> &from_keys = from->get_keys();

        bool leaf = is_leaf(to);

        // transfer all keys, erase parent key, erase child pointer and delete block
        // if not a leaf node, must also handle the transfer of children

        if (right_to_left)
        {
            to_keys.push_back(parent_keys.at(parent_key_index));
            to_keys.insert(to_keys.end(), from_keys.begin(), from_keys.end());

            if (!leaf)
            {
                std::vector<Block *> &to_children = to->get_children();
                std::vector<Block *> &from_children = from->get_children();

                to_children.insert(to_children.end(), from_children.begin(), from_children.end());
            }

            parent_keys.erase(parent_keys.begin() + parent_key_index);
            parent_children.erase(parent_children.begin() + get_child_index(parent, from));
            delete from;
        }
        else if (left_to_right)
        {
            to_keys.insert(to_keys.begin(), parent_keys.at(parent_key_index));
            to_keys.insert(to_keys.begin(), from_keys.begin(), from_keys.end());

            if (!leaf)
            {
                std::vector<Block *> &to_children = to->get_children();
                std::vector<Block *> &from_children = from->get_children();

                to_children.insert(to_children.begin(), from_children.begin(), from_children.end());
            }

            parent_keys.erase(parent_keys.begin() + parent_key_index);
            parent_children.erase(parent_children.begin() + get_child_index(parent, from));
            delete from;
        }

        if (parent_keys.size() < parent->get_min_keys())
        {
            return remove_restructure(parent, path);
        }
    }

public:
    B_Tree()
    {
        this->root = new Block(2);
    }

    B_Tree(int b_count)
    {
        this->root = new Block(b_count);
    }

    ~B_Tree()
    {
        destroy(this->root);
        this->root = nullptr;
    }

    // the tree owns raw Block pointers and frees them in the destructor, so a
    // default copy would hand two trees the same blocks and double free them
    B_Tree(const B_Tree &) = delete;
    B_Tree &operator=(const B_Tree &) = delete;

    void set_verbose(bool on) { this->verbose = on; }

    // true if the tree is still a legal b-tree. on failure, error says why
    bool validate(std::string &error)
    {
        error.clear();
        return check_block(this->root, nullptr, nullptr, true, error) >= 0;
    }

    bool validate()
    {
        std::string error;
        return validate(error);
    }

    void insert(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty())
            return;

        Block *last_block_seen = path.back();
        path.pop_back();
        int index = get_index(last_block_seen, key);

        if (index > 0 && last_block_seen->get_keys().at(index - 1) == key)
        {
            if (this->verbose)
                std::cout << std::left << std::setw(7) << key << " is already in the tree.\n";
        }
        else
        {
            insert_helper(last_block_seen, key, path);
            if (this->verbose)
                std::cout << std::left << std::setw(7) << key << " was added to the tree.\n";
        }
    }

    void remove(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty() || path.back() == nullptr)
            return;

        Block *target_block = path.back();
        std::vector<K> &keys = target_block->get_keys();

        int index = get_index(target_block, key);

        if (index > 0 && keys.at(index - 1) == key)
        {
            path.pop_back();
            remove_helper(target_block, key, path);
            if (this->verbose)
                std::cout << std::left << std::setw(7) << key << " was removed from the tree.\n";
        }
        else
        {
            if (this->verbose)
                std::cout << std::left << std::setw(7) << key << " is NOT in the tree.\n";
        }
    }

    void search(K key)
    {
        if (in_tree(key))
        {
            if (this->verbose)
                std::cout << std::left << std::setw(7) << key << " is in the tree.\n";
        }
        else
        {
            if (this->verbose)
                std::cout << std::left << std::setw(7) << key << " is NOT in the tree.\n";
        }
    }

    bool in_tree(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty())
            return false;

        Block *last_block_seen = path.back();
        path.pop_back();

        int index = get_index(last_block_seen, key);

        if (index > 0 && last_block_seen->get_keys().at(index - 1) == key)
        {
            return true;
        }
        return false;
    }
};

// main

std::vector<int> data_gen(int count)
{
    std::vector<int> result(count);

    // 1. Fill with 1, 2, 3, ... count
    // std::iota is a clean way to fill a range with increasing values
    std::iota(result.begin(), result.end(), 1);

    // 2. Shuffle using a stable random engine
    static std::random_device rd;
    static std::mt19937 engine(rd());
    std::shuffle(result.begin(), result.end(), engine);

    return result;
}

void run_comprehensive_test(int b_count)
{
    std::cout << "\n=== STARTING COMPREHENSIVE B-TREE SET TEST (b=" << b_count << ") ===\n";
    B_Tree<int> tree(b_count);
    tree.set_verbose(false);
    int total_items = 1000;
    std::string error;

    // TEST 1: Insertion
    std::cout << "[TEST 1] Inserting " << total_items << " items... ";
    for (int i = 1; i <= total_items; i++)
    {
        tree.insert(i);
    }

    bool insert_ok = true;
    for (int i = 1; i <= total_items; i++)
    {
        if (!tree.in_tree(i))
        {
            std::cout << "\nFAILED: Key " << i << " missing after insertion.";
            insert_ok = false;
            break;
        }
    }
    if (insert_ok && !tree.validate(error))
    {
        std::cout << "\nFAILED: " << error << " after insertion.";
        insert_ok = false;
    }
    if (insert_ok)
        std::cout << "PASSED\n";

    // TEST 2: Duplicate Insertion (should be a no-op, key stays present)
    std::cout << "[TEST 2] Testing Duplicate Insertion... ";
    tree.insert(500);
    if (tree.in_tree(500))
        std::cout << "PASSED (Key still present)\n";
    else
        std::cout << "FAILED (Key lost on duplicate insert)\n";

    // TEST 3: Non-Leaf Deletion (Internal Node)
    // In a tree with 1000 items, low numbers like 10 or 20 are likely in internal nodes
    std::cout << "[TEST 3] Deleting Internal Node Keys... ";
    int internal_key = 10;
    tree.remove(internal_key);
    if (tree.in_tree(internal_key))
    {
        std::cout << "FAILED: Key " << internal_key << " still exists after remove.\n";
    }
    else
    {
        std::cout << "PASSED\n";
    }

    // TEST 4: Massive Random Deletion (Triggers Borrow & Merge)
    std::cout << "[TEST 4] Random Deletion (Borrow/Merge Stress)... ";
    std::vector<int> random_keys = data_gen(total_items);
    bool delete_ok = true;
    for (int key : random_keys)
    {
        if (key == internal_key)
            continue; // already deleted
        tree.remove(key);
        if (tree.in_tree(key))
        {
            std::cout << "\nFAILED: Key " << key << " still found after removal.";
            delete_ok = false;
            break;
        }
        if (!tree.validate(error))
        {
            std::cout << "\nFAILED: " << error << " after removing key " << key << ".";
            delete_ok = false;
            break;
        }
    }
    if (delete_ok)
        std::cout << "PASSED\n";

    // TEST 5: Empty Tree Integrity
    std::cout << "[TEST 5] Empty Tree State... ";
    if (tree.in_tree(1))
        std::cout << "FAILED (Tree should be empty)\n";
    else
        std::cout << "PASSED\n";

    std::cout << "=== ALL TESTS COMPLETE ===\n\n";
}

// runs a random mix of inserts and removes against a std::set holding the same
// keys, and after every single operation checks both that the tree is still a
// legal b-tree and that it agrees with std::set on every key in range
void run_differential_test(int b_count, int operations)
{
    const int key_range = 500;

    std::cout << "=== DIFFERENTIAL TEST vs std::set (b=" << b_count << ", "
              << operations << " ops) ===\n";

    B_Tree<int> tree(b_count);
    tree.set_verbose(false);
    std::set<int> reference;

    std::mt19937 engine(12345); // fixed seed so a failure is reproducible
    std::uniform_int_distribution<int> key_dist(1, key_range);
    std::uniform_int_distribution<int> op_dist(0, 1);

    std::string error;

    for (int op = 1; op <= operations; op++)
    {
        int key = key_dist(engine);
        bool inserting = op_dist(engine) == 0;

        if (inserting)
        {
            tree.insert(key);
            reference.insert(key);
        }
        else
        {
            tree.remove(key);
            reference.erase(key);
        }

        if (!tree.validate(error))
        {
            std::cout << "FAILED: " << error << " after op " << op << " ("
                      << (inserting ? "insert " : "remove ") << key << ")\n";
            return;
        }

        for (int probe = 1; probe <= key_range; probe++)
        {
            if (tree.in_tree(probe) != (reference.count(probe) > 0))
            {
                std::cout << "FAILED: tree and std::set disagree on key " << probe
                          << " after op " << op << "\n";
                return;
            }
        }
    }

    std::cout << "PASSED (invariants and std::set agreement checked after each of "
              << operations << " ops, " << reference.size() << " keys left in tree)\n\n";
}

void test_tree(int b_count, int num_of_items)
{
    b_count = std::max(2, b_count);
    B_Tree<int> *tree = new B_Tree<int>(b_count);
    tree->set_verbose(false); // otherwise the timings measure iostream, not the tree
    std::vector<int> nums = data_gen(num_of_items);

    std::cout << "\n------------------------------------------------\n";
    std::cout << std::endl;
    std::cout << "Inserting " << num_of_items << " items...";
    auto i_start = std::chrono::high_resolution_clock::now();
    for (int num : nums)
    {
        tree->insert(num);
    }
    auto i_end = std::chrono::high_resolution_clock::now();

    auto i_us = std::chrono::duration_cast<std::chrono::microseconds>(i_end - i_start).count();
    std::cout << "\n  -> Took: " << i_us << " us ("
              << std::fixed << std::setprecision(3) << (double)i_us / 1000.0 << " ms)\n\n";

    std::cout << "Searching " << num_of_items << " items...";
    auto s_start = std::chrono::high_resolution_clock::now();
    for (int num : nums)
    {
        tree->search(num);
    }
    auto s_end = std::chrono::high_resolution_clock::now();

    auto s_us = std::chrono::duration_cast<std::chrono::microseconds>(s_end - s_start).count();
    std::cout << "\n  -> Took: " << s_us << " us ("
              << (double)s_us / 1000.0 << " ms)\n\n";

    // int fail_count = 0;
    std::cout << "Removing " << num_of_items << " items...";
    auto r_start = std::chrono::high_resolution_clock::now();
    for (int num : nums)
    {
        // used for testing remove
        // if (!tree->in_tree(num))
        // {
        //     fail_count++;
        // }
        tree->remove(num);
    }
    auto r_end = std::chrono::high_resolution_clock::now();

    auto r_us = std::chrono::duration_cast<std::chrono::microseconds>(r_end - r_start).count();
    std::cout << "\n  -> Took: " << r_us << " us ("
              << (double)r_us / 1000.0 << " ms)\n";

    std::cout << "\n------------------------------------------------";
    std::cout << "\nStats:";
    std::cout << "\nB-Tree Degree (b): " << b_count;
    // std::cout << "\nFailures: " << fail_count << " / " << num_of_items;
    std::cout << "\nTotal Time: " << (i_us + s_us + r_us) / 1000000.0 << " seconds" << std::endl;
    std::cout << std::endl;

    delete tree;
}


// ---------------------------------------------------------------------------
// benchmarks
// ---------------------------------------------------------------------------

struct Timing
{
    double insert_ms, search_ms, remove_ms;
    double total() const { return insert_ms + search_ms + remove_ms; }
};

static double ms_between(std::chrono::high_resolution_clock::time_point a,
                         std::chrono::high_resolution_clock::time_point b)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count() / 1000.0;
}

// times insert / search / remove over nums, then checks the work actually
// happened so a broken fast path can never look like a fast implementation
Timing time_btree(int b_count, const std::vector<int> &nums)
{
    B_Tree<int> tree(b_count);
    tree.set_verbose(false);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int x : nums)
        tree.insert(x);
    auto t1 = std::chrono::high_resolution_clock::now();

    int found = 0;
    for (int x : nums)
        found += tree.in_tree(x) ? 1 : 0;
    auto t2 = std::chrono::high_resolution_clock::now();

    for (int x : nums)
        tree.remove(x);
    auto t3 = std::chrono::high_resolution_clock::now();

    std::string error;
    if (found != (int)nums.size())
    {
        std::cout << "BENCHMARK BUG: found " << found << " of " << nums.size() << " keys\n";
        exit(1);
    }
    if (!tree.validate(error))
    {
        std::cout << "BENCHMARK BUG: " << error << "\n";
        exit(1);
    }
    for (int x : nums)
    {
        if (tree.in_tree(x))
        {
            std::cout << "BENCHMARK BUG: key " << x << " survived removal\n";
            exit(1);
        }
    }

    return {ms_between(t0, t1), ms_between(t1, t2), ms_between(t2, t3)};
}

Timing time_std_set(const std::vector<int> &nums)
{
    std::set<int> reference;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int x : nums)
        reference.insert(x);
    auto t1 = std::chrono::high_resolution_clock::now();

    int found = 0;
    for (int x : nums)
        found += reference.count(x) ? 1 : 0;
    auto t2 = std::chrono::high_resolution_clock::now();

    for (int x : nums)
        reference.erase(x);
    auto t3 = std::chrono::high_resolution_clock::now();

    if (found != (int)nums.size() || !reference.empty())
    {
        std::cout << "BENCHMARK BUG in the std::set baseline\n";
        exit(1);
    }

    return {ms_between(t0, t1), ms_between(t1, t2), ms_between(t2, t3)};
}

std::vector<int> shuffled_keys(int n)
{
    std::vector<int> nums(n);
    std::iota(nums.begin(), nums.end(), 1);
    std::mt19937 engine(99); // fixed seed so runs are comparable
    std::shuffle(nums.begin(), nums.end(), engine);
    return nums;
}

// how much the minimum degree matters. a bigger b means a shallower tree and
// fewer pointer chases per operation, at the cost of shifting more keys inside
// a block on each insert or delete
void run_fanout_sweep(int n, int trials)
{
    std::cout << "=== FANOUT SWEEP (n=" << n << ", best of " << trials
              << " trials, milliseconds) ===\n\n";

    std::vector<int> nums = shuffled_keys(n);

    std::cout << std::fixed << std::setprecision(1);
    std::cout << std::left << std::setw(10) << "b"
              << std::right << std::setw(10) << "insert"
              << std::setw(10) << "search"
              << std::setw(10) << "remove"
              << std::setw(10) << "total" << "\n";
    std::cout << std::string(50, '-') << "\n";

    int degrees[] = {2, 4, 8, 16, 32, 64, 128};

    for (int b : degrees)
    {
        Timing best{1e18, 1e18, 1e18};

        for (int t = 0; t < trials; t++)
        {
            Timing run = time_btree(b, nums);
            best.insert_ms = std::min(best.insert_ms, run.insert_ms);
            best.search_ms = std::min(best.search_ms, run.search_ms);
            best.remove_ms = std::min(best.remove_ms, run.remove_ms);
        }

        std::cout << std::left << std::setw(10) << b
                  << std::right << std::setw(10) << best.insert_ms
                  << std::setw(10) << best.search_ms
                  << std::setw(10) << best.remove_ms
                  << std::setw(10) << best.total() << "\n";
    }

    std::cout << "\n";
}

// the same workload against std::set, which is a red-black tree. the two are
// run in alternating order so allocator warm-up does not favour one of them
void run_stl_comparison(int n, int b_count, int trials)
{
    std::cout << "=== VS std::set (n=" << n << ", b=" << b_count << ", best of "
              << trials << " trials, milliseconds) ===\n\n";

    std::vector<int> nums = shuffled_keys(n);

    double tree_best = 1e18;
    double std_best = 1e18;

    for (int t = 0; t < trials; t++)
    {
        if (t % 2 == 0)
        {
            std_best = std::min(std_best, time_std_set(nums).total());
            tree_best = std::min(tree_best, time_btree(b_count, nums).total());
        }
        else
        {
            tree_best = std::min(tree_best, time_btree(b_count, nums).total());
            std_best = std::min(std_best, time_std_set(nums).total());
        }
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << std::left << std::setw(22) << "this B-Tree"
              << std::right << std::setw(10) << tree_best << " ms\n";
    std::cout << std::left << std::setw(22) << "std::set"
              << std::right << std::setw(10) << std_best << " ms\n";
    std::cout << std::left << std::setw(22) << "ratio"
              << std::right << std::setw(10) << std::setprecision(2)
              << (std_best / tree_best) << "x\n\n";
    std::cout << (tree_best < std_best ? "this B-Tree is faster at this size\n"
                                       : "std::set is faster at this size\n");
    std::cout << "\n";
}

int main(int argc, char **argv)
{
    // the benchmarks take a size on the command line so the bigger runs are
    // easy to reproduce, e.g. ./b_tree_set 2000000
    int n = (argc > 1) ? std::atoi(argv[1]) : 200000;
    int trials = 3;

    run_comprehensive_test(2);
    run_comprehensive_test(4);

    run_differential_test(2, 2000);
    run_differential_test(4, 2000);

    run_fanout_sweep(n, trials);
    run_stl_comparison(n, 64, trials);

    return 0;
}
