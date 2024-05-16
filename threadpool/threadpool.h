#ifndef THREADPOOL
#define THREADPOOL

#include <cstdio>
#include <pthread.h>
#include <exception>
#include <list>
#include "../locker/locker.h"

template <typename T>
class threadpool {
private:
	// T 请求类型
	int m_thread_number;
	int m_max_requests;
	pthread_t* m_threads; // 线程标识符数组
	std::list<T*> m_workqueue;
	locker m_queuelocker;
	sem m_queststat;
	bool m_stop;

private:
	static void* worker(void* arg);// 工作线程运行的函数数组 void* arg
	void run();

public:
	threadpool(int thread_number = 8, int max_request = 10000);
	~threadpool();
	// 加入任务
	bool append(T* request);
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_request) :
	m_thread_number(thread_number), m_max_requests(max_request), m_stop(false),
	m_threads(nullptr)
{
	if (thread_number <= 0 || max_request <= 0)
		throw std::exception();
	// 初始化线程**标识符**&数组
	m_threads = new pthread_t[m_thread_number];
	if (!m_threads)
		throw std::exception();
	// 创建线程，全部设置为脱离线程
	// 脱离线程在运行结束后会自行释放资源，便于管理
	for (int i = 0; i < m_thread_number; ++i) {
		printf("creating thread %d\n", i);
		// worker 是 static的成员函数，意思就创建的线程全部执行worker去
		// m_threads + i 是 pthread_t，标识一个线程，真正的线程还没有创建
		if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
			delete[] m_threads;
			throw std::exception();
		}
		if (pthread_detach(m_threads[i])) {
			delete[] m_threads;
			throw std::exception();
		}
	}
}

template<typename T>
threadpool<T>::~threadpool() {
	delete[] m_threads;
	m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request) {
	m_queuelocker.lock();
	if (m_workqueue.size() > m_max_requests) {
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queststat.post();
	return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}

template<typename T>
void threadpool<T>::run() {
	while (!m_stop) {
		m_queststat.wait();
		m_queuelocker.lock();
		if (m_workqueue.empty()) {
			m_queuelocker.unlock();
			continue;
		}
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();
		if (!request) {
			continue;
		}
		// 在T类中实现 process()
		request->process();
	}
}
#endif // !THREADPOOL