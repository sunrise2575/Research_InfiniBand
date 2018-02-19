#ifndef THREAD_SAFE_STRUCT_HEADER
#define THREAD_SAFE_STRUCT_HEADER

#include <queue>
#include <list>
#include <map>

#include <mutex>
#include <condition_variable>

namespace thread_safe {
    template <typename T> class queue {
    private:
        std::queue<T> q;
    protected:
        std::mutex mtx;
        std::condition_variable cv;
    public:
        T pop();
        void pop(T& item);
        void push(const T& item);
        void push(T&& item);
        size_t size();
    };

    template <typename T> class list : public queue<T> {
    private:
        std::list<T> l;
    public:

    };

    template <typename KEY, typename VALUE> class map {
    private:
        std::map<KEY, VALUE> m;
        std::mutex mtx;
        std::condition_variable cv;
    public:
        auto begin() -> decltype(this->m.begin());
        auto end() -> decltype(this->m.end());
        size_t size();
        VALUE& operator[](const KEY& key);
        void erase();
        int find();
    };
}

#endif
