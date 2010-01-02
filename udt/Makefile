include ../Makefile.common

CCFLAGS += -D LINUX -D IA32
LDFLAGS += 

OBJS = md5.o common.o window.o list.o buffer.o packet.o channel.o queue.o core.o cache.o api.o ccc.o

all: libudt.so libudt.a

%.o: %.cpp %.h
	$(C++) -fPIC $(CCFLAGS) $< -c

libudt.so: $(OBJS)
	$(C++) -shared -o $@ $^

libudt.a: $(OBJS)
	ar -rcs $@ $^

clean:
	rm -f *.o *.so *.a

install:
	mv libudt.so libudt.a ../lib
