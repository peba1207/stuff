#include <iostream>
#include <string>
#include "getopt.h"
#include <fstream>
#include <vector>
#include <cmath>
#include <thread>
#include <barrier>
#include <set>
#include <ctime>
#include <mutex>
#include <atomic>
#include "primitives.h"
using namespace std;
enum LockType {
	PTHREAD,
	TAS,
	TTAS,
	TICKET,
	MCS
};
enum LockType lockType;

struct timespec startTime, endTime;
CustomBarrier *bar; 
thread_local int mytid = -1;



void thread_bucket(int tid, set<int>* buckets[], int mins[], vector<int>* numbers, int numThreads, int numBuckets, CustomLock* bucket_locks[]){
	mytid = tid;
	cout<<mytid;
	bar->arrive_and_wait();
	if(tid==0){
		clock_gettime(CLOCK_MONOTONIC,&startTime);
	}
	bar->arrive_and_wait();

	int size = numbers->size();
	//initialize bucket mins
	if(tid == 0){
		mins[0] = 0;
		mins[numBuckets-1] = INT_MAX;
		//collect a portion of the set to estimate bucket sizes
		set<int> sample(numbers->begin(), numbers->begin()+numBuckets-2);
		set<int>::iterator it = sample.begin();
		for(int i = 1;i<numBuckets-1;i++){
			mins[i] = *it;
			advance(it, 1);
		}
	}
	bar->arrive_and_wait();
	for(int i = tid; i<numbers->size(); i+=numThreads){
		for(int j = 0; j<numBuckets; j++){
			if((*numbers)[i]<mins[j]){ // find bucket to insert 
				bucket_locks[j-1]->lock();
					buckets[j-1]->insert((*numbers)[i]);
				bucket_locks[j-1]->unlock();
				break;
			}
		}
	}
	bar->arrive_and_wait();
	if(tid==0){
		clock_gettime(CLOCK_MONOTONIC,&endTime);
	}
	bar->arrive_and_wait();
}

vector<int>* bucketSort(vector<int>* numbers,int numThreads,int numBuckets){
	//NOTE: vector not the most thread safe for parallel reads but will work in this case i think
	vector<int>* sorted = new vector<int>();
	set<int>** buckets = new set<int>*[numBuckets];
	int* mins = new int[numBuckets];
	CustomLock** bucket_locks = new CustomLock*[numBuckets];
	
	for(int i = 0; i<numBuckets;i++){
		buckets[i] = new set<int>();
		switch(lockType){
			case PTHREAD:
				bucket_locks[i] = new Pthread();
				break;
			case TAS:
				bucket_locks[i] = new TestAndSet();
				break;
			case TTAS:
				bucket_locks[i] = new TestAndTestAndSet();
				break;
			case TICKET:
				bucket_locks[i] = new MCSLock(numThreads);
				break;
			case MCS:
				bucket_locks[i] = new MCSLock(numThreads);
				break;
		}
	}
	// launch threads
	vector<thread*> threads;
	int ret;
	threads.resize(numThreads);
	for(int i=1; i<numThreads; i++){
		threads[i] = new thread(thread_bucket,i,&buckets[0],mins, numbers,numThreads,numBuckets,&bucket_locks[0]);
	}
	thread_bucket(0,&buckets[0],mins, numbers,numThreads,numBuckets,&bucket_locks[0]); // master also calls thread_bucket
	for(size_t i=1; i<numThreads; i++){
		threads[i]->join();
		delete threads[i];
	}

	//concantenate buckets
	for(int i = 0; i<numBuckets; i++){
		for(auto val:*(buckets[i])){
			sorted->push_back(val);
			//cout<<val<< " ";
		}
		delete buckets[i];
		delete bucket_locks[i];
	}
	delete[] buckets;
	delete[] mins;
	delete[] bucket_locks;
	//cout<<endl;
	return sorted;
}

int main(int argc, char* argv[]) { 
	//parse flags
	int opt; 
	int option_index = 0;
	string sourcefile_name = "";
	string outfile_name = "";
	string barParam = "";
	string lock = "";
	int numThreads = 1;
    static struct option long_options[] = {
            {"name",     0, 0,  'n'},
            {"i",  1, 0, 'i'},
            {"o",    1, 0,  'o' },
			{"t", 1, 0, 't'},
	        {"bar",    1, 0,  'b' },
			{"lock",    1, 0,  'l' },
            {0,         0,                 0,  0 }
        };  
    while((opt = getopt_long(argc, argv, "n:i:o:t:b:l:", long_options,&option_index)) != -1) 
    { 
      switch(opt){
		case 'n':
			cout<<"Peyton Barger"<<endl;
			break;
		case 'i':
			sourcefile_name = optarg;
			//cout<<sourcefile_name;
			break;
		case 'o':
			outfile_name = optarg;
			//cout<<outfile_name;
			break;
		case 't':
			numThreads = stoi(optarg);
		case 'b':
			barParam = optarg;
			break;
		case 'l':
			lock = optarg;
			break;
	  }
    } 
	if(lock == "pthread"){
		lockType = PTHREAD;
	}else if(lock == "tas"){
		lockType = TAS;
	} else if(lock == "ttas"){
		lockType = TTAS;
	} else if(lock == "ticket"){
		lockType = TICKET;
	} else if(lock == "mcs"){
		lockType = MCS;
	}  
	if(barParam == "sense"){
		bar = new SenseReversal(numThreads);
	} else{
		bar = new PthreadBarrier(numThreads);
	} 
		
	
	//Read file
	if(sourcefile_name == "") return 0;
	string text;
	ifstream MyReadFile(sourcefile_name);
	if(!MyReadFile.is_open()){
		cout<<"ERROR: file not found"<<endl;
		return -1;
	}
	vector<int>* numbers = new vector<int>();
	while(getline(MyReadFile,text)){
		numbers->push_back(stoi(text));
	}

	//Sort
	vector<int>* sorted;
	int numBuckets = numThreads * 10;
	sorted = bucketSort(numbers, numThreads, numBuckets);

	//for(auto i: *sorted) cout<<i<<endl;
	
	if(outfile_name != ""){
		ofstream outFile(outfile_name);
		for (int i:*sorted ){
	        outFile << i << endl;
		}
	}
	unsigned long long elapsed_ns;
	elapsed_ns = (endTime.tv_sec-startTime.tv_sec)*1000000000 + (endTime.tv_nsec-startTime.tv_nsec);
	printf("Elapsed (ns): %llu\n",elapsed_ns);
	double elapsed_s = ((double)elapsed_ns)/1000000000.0;
	printf("Elapsed (s): %lf\n",elapsed_s);
	return 0; 
}