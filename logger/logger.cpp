#include "logger.h"
#include <stdarg.h>


//inline log::~log() {
//	if (m_buf != nullptr)
//		delete[] m_buf;
//
//	if (m_fp != NULL) {
//		fclose(m_fp);
//	}
//}

bool log::init(const char * file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) {
	if (max_queue_size >= 1) {
		is_async = true;
		m_log_queue = &blockQueue<string>::getInstance(max_queue_size);
		pthread_t pid;
		pthread_create(&pid, NULL, log_thread_work, NULL);
	}
	m_close_log = close_log;
	m_log_buf_size = log_buf_size;
	m_split_lines = split_lines;
	m_buf = new char[log_buf_size];
	memset(m_buf, '\0', sizeof m_buf);
	time_t t = time(NULL);
	struct tm *sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;

	const char* p = strrchr(file_name, '/');
	char log_full_name[256] = { 0 };
	if (p == NULL)
	{
		snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
	}
	else
	{
		strcpy(log_name, p + 1);
		strncpy(dir_name, file_name, p - file_name + 1);
		snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
	}
	m_today = my_tm.tm_mday;

	m_fp = fopen(log_full_name, "a");
	if (m_fp == NULL)
	{
		return false;
	}

	return true;
}

void log::write_log(int level, const char * format, ...)
{
	struct timeval now = { 0, 0 };
	gettimeofday(&now, NULL);
	time_t t = now.tv_sec;
	struct tm *sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;
	char s[16] = { 0 };
	switch (level)
	{
	case 0:
		strcpy(s, "[debug]:");
		break;
	case 1:
		strcpy(s, "[info]:");
		break;
	case 2:
		strcpy(s, "[warn]:");
		break;
	case 3:
		strcpy(s, "[erro]:");
		break;
	default:
		strcpy(s, "[info]:");
		break;
	}
	//写入一个log，对m_count++, m_split_lines最大行数
	log::m_mutex.lock();
	m_count++;

	if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
	{

		char new_log[256] = { 0 };
		fflush(m_fp);
		fclose(m_fp);
		char tail[16] = { 0 };

		snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

		if (m_today != my_tm.tm_mday)
		{
			snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
			m_today = my_tm.tm_mday;
			m_count = 0;
		}
		else
		{
			snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
		}
		m_fp = fopen(new_log, "a");
	}

	log::m_mutex.unlock();

	va_list valst;
	va_start(valst, format);

	string log_str;
	log::m_mutex.lock();

	//写入的具体时间内容格式
	int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
		my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
		my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

	int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
	m_buf[n + m] = '\n';
	m_buf[n + m + 1] = '\0';
	log_str = m_buf;

	log::m_mutex.unlock();

	if (is_async && !m_log_queue->is_full())
	{
		m_log_queue->push(log_str);
	}
	else
	{
		log::m_mutex.lock();
		fputs(log_str.c_str(), m_fp);
		log::m_mutex.unlock();
	}

	va_end(valst);
}

void log::flush(void)
{
	log::m_mutex.lock();
	fflush(m_fp);
	log::m_mutex.unlock();
}
