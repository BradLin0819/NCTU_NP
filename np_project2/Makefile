CXX 	=	g++
COMMON_OBJS	=	Parser.o	Command.o utils.o
NP_SIMPLE_OBJS	=	np_simple.o $(COMMON_OBJS)
NP_SINGLE_OBJS	=	np_single_proc.o $(COMMON_OBJS)
NP_MULTI_OBJS	=	np_multi_proc.o $(COMMON_OBJS)	semaphore.o
PARSER_TEST_OBJS	=	Parser_test.o $(COMMON_OBJS)
all:	np_simple	 np_single_proc	np_multi_proc

np_simple: $(NP_SIMPLE_OBJS)	
	$(CXX)	$(NP_SIMPLE_OBJS)		-o	$@
np_simple.o:	np_simple.cpp	Parser.h utils.h
	$(CXX)	-c	-g -o $@ $<
np_single_proc: $(NP_SINGLE_OBJS)	
	$(CXX)	$(NP_SINGLE_OBJS)		-o	$@
parser_test:	$(PARSER_TEST_OBJS)
	$(CXX)	$(PARSER_TEST_OBJS)		-o	$@
np_single_proc.o:	np_single_proc.cpp	Parser.h utils.h
	$(CXX)	-c	-g -o $@ $<
np_multi_proc: $(NP_MULTI_OBJS)	
	$(CXX)	$(NP_MULTI_OBJS)		-o	$@
np_multi_proc.o:	np_multi_proc.cpp	Parser.h utils.h	semaphore.h
	$(CXX)	-c	-g -o $@ $<
Parser_test.o:	Parser_test.cpp	Parser.h utils.h
	$(CXX)	-c	-g -o $@ $<
Parser.o:	Parser.cpp	Parser.h Command.h
	$(CXX)	-c	-o	$@	$<
Command.o:	Command.cpp	Command.h
	$(CXX)	-c	-o	$@	$<
utils.o:	utils.cpp	utils.h	Parser.h
	$(CXX)	-c 	-o	$@	$<
semaphore.o:	semaphore.cpp	semaphore.h
	$(CXX)	-c 	-o	$@	$<
clean:
	rm *.o	np_simple	 np_single_proc	np_multi_proc
