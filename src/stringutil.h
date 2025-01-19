#ifndef STRING_UTIL_H_
#define STRING_UTIL_H_

#include <vector>
#include <string>
#include <unordered_map>

#include <errno.h>


class StringUtil
{
public:
    inline static
    std::vector<std::string> str_split(
        const std::string &str,
        const std::string &delim);

    /**
     * @brief 获取以delim分割的第一个token，不含delim，
     *        只有delim或者delim左边没有字符的时候返回查找成功，但是是空字符串
     * @param dst 保存token的字符串
     * @param src 待分割的字符串
     * @param delim 分割符
     * @param start_pos 分割起始位置
     * @return 是否成功获取到token
     */
    inline static
    bool str_get_first_token(
        std::string &dst,
        const std::string &src,
        const std::string &delim,
        std::size_t start_pos = 0);

    /**
     * @brief 获取以delim分割的key-val对，
     *        如果key为空会返回查找失败，val为空会正常返回，
     *        没找到分隔符返回失败
     * @param dst 保存key-val对的map
     * @param src 待分割的字符串
     * @param delim 分割符
     * @param start_pos 分割起始位置
     * @return 是否成功获取到key-val对
     */
    inline static
    bool str_get_key_val(
        std::unordered_map<std::string, std::string> &dst,
        const std::string &src,
        const std::string &delim,
        std::size_t start_pos = 0
    );

    // constexpr 默认是inline类型的语句
    // 不同的是如果使用外部定义，定义时也需要加上constexpr修饰符
    static constexpr
    bool ch_str_is_equal(const char *str1, const char *str2);

    /**
     * @brief 字符串转整形
     * @param ret 保存转换结果
     * @param str 待转换的字符串
     * @param fn 转换函数，支持: strtol、strtoul、strtoll、strtoull
     * @return 是否转换成功
     */
    template<typename T, typename F>
    static bool str_to_inum(T &ret, const std::string &str, F fn);
};

std::vector<std::string> StringUtil::str_split(
    const std::string &str,
    const std::string &delim)
{
    std::vector<std::string> ret;
    std::size_t start_pos = 0;

    while (1) {
        std::size_t pos = str.find(delim, start_pos);
        if (pos == std::string::npos) {
            ret.push_back(str.substr(start_pos));
            break;
        }
        ret.push_back(str.substr(start_pos, pos - start_pos));
        start_pos = pos + delim.size();
    }

    return ret;
}

bool StringUtil::str_get_first_token(
    std::string &dst,
    const std::string &src,
    const std::string &delim,
    std::size_t start_pos)
{
    std::size_t pos = src.find(delim, start_pos);
    if (pos == std::string::npos) {
        return false;
    } else {
        dst = src.substr(start_pos, pos - start_pos);
        return true;
    }
}

bool StringUtil::str_get_key_val(
    std::unordered_map<std::string, std::string> &dst,
    const std::string &src,
    const std::string &delim,
    std::size_t start_pos)
{
    std::size_t pos = src.find(delim, start_pos);
    if (pos == std::string::npos) {
        return false;
    } else {
        std::string key = src.substr(start_pos, pos - start_pos);
        std::string val = src.substr(pos + delim.size());
        if (key.empty()) { return false; }
        dst.emplace(key, val);
        return true;
    }
}

constexpr bool StringUtil::ch_str_is_equal(const char *str1, const char *str2)
{
    while (*str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2) {
            return false;
        }
        ++str1;
        ++str2;
    }

    if (*str1 == '\0' && *str2 == '\0') {
        return true;
    } else {
        return false;
    }
}

template<typename T, typename F>
bool StringUtil::str_to_inum(T &ret, const std::string &str, F fn)
{
    char *end_ptr = nullptr;

    ret = fn(str.c_str(), &end_ptr, 10);
    if (*end_ptr != '\0') {
        return false;
    }
    if (errno == ERANGE) {
        return false;
    }

    return true;
}
#endif //STRING_UTIL_H_
