#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <random>

namespace inferno {
namespace storage {

const int ZSKIPLIST_MAXLEVEL = 32;
const double ZSKIPLIST_P = 0.25;

struct ZSkipListNode {
    std::string ele;
    double score;
    ZSkipListNode* backward;
    std::vector<ZSkipListNode*> forward;

    ZSkipListNode(int level, double s, std::string e)
        : ele(std::move(e)), score(s), backward(nullptr), forward(level, nullptr) {}
};

class ZSkipList {
public:
    ZSkipList();
    ~ZSkipList();

    ZSkipListNode* insert(double score, const std::string& ele);
    int deleteNode(double score, const std::string& ele);
    
    // Returns the first node with score >= min
    ZSkipListNode* firstInRange(double min, double max) const;
    // Returns the last node with score <= max
    ZSkipListNode* lastInRange(double min, double max) const;
    
    ZSkipListNode* header() const { return header_; }
    ZSkipListNode* tail() const { return tail_; }
    size_t length() const { return length_; }

private:
    int randomLevel();

    ZSkipListNode* header_;
    ZSkipListNode* tail_;
    size_t length_;
    int level_;
    
    std::mt19937 rng_;
};

class ZSet {
public:
    ZSet() = default;
    ~ZSet() = default;

    // Adds a new element or updates the score of an existing one.
    // Returns true if a new element was added, false if updated.
    bool add(const std::string& ele, double score);
    
    // Removes an element. Returns true if it was removed.
    bool remove(const std::string& ele);
    
    // Gets the score of an element. Returns true if found.
    bool score(const std::string& ele, double& out_score) const;
    
    size_t size() const { return dict_.size(); }
    
    const ZSkipList& zsl() const { return zsl_; }

private:
    std::unordered_map<std::string, double> dict_;
    ZSkipList zsl_;
};

} // namespace storage
} // namespace inferno
