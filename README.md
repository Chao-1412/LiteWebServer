# README

### 介绍

一个简单的C++ WebServer，学习使用，单服务，多线程，万级并发

### 编译环境及说明

C++版本：>=C++11(C++11和C++14测试通过)

在项目根目录下，
```bash
mkdir build
cd build
cmake ..
make
```

### 测试环境及性能

测试环境：

虚拟机：Ubuntu 22.04.4 LTS

处理器：13th Gen Intel® Core™ i5-13500HX（虚拟机8核）

内存：8GB

服务程序，测试程序在同一台虚拟机

1. WebBench测试

### 使用方法

编译后build目录下执行：`./litewebserver`

### 感谢

TinyWebServer：https://github.com/qinguoyi/TinyWebServer

WebServer：https://github.com/linyacool/WebServer

### 三方库

spdlog（日志）: https://github.com/gabime/spdlog

ThreadPool（线程池）：https://github.com/progschj/ThreadPool
