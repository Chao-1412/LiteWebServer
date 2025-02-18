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

#endif //SRC_FILEPATHUTIL_H_
