include ../Makefile.common

CCFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26

ifeq ($(STATIC), 1)
   LDFLAGS += ../lib/libclient.a ../lib/libsecurity.a ../lib/librpc.a ../lib/libcommon.a ../lib/libudt.a
else
   LDFLAGS += -L../lib -lsecurity -lrpc -ludt -lcommon -lclient
endif

LDFLAGS += -lfuse

all: sector-fuse

OBJS = sectorfs.o

%.o: %.cpp %.h
	$(C++) -fPIC $(CCFLAGS) $< -c

sector-fuse: sector-fuse.cpp $(OBJS) 
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

clean:
	rm -f *.o sector-fuse
