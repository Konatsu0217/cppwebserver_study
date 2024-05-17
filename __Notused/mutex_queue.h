#pragma once
// Modified by Laix 2024/04/29
#include <mutex>
#include <condition_variable>
#include <queue>

template <typename T>
class threadsafe_queue {
	typedef std::lock_guard<std::mutex> mLock;
	typedef std::unique_lock<std::mutex> uLock;

private:
	mutable std::mutex mtx;
	std::queue<T> m_data;
	std::condition_variable data_cond;
public:
	threadsafe_queue() {};
	void pop() {
		uLock lck(mtx);
		data_cond.wait(lck, [this] {return !m_data.empty(); });
		m_data.pop();
	};
	bool empty() {
		mLock lck(mtx);
		return m_data.empty();
	}
	T front() {
		uLock lck(mtx);
		data_cond.wait(lck, [this] {return !m_data.empty(); });
		return m_data.front();
	};
	void push(const T& newItem) {
		// ÁÙ½çÇø
		{
			mLock lck(mtx);
			m_data.push(newItem);
		}
		// End
		data_cond.notify_one();
	};
};