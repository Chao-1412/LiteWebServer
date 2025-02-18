# README

### 介绍

一个C++实现的简单，但是性能还不错的web server，学习用，代码结构简单，功能陆续添加中

单服务，多线程，万级并发，支持自定义路由

### 编译环境及说明

**C++版本：>=C++11(C++11和C++14测试通过)**

在项目根目录下:
```bash
mkdir build
cd build
cmake ..
make
```

### 用法

编译后build目录下执行：`./litewebserver`

### 测试环境及性能

测试环境：

虚拟机：Ubuntu 22.04.4 LTS

处理器：13th Gen Intel® Core™ i5-13500HX（虚拟机8核）

内存：8GB

服务程序，测试程序在同一台虚拟机

1. WebBench测试
   
   4线程，10000客户端，禁用所有日志，测试60秒，简单页面测试：

   ```C++
   static const std::string index_body = "<!DOCTYPE html>\r\n"
                                      "<html>\r\n"
                                      "<head><title>Simple Web Server</title></head>\r\n"
                                      "<body><h1>Hello, World! This is a simple web server.</h1></body>\r\n"
                                      "</html>\r\n";
   ```

   ```bash
   ./webbench -t 60 -c 10000 -2 --get http://127.0.0.1:8080/
   ```

   LT+LT模式，约 64416 QPS，ET(accept连接)+LT模式，约 68292 QPS，因为实现中LT和ET均采用了一次性accept的方式，所以实际上差的不是很大

   LT+LT：

   ![LTLT](README.assets/LT+LT.png)

   ET+LT：

   ![ETLT](README.assets/ET+LT.png)

### 感谢

TinyWebServer：https://github.com/qinguoyi/TinyWebServer

WebServer：https://github.com/linyacool/WebServer

### 三方库

spdlog（日志）: https://github.com/gabime/spdlog

ThreadPool（线程池）：https://github.com/progschj/ThreadPool
