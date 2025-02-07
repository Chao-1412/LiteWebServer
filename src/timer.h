#ifndef SRC_EPOLLTIMER_H_
#define SRC_EPOLLTIMER_H_

#include <chrono>
#include <atomic>
#include <queue>
#include <deque>
#include <unordered_map>
#include <mutex>

#include <stdint.h>

#include "spdlog/spdlog.h"

using SteadyClock = std::chrono::steady_clock;
using MilliSeconds = std::chrono::milliseconds;
constexpr const int DEF_CHK_MIN_TIME_MS = 10 * 1000;
constexpr const int DEF_TIMER_EXPIRE_MS = DEF_CHK_MIN_TIME_MS;


struct TimerNode 
{
public:
    TimerNode(int id,
              SteadyClock::time_point expire =
                (SteadyClock::now()+MilliSeconds(DEF_TIMER_EXPIRE_MS)))
        : id(id)
        , expire_(expire)
        {}

public:
    bool operator>(const TimerNode& other) const {
        return expire_ > other.expire_;
    }

public:
    int id;
    SteadyClock::time_point expire_;
};


/**
 * //TODO 改用自旋锁的方式，看看性能会不会有提升
 * @brief 定时器管理类
 *        1. 维护最小堆priority_queue的时间节点队列，
 *        并维护一个id->expireTime的unordered_map，用于快速查找和删除
 *        2. 该定时管理类的思路是，空间换时间：
 *           假设有A-B-C，三个链接分别对应三个定时器
 *           当B链接在超时等待期间接收到新请求或关闭链接时，需要更新B的超时时间或删除B
 *           此时如果直接更新priority_queue，需要遍历整个队列，查找，删除，插入，导致性能较差
 *           这里维护一个额外的unordered_map，
 *           这样，当B链接，需要更新或删除超时时间时，
 *           就可以直接将新的值其插入到priority_queue，像：A-B-C-B这样，然后更新id->expireTime的unordered_map
 *           在进行判断是否超时时，先判断id->expireTime的unordered_map中是否存在该id，以及超时时间是否正确
 *           如果不存在或者超时时间不正确，则直接从priority_queue中弹出该节点，不做任何操作，
 *           当遇到关闭的链接时也是同理，这样就减少了遍历，提高了性能
 */
class TimerManager {
public:
    TimerManager()
        : timer_map_(10000) // 预分配空间，避免频繁扩容
        {}

public:
    void add_timer(int id,
                   SteadyClock::time_point expire_time =
                     (SteadyClock::now()+MilliSeconds(DEF_TIMER_EXPIRE_MS)))
    {
        std::lock_guard<std::mutex> lock(mgr_mutex_);
        timer_queue_.emplace(id, expire_time);
        timer_map_[id] = expire_time;
    }

    void add_timer(const TimerNode &node)
    {
        std::lock_guard<std::mutex> lock(mgr_mutex_);
        timer_queue_.emplace(node);
        timer_map_[node.id] = node.expire_;
    }

    /**
     * @brief 更新定时器
     *        目前就是直接再添加一次
     */
    void update_timer(int id, SteadyClock::time_point expire_time)
    {
        add_timer(id, expire_time);
    }

    void rm_timer(int id)
    {
        std::lock_guard<std::mutex> lock(mgr_mutex_);
        timer_map_.erase(id);
    }

    void handle_expired_timers(std::vector<int> &expired)
    {
        // SPDLOG_DEBUG("handle_expired_timers...");
        {
            auto now = SteadyClock::now();
            std::lock_guard<std::mutex> lock(mgr_mutex_);
            while (!timer_queue_.empty()) {
                auto top = timer_queue_.top();

                if (now < top.expire_) { break; }

                // 不管是否在timer_map_中存在，都弹出
                timer_queue_.pop();

                auto one_timer = timer_map_.find(top.id);
                if (one_timer == timer_map_.end()
                      || one_timer->second != top.expire_) {
                    continue;
                }

                expired.push_back(top.id);
                timer_map_.erase(top.id);
            }
        }
    }

    /**
     * @brief 获取堆顶元素
     * @note 仅作测试用，
     *       因为timer_queue中存在重复元素，
     *       所以查找正真的top元素需要拷贝副本，再遍历
     * @return 成功正常返回，失败返回ID=-1的TimerNode
     */
    TimerNode get_top()
    {
        std::lock_guard<std::mutex> lock(mgr_mutex_);
        auto copy = timer_queue_;
        
        while (!copy.empty()) {
            auto top = copy.top();
            auto one_timer = timer_map_.find(top.id);
            if (one_timer == timer_map_.end()
                  || one_timer->second != top.expire_) {
                copy.pop();
                continue;
            }
            return top;
        }

        return TimerNode(-1, SteadyClock::now());
    }
 
    std::size_t queue_size()
    {
        std::lock_guard<std::mutex> lock(mgr_mutex_);
        return timer_map_.size();
    }

private:
    std::priority_queue<TimerNode,
                        std::deque<TimerNode>,
                        std::greater<TimerNode> > timer_queue_;
    std::unordered_map<int, SteadyClock::time_point> timer_map_;
    std::mutex mgr_mutex_;
};

#endif //SRC_EPOLLTIMER_H_
