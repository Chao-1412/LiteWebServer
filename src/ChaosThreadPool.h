#ifndef CHAOS_THREAD_POOL_H_
#define CHAOS_THREAD_POOL_H_
#include <queue>
#include <functional>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <utility>
#include <type_traits>
#include <memory>
#include <stdexcept>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <algorithm>


namespace chaos {

#define THREAD_STOP_ALL -1
#define THREAD_MGR_TIMEOUT_S 10 // 管理线程的工作周期，单位：秒
#define QUEUE_BUSY_NUM 100      // 队列中的任务数量大于该值时，认为线程池处于繁忙状态
#define QUEUE_IDLE_NUM 10       // 队列中的任务数量小于该值时，认为线程池处于空闲状态
#define THREAD_STATUS_TIMES 3   // 线程池处于繁忙的或空闲的状态的次数，超过该次数则认为线程池处于该状态
#define THREAD_CHANGE_SETUP 1   // 动态调整线程池大小时，每次调整的线程数量
#define POOL_MIN_WORKERS 1      // 线程池中最少存在的工作线程数量


class ThreadPool {
private:
    enum class QueueStatus {
        QUEUE_NORMAL = 0,
        QUEUE_BUSY = 1,
        QUEUE_IDLE = 2
    };

public:
    /**
     * @brief 创建并运行线程池，
     *        默认存在一个管理线程，所以：
     *        实际运行的线程数量为 idle_num + 1
     *        实际最大可运行的线程数量为 max_num + 1
     *        
     *        线程池在退出时会等待所有任务完成，
     *        如果有无法退出的任务则会导致卡死
     * 
     *        目前线程的调整策略是：
     *        1. 每THREAD_MGR_TIMEOUT_S秒管理线程检查一次，线程状态
     *        2. 任务队列>=QUEUE_BUSY_NUM 时，记为一次繁忙
     *        3. 任务队列<=QUEUE_IDLE_NUM 时，记为一次空闲
     *        4. 繁忙或空闲状态持续超过THREAD_STATUS_TIMES次，则认为线程池处于该状态
     *        5. 根据状态对线程池的数量进行调整
     * @param idle_num 线程池中默认存在的工作线程数量
     * @param max_num 线程池中最大可存在的工作线程数量
     * @param dynamic 是否动态调整线程池大小
     */
    ThreadPool(size_t idle_num = 5,
               size_t max_num = 10,
               bool dynamic = false);
    ~ThreadPool();

public:
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    void create_workers(size_t n);
    void create_mgr();
    bool make_adjust_complete();
    auto check_queue_status() -> QueueStatus;
    void handle_queue_busy();
    void handle_queue_idle();

private:
    // 最大可运行的线程数量，不包括默认存在的一个管理线程池的线程
    size_t max_num_;
    // 停止指定数量的工作线程线程，随着被停止的工作线程的退出，stop_num_会减少，直至0
    // 当值设为THREAD_STOP_ALL，表示停止所有工作线程，同时该值可能再不会被重置为0
    std::atomic<int> stop_num_;
    bool stop_mgr_;
    std::thread mgr_;
    std::list<std::thread> workers_;
    // 用于记载被退出的线程的id，在被退出的线程安全join并从workers_中移除后，
    // 将会从exited_workers_中移除，该线程ID
    // 正常情况下将在所有停止的线程从workers_中移除后，变为空
    std::unordered_set<std::thread::id> exited_workers_;
    std::queue< std::function<void()> > tasks_;
    std::unordered_map<QueueStatus, unsigned int> queue_status_times_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cond_;
    std::mutex mgr_mutex_;
    std::condition_variable mgr_cond_;
    std::mutex workers_mutex_;
};

inline ThreadPool::ThreadPool(size_t idle_num, 
                              size_t max_num,
                              bool dynamic)
    : max_num_(max_num)
    , stop_num_(0)
    , stop_mgr_(false)
    , queue_status_times_({{QueueStatus::QUEUE_BUSY, 0},
                           {QueueStatus::QUEUE_IDLE, 0}})
{
    if (idle_num > max_num) {
        idle_num = max_num;
    }
    create_workers(idle_num);
    if (dynamic) {
        create_mgr();
    }
}

inline ThreadPool::~ThreadPool()
{
    // 先join管理线程，
    // 防止管理线程在工作线程退出后，再次调整线程池大小
    if (mgr_.joinable()) {
        {
            std::unique_lock<std::mutex> lock(mgr_mutex_);
            stop_mgr_ = true;
        }
        mgr_cond_.notify_one();
        mgr_.join();
    }

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_num_ = THREAD_STOP_ALL;
    }

    queue_cond_.notify_all();   
    for (auto &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto p_task = std::make_shared<std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    auto ret = p_task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_num_ == THREAD_STOP_ALL) {
            throw std::runtime_error("ThreadPool is stopped!");
        }
        tasks_.emplace( [p_task](){ (*p_task)(); } );
    }

    queue_cond_.notify_one();

    return ret;
}

inline void ThreadPool::create_workers(size_t n)
{
    for (unsigned int i = 0; i < n; ++i) {
        workers_.emplace_back(
            [this] () -> void {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        queue_cond_.wait(lock, 
                            [this]() {
                                return stop_num_ != 0
                                       || !tasks_.empty();
                            }
                        );

                        // 允许先退出部分线程
                        if (stop_num_ > 0) {
                            {
                                std::lock_guard<std::mutex> lock(workers_mutex_);
                                exited_workers_.emplace(std::this_thread::get_id());
                            }
                            --stop_num_;
                            return;
                        }

                        // 退出所有线城时，
                        // 目前必须等待所有任务都完成，才允许线程退出，
                        // 因为返回的future可能还在等待结果，
                        // 如果任务没有完成就被销毁，可能会有意想不到的结果
                        if (stop_num_ == THREAD_STOP_ALL
                            && tasks_.empty()) {
                            return;
                        }
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            }
        );
    }
}

inline void ThreadPool::create_mgr()
{
    mgr_ = std::thread(
        [this] () -> void {
            for (;;) {
                std::unique_lock<std::mutex> lock(mgr_mutex_);
                mgr_cond_.wait_for(lock, 
                    std::chrono::seconds(THREAD_MGR_TIMEOUT_S),
                    [this] { return stop_mgr_; });
                if (stop_mgr_) {
                    return;
                }

                if (make_adjust_complete() == false) {
                    continue;
                }

                QueueStatus status = check_queue_status();
                if (status == QueueStatus::QUEUE_BUSY) {
                    handle_queue_busy();
                } else if (status == QueueStatus::QUEUE_IDLE) {
                    handle_queue_idle();
                }
            }
        }
    );
}

inline bool ThreadPool::make_adjust_complete()
{
    if (stop_num_ > 0)  { return false; }

    std::lock_guard<std::mutex> lock(workers_mutex_);
    if (exited_workers_.empty()) {
        return true;
    } else {
        for (auto id_it = exited_workers_.begin();
               id_it != exited_workers_.end(); ) {
            auto found_it = std::find_if(workers_.begin(), workers_.end(),
                [&id_it](const std::thread &worker) {
                    return worker.get_id() == *id_it;
                }
            );
            if(found_it != workers_.end()) {
                found_it->join();
                workers_.erase(found_it);
                id_it = exited_workers_.erase(id_it);
            } else {
                ++id_it;
            }
        }
        return false;
    }
}

inline auto ThreadPool::check_queue_status() -> QueueStatus
{
    if (tasks_.size() > QUEUE_BUSY_NUM) {
        ++queue_status_times_[QueueStatus::QUEUE_BUSY];
        queue_status_times_[QueueStatus::QUEUE_IDLE] = 0;
    } else if (tasks_.size() < QUEUE_IDLE_NUM) {
        ++queue_status_times_[QueueStatus::QUEUE_IDLE];
        queue_status_times_[QueueStatus::QUEUE_BUSY] = 0;
    } else {
        queue_status_times_[QueueStatus::QUEUE_BUSY] = 0;
        queue_status_times_[QueueStatus::QUEUE_IDLE] = 0;
    }

    if (queue_status_times_[QueueStatus::QUEUE_BUSY] >= THREAD_STATUS_TIMES) {
        return QueueStatus::QUEUE_BUSY;
    } else if (queue_status_times_[QueueStatus::QUEUE_IDLE] >= THREAD_STATUS_TIMES) {
        return QueueStatus::QUEUE_IDLE;
    } else {
        return QueueStatus::QUEUE_NORMAL;
    }
}

inline void ThreadPool::handle_queue_busy()
{
    size_t new_workers = THREAD_CHANGE_SETUP;
    
    if (workers_.size() + new_workers > max_num_) {
        new_workers = max_num_ - workers_.size();
    }

    if (new_workers > 0) {
        create_workers(new_workers);
    }
}

inline void ThreadPool::handle_queue_idle()
{
    size_t rm_workers = THREAD_CHANGE_SETUP;

    if (workers_.size() - rm_workers < POOL_MIN_WORKERS) {
        rm_workers = workers_.size() - POOL_MIN_WORKERS;
    }

    if (rm_workers > 0) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_num_ == 0) {
                stop_num_.store(static_cast<int>(rm_workers));
            }
        }

        queue_cond_.notify_all();
    }
}

} // namespace chaos
#endif // CHAOS_THREAD_POOL_H_
