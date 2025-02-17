# THINKING

1. **怎么判断单次连接的数据是否接收完全？**
   
   A1. 每个连接维护一个read-buffer，当收到数据时，将数据写入read-buffer，然后解析read-buffer的数据，判断是否接收完全，不完全就继续接收，直到接收完全。

2. **如果系统占用满了，没有资源再分配数据了，应该怎样优雅的处理接收的连接？**

   A1. 预先要先限制最大连接数，超过最大连接数的连接就直接关闭

3. **测试环境该如何安排？**
   
   A1. 本次测试采用WebBench进行测试，在同一台虚拟机中进行，测试比较简单，只测试了非keep-alive的情况，后面需要采用像jmeter这样的工具再进行更深入的测试

4. **ngnix 和 apache 与自己写的webserver有什么不同点和共同点？libevent 和 muduo又和ngnix，apache有什么不同点和共同点呢？**

5. **如何调优？CPU，内存，内核读写队列，代码的读写队列？**

   A1. 通过perf、google-perftools、Intel VTune Profiler分析代码瓶颈。不过这个只能分析CPU热点，具体的有可能出现CPU占用很低的情况，这种情况一般是线程加锁导致频繁线程切换让出CPU，使CPU闲置导致的，这种情况就要分析是否加锁过于频繁，是否可以优化减少加锁

   A2. 原先采用single-reactor的模式，将任务分发到不同线程时，需要加锁，导致CPU切换的开销很大，参考了其他人的实现，在multi-reactor的分支中，每个线程维护一个epoll，这样分配任务的时候不需要加锁，性能提高了将近一倍，详见分支：multi-reactor

   A3. 在multi-reactor的分支下，还是需要加锁添加，原代码在添加时timermanager需要加锁，导致很多使用timermanager的地方都因为锁而影响了性能，后续实现了一个待添加的fd队列，并通过socketpair发送通知给eventloop线程，这样操作timermanager就变为单线程操作，只需要在添加fd队列时加锁，提高了20%性能，详见提交：f2ae2c19

6. **如何实现类似flask的框架？如何将C++web程序挂到apache？**

7. **多个进程如何共享文件描述符，实现多进程的webserver？**
