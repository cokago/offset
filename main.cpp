
#include <math.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <unistd.h>

#define INTMAX 9999999
#define DOUBLE_PRECISION 0.000001

enum GAP_TYPE {
    NORMAL = 0,
    LOW_BOUND = -1,
    HIGH_BOUND = 1,
};

class GapKey {
public:
    int64_t resource_id{1};
    double weight{0};
    long createtime{0};
    GAP_TYPE gap_type{GAP_TYPE::NORMAL};

    GapKey() = default;
    GapKey(int64_t id, double w, long ct) 
      : resource_id(id), weight(w), createtime(ct) {
    }
};

static bool operator==(const GapKey& k1, const GapKey& k2) {

    if(k1.gap_type != k2.gap_type)
        return false;
    else if(k1.gap_type != GAP_TYPE::NORMAL)
        return true;

    if(k1.gap_type != k2.gap_type ||
        abs(k1.weight-k2.weight) >= DOUBLE_PRECISION ||
        k1.createtime != k2.createtime
        )
        return false;
    return true;
}
static bool operator!=(const GapKey& k1, const GapKey& k2) {
    return !(k1 == k2);
}

// 非确定
static int operator-(const GapKey& k1, const GapKey& k2) {
    int diff = k1.gap_type - k2.gap_type;
    if(diff != 0) {
        return diff>0?INTMAX:-INTMAX;
    } else if(k1.gap_type != GAP_TYPE::NORMAL) {
        return 0;
    }
    return k1.resource_id - k2.resource_id;
}

static bool operator<(const GapKey& k1, const GapKey& k2) {
    if(k1.gap_type == GAP_TYPE::LOW_BOUND) {
        if(k2.gap_type == GAP_TYPE::LOW_BOUND) 
            return false;
        return true;
    } else if(k1.gap_type == GAP_TYPE::HIGH_BOUND) {
        return false;
    } else if(k2.gap_type == GAP_TYPE::LOW_BOUND) {
        return false;
    } else if(k2.gap_type == GAP_TYPE::HIGH_BOUND) {
        if(k1.gap_type == GAP_TYPE::HIGH_BOUND)
            return false;
        return true;
    }

    if(k1.weight - k2.weight < DOUBLE_PRECISION ||
        k1.createtime < k2.createtime)
        return true;
    return false;
}

static bool operator<=(const GapKey& k1, const GapKey& k2) {
    if(k1 == k2)
        return true;
    if(k1 < k2)
        return true;
    return false;
}

class GapPair {
public:
    GapKey first;
    GapKey second;
};

class GapList {
public:

    void Clear() {
        lst.clear();
    }

    void Merge(const GapKey& low, const GapKey& high, GapList& new_gaps, int limit=0) {
        // lst为排序，且无交叉覆盖项
        if(lst.empty()) {
            GapPair pair1, pair2;
            pair1.first.gap_type = GAP_TYPE::LOW_BOUND;
            pair1.second = low;
            pair2.first = high;
            pair2.second.gap_type = GAP_TYPE::HIGH_BOUND;
            new_gaps.lst.emplace_back(std::move(pair1));
            new_gaps.lst.emplace_back(std::move(pair2));
            return ;
        }

        for(int i=0; i<lst.size(); i++) {
            const GapPair& pair = lst[i];
            if(pair.second < low) {
                new_gaps.lst.push_back(pair);
                continue;
            } else if(pair.first < low) {
                GapPair new_pair;
                new_pair.first = pair.first;
                new_pair.second = low;          // 更新
                new_gaps.lst.emplace_back(std::move(new_pair));
                continue;
            } 
        }
        for(int i=0; i<lst.size(); i++) {
            const GapPair& pair = lst[i];
            if(pair.second <= high) {
                continue;
            } else if(pair.first < high) {
                GapPair new_pair;
                new_pair.first = high;          // 更新
                new_pair.second = pair.second;
                new_gaps.lst.emplace_back(std::move(new_pair));
            } else {
                new_gaps.lst.push_back(pair);
            }
        }
        // 项数太多
        // 取最新的，并调整下限(允许重复)
        if(limit >0 && new_gaps.lst.size() > limit) {

            // 优先删除gap距离较小的
            for(int i=0; i<new_gaps.lst.size(); ) {
                const GapPair& pair = new_gaps.lst[i];
                if(pair.second - pair.first < 10) {
                    auto iter = new_gaps.lst.begin();
                    std::advance(iter, i);
                    std::cerr<<"AAAAAAAAAAAAAA "<<(*iter).second.resource_id<<" "<<(*iter).first.resource_id<<std::endl;
                    new_gaps.lst.erase(iter);
                    continue;
                }
                i++;
            }

            auto begin = new_gaps.lst.begin();
            auto last = begin;
            std::advance(last, new_gaps.lst.size() - limit);
            new_gaps.lst.erase(begin, last);
            new_gaps.lst[0].first.gap_type = GAP_TYPE::LOW_BOUND;
        }
    }
    
public:
    std::vector<GapPair> lst;
    std::string category{"common"};
};

void make_query_sql(const GapList& gaps) {
    std::vector<std::string> conditions;
    for(int i=0; i<gaps.lst.size(); i++) {
        const GapPair& pair = gaps.lst[i];
        bool empty = true;
        std::stringstream ss;
        if(pair.first.gap_type != GAP_TYPE::LOW_BOUND) {
            empty = false;
            ss<<pair.first.resource_id<<" < resource_id";
        }
        if(pair.second.gap_type != GAP_TYPE::HIGH_BOUND) {
            if(!empty)
                ss<<" AND ";
            ss<<"resource_id < "<<pair.second.resource_id;
        }
        std::string condition = ss.str();
        conditions.push_back(condition);
    }
    std::stringstream sss;
    sss<<"select * from <table> where category = '"<<gaps.category<<"'";
    if(conditions.size() > 0) {
        sss<<" AND (";
    }
    for(int i=0; i<conditions.size(); i++) {
        std::string& condition = conditions[i];
        if(i != conditions.size()-1)
            sss<<"("<<condition<<") OR ";
        else
            sss<<"("<<condition<<")";
    }
    if(conditions.size() > 0) {
        sss<<")";
    }
    
    std::string sql = sss.str();
    std::cerr<<"SQL: "<<sql<<std::endl;
}

std::string toString(const GapKey& gap) {
    std::stringstream ss;
    ss<<"("<<gap.gap_type<<", "<<gap.resource_id<<", "<<gap.weight<<", "<<gap.createtime<<")";
    return ss.str();
}

std::string toString(const GapPair& pair) {
    std::stringstream ss;
    ss<<"["
      <<" "<<toString(pair.first)<<","
      <<" "<<toString(pair.second)
      <<"]";
    return ss.str();
}

std::string toString(const GapList& gaps) {
    std::stringstream ss;
    ss<<"{\n";
    for(int i=0; i<gaps.lst.size(); i++) {
        const GapPair& pair = gaps.lst[i];
        ss<<toString(pair)<<"\n";
    }
    ss<<"}";
    return ss.str();
}

void Print(const std::string& name, GapList& gaps) {
    std::cerr<<name<<":\n"<<toString(gaps)<<std::endl;
}


int main(int argc, char** argv) {

    srand(time(NULL));

    std::vector<GapKey> data;

    GapList gaps;
    GapList new_gaps;
    gaps.Clear();
    new_gaps.Clear();

    make_query_sql(new_gaps);

    double weight1 = 10000;
    long createtime1 = 100000;

    bool down = false;
    bool up = false;

    int id = 1;

    // 随机生成一些数据
    for(int i=0; i<20; i++) {
        weight1++;
        createtime1++;
        GapKey item{id++, weight1, createtime1};
        data.emplace_back(std::move(item));
    }

    for(int i=0; i<1000; i++) {

        // swap
        gaps = new_gaps;
        new_gaps.Clear();

        int count = rand() % 20; // 模拟随机插入一些新闻
        for(int i=0; i<count; i++) {
            weight1++;
            createtime1++;
            GapKey item{id++, weight1, createtime1};
            data.emplace_back(std::move(item));
        }

        // 当前数据
        std::cerr<<"当前数据(top15): "<<std::endl;
        for(int i=data.size()-1; i>=data.size()-16 && i>=0; i--) {
            GapKey& d = data[i];
            std::cerr<<d.resource_id<<" "<<d.weight<<" "<<d.createtime<<std::endl;
        }
 
        // 取数据
        std::cerr<<"fetched: "<<std::endl;
        std::vector<GapKey> fetched;
        for(int i=0; i<10; i++) {
            if(data.empty()) 
                break;
            GapKey d = data.back();
            std::cerr<<d.resource_id<<" "<<d.weight<<" "<<d.createtime<<std::endl;
            fetched.insert(fetched.begin(), d);
            data.pop_back();
        }
        
        if(fetched.empty()) {
            std::cerr<<"not changed"<<std::endl;
            continue;
        }
        GapKey low = fetched[0];
        GapKey high = fetched[fetched.size()-1];
        gaps.Merge(low, high, new_gaps, 100);

        Print("gaps", gaps);
        Print("new_gaps", new_gaps);
        make_query_sql(new_gaps);

        std::cerr<<std::endl;
        std::cerr<<std::endl;

        std::cerr<<"press any key to continue ..."<<std::endl;
        getchar();
        //sleep(1);
    }

    return 0;
}

