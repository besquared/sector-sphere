C++ = g++
CCFLAGS = -I../../common -shared -fPIC -O3 -g

all: streamhash.so

streamhash.so: streamhash.cpp
	$(C++) $(CCFLAGS) -o $@ $^ -lstdc++

clean:
	rm -f *.so
