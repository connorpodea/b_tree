#include <vector>
#include <iostream>
#include <iomanip> // for print formatting
#include <limits>  // access to INT_MIN and INT_MAX

// for the testing data
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

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

        std::vector<int> keys;
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

        std::vector<int> &get_keys() { return this->keys; }
        std::vector<Block *> &get_children() { return this->children; }

        int get_b_count() { return this->b_count; }
        int get_min_keys() { return this->min_keys; }
        int get_max_keys() { return this->max_keys; }
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

    int get_index(Block *block, int key)
    {
        std::vector<int> &keys = block->get_keys();
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

    void insert_helper(Block *trav, int key, std::vector<Block *> &path)
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

        std::vector<int> &keys = trav->get_keys();
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
        std::vector<int> &keys_to_restructure = block->get_keys();
        std::vector<Block *> &children_to_restructure = block->get_children();

        int key_to_move_up = keys_to_restructure.at(b_count);

        Block *right_half = new Block(b_count);
        std::vector<int> &right_half_keys = right_half->get_keys();
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

        std::vector<int> &parent_keys = parent->get_keys();
        std::vector<Block *> &parent_children = parent->get_children();

        int parent_index = get_index(parent, key_to_move_up);

        parent_keys.insert(parent_keys.begin() + parent_index, key_to_move_up);
        parent_children.insert(parent_children.begin() + parent_index + 1, right_half);

        if (parent_keys.size() > parent->get_max_keys())
        {
            insert_restructure(parent, path);
        }
    }

    void search_helper(Block *trav, int target_key, std::vector<Block *> &path)
    {
        path.push_back(trav);
        std::vector<int> &keys = trav->get_keys();
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

    void remove_helper(Block *target_block, int key, std::vector<Block *> &path)
    {
        std::vector<int> &target_keys = target_block->get_keys();
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
            int replacement_key;
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
            std::vector<Block *> &children = block->get_children();

            if (children.size() == 1)
            {
                Block *new_root = children.front();
                children.clear();
                // delete block;
                this->root = new_root;
            }
            return;
        }

        // this method is active when block underflowed
        Block *parent = path.back();
        path.pop_back();
        std::vector<int> &parent_keys = parent->get_keys();

        Block *left_sibling = get_sibling(parent, block, true, false);
        Block *right_sibling = get_sibling(parent, block, false, true);

        std::vector<int> &block_keys = block->get_keys();

        // two additional edge cases, however you should always try stealing from a sibling
        // edge case 1: stealing a key from the sibling causes underflow, requiring a merge of the 2 siblings
        // edge case 2: merging requires bridging the gap by pulling down a key from the parent block. if this
        //              causes the parent block to underflow, must recursively restructure the parent block.

        // edge case 2 will be handled within merge

        // always steal from a sibling
        if (right_sibling != nullptr) // slightly more efficient on average, prioritze
        {
            int index_of_parent_key = get_child_index(parent, block);

            std::vector<int> &right_sibling_keys = right_sibling->get_keys();

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
            std::vector<int> &left_sibling_keys = left_sibling->get_keys();

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

        std::vector<int> &parent_keys = parent->get_keys();
        std::vector<Block *> &parent_children = parent->get_children();

        std::vector<int> &to_keys = to->get_keys();
        std::vector<int> &from_keys = from->get_keys();

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

    void insert(int key)
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
            // std::cout << std::left << std::setw(7) << "is already in the tree.\n";
        }
        else
        {
            insert_helper(last_block_seen, key, path);
            // std::cout << std::left << std::setw(7) << key << " was successfully added to the tree.\n";
        }
    }

    void remove(int key)
    {
        std::vector<Block *> path;
        search_helper(this->root, key, path);
        Block *block_containing_key = path.back();
        path.pop_back();

        if (!path.empty() && block_containing_key != nullptr)
        {
            remove_helper(block_containing_key, key, path);
            // std::cout << std::left << std::setw(7) << key << " was removed from the tree.\n";
        }
        else
        {
            // std::cout << std::left << std::setw(7) << key << " is NOT in the tree (REMOVE).\n";
        }
    }

    void search(int key)
    {
        if (in_tree(key))
        {
            // std::cout << std::left << std::setw(7) << key << " is in the tree.\n";
        }
        else
        {
            // std::cout << std::left << std::setw(7) << key << " is NOT in the tree.\n";
        }
    }

    bool in_tree(int key)
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

void test_tree(int b_count, int num_of_items)
{
    b_count = std::max(2, b_count);
    B_Tree *tree = new B_Tree(b_count);
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

    int fail_count = 0;
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
    std::cout << "\nFailures: " << fail_count << " / " << num_of_items;
    std::cout << "\nTotal Time: " << (i_us + s_us + r_us) / 1000000.0 << " seconds" << std::endl;
    std::cout << std::endl;

    delete tree;
}

int main()
{
    test_tree(2, 1000000);
    return 0;
}