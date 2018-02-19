#include "thread_safe_struct.h"

template <typename T> T thread_safe::queue<T>::pop() {
    std::unique_lock<std::mutex> lock(this->mtx);
    while (this->q.empty()) {
        this->cv.wait(lock);
    }
    auto item = this->q.front();
    this->q.pop();
    return item;
}

template <typename T> void thread_safe::queue<T>::pop(T& item) {
    std::unique_lock<std::mutex> lock(this->mtx);
    while (this->q.empty()) {
        this->cv.wait(lock);
    }
    item = this->q.front();
    this->q.pop();
}

template <typename T> void thread_safe::queue<T>::push(const T& item) {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->q.push(item);
    lock.unlock();
    this->cv.notify_one();
}

template <typename T> void thread_safe::queue<T>::push(T&& item) {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->q.push(std::move(item));
    lock.unlock();
    this->cv.notify_one();
}

template <typename T> size_t thread_safe::queue<T>::size() {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->q.size();
}
