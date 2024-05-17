
#include <time.h>
#include <queue>
#include <unordered_map>
#define TIMESLOT 5
#define TIMEOUT 60
#define BUFFER_SIZE 1024

class util_timer;
class http_conn;

class client_data
{
public:
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
	util_timer* timer;
};

// 定时器类
class util_timer {
public:
	client_data* user_data;
	time_t expire;
	util_timer(int delay) {
		expire = time(NULL) + delay;
	}
	bool reset(int delay) {
		expire = time(NULL) + delay;
		if (expire < time(NULL))	return false;
		else return true;
	}
	// 回调函数
	void(*cb_func)(util_timer*);
};

class timer_cmp {
public:
	bool operator()(util_timer* a, util_timer* b) {
		return a->expire > b->expire;
	}
};

// 时间堆类
class time_heap {
private:
	std::priority_queue<util_timer*, std::vector<util_timer*>, timer_cmp> pq;
	std::unordered_map<int, util_timer*> hash;
	int top_time = -1;

public:

	~time_heap() {
	}

	// 添加定时器
	void add_timer(util_timer* timer) {
		if (timer) {
			pq.push(timer);
			hash[timer->user_data->sockfd] = timer;
		}
	}

	// 删除定时器
	void del_timer(util_timer* timer) {
		// 标记删除
		if (!timer) {
			return;
		}
		timer->cb_func = NULL; // 回调函数标记为空，表示删除
	}

	// 获取最近一次的到期计时器
	util_timer* top() const {
		return pq.empty() ? nullptr : pq.top();
	}
	// 重置
	bool reset(int connfd, int delay=TIMEOUT) {
		return hash[connfd]->reset(delay);
	}

	bool getTopTime(int& t) {
		if (top_time == -1)
			return false;

		t = top_time;
		return true;
	}

	// 处理到期的定时器
	void tick() {
		if (pq.empty()) {
			return;
		}
		time_t cur = time(NULL);  // 当前时间
		while (!pq.empty()) {
			util_timer* tmp = pq.top();
			if (tmp->expire > cur) {
				top_time = tmp->expire - cur;
				break;  // 没有到期的定时器
			}

			// 执行定时器任务
			if (tmp->cb_func) {
				tmp->cb_func(tmp);
			}
			pq.pop();  // 移除处理过的定时器
		}
	}
};