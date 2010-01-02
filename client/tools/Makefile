include ../../Makefile.common

CCFLAGS += -I../ -I../../common -I../../security -I../../udt -I../../gmp

ifeq ($(STATIC), 1)
   LDFLAGS += ../libclient.a ../../lib/libsecurity.a ../../lib/librpc.a ../../lib/libcommon.a ../../lib/libudt.a
else
   LDFLAGS += -L../ -L../../lib -lsecurity -lrpc -ludt -lcommon -lclient
endif

all: download upload ls rm mkdir stat sysinfo mv cp stream

download: download.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

upload: upload.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

ls: ls.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

rm: rm.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

mkdir: mkdir.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

stat: stat.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

sysinfo: sysinfo.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

mv: mv.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

cp: cp.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

stream: stream.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

clean:
	rm -f *.o download upload ls rm mkdir stat sysinfo mv cp stream
