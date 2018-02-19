#include "play_n_stop_thread.h"
// private

void play_n_stop_thread::routine() {
    while(true) {
        std::unique_lock<std::mutex> lock(this->mtx);
        this->cv.wait(lock, [&]{ return this->ready == true; });
        if (this->terminate) {
            break;
        }

        this->function();

        this->processed = true;
        this->ready = false;
        lock.unlock();
        this->cv.notify_one();
    }
}

// public

play_n_stop_thread::play_n_stop_thread(std::function<void(void)> function) {
    this->ready = false;
    this->processed = false;
    this->terminate = false;
    this->function = function;
    this->thread = std::thread([&]{ this->routine(); });
}

play_n_stop_thread::~play_n_stop_thread() {
    this->terminate = true;
    this->ready = true;
    this->cv.notify_one();
    this->thread.join();
}

void play_n_stop_thread::play() {
    this->processed = false;
    std::lock_guard<std::mutex> lock(this->mtx);
    this->ready = true;
    this->cv.notify_one();
}

void play_n_stop_thread::wait() {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->cv.wait(lock, [&]{ return this->processed == true; });
}
