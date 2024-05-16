#pragma once
#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem {
private:
	sem_t m_sem;
public:
	sem() {
		if (sem_init(&m_sem, 0, 0) != 0) {
			throw std::exception();
		}
	}
	~sem()
	{
		sem_destroy(&m_sem);
	}
	// wait_P操作，尝试占用1份资源 or 阻塞等待
	bool wait() {
		return sem_wait(&m_sem) == 0;
	}
	// V操作，信号量+1
	bool post() {
		return sem_post(&m_sem) == 0;
	}
};

class locker {
private:
	pthread_mutex_t m_mtx;
public:
	locker() {
		if (pthread_mutex_init(&m_mtx, NULL) != 0) {
			throw std::exception();
		}
	}
	~locker() {
		pthread_mutex_destroy(&m_mtx);
	}
	// 获取互斥锁
	bool lock() {
		return pthread_mutex_lock(&m_mtx) == 0;
	}
	// 释放互斥锁
	bool unlock() {
		return pthread_mutex_unlock(&m_mtx) == 0;
	}
	pthread_mutex_t* get() {
		return &m_mtx;
	}
};

class cond {
private:
	pthread_mutex_t m_mtx;
	pthread_cond_t m_cond;
public:
	cond() {
		if (pthread_mutex_init(&m_mtx, NULL) != 0)	throw std::exception();
		if (pthread_cond_init(&m_cond, NULL) != 0) throw std::exception();
	}
	~cond() {
		pthread_mutex_destroy(&m_mtx);
		pthread_cond_destroy(&m_cond);
	}
	bool wait() {
		int ret = 0;
		pthread_mutex_lock(&m_mtx);
		ret = pthread_cond_wait(&m_cond, &m_mtx);
		pthread_mutex_unlock(&m_mtx);
		return ret == 0;
	}
	bool wait(pthread_mutex_t *m_mtx) {
		int ret = 0;
		//pthread_mutex_lock(&m_mtx);
		ret = pthread_cond_wait(&m_cond, m_mtx);
		//pthread_mutex_unlock(&m_mtx);
		return ret == 0;
	}
	bool signal() {
		return pthread_cond_signal(&m_cond) == 0;
	}
};
