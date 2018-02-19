#include "thread_safe_struct.h"

template <typename KEY, typename VALUE>
auto thread_safe::map<KEY, VALUE>::begin() -> decltype(this->m.begin()) {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->m.begin();
}

template <typename KEY, typename VALUE>
auto thread_safe::map<KEY, VALUE>::end() -> decltype(this->m.end()) {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->m.end();
}

template <typename KEY, typename VALUE>
size_t thread_safe::map<KEY, VALUE>::size() {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->m.size();
}

template <typename KEY, typename VALUE>
VALUE& thread_safe::map<KEY, VALUE>::operator[](const KEY& key) {
    std::unique_lock<std::mutex> lock(this->mtx);
    return m[key];
}

template <typename KEY, typename VALUE>
void thread_safe::map<KEY, VALUE>::erase(const KEY& key) {
    std::unique_lock<std::mutex> lock(this->mtx);
    this->m.erase();
}

template <typename KEY, typename VALUE>
size_t thread_safe::map<KEY, VALUE>::find(const KEY& key) {
    std::unique_lock<std::mutex> lock(this->mtx);
    return this->q.find();
}
