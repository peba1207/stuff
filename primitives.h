using namespace std;
#include <atomic>
#include <thread>
#include <mutex>

bool tas(atomic<bool> &x, memory_order MEM){
	bool expected = false;
	bool desired = true;
	return x.compare_exchange_strong(expected, desired, MEM);
}

int fai(atomic<int> &x, int amount, memory_order MEM){
	return x.fetch_add(amount, MEM);
}

template <typename T>
	bool cas(atomic<T> &x, T expected, T desired, memory_order MEM){
	T expected_ref = expected;
	return x.compare_exchange_strong(expected_ref, desired, MEM);
}
template <typename T>
	T vcas(atomic<T> &x, T expected, T desired, memory_order MEM){
	T expected_ref = expected;
	x.compare_exchange_strong(expected_ref, desired, MEM);
	return expected_ref;
}

class CustomLock{
public:
	virtual void lock() = 0;
	virtual void unlock() = 0;
};

class TestAndSet:public CustomLock{
public:
	TestAndSet(){flag = false;}
	void lock(){while(tas(flag,memory_order_seq_cst)==false){}}
	void unlock(){flag.store(false);}
private:	
	atomic<bool> flag; 
};
class TestAndTestAndSet:public CustomLock{
public:
	TestAndTestAndSet(){flag = false;}
	void lock(){while(flag==true || tas(flag,memory_order_seq_cst)==false){}}
	void unlock(){flag.store(false);}
private:	
	atomic<bool> flag; 
};

class Ticket:public CustomLock{
public:
	Ticket(){
		nextNum = 0;
		nowServing = 0;
	}
	void lock(){
		int my_num = fai(nextNum, 1, memory_order_relaxed);
		while(nowServing.load(memory_order_seq_cst)!=my_num){}
	}
	void unlock(){
		fai(nowServing,1,memory_order_relaxed);
	}
private:
	atomic<int> nextNum;
	atomic<int> nowServing;
};

class Pthread:public CustomLock{
public:
	Pthread(){lk = new mutex();}
	void lock(){lk->lock();}
	void unlock(){lk->unlock();}
private:
	mutex* lk;
};


class Node{
public:
	Node(){next = nullptr; wait = false;}
	atomic<Node*> next;
	atomic<bool> wait;
};


extern thread_local int mytid;

class MCSLock:public CustomLock{
	public:
	Node** nodes;

atomic<Node*> tail;
		MCSLock(int threads){
			tail = nullptr;
			nodes = new Node*[threads]();
			for(int i = 0; i<threads; i++){
				nodes[i] = new Node();
			}
		}
void acquire(Node* myNode){
			Node* oldTail = tail.load(memory_order_seq_cst);
			myNode->next.store(nullptr, memory_order_relaxed);
			while(!cas(tail,oldTail,myNode,memory_order_seq_cst)){
				oldTail = tail.load(memory_order_seq_cst);
			}
	
			if(oldTail != nullptr){
				myNode->wait.store(true,memory_order_relaxed);
				oldTail-> next.store(myNode, memory_order_seq_cst);
				while (myNode->wait.load(memory_order_seq_cst)){}
			}
		}
		void release(Node* myNode){
			Node* m = myNode;
			if(cas(tail, m, (Node*)nullptr, memory_order_seq_cst)){}
			else{
				while(myNode->next.load(memory_order_seq_cst) == nullptr){}
				myNode->next.load()->wait.store(false,memory_order_seq_cst);
			}
		}
		void lock(){acquire(nodes[mytid]);}
		void unlock(){release(nodes[mytid]);}
	private:
		
		
};

class CustomBarrier{
public:
	virtual void arrive_and_wait() = 0;
};


class SenseReversal:public CustomBarrier{
public:
	SenseReversal(int nThreads){
		N = nThreads;
		senses = new bool[N]();
		for(int i = 0; i<N; i++){
			senses[i] = false;
		}
	}
	void arrive_and_wait(){
		bool my_sense = !senses[mytid];
		senses[mytid] = my_sense;
		int cnt_cpy = fai(count,1,memory_order_seq_cst);
		if(cnt_cpy == N-1){
			count.store(0,memory_order_seq_cst);
			sense.store(my_sense,memory_order_seq_cst);
		}
		else{
			while(sense.load(memory_order_seq_cst) != my_sense){}
		}
	}
private:
	atomic<int> count = 0;
	atomic<bool> sense = false;
	int N;
	bool* senses;
};

class PthreadBarrier:public CustomBarrier{
public:
	PthreadBarrier(int threads){
		bar = new barrier(threads);
	}
	void arrive_and_wait(){
		bar->arrive_and_wait();
	}
private:
	barrier<> *bar; 
};


//https://canvas.colorado.edu/courses/142986/files?preview=85479469