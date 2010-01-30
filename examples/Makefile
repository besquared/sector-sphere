include ../Makefile.common

ifeq ($(STATIC), 1)
   LDFLAGS += ../lib/libclient.a ../lib/libsecurity.a ../lib/librpc.a ../lib/libcommon.a ../lib/libudt.a 
else
   LDFLAGS += -L../lib -lsecurity -lrpc -ludt -lcommon -lclient
endif

all: testfs testdc wordcount text.idx mrsort mrword funcs

testfs: testfs.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

testdc: testdc.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

wordcount: wordcount.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

text.idx: text.idx.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

mrsort: mrsort.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

mrword: mrword.cpp
	$(C++) $^ -o $@ $(CCFLAGS) $(LDFLAGS)

funcs: _always_check_
	cd ./funcs; make; cd ../

_always_check_:
	true

clean:
	rm -f *.o *.so *.a testfs testdc wordcount text.idx mrsort mrword
	rm -f ./funcs/*.so

install:
	
