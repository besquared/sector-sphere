include ../Makefile.common

CCFLAGS += -I../udt -I../gmp -I../common -I../security

OBJS = fscache.o client.o fsclient.o dcclient.o clientmgmt.o

all: libclient.so libclient.a

%.o: %.cpp
	$(C++) -fPIC $(CCFLAGS) $< -c

libclient.so: $(OBJS)
	$(C++) -shared -o $@ $^

libclient.a: $(OBJS)
	ar -rcs $@ $^

clean:
	rm -f *.o *.so *.a

install:
	mv libclient.so libclient.a ../lib
