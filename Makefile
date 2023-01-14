CXX=g++ --std=c++20
DEFAULT: main.o Object.o ReflMgrInit.o JSON.o TypeID.o ReflMgr.o
	$(CXX) main.o Object.o ReflMgrInit.o JSON.o TypeID.o ReflMgr.o -o refl
Object.o: Object.cpp
	$(CXX) -c Object.cpp
ReflMgrInit.o: ReflMgrInit.cpp
	$(CXX) -c ReflMgrInit.cpp
JSON.o: JSON.cpp
	$(CXX) -c JSON.cpp
TypeID.o: TypeID.cpp
	$(CXX) -c TypeID.cpp
ReflMgr.o: ReflMgr.cpp
	$(CXX) -c ReflMgr.cpp
main.o: main.cpp ReflMgr.h
	$(CXX) -c main.cpp
clean:
	rm *.o
