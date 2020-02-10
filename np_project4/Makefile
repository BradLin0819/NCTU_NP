CXX=g++
CXXFLAGS=-std=c++11 -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

all: socks_server hw4.cgi

socks_server: socks_server.o utils.o
	$(CXX) $^ -o $@

socks_server.o:	socks_server.cpp utils.h
	$(CXX) -c -o $@ $<

hw4.cgi: console.cpp
	$(CXX) $< -o $@ $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

utils.o:	utils.cpp	utils.h
	$(CXX) -c -o $@ $<

clean:
	rm -f *.cgi socks_server *.o
