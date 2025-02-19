#include <gtest/gtest.h>
#include "filepathutil.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
using namespace std::chrono;


TEST(FilePathUtilTest, DirFix) {
    std::string path = "/";
    dir_fix_last_slash(path);
    EXPECT_EQ(path, "/");

    path = "/abc";
    dir_fix_last_slash(path);
    EXPECT_EQ(path, "/abc/");
}

TEST(FilePathUtilTest, GetExtension) {
    std::string path = "/";
    std::string extension = "";

    extension = get_file_extension(path);
    EXPECT_EQ(extension, "");

    path = "/abc";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, "");

    path = "/ab.c";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, ".c");
    
    path = "/ab.cd";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, ".cd");

    path = "/ab.txt";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, ".txt");

    path = "/ab.tar.gz";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, ".gz");

    path = "abd./ab";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, "./ab");

    path = "/for/bar.c/";
    extension = get_file_extension(path);
    EXPECT_EQ(extension, ".c/");
}
