include ../Makefile.common

CCFLAGS += -I../udt -I../common

ifeq ($(STATIC), 1)
   LDFLAGS += librpc.a ../lib/libcommon.a ../lib/libudt.a
else
   LDFLAGS += -L../lib -lrpc -lcommon -ludt
endif

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
