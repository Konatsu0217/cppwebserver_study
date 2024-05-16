#pragma once
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include "../locker/locker.h"
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>

using namespace std;

// 增、删、空？/满？、
template <typename T>
class blockQueue {
private:
	locker m_mutex;
	cond m_cond;

	T* m_array;	// new[]
	int m_size;
	int m_max_size;
	int m_front;
	int m_back;

	blockQueue(int max_size = 1000);
	~blockQueue();
	blockQueue(const blockQueue&) = delete;
	blockQueue& operator =(const blockQueue&) = delete;
public:
	static blockQueue& getInstance(int size) {
		static blockQueue inst(size);
		return inst;
	}
	bool fpop(T& item) {
		m_mutex.lock();
		while (m_size <= 0) {
			if (!m_cond.wait(m_mutex.get())) {
				m_mutex.unlock();
				return false;
			}
		}
		item = m_array[m_front];
		m_front = (m_front + 1) % m_max_size;
		--m_size;
		m_mutex.unlock();
		return true;
	}

	void clear();

	bool is_full();

	bool is_empty();

	bool front(T& value);

	int size();

	bool push(T& value);

	bool pop();
};

template<typename T>
inline blockQueue<T>::blockQueue(int max_size) {
	assert(max_size > 0);
	m_max_size = max_size;
	m_front = -1;
	m_back = -1;
	m_size = 0;
	m_array = new T[max_size];
}

template<typename T>
inline blockQueue<T>::~blockQueue() {
	m_mutex.lock();
	if(m_array != nullptr)
		delete[] m_array;
	m_mutex.unlock();
}

template<typename T>
inline void blockQueue<T>::clear() {
	m_mutex.lock();
	m_front = -1;
	m_back = -1;
	m_size = 0;
	m_mutex.unlock();
}

template<typename T>
inline bool blockQueue<T>::is_full() {
	m_mutex.lock();
	if (m_size >= m_max_size)
	{
		m_mutex.unlock();
		return true;
	}
	m_mutex.unlock();
	return false;
}

template<typename T>
inline bool blockQueue<T>::is_empty() {
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return true;
	}
	m_mutex.unlock();
	return false;
}

template<typename T>
inline bool blockQueue<T>::front(T & value) {
	m_mutex.lock();
	if (m_size == 0) {
		m_mutex.unlock();
		return false;
	}
	value = m_array[m_front];
	m_mutex.unlock();
	return true;
}

template<typename T>
inline int blockQueue<T>::size() {
	int ret = 0;
	m_mutex.lock();
	ret = m_size;
	m_mutex.unlock();
	return ret;
}

template<typename T>
inline bool blockQueue<T>::push(T & value) {
	m_mutex.lock();
	if (m_size >= m_max_size) {
		m_cond.signal();
		m_mutex.unlock();
		return false;
	}
	// 循环数组
	m_back = (m_back + 1) % m_max_size;
	m_array[m_back] = value;
	++m_size;
	m_cond.signal();
	m_mutex.unlock();
	return true;
}

template<typename T>
inline bool blockQueue<T>::pop() {
	m_mutex.lock();
	while(m_size <= 0) {
		if (!m_cond.wait()) {
			m_mutex.unlock();
			return false;
		}
	}
	m_front = (m_front + 1) % m_max_size;
	--m_size;
	m_mutex.unlock();
	return true;
}


#endif // !BLOCKQUEUE_H