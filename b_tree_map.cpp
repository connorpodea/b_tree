#include <vector>
#include <iostream>
#include <utility> // for std::pair
#include <iomanip> // for print formatting
#include <limits>  // access to INT_MIN and INT_MAX

// for the testing data
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstdlib> // std::atoi for the benchmark size argument
#include <map>    // reference container for the differential test
#include <string> // invariant checker error messages

template <typename K, typename V>

class B_Tree
{
private:
    class Block
    {
    private:
        int b_count;

        int min_kv_pairs;
        int max_kv_pairs;

        int min_children;
        int max_children;

        std::vector<std::pair<K, V>> kv_pairs;
        std::vector<Block *> children;

    public:
        Block(int b_count)
        {
            this->b_count = b_count;

            this->min_kv_pairs = b_count - 1;
            this->max_kv_pairs = 2 * b_count - 1;

            this->min_children = b_count;
            this->max_children = 2 * b_count;

            this->kv_pairs.reserve(this->max_kv_pairs + 1);
            this->children.reserve(this->max_children + 1);
        }

        std::vector<std::pair<K, V>> &get_kv_pairs() { return this->kv_pairs; }
        std::vector<Block *> &get_children() { return this->children; }

        int get_b_count() { return this->b_count; }

        int get_min_kv_pairs() { return this->min_kv_pairs; }
        int get_max_kv_pairs() { return this->max_kv_pairs; }

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
        std::vector<std::pair<K, V>> &kv_pairs = block->get_kv_pairs();
        int left = 0;
        int right = kv_pairs.size();

        while (left < right)
        {
            int mid = (left + right) / 2;
            if (kv_pairs.at(mid).first > key)
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
        std::vector<std::pair<K, V>> &kv_pairs = block->get_kv_pairs();
        std::vector<Block *> &children = block->get_children();

        // 1. pair count bounds. only the root is allowed under the minimum
        if ((int)kv_pairs.size() > block->get_max_kv_pairs())
        {
            error = "a block holds more than 2b - 1 pairs";
            return -1;
        }
        if (!root && (int)kv_pairs.size() < block->get_min_kv_pairs())
        {
            error = "a non-root block holds fewer than b - 1 pairs";
            return -1;
        }
        if (root && kv_pairs.empty() && !children.empty())
        {
            error = "the root has children but no pairs";
            return -1;
        }

        // 2. keys inside a block are strictly increasing (no duplicates)
        for (size_t i = 1; i < kv_pairs.size(); i++)
        {
            if (!(kv_pairs.at(i - 1).first < kv_pairs.at(i).first))
            {
                error = "keys inside a block are not in sorted order";
                return -1;
            }
        }

        // 3. every key falls inside the range this subtree is responsible for
        if (!kv_pairs.empty())
        {
            if (lower != nullptr && !(*lower < kv_pairs.front().first))
            {
                error = "a key is not greater than the separator to its left";
                return -1;
            }
            if (upper != nullptr && !(kv_pairs.back().first < *upper))
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

        // 5. an internal block with k pairs has exactly k + 1 children
        if (children.size() != kv_pairs.size() + 1)
        {
            error = "an internal block has the wrong number of children";
            return -1;
        }

        // 6. recurse, narrowing the allowed range for each child, and require
        //    every leaf to sit at the same depth
        int height = -1;
        for (size_t i = 0; i < children.size(); i++)
        {
            const K *child_lower = (i == 0) ? lower : &kv_pairs.at(i - 1).first;
            const K *child_upper = (i == kv_pairs.size()) ? upper : &kv_pairs.at(i).first;

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

    void insert_helper(Block *trav, K key, V value, std::vector<Block *> &path)
    {
        // recursively go to leaf
        if (!is_leaf(trav))
        {
            path.push_back(trav);
            int child_index = get_index(trav, key);
            Block *travs_child = trav->get_children().at(child_index);
            insert_helper(travs_child, key, value, path);
            return;
        }

        // at leaf

        std::vector<std::pair<K, V>> &kv_pairs = trav->get_kv_pairs();
        int insert_index = get_index(trav, key);
        kv_pairs.emplace(kv_pairs.begin() + insert_index, key, value);

        // overflow : needs restructure
        if (kv_pairs.size() > trav->get_max_kv_pairs())
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
        std::vector<std::pair<K, V>> &pairs_to_restructure = block->get_kv_pairs();
        std::vector<Block *> &children_to_restructure = block->get_children();

        std::pair<K, V> pair_to_move_up = pairs_to_restructure.at(b_count);

        Block *right_half = new Block(b_count);
        std::vector<std::pair<K, V>> &right_half_kv_pairs = right_half->get_kv_pairs();
        std::vector<Block *> &right_half_children = right_half->get_children();

        // first b stay in left half [index 0 to b_count - 1]
        // the next entry goes into the parent [index b_count]
        // the last b-1 go into right half [index b_count + 1 to size - 1]

        // move entries to right half
        for (int i = b_count + 1; i < pairs_to_restructure.size(); i++)
        {
            right_half_kv_pairs.push_back(pairs_to_restructure.at(i));
        }

        // remove middle key and everything to the right from left block
        pairs_to_restructure.erase(pairs_to_restructure.begin() + b_count, pairs_to_restructure.end());

        // move children if not a leaf
        if (!is_leaf(block))
        {
            for (int i = b_count + 1; i < children_to_restructure.size(); i++)
            {
                right_half_children.push_back(children_to_restructure.at(i));
            }

            children_to_restructure.erase(children_to_restructure.begin() + b_count + 1, children_to_restructure.end());
        }

        std::vector<std::pair<K, V>> &parent_kv_pairs = parent->get_kv_pairs();
        std::vector<Block *> &parent_children = parent->get_children();

        int parent_index = get_index(parent, pair_to_move_up.first);

        parent_kv_pairs.insert(parent_kv_pairs.begin() + parent_index, pair_to_move_up);
        parent_children.insert(parent_children.begin() + parent_index + 1, right_half);

        if (parent_kv_pairs.size() > parent->get_max_kv_pairs())
        {
            insert_restructure(parent, path);
        }
    }

    void search_helper(Block *trav, K target_key, std::vector<Block *> &path)
    {
        path.push_back(trav);
        std::vector<std::pair<K, V>> &travs_kv_pairs = trav->get_kv_pairs();
        int index = get_index(trav, target_key);

        // base case : target key exists in current blocks keys
        if (index > 0 && travs_kv_pairs.at(index - 1).first == target_key)
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
        std::vector<std::pair<K, V>> &target_pairs = target_block->get_kv_pairs();
        int index = get_index(target_block, key);

        if (is_leaf(target_block))
        {
            if (index > 0 && target_pairs.at(index - 1).first == key)
            {
                target_pairs.erase(target_pairs.begin() + index - 1);
            }

            // underflow can only occur when removing a key from a leaf
            if (target_pairs.size() < target_block->get_min_kv_pairs())
            {
                remove_restructure(target_block, path);
            }
        }
        else
        {
            std::vector<Block *> &children = target_block->get_children();
            Block *replacement_block = nullptr;
            std::pair<K, V> replacement_pair;
            path.push_back(target_block);

            if (index > 0 && children.at(index - 1) != nullptr)
            {
                Block *left_child = children.at(index - 1);
                // find max key of left subtree
                replacement_block = get_replacement(left_child, false, true, path);

                if (replacement_block != nullptr)
                {
                    replacement_pair = replacement_block->get_kv_pairs().back();
                }
            }

            else if (index < children.size() && children.at(index) != nullptr)
            {
                Block *right_child = children.at(index);
                // find min key of right subtree
                replacement_block = get_replacement(right_child, true, false, path);

                if (replacement_block != nullptr)
                {
                    replacement_pair = replacement_block->get_kv_pairs().front();
                }
            }

            target_pairs.at(index - 1) = replacement_pair;
            path.pop_back();
            remove_helper(replacement_block, replacement_pair.first, path);
        }
    }

    void remove_restructure(Block *block, std::vector<Block *> &path)
    {
        // an edge cases arise from merging the only two children of a root
        // edge case 1: root has a child and is now empty.

        if (is_root(block))
        {
            if (!is_leaf(block) && block->get_kv_pairs().empty())
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
        std::vector<std::pair<K, V>> &parent_kv_pairs = parent->get_kv_pairs();

        Block *left_sibling = get_sibling(parent, block, true, false);
        Block *right_sibling = get_sibling(parent, block, false, true);

        std::vector<std::pair<K, V>> &block_kv_pairs = block->get_kv_pairs();

        // two additional edge cases, however you should always try stealing from a sibling
        // edge case 1: stealing a key from the sibling causes underflow, requiring a merge of the 2 siblings
        // edge case 2: merging requires bridging the gap by pulling down a key from the parent block. if this
        //              causes the parent block to underflow, must recursively restructure the parent block.

        // edge case 2 will be handled within merge

        // always steal from a sibling
        if (right_sibling != nullptr) // slightly more efficient on average, prioritze
        {
            int index_of_parent_key = get_child_index(parent, block);

            std::vector<std::pair<K, V>> &right_sibling_kv_pairs = right_sibling->get_kv_pairs();

            // edge case 1 (size - 1 because this checks if after stealing, right will be under min keys)
            if (right_sibling_kv_pairs.size() - 1 < right_sibling->get_min_kv_pairs())
            {
                merge(parent, right_sibling, block, true, false, path);
                return;
            }

            // push the parent key to the back of block_keys, move up and erase the first key of right_sibling
            block_kv_pairs.push_back(parent_kv_pairs.at(index_of_parent_key));
            parent_kv_pairs.at(index_of_parent_key) = right_sibling_kv_pairs.front();
            right_sibling_kv_pairs.erase(right_sibling_kv_pairs.begin());

            if (!is_leaf(right_sibling))
            {
                block->get_children().push_back(right_sibling->get_children().front());
                right_sibling->get_children().erase(right_sibling->get_children().begin());
            }
        }
        else if (left_sibling != nullptr)
        {
            int index_of_parent_key = get_child_index(parent, block) - 1;
            std::vector<std::pair<K, V>> &left_sibling_kv_pairs = left_sibling->get_kv_pairs();

            // edge case 1 (size - 1 because this checks if after stealing, left will be under min keys)
            if (left_sibling_kv_pairs.size() - 1 < left_sibling->get_min_kv_pairs())
            {
                merge(parent, left_sibling, block, false, true, path);
                return;
            }

            // push the parent key to the front of block_keys, move up and erase the last element of left_sibling
            block_kv_pairs.insert(block_kv_pairs.begin(), parent_kv_pairs.at(index_of_parent_key));
            parent_kv_pairs.at(index_of_parent_key) = left_sibling_kv_pairs.back();
            left_sibling_kv_pairs.pop_back();

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
        int parent_pair_index;

        if (right_to_left)
        {
            parent_pair_index = to_index;
        }
        else if (left_to_right)
        {
            parent_pair_index = to_index - 1;
        }

        std::vector<std::pair<K, V>> &parent_pairs = parent->get_kv_pairs();
        std::vector<Block *> &parent_children = parent->get_children();

        std::vector<std::pair<K, V>> &to_pairs = to->get_kv_pairs();
        std::vector<std::pair<K, V>> &from_pairs = from->get_kv_pairs();

        bool leaf = is_leaf(to);

        // transfer all keys, erase parent key, erase child pointer and delete block
        // if not a leaf node, must also handle the transfer of children

        if (right_to_left)
        {
            to_pairs.push_back(parent_pairs.at(parent_pair_index));
            to_pairs.insert(to_pairs.end(), from_pairs.begin(), from_pairs.end());

            if (!leaf)
            {
                std::vector<Block *> &to_children = to->get_children();
                std::vector<Block *> &from_children = from->get_children();

                to_children.insert(to_children.end(), from_children.begin(), from_children.end());
            }

            parent_pairs.erase(parent_pairs.begin() + parent_pair_index);
            parent_children.erase(parent_children.begin() + get_child_index(parent, from));
            delete from;
        }
        else if (left_to_right)
        {
            to_pairs.insert(to_pairs.begin(), parent_pairs.at(parent_pair_index));
            to_pairs.insert(to_pairs.begin(), from_pairs.begin(), from_pairs.end());

            if (!leaf)
            {
                std::vector<Block *> &to_children = to->get_children();
                std::vector<Block *> &from_children = from->get_children();

                to_children.insert(to_children.begin(), from_children.begin(), from_children.end());
            }

            parent_pairs.erase(parent_pairs.begin() + parent_pair_index);
            parent_children.erase(parent_children.begin() + get_child_index(parent, from));
            delete from;
        }

        if (parent_pairs.size() < parent->get_min_kv_pairs())
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

    void insert(K key, V value)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty())
            return;

        Block *last_block_seen = path.back();
        path.pop_back();
        int index = get_index(last_block_seen, key);

        if (index > 0 && last_block_seen->get_kv_pairs().at(index - 1).first == key)
        {
            // replace the value associated with that key
            V prev_val = last_block_seen->get_kv_pairs().at(index - 1).second;
            last_block_seen->get_kv_pairs().at(index - 1).second = value;
            if (this->verbose)
                std::cout << "the key " << key << " with previous value " << prev_val << " was reassigned with value " << value << std::endl;
        }
        else
        {
            insert_helper(last_block_seen, key, value, path);
        }
    }

    void remove(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty() || path.back() == nullptr)
            return;

        Block *target_block = path.back();

        int index = get_index(target_block, key);

        if (index > 0 && target_block->get_kv_pairs().at(index - 1).first == key)
        {
            path.pop_back();
            V value = target_block->get_kv_pairs().at(index - 1).second;
            remove_helper(target_block, key, path);

            if (this->verbose)
                std::cout << "the key " << key << " and its value " << value << " were removed from the tree" << std::endl;
        }
        else
        {
            if (this->verbose)
                std::cout << "the key " << key << " was not found in the tree" << std::endl;
        }
    }

    void search(K key)
    {
        try
        {
            V &value = at(key);

            if (this->verbose)
                std::cout << key << " was found in the tree, and is paired with the value " << value << std::endl;
        }
        catch (const std::out_of_range &)
        {
            if (this->verbose)
                std::cout << key << " was not found in the tree" << std::endl;
        }
    }

    V &at(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty())
        {
            throw std::out_of_range("tree is empty");
        }

        Block *last_block_seen = path.back();
        int index = get_index(last_block_seen, key);

        if (index > 0 && last_block_seen->get_kv_pairs().at(index - 1).first == key)
        {
            return last_block_seen->get_kv_pairs().at(index - 1).second;
        }

        throw std::out_of_range("key not found");
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

        if (index > 0 && last_block_seen->get_kv_pairs().at(index - 1).first == key)
        {
            return true;
        }
        return false;
    }
};

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
    std::cout << "\n=== STARTING COMPREHENSIVE B-TREE MAP TEST (b=" << b_count << ") ===\n";
    B_Tree<int, int> tree(b_count);
    tree.set_verbose(false);
    int total_items = 1000;
    std::string error;

    // TEST 1: Insertion & Correct Value Mapping
    std::cout << "[TEST 1] Inserting " << total_items << " items... ";
    for (int i = 1; i <= total_items; i++)
    {
        tree.insert(i, i * 10); // Value is 10x the key
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

    // TEST 2: Map Update Logic (Upsert)
    std::cout << "[TEST 2] Testing Value Updates... ";
    tree.insert(500, 9999); // Overwrite old value (5000) with 9999
    if (!tree.in_tree(500))
        std::cout << "FAILED (Key lost during update)\n";
    else if (tree.at(500) != 9999)
        std::cout << "FAILED (Key kept its old value " << tree.at(500) << ")\n";
    else
        std::cout << "PASSED (Key exists with the new value)\n";

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

// runs a random mix of inserts and removes against a std::map holding the same
// pairs, and after every single operation checks both that the tree is still a
// legal b-tree and that it agrees with std::map on every key in range
void run_differential_test(int b_count, int operations)
{
    const int key_range = 500;

    std::cout << "=== DIFFERENTIAL TEST vs std::map (b=" << b_count << ", "
              << operations << " ops) ===\n";

    B_Tree<int, int> tree(b_count);
    tree.set_verbose(false);
    std::map<int, int> reference;

    std::mt19937 engine(12345); // fixed seed so a failure is reproducible
    std::uniform_int_distribution<int> key_dist(1, key_range);
    std::uniform_int_distribution<int> value_dist(1, 1000000);
    std::uniform_int_distribution<int> op_dist(0, 1);

    std::string error;

    for (int op = 1; op <= operations; op++)
    {
        int key = key_dist(engine);
        bool inserting = op_dist(engine) == 0;

        if (inserting)
        {
            int value = value_dist(engine);
            tree.insert(key, value);
            reference[key] = value; // upsert, same as the tree
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

        // every key in range must agree on presence, and every present key on value
        for (int probe = 1; probe <= key_range; probe++)
        {
            bool in_reference = reference.count(probe) > 0;

            if (tree.in_tree(probe) != in_reference)
            {
                std::cout << "FAILED: tree and std::map disagree on whether key " << probe
                          << " is present, after op " << op << "\n";
                return;
            }

            if (in_reference && tree.at(probe) != reference.at(probe))
            {
                std::cout << "FAILED: key " << probe << " holds " << tree.at(probe)
                          << " but std::map holds " << reference.at(probe)
                          << ", after op " << op << "\n";
                return;
            }
        }
    }

    std::cout << "PASSED (invariants and std::map agreement checked after each of "
              << operations << " ops, " << reference.size() << " keys left in tree)\n\n";
}

void test_tree(int b_count, int num_of_items)
{
    b_count = std::max(2, b_count);
    B_Tree<int, int> *tree = new B_Tree<int, int>(b_count);
    tree->set_verbose(false); // otherwise the timings measure iostream, not the tree
    std::vector<int> nums = data_gen(num_of_items);

    std::cout << "\n------------------------------------------------\n";
    std::cout << std::endl;
    std::cout << "Inserting " << num_of_items << " items...";
    auto i_start = std::chrono::high_resolution_clock::now();
    for (int num : nums)
    {
        tree->insert(num, num * 10);
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

    std::cout << "Removing " << num_of_items << " items...";
    auto r_start = std::chrono::high_resolution_clock::now();
    for (int num : nums)
    {
        tree->remove(num);
    }
    auto r_end = std::chrono::high_resolution_clock::now();

    auto r_us = std::chrono::duration_cast<std::chrono::microseconds>(r_end - r_start).count();
    std::cout << "\n  -> Took: " << r_us << " us ("
              << (double)r_us / 1000.0 << " ms)\n";

    std::cout << "\n------------------------------------------------";
    std::cout << "\nStats:";
    std::cout << "\nB-Tree Degree (b): " << b_count;
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
    B_Tree<int, int> tree(b_count);
    tree.set_verbose(false);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int x : nums)
        tree.insert(x, x);
    auto t1 = std::chrono::high_resolution_clock::now();

    int found = 0;
    for (int x : nums)
        found += tree.in_tree(x) ? 1 : 0;
    auto t2 = std::chrono::high_resolution_clock::now();

    // spot check that the values survived too, not just the keys
    for (size_t i = 0; i < nums.size(); i += 1 + nums.size() / 1000)
    {
        if (tree.at(nums.at(i)) != nums.at(i))
        {
            std::cout << "BENCHMARK BUG: key " << nums.at(i) << " holds the wrong value\n";
            exit(1);
        }
    }

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

Timing time_std_map(const std::vector<int> &nums)
{
    std::map<int, int> reference;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int x : nums)
        reference[x] = x;
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
        std::cout << "BENCHMARK BUG in the std::map baseline\n";
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

// the same workload against std::map, which is a red-black tree. the two are
// run in alternating order so allocator warm-up does not favour one of them
void run_stl_comparison(int n, int b_count, int trials)
{
    std::cout << "=== VS std::map (n=" << n << ", b=" << b_count << ", best of "
              << trials << " trials, milliseconds) ===\n\n";

    std::vector<int> nums = shuffled_keys(n);

    double tree_best = 1e18;
    double std_best = 1e18;

    for (int t = 0; t < trials; t++)
    {
        if (t % 2 == 0)
        {
            std_best = std::min(std_best, time_std_map(nums).total());
            tree_best = std::min(tree_best, time_btree(b_count, nums).total());
        }
        else
        {
            tree_best = std::min(tree_best, time_btree(b_count, nums).total());
            std_best = std::min(std_best, time_std_map(nums).total());
        }
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << std::left << std::setw(22) << "this B-Tree"
              << std::right << std::setw(10) << tree_best << " ms\n";
    std::cout << std::left << std::setw(22) << "std::map"
              << std::right << std::setw(10) << std_best << " ms\n";
    std::cout << std::left << std::setw(22) << "ratio"
              << std::right << std::setw(10) << std::setprecision(2)
              << (std_best / tree_best) << "x\n\n";
    std::cout << (tree_best < std_best ? "this B-Tree is faster at this size\n"
                                       : "std::map is faster at this size\n");
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
