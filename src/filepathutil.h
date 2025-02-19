#ifndef SRC_FILEPATHUTIL_H_
#define SRC_FILEPATHUTIL_H_

#include <string>


inline void dir_fix_last_slash(std::string &dir)
{
    if (dir.back() == '/') {
        return;
    } else {
        dir.push_back('/');
    }
}

/**
 * @brief 获取文件后缀名
 *        简单的实现，只判断第一个找到的'.'
 *        "/ab.cd"，"/ab.tar.gz"，"abd./ab"，"/for/bar.c/"
 *        这样的路径可能返回奇怪的结果
 *        不打算修复，对于现在来说够用
 * @param file_path 文件路径
 * @return 文件后缀名，如果没有文件后缀名，返回空字符串
 */
inline std::string get_file_extension(const std::string &file_path)
{
    // 直接返回临时变量，便于RVO优化
    size_t pos = file_path.rfind('.');
    if (pos != std::string::npos) {
        return file_path.substr(pos);
    }
    return "";
}

#endif //SRC_FILEPATHUTIL_H_
