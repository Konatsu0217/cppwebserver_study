#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <semaphore.h>

// 自旋锁
class spinlock_mutex {
	std::atomic_flag flag;
public:
	spinlock_mutex() :flag(ATOMIC_FLAG_INIT) {}
	void lock() {
		// 自旋上锁
		while (flag.test_and_set(std::memory_order_acquire));
	}
	void unlock() {
		flag.clear(std::memory_order_release);
	}
};

//template <typename T>
//class lock_free_stack {
//private:
//	struct node {
//		T data;
//		node* next;
//
//		node(T const& data_) :data(data_) {};
//	};
//	std::atomic<node*> head;
//public:
//	void push(T const& data) {
//		// 创建新的node
//		node* const pnew_node = new node(data);
//		// 新对象->next 指向 当前的atomic head
//		pnew_node->next = head.load();
//		// 比较/交换操作，比较head和new_node->next是否相同
//		// 如果相同，将 head 修改为 pnew_node，返回true---->head指向新的对象
//		// 如果不同，将 pnew_node->next 修改为 head，返回false----->新对象的next指旧的head，head未发生修改，自旋等待
//		while (!head.compare_exchange_weak(new_node->next, pnew_node));
//		// 如果进入while里了，说明**现在的head和 pnew_node->next已经不一样了
//		// 被其他的线程修改了，因此重新将 pnew_node->next更新为 **修改后的head** ，再重试
//	}
//
//	// 1.解决了 head->next可能是nullptr的访问问题
//	// 2.避免在返回值时拷贝，（如果抛出异常会导致拷贝失效，无法正常返回），将不正确的返回nullptr交给下一层处理
//	std::shared_ptr<T> pop() {
//		node* old_head = head.load();
//		while (old_head &&
//			!head.compare_exchange_weak(old_head, old_head->next));
//		return old_head ? old_head->data : std::shared_ptr<T>();
//	}
//};

template <typename T>
class lock_free_queue {
	struct Node {
		//enum State
		//{
		//	EMPTY = 0, TAKING, FILLING, FILLED,
		//};
		T val;
		//std::atomic<State> state = EMPTY;
	};
public:
	Node* data;
	std::atomic<int> head, tail;
	int size;
	// 记录 已使用/剩余 空间
	std::atomic<int> slot_available, data_available;


public:
	lock_free_queue(int size_) {
		data = new Node[size_], size = size_, head = 0, tail = 0, slot_available = size_,
			data_available = 0;
	}
	// 入队
	// 1.空间够不够？
	// 2.
	bool enqueue(T new_val) {
		// 1
		if (!slot_available) {
			return false;
		}
		int cur_tail = tail.load();
		int new_tail = cur_tail + 1;
		while (!tail.compare_exchange_strong(cur_tail, new_tail)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
		}
		data[new_tail].val = new_val;
		++data_available, --slot_available;
		return true;
	}

	bool dequeue(T& val) {
		if (!data_available) {
			return false;
		}
		int cur_head = head.load();
		int new_head = cur_head + 1;
		while (!head.compare_exchange_strong(cur_head, new_head)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
		}
		val = data[new_head].val;
		--data_available, ++slot_available;
		return true;
	}
};