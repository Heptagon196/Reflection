CXX=g++ --std=c++20
DEFAULT: main.o Object.o ReflMgrInit.o JSON.o
	$(CXX) main.o Object.o ReflMgrInit.o JSON.o -o refl
Object.o: Object.cpp
	$(CXX) -c Object.cpp
ReflMgrInit.o: ReflMgrInit.cpp
	$(CXX) -c ReflMgrInit.cpp
JSON.o: JSON.cpp
	$(CXX) -c JSON.cpp
main.o: main.cpp
	$(CXX) -c main.cpp
clean:
	rm *.o
