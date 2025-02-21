#ifndef SRC_DEBUG_HELPER_UTIL_H_
#define SRC_DEBUG_HELPER_UTIL_H_

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

#include <stdint.h> 
#include <sys/epoll.h>


/**
 * @brief 将string转换为hex字符串
 * @param s 要转换的字符串
 * @param s_size 要转换的字符串长度，这里需要传递长度是因为，将s认为是二进制数据
 * @param num_per_line 每行显示多少个hex
 */
inline static std::vector<std::string> str_to_hex(const std::string& s, size_t s_size, int num_per_line = 8)
{
    std::vector<std::string> hex_str;
    if (s_size <= 0) {
        return hex_str;
    }

    if (num_per_line <= 0) {
        num_per_line = 8;
    }

    std::stringstream ss;
    for (size_t i = 0; i < s_size; ++i)
    {
        if (i != 0 && i % num_per_line == 0) {
            hex_str.emplace_back(ss.str());
            ss.clear();
            ss.str("");
        }

        ss << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)s[i] << " ";
    }

    if (!ss.str().empty()) {
        hex_str.emplace_back(ss.str());
    }

    return hex_str;
}

inline static std::string events_to_str(uint32_t events)
{
    std::string s;

    if (events & EPOLLIN) {
        s += "EPOLLIN ";
    }
    if (events & EPOLLOUT) {
        s += "EPOLLOUT ";
    }
    if (events & EPOLLRDHUP) {
        s += "EPOLLRDHUP ";
    }
    if (events & EPOLLPRI) {
        s += "EPOLLPRI ";
    }
    if (events & EPOLLERR) {
        s += "EPOLLERR ";
    }
    if (events & EPOLLHUP) {
        s += "EPOLLHUP ";
    }
    return s;
}

inline static std::string buffer_data_to_str(std::string buffer, size_t data_bytes)
{
    return {buffer, 0, data_bytes};
}

/**
 * @brief 简易计时器
 *        使用方法：
 *        {
 *            TimeCount tc("desc", times);
 *            ...
 *        }
 *        花括号中创建计时器，同时执行需要计时的代码
 *        离开花括号时，自动打印执行时间
 */
class TimeCount
{
public:
    TimeCount(const std::string& desc, long long unsigned times)
        : desc_(desc)
        , times_(times)
        , start_(std::chrono::high_resolution_clock::now()){}

    ~TimeCount()
    {
        end_ = std::chrono::high_resolution_clock::now();
        std::cout << desc_ << " " << times_ << " times cost: "
                  << std::chrono::duration_cast<std::chrono::seconds>(end_ - start_).count()
                  << "s" << " | "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end_ - start_).count()
                  << "ms" << " | "
                  << std::chrono::duration_cast<std::chrono::microseconds>(end_ - start_).count()
                  << "mis" << " | "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(end_ - start_).count()
                  << "ns" << std::endl;
    }

private:
    std::string desc_;
    long long unsigned times_;
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point end_;
};

#endif // SRC_DEBUG_HELPER_UTIL_H_
