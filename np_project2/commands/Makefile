CXXFLAGS = -Wall
OUTPATH = ../bin
TARGETS = $(addprefix $(OUTPATH)/,noop number removetag removetag0)

all: $(TARGETS)

%.o: %.cpp
	$(CXX) $< -o $@ $(CXXFLAGS)

clean:
	rm $(TARGETS)