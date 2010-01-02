C++ = g++
CCFLAGS = -I../../../common -shared -fPIC -O3 -g
LDFLAGS = -lstdc++ ../../../lib/libcommon.a

all: randwriter.so sort.so sorthash.so wordbucket.so gen_idx.so mr_sort.so mr_word.so

randwriter.so: randwriter.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

sort.so: sort.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

sorthash.so: sorthash.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

wordbucket.so: wordbucket.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

gen_idx.so: gen_idx.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

mr_sort.so: mr_sort.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

mr_word.so: mr_word.cpp
	$(C++) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.so
