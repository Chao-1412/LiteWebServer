#include <gtest/gtest.h>
#include "timer.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
using namespace std::chrono;


TEST(TimerTest, iswork) {
    /**
     * @brief 测试基本用法
     */
    auto now = steady_clock::now();
    TimerNode timer_node1{10, now + seconds(1)};
    TimerNode timer_node2{11, now + seconds(2)};
    EXPECT_GT(timer_node2, timer_node1);

    /**
     * @brief 测试添加timer
     */
    TimerManager<MilliSeconds> timer_mgr;
    timer_mgr.add_timer(timer_node1);
    timer_mgr.add_timer(timer_node2);
    timer_mgr.add_timer(12, now + seconds(3));
    timer_mgr.add_timer(13);
    EXPECT_EQ(timer_mgr.queue_size(), 4);

    const TimerNode& node3 = timer_mgr.get_top();
    EXPECT_EQ(node3.id, 10);

    /**
     * @brief 测试移除timer
     */
    timer_mgr.rm_timer(11);
    EXPECT_EQ(timer_mgr.queue_size(), 3);
    timer_mgr.rm_timer(13);
    EXPECT_EQ(timer_mgr.queue_size(), 2);
    timer_mgr.rm_timer(1000);
    EXPECT_EQ(timer_mgr.queue_size(), 2);

    /**
     * @brief 测试过期timer
     *        测试耗时比较久，测过了释掉
     */
    std::this_thread::sleep_for(seconds(2));
    vector<int> expired;
    timer_mgr.handle_expired_timers(expired);
    // 因为timer_mgr的最小检查周期是DEF_CHK_MIN_TIME_MS，所以这里应该是0
    EXPECT_EQ(expired.size(), 0);
    EXPECT_EQ(timer_mgr.queue_size(), 2);
    // std::this_thread::sleep_for(seconds(10));
    // expired.clear()
    // timer_mgr.handle_expired_timers(expired);
    // EXPECT_EQ(expired.size(), 2);
    // EXPECT_EQ(timer_mgr.queue_size(), 0);

    // 用1秒检查一次的来测试
    TimerManager<MilliSeconds> timer_mgr2(MilliSeconds(1000));
    TimerNode timer_node3{1, SteadyClock::now() + seconds(1)};
    timer_mgr2.add_timer(timer_node3);
    timer_mgr2.add_timer(11, SteadyClock::now() + seconds(2));
    timer_mgr2.add_timer(12, SteadyClock::now() + seconds(3));
    timer_mgr2.add_timer(13);
    expired.clear();
    timer_mgr2.handle_expired_timers(expired);
    EXPECT_EQ(expired.size(), 0);
    EXPECT_EQ(timer_mgr2.queue_size(), 4);
    std::this_thread::sleep_for(seconds(3));
    expired.clear();
    timer_mgr2.handle_expired_timers(expired);
    EXPECT_EQ(expired.size(), 3);
    EXPECT_EQ(timer_mgr2.queue_size(), 1);
}
