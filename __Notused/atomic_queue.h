#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <semaphore.h>

// ������
class spinlock_mutex {
	std::atomic_flag flag;
public:
	spinlock_mutex() :flag(ATOMIC_FLAG_INIT) {}
	void lock() {
		// ��������
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
//		// �����µ�node
//		node* const pnew_node = new node(data);
//		// �¶���->next ָ�� ��ǰ��atomic head
//		pnew_node->next = head.load();
//		// �Ƚ�/�����������Ƚ�head��new_node->next�Ƿ���ͬ
//		// �����ͬ���� head �޸�Ϊ pnew_node������true---->headָ���µĶ���
//		// �����ͬ���� pnew_node->next �޸�Ϊ head������false----->�¶����nextָ�ɵ�head��headδ�����޸ģ������ȴ�
//		while (!head.compare_exchange_weak(new_node->next, pnew_node));
//		// �������while���ˣ�˵��**���ڵ�head�� pnew_node->next�Ѿ���һ����
//		// ���������߳��޸��ˣ�������½� pnew_node->next����Ϊ **�޸ĺ��head** ��������
//	}
//
//	// 1.����� head->next������nullptr�ķ�������
//	// 2.�����ڷ���ֵʱ������������׳��쳣�ᵼ�¿���ʧЧ���޷��������أ���������ȷ�ķ���nullptr������һ�㴦��
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
	// ��¼ ��ʹ��/ʣ�� �ռ�
	std::atomic<int> slot_available, data_available;


public:
	lock_free_queue(int size_) {
		data = new Node[size_], size = size_, head = 0, tail = 0, slot_available = size_,
			data_available = 0;
	}
	// ���
	// 1.�ռ乻������
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