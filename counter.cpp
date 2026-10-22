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

vector<thread*> threads;
int numThreads = 1;
int cntr=0;
struct timespec startTime, endTime;
CustomBarrier *bar; 
CustomLock *lk;
thread_local int mytid = -1;

void global_cleanup(){
	delete bar;
	delete lk;
}

void* thread_bar(int tid, int iterations){
	mytid = tid;
	bar->arrive_and_wait();
	if(tid==0){
		clock_gettime(CLOCK_MONOTONIC,&startTime);
	}
	bar->arrive_and_wait();
	
	// do something
	for(int i = 0; i<iterations*numThreads; i++){
		if(i%numThreads==tid){
			cntr++;
		}
		bar->arrive_and_wait();
	}


	bar->arrive_and_wait();
	if(tid==0){
		clock_gettime(CLOCK_MONOTONIC,&endTime);
	}
	
	return 0;
}

void* thread_lock(int tid, int iterations){
	mytid = tid;
	bar->arrive_and_wait();
	if(tid==0){
		clock_gettime(CLOCK_MONOTONIC,&startTime);
	}
	bar->arrive_and_wait();
	
	// do something
	for(int i = 0; i<iterations; i++){
		lk->lock();
		cntr++;
		lk->unlock();
	}

	bar->arrive_and_wait();
	if(tid==0){
		clock_gettime(CLOCK_MONOTONIC,&endTime);
	}
	
	return 0;
}

int main(int argc, char* argv[]) { 
	//parse flags
	int opt; 
	int option_index = 0;
	string outfile_name = "";
	string barParam = "";
	string lock = "";
	int iterations = 1;
    static struct option long_options[] = {
            {"name",     0, 0,  'N'},
            {"t", 1, 0, 't'},
			{"n", 1, 0, 'n'},
	        {"bar",    1, 0,  'b' },
			{"lock",    1, 0,  'l' },
			{"o",    1, 0,  'o' },
            {0,         0,                 0,  0 }
        };  
    while((opt = getopt_long(argc, argv, "n:t:N:b:l:o:", long_options,&option_index)) != -1) 
    { 
      switch(opt){
		case 'N':
			cout<<"Peyton Barger"<<endl;
			break;
		case 'n':
			iterations = stoi(optarg);
			break;
		case 'o':
			outfile_name = optarg;
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
	//initialize lock
	if(lock == "tas"){
		lk = new TestAndSet();
	} else if(lock == "ttas"){
		lk = new TestAndTestAndSet();
	} else if(lock == "ticket"){
		lk = new Ticket();
	} else if(lock == "mcs"){
		lk = new MCSLock(numThreads);
	} else {
		//default
		lk = new Pthread();
	} 
	//initialize Barrier
	if(barParam == "sense"){
		bar = new SenseReversal(numThreads);
	} else{
		//default
		bar = new PthreadBarrier(numThreads);
	} 

	// launch threads
	int ret; int i;
	threads.resize(numThreads);
	for(i=1; i<numThreads; i++){
		if(barParam != ""){
			threads[i] = new thread(thread_bar,i,iterations);
		}else{
			threads[i] = new thread(thread_lock,i,iterations);
		}
	}
	i = 0;
	 // master also calls thread_main
	if(barParam != ""){
		thread_bar(i,iterations);
	}else{
		thread_lock(i,iterations);
	}
	
	if(outfile_name != ""){
		ofstream outFile(outfile_name);
	    outFile << cntr << endl;
	}
	unsigned long long elapsed_ns;
	elapsed_ns = (endTime.tv_sec-startTime.tv_sec)*1000000000 + (endTime.tv_nsec-startTime.tv_nsec);
	printf("Elapsed (ns): %llu\n",elapsed_ns);
	double elapsed_s = ((double)elapsed_ns)/1000000000.0;
	printf("Elapsed (s): %lf\n",elapsed_s);
	return 0; 
}