include ../../Makefile.common

CCFLAGS = -I../../include $(DYNAMIC_FLAGS) -fPIC -O3 -g

all: streamhash.so

streamhash.so: streamhash.cpp
	$(C++) $(CCFLAGS) -o $@ $^ -lstdc++

clean:
	rm -f *.so
