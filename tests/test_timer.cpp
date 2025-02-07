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
    TimerManager timer_mgr;
    timer_mgr.add_timer(timer_node1);
    timer_mgr.add_timer(timer_node2);
    timer_mgr.add_timer(12, now + seconds(10));
    timer_mgr.add_timer(13, now + seconds(7));
    TimerNode node3 = timer_mgr.get_top();
    EXPECT_EQ(node3.id, timer_node1.id);
    timer_mgr.add_timer(1, now);
    TimerNode node4 = timer_mgr.get_top();
    EXPECT_EQ(node4.id, 1);
    EXPECT_EQ(timer_mgr.queue_size(), 5);

    /**
     * @brief 测试移除timer
     */
    // 移除timer
    timer_mgr.rm_timer(1);
    const TimerNode& top = timer_mgr.get_top();
    EXPECT_EQ(top.id, 10);
    EXPECT_EQ(timer_mgr.queue_size(), 4);
    timer_mgr.rm_timer(13);
    EXPECT_EQ(timer_mgr.queue_size(), 3);
    // 删除不存在的timer，应该不影响程序工作
    timer_mgr.rm_timer(1000);
    EXPECT_EQ(timer_mgr.queue_size(), 3);

    /**
     * @brief 测试过期timer
     */
    vector<int> expired;
    timer_mgr.handle_expired_timers(expired);
    EXPECT_EQ(expired.size(), 0);
    std::this_thread::sleep_for(seconds(1));
    expired.clear();
    timer_mgr.handle_expired_timers(expired);
    EXPECT_EQ(expired.size(), 1);
    EXPECT_EQ(timer_mgr.queue_size(), 2);
    std::this_thread::sleep_for(seconds(1));
    expired.clear();
    timer_mgr.handle_expired_timers(expired);
    EXPECT_EQ(expired.size(), 1);
    EXPECT_EQ(timer_mgr.queue_size(), 1);
}
