C++ = g++
CCFLAGS = -I../../include -shared -fPIC -O3 -g

all: streamhash.so

streamhash.so: streamhash.cpp
	$(C++) $(CCFLAGS) -o $@ $^ -lstdc++

clean:
	rm -f *.so
