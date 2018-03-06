#include "play_wait_thread.h"

play_wait_thread::play_wait_thread(std::function<void(void)> function) {
	this->ready = false;
	this->processed = false;
	this->terminate = false;
	this->function = function;
	this->thread = std::thread([&]{ this->routine(); });
}

play_wait_thread::~play_wait_thread() {
	this->terminate = true;
	this->ready = true;
	this->cv.notify_one();
	this->thread.join();
}

void play_wait_thread::routine() {
	while (true) {
		std::unique_lock<std::mutex> lock(this->mutex);
		this->cv.wait(lock, [&]{ return this->ready == true; });
		if (this->terminate) break;

		this->function();

		this->processed = true;
		this->ready = false;
		lock.unlock();
		this->cv.notify_one();
	}
}

void play_wait_thread::play() {
	this->processed = false;
	std::lock_guard<std::mutex> lock(this->mutex);
	this->ready = true;
	this->cv.notify_one();
}

void play_wait_thread::wait() {
	std::unique_lock<std::mutex> lock(this->mutex);
	this->cv.wait(lock, [&]{ return this->processed == true; });
}
