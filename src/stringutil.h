#ifndef STRING_UTIL_H_
#define STRING_UTIL_H_

#include <vector>
#include <string>

class StringUtil
{
public:
    inline static
    std::vector<std::string> str_split(
        const std::string &str,
        const std::string &delim);

    /**
     * @brief 获取以delim分割的第一个token，不含delim
     *        只有delim的时候返回查找成功，但是是空字符串
     * @param dst 保存token的字符串
     * @param src 待分割的字符串
     * @param delim 分割符
     * @param start_pos 分割起始位置
     * @return 是否成功获取到token
     */
    inline static
    bool StringUtil::str_get_first_token(
        std::string &dst,
        const std::string &src,
        const std::string &delim,
        std::size_t start_pos = 0);

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
    std::size_t start_pos = 0)
{
    std::size_t pos = src.find(delim, start_pos);
    if (pos == std::string::npos) {
        return false;
    } else {
        dst = src.substr(start_pos, pos - start_pos);
        return true;
    }
}

#endif //STRING_UTIL_H_