#ifndef PLAY_N_STOP_THREAD_HEADER
#define PLAY_N_STOP_THREAD_HEADER

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class play_n_stop_thread {
private:
    std::thread thread;

    std::mutex mtx;
    std::condition_variable cv;

    std::atomic<bool> ready;
    std::atomic<bool> terminate;
    std::atomic<bool> processed;

    std::function<void(void)> function;
    void routine();

public:
    play_n_stop_thread(std::function<void(void)> function);
    ~play_n_stop_thread();

    void play();
    void wait();
};

#endif
