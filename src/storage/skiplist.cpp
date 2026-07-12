#include "skiplist.h"
#include <chrono>
#include <stdexcept>

namespace inferno {
namespace storage {

ZSkipList::ZSkipList() : length_(0), level_(1) {
    rng_.seed(std::chrono::system_clock::now().time_since_epoch().count());
    header_ = new ZSkipListNode(ZSKIPLIST_MAXLEVEL, 0, "");
    tail_ = nullptr;
}

ZSkipList::~ZSkipList() {
    ZSkipListNode* node = header_->forward[0];
    while (node) {
        ZSkipListNode* next = node->forward[0];
        delete node;
        node = next;
    }
    delete header_;
}

int ZSkipList::randomLevel() {
    int level = 1;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    while (dist(rng_) < ZSKIPLIST_P && level < ZSKIPLIST_MAXLEVEL) {
        level++;
    }
    return level;
}

ZSkipListNode* ZSkipList::insert(double score, const std::string& ele) {
    ZSkipListNode* update[ZSKIPLIST_MAXLEVEL];
    ZSkipListNode* x = header_;
    
    for (int i = level_ - 1; i >= 0; i--) {
        while (x->forward[i] && 
               (x->forward[i]->score < score || 
                (x->forward[i]->score == score && x->forward[i]->ele < ele))) {
            x = x->forward[i];
        }
        update[i] = x;
    }
    
    int level = randomLevel();
    if (level > level_) {
        for (int i = level_; i < level; i++) {
            update[i] = header_;
        }
        level_ = level;
    }
    
    x = new ZSkipListNode(level, score, ele);
    
    for (int i = 0; i < level; i++) {
        x->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = x;
    }
    
    x->backward = (update[0] == header_) ? nullptr : update[0];
    if (x->forward[0]) {
        x->forward[0]->backward = x;
    } else {
        tail_ = x;
    }
    
    length_++;
    return x;
}

int ZSkipList::deleteNode(double score, const std::string& ele) {
    ZSkipListNode* update[ZSKIPLIST_MAXLEVEL];
    ZSkipListNode* x = header_;
    
    for (int i = level_ - 1; i >= 0; i--) {
        while (x->forward[i] && 
               (x->forward[i]->score < score || 
                (x->forward[i]->score == score && x->forward[i]->ele < ele))) {
            x = x->forward[i];
        }
        update[i] = x;
    }
    
    x = x->forward[0];
    if (x && score == x->score && x->ele == ele) {
        for (int i = 0; i < level_; i++) {
            if (update[i]->forward[i] == x) {
                update[i]->forward[i] = x->forward[i];
            }
        }
        
        if (x->forward[0]) {
            x->forward[0]->backward = x->backward;
        } else {
            tail_ = x->backward;
        }
        
        while (level_ > 1 && header_->forward[level_ - 1] == nullptr) {
            level_--;
        }
        
        length_--;
        delete x;
        return 1;
    }
    return 0;
}

ZSkipListNode* ZSkipList::firstInRange(double min, double max) const {
    ZSkipListNode* x = header_;
    for (int i = level_ - 1; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->score < min) {
            x = x->forward[i];
        }
    }
    x = x->forward[0];
    if (x && x->score <= max) {
        return x;
    }
    return nullptr;
}

ZSkipListNode* ZSkipList::lastInRange(double min, double max) const {
    ZSkipListNode* x = header_;
    for (int i = level_ - 1; i >= 0; i--) {
        while (x->forward[i] && x->forward[i]->score <= max) {
            x = x->forward[i];
        }
    }
    if (x != header_ && x->score >= min) {
        return x;
    }
    return nullptr;
}

bool ZSet::add(const std::string& ele, double score) {
    auto it = dict_.find(ele);
    if (it != dict_.end()) {
        double cur_score = it->second;
        if (cur_score != score) {
            zsl_.deleteNode(cur_score, ele);
            zsl_.insert(score, ele);
            it->second = score;
        }
        return false; // Updated, not added
    } else {
        dict_[ele] = score;
        zsl_.insert(score, ele);
        return true; // Added
    }
}

bool ZSet::remove(const std::string& ele) {
    auto it = dict_.find(ele);
    if (it != dict_.end()) {
        zsl_.deleteNode(it->second, ele);
        dict_.erase(it);
        return true;
    }
    return false;
}

bool ZSet::score(const std::string& ele, double& out_score) const {
    auto it = dict_.find(ele);
    if (it != dict_.end()) {
        out_score = it->second;
        return true;
    }
    return false;
}

} // namespace storage
} // namespace inferno
