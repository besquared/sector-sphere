include ../Makefile.common

CCFLAGS += -I../udt -I../common

OBJS = gmp.o message.o prec.o

all: librpc.so librpc.a

%.o: %.cpp %.h
	$(C++) -fPIC $(CCFLAGS) $< -c

librpc.so: $(OBJS)
	$(C++) -shared -o $@ $^

librpc.a: $(OBJS)
	ar -rcs $@ $^

clean:
	rm -f *.o *.so *.a

install:
	mv librpc.so librpc.a ../lib
