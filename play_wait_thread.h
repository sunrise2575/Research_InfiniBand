#ifndef PLAY_WAIT_THREAD
#define PLAY_WAIT_THREAD

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class play_wait_thread {
private:
	std::thread thread;
	std::mutex mutex;
	std::condition_variable cv;

	std::atomic_bool ready;
	std::atomic_bool terminate;
	std::atomic_bool processed;

	std::function<void(void)> function;

	void routine();

public:
	play_wait_thread(std::function<void(void)> function);
	~play_wait_thread();

	void play();
	void wait();
};

#endif
