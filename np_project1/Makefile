CXX 	=	g++
OBJS	=	npshell.o Parser.o	Command.o utils.o
all:	npshell 

npshell: $(OBJS)	
	$(CXX)	$(OBJS)		-o	$@
npshell.o:	npshell.cpp	Parser.h utils.h
	$(CXX)	-c	-g -o $@ $<
Parser.o:	Parser.cpp	Parser.h Command.h
	$(CXX)	-c	-o	$@	$<
Command.o:	Command.cpp	Command.h
	$(CXX)	-c	-o	$@	$<
utils.o:	utils.cpp	utils.h	Parser.h
	$(CXX)	-c 	-o	$@	$<
clean:
	rm *.o	npshell
