#include <iostream>

#include <gtest/gtest.h>
#include "stringutil.h"


TEST(StringuitlTest, FuncTest) {
    std::string str = "close";
    StringUtil::str_to_lower(str);
    EXPECT_EQ(str, "close");

    str = "CLOSE";
    StringUtil::str_to_lower(str);
    EXPECT_EQ(str, "close");
    
    str = "Close";
    StringUtil::str_to_lower(str);
    EXPECT_EQ(str, "close");

    str = "";
    StringUtil::str_to_lower(str);
    EXPECT_EQ(str, ""); 
}
