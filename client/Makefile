include ../Makefile.common

CCFLAGS += -I../udt -I../gmp -I../common -I../security

OBJS = util.o fscache.o client.o fsclient.o dcclient.o probot.o

all: libclient.so libclient.a tools examples

%.o: %.cpp %.h
	$(C++) -fPIC $(CCFLAGS) $< -c

libclient.so: $(OBJS)
	$(C++) -shared -o $@ $^

libclient.a: $(OBJS)
	ar -rcs $@ $^

tools: _always_check_
	cd ./tools; make; cd ../

examples: _always_check_
	cd ./examples; make; cd ../

_always_check_:
	true

clean:
	rm -f *.o *.so *.a
	cd ./tools; make clean; cd ../
	cd ./examples; make clean; cd ../

install:
	mv libclient.so libclient.a ../lib
