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
            std::vector<Block *> &children = block->get_children();

            if (children.size() == 1)
            {
                Block *new_root = children.front();
                children.clear();
                // delete block;
                this->root = new_root;
                delete block;
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

        if (path.empty())
            return;

        Block *block_containing_key = path.back();
        path.pop_back();

        int index = get_index(block_containing_key, key);
        if (index > 0 && block_containing_key->get_kv_pairs().at(index - 1).first == key)
        {
            V value = block_containing_key->get_kv_pairs().at(index - 1).second;
            remove_helper(block_containing_key, key, path);

            std::cout << "the key " << key << " and its value " << value << " were removed from the tree";
        }
    }

    void search(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty())
        {
            std::cout << "the tree is empty." << std::endl;
            return;
        }

        Block *last_block_seen = path.back();
        path.pop_back();

        int index = get_index(last_block_seen, key);

        if (index > 0 && last_block_seen->get_kv_pairs().at(index - 1).first == key)
        {
            std::cout << key << " was found in the tree, and is paired with the value " << last_block_seen->get_kv_pairs().at(index - 1).second << std::endl;
        }
        else
        {
            std::cout << key << " was not found in the tree" << std::endl;
        }
    }

    V &at(K key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);

        if (path.empty())
        {
            std::cout << "the tree is empty." << std::endl;
            return;
        }

        Block *last_block_seen = path.back();
        int index = get_index(last_block_seen, key);

        if (index > 0 && last_block->get_kv_pairs().at(index - 1).first == key)
        {
            return last_block->get_kv_pairs().at(index - 1).second;
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
    int total_items = 1000;

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
    if (insert_ok)
        std::cout << "PASSED\n";

    // TEST 2: Map Update Logic (Upsert)
    std::cout << "[TEST 2] Testing Value Updates... ";
    tree.insert(500, 9999); // Overwrite old value (5000) with 9999
    // Note: You might need to add a 'get' or 'at' method to your B_Tree
    // to verify the value 9999 is actually there.
    if (tree.in_tree(500))
        std::cout << "PASSED (Key exists)\n";
    else
        std::cout << "FAILED (Key lost during update)\n";

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

// main
int main()
{
    return 0;
}
