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

TEST(FilePathUtilTest, CombineTwoPath) {
    std::string path1 = "/aaabbb";
    std::string path2 = "/cccddd";
    std::string newpath = "";

    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "/aaabbb/cccddd");

    path1 = "/aaabbb";
    path2 = "cccddd";
    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "/aaabbb/cccddd");

    path1 = "/aaabbb/";
    path2 = "/cccddd";
    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "/aaabbb/cccddd");

    path1 = "/aaabbb/";
    path2 = "cccddd";
    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "/aaabbb/cccddd");    

    path1 = "";
    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "cccddd");

    path2 = "";
    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "");

    path1 = "/aaabbb";
    newpath = combine_two_path(path1, path2);
    EXPECT_EQ(newpath, "/aaabbb");
}
