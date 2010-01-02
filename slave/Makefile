include ../Makefile.common

CCFLAGS += -I../udt -I../gmp -I../security -I../common

ifeq ($(STATIC), 1)
   LDFLAGS += libslave.a ../lib/librpc.a ../lib/libsecurity.a ../lib/libcommon.a ../lib/libudt.a 
else
   LDFLAGS += -L../lib -lslave -lrpc -lsecurity -lcommon -ludt
endif

OBJS = slave.o serv_file.o serv_spe.o

all: libslave.so libslave.a start_slave sphere

%.o: %.cpp
	$(C++) -fPIC $(CCFLAGS) $< -c

libslave.so: $(OBJS)
	$(C++) -shared -o $@ $^

libslave.a: $(OBJS)
	ar -rcs $@ $^

start_slave: start_slave.cpp
	$(C++) start_slave.cpp -o start_slave $(CCFLAGS) $(LDFLAGS)

sphere: _always_check_
	cd sphere; make; cd ../

_always_check_:
	true

clean:
	rm -f *.o *.so *.a start_slave
	rm -f ./sphere/*.so

install:
	mv libslave.so libslave.a ../lib
