
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

// ��ʱ����
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
	// �ص�����
	void(*cb_func)(util_timer*);
};

class timer_cmp {
public:
	bool operator()(util_timer* a, util_timer* b) {
		return a->expire > b->expire;
	}
};

// ʱ�����
class time_heap {
private:
	std::priority_queue<util_timer*, std::vector<util_timer*>, timer_cmp> pq;
	std::unordered_map<int, util_timer*> hash;
	int top_time = -1;

public:

	~time_heap() {
	}

	// ��Ӷ�ʱ��
	void add_timer(util_timer* timer) {
		if (timer) {
			pq.push(timer);
			hash[timer->user_data->sockfd] = timer;
		}
	}

	// ɾ����ʱ��
	void del_timer(util_timer* timer) {
		// ���ɾ��
		if (!timer) {
			return;
		}
		timer->cb_func = NULL; // �ص��������Ϊ�գ���ʾɾ��
	}

	// ��ȡ���һ�εĵ��ڼ�ʱ��
	util_timer* top() const {
		return pq.empty() ? nullptr : pq.top();
	}
	// ����
	bool reset(int connfd, int delay=TIMEOUT) {
		return hash[connfd]->reset(delay);
	}

	bool getTopTime(int& t) {
		if (top_time == -1)
			return false;

		t = top_time;
		return true;
	}

	// �����ڵĶ�ʱ��
	void tick() {
		if (pq.empty()) {
			return;
		}
		time_t cur = time(NULL);  // ��ǰʱ��
		while (!pq.empty()) {
			util_timer* tmp = pq.top();
			if (tmp->expire > cur) {
				top_time = tmp->expire - cur;
				break;  // û�е��ڵĶ�ʱ��
			}

			// ִ�ж�ʱ������
			if (tmp->cb_func) {
				tmp->cb_func(tmp);
			}
			pq.pop();  // �Ƴ�������Ķ�ʱ��
		}
	}
};