#include <gtest/gtest.h>
#include "httpdata.h"  // 导入待测函数所在的头文件

TEST(HttpRequestTest, Parse) {
    /**
     * @brief 不含参数的GET请求测试
     */
    HttpRequest httpReq1;
    std::string data1 = "GET /index.html HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    uint32_t parsedBytes1 = httpReq1.parse(data1, 0);
    EXPECT_EQ(parsedBytes1, data1.size());

    /**
     * @brief 含参数的GET请求测试
     *        1. 一个参数
     *        2. 多个参数
     */
    std::string val;
    bool got = false;

    // 1
    HttpRequest httpReq2;
    std::string data2 = "GET /search?q=test HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    uint32_t parsedBytes2 = httpReq2.parse(data2, 0);
    EXPECT_EQ(parsedBytes2, data2.size());
    got = httpReq2.get_param(val, "q");
    EXPECT_TRUE(got);
    EXPECT_EQ(val, "test");
    
    // 2
    HttpRequest httpReq3;
    std::string data3 = "GET /search?q=test&page=2 HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    uint32_t parsedBytes3 = httpReq3.parse(data3, 0);
    EXPECT_EQ(parsedBytes3, data3.size());
    got = httpReq3.get_param(val, "q");
    EXPECT_TRUE(got);
    EXPECT_EQ(val, "test");
    got = httpReq3.get_param(val, "page");
    EXPECT_TRUE(got);
    EXPECT_EQ(val, "2");

    /**
     * @brief 含有多余数据的情况
     */    
    HttpRequest httpReq4;
    std::string data4 = "GET /search?q=test&page=2 HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection: close\r\n"
                        "\r\n";
    std::string data5 = "extra data";
    std::string data6 = data4 + data5;
    uint32_t parsedBytes4 = httpReq4.parse(data6, 0);
    EXPECT_EQ(parsedBytes4, data4.size());

    /**
     * @brief 不完整的请求数据测试
     *        1. 简单的不完整请求行
     *        2. 复杂的不完整请求行
     *        3. 不完整的请求头
     */
    // 1
    HttpRequest httpReq5;
    std::string data7 = "GET /search?q=test&page=2 HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection: close\r\n";
    uint32_t parsedBytes5 = httpReq5.parse(data7, 0);
    EXPECT_FALSE(httpReq5.parse_complete());
    data7 += "\r\n";
    uint32_t parsedBytes6 = httpReq5.parse(data7, parsedBytes5);
    EXPECT_TRUE(httpReq5.parse_complete());
    EXPECT_EQ(parsedBytes5 + parsedBytes6, data7.size());
    
    // 2
    HttpRequest httpReq6;
    std::string data8 = "GET /search?q=test&page=2 HTTP";
    uint32_t parsedBytes7 = httpReq6.parse(data8, 0);
    EXPECT_FALSE(httpReq6.parse_complete());
    data8 += "/1.1\r\n"
             "Host: www.example.com\r\n"
             "Connection: close\r\n"
             "\r\n";
    uint32_t parsedBytes8 = httpReq6.parse(data8, parsedBytes7);
    EXPECT_TRUE(httpReq6.parse_complete());
    EXPECT_EQ(parsedBytes7 + parsedBytes8, data8.size());

    // 3
    HttpRequest httpReq7;
    std::string data9 = "GET /search?q=test&page=2 HTTP/1.1\r\n"
                        "Host: www.example.com\r\n"
                        "Connection:";
    uint32_t parsedBytes9 = httpReq7.parse(data9, 0);
    EXPECT_FALSE(httpReq7.parse_complete());
    data9 += " close\r\n";
    uint32_t parsedBytes10 = httpReq7.parse(data9, parsedBytes9);
    EXPECT_FALSE(httpReq7.parse_complete());
    data9 += "\r\n";
    uint32_t parsedBytes11 = httpReq7.parse(data9, parsedBytes9 + parsedBytes10);
    EXPECT_TRUE(httpReq7.parse_complete());
    EXPECT_EQ(parsedBytes9 + parsedBytes10 + parsedBytes11, data9.size());

    /**
     * @brief 非法请求测试
     *        1. 错误的请求行
     *        2. 错误的请求头
     */
    // 1
    HttpRequest httpReq8;
    std::string data10 = "GET/search?q=test&page=2 HTTP/1.1\r\n";
    uint32_t parsedBytes12 = httpReq8.parse(data10, 0);
    EXPECT_TRUE(httpReq8.parse_complete());
    EXPECT_TRUE(httpReq8.is_bad_req());

    // 2
    HttpRequest httpReq9;
    std::string data11 = "GET /search?q=test&page=2 HTTP/1.1\r\n"
                         "Hostwww.example.com\r\n"
                         "Connection: close\r\n"
                         "\r\n";
    uint32_t parsedBytes13 = httpReq9.parse(data11, 0);
    EXPECT_TRUE(httpReq9.parse_complete());
    EXPECT_TRUE(httpReq9.is_bad_req());
    
    /**
     * @brief 测试POST请求
     *        1. 不含body
     *        2. 含body
     *        3. body字段正好不够
     *        4. body字段有数据读取，但还不够
     */
    // 1
    HttpRequest httpReq10;
    std::string data12 = "POST /submit HTTP/1.1\r\n"
                         "Host: www.example.com\r\n"
                         "\r\n";
    uint32_t parsedBytes14 = httpReq10.parse(data12, 0);
    EXPECT_TRUE(httpReq10.parse_complete());
    EXPECT_FALSE(httpReq10.is_bad_req());
    EXPECT_EQ(httpReq10.get_method(), HttpMethod::POST);

    // 2
    HttpRequest httpReq11;
    std::string data13 = "POST /submit HTTP/1.1\r\n"
                         "Host: www.example.com\r\n"
                         "Content-Type: application/x-www-form-urlencoded\r\n"
                         "Content-Length: 27\r\n"
                         "\r\n"
                         "field1=value1&field2=value2";
    uint32_t parsedBytes15 = httpReq11.parse(data13, 0);
    EXPECT_TRUE(httpReq11.parse_complete());
    EXPECT_FALSE(httpReq11.is_bad_req());
    const std::string& req_body = httpReq11.get_body();
    EXPECT_EQ(req_body, "field1=value1&field2=value2");

    // 3
    HttpRequest httpReq12;
    std::string data14 = "POST /submit HTTP/1.1\r\n"
                         "Host: www.example.com\r\n"
                         "Content-Type: application/x-www-form-urlencoded\r\n"
                         "Content-Length: 27\r\n"
                         "\r\n";
    uint32_t parsedBytes16 = httpReq12.parse(data14, 0);
    EXPECT_FALSE(httpReq12.parse_complete());
    data14 += "field1=value1&field2=value2";
    uint32_t parsedBytes17 = httpReq12.parse(data14, parsedBytes16);
    EXPECT_TRUE(httpReq12.parse_complete());
    EXPECT_EQ(parsedBytes16 + parsedBytes17, data14.size());

    // 4
    HttpRequest httpReq13;
    std::string data15 = "POST /submit HTTP/1.1\r\n"
                         "Host: www.example.com\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 30\r\n"
                         "\r\n"
                         "{\"req\":{\"name\":\"wu\",\"";
    uint32_t parsedBytes18 = httpReq13.parse(data15, 0);
    EXPECT_FALSE(httpReq13.parse_complete());
    data15 += "age\":18}}";
    uint32_t parsedBytes19 = httpReq13.parse(data15, parsedBytes18);
    EXPECT_TRUE(httpReq13.parse_complete());
    EXPECT_EQ(parsedBytes18 + parsedBytes19, data15.size());
    const std::string& req_body1 = httpReq13.get_body();
    EXPECT_EQ(req_body1, "{\"req\":{\"name\":\"wu\",\"age\":18}}");

    /**
     * @brief 分段数据测试
     */
    HttpRequest httpReq14;
    std::string data16 = "GET /index.html HTTP/1.1\r\n";
    uint32_t parsedBytes20 = httpReq14.parse(data16, 0);
    EXPECT_EQ(parsedBytes20, data16.size());
    std::string data17 = "Host: www.example.com\r\n";
    uint32_t parsedBytes21 = httpReq14.parse(data17, 0);
    EXPECT_EQ(parsedBytes21, data17.size());
    std::string data18 = "Connection: close\r\n";
    uint32_t parsedBytes22 = httpReq14.parse(data18, 0);
    EXPECT_EQ(parsedBytes22, data18.size());
    std::string data19 = "\r\n";
    uint32_t parsedBytes23 = httpReq14.parse(data19, 0);
    EXPECT_EQ(parsedBytes23, data19.size());
    EXPECT_TRUE(httpReq14.parse_complete());
    EXPECT_FALSE(httpReq14.is_bad_req());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
