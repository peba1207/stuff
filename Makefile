all: exampleC exampleCPP mysort counter

exampleCPP: exampleCPP.cpp primitives.h
	g++ exampleCPP.cpp -pthread -O3 -std=c++2a -g -o exampleCPP

exampleC: exampleC.c
	gcc exampleC.c -pthread -O3 -g -o exampleC

mysort: mysort.cpp
	g++ mysort.cpp -pthread -O3 -std=c++2a -g -o mysort

counter: counter.cpp primitives.h
	g++ counter.cpp -pthread -O3 -std=c++2a -g -o counter

.PHONY : clean
clean:
	rm exampleC exampleCPP
