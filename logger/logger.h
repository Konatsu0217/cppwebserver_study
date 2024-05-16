#ifndef LOGGER_H
#define LOGGER_H
#include "blockQueue.h"
#include <stdio.h>
#include <cstring>
#include <string>
#include <fstream>
#include <pthread.h>

using namespace std;

class log {
private:
	log() = default;
	~log(){
		if (m_buf != nullptr)
			delete[] m_buf;

		if (m_fp != NULL) {
			fclose(m_fp);
		}
	}
	log(const log&) = delete;
	log operator= (const log&) = delete;
public:
	static log* getInstance() {
		static log inst;
		return &inst;
	}

	static void is_uselog(int uselog) {
		log::getInstance()->m_close_log = uselog;
	}

	static void* log_thread_work(void*) {
		log::getInstance()->cb_func_log();
	}

	bool init(const char* , int, int, int, int);

	void write_log(int level, const char* format, ...);

	void flush(void);

private:
	char dir_name[128];
	char log_name[128];
	int m_split_lines;
	int m_log_buf_size;
	long long m_count;
	int m_today;
	FILE* m_fp;		//fopen should be close;
	char* m_buf;	// new
	blockQueue<string> *m_log_queue;	// singleton
	bool is_async;
	locker m_mutex;
	int m_close_log;

	void* cb_func_log() {
		string single_log;
		while (m_log_queue->fpop(single_log)) {
			log::m_mutex.lock();
			fputs(single_log.c_str(), m_fp);
			log::m_mutex.unlock();
		}
	}
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {log::getInstance()->write_log(0, format, ##__VA_ARGS__); log::getInstance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {log::getInstance()->write_log(1, format, ##__VA_ARGS__); log::getInstance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {log::getInstance()->write_log(2, format, ##__VA_ARGS__); log::getInstance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {log::getInstance()->write_log(3, format, ##__VA_ARGS__); log::getInstance()->flush();}

#endif