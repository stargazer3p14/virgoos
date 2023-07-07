
all:	e2sample

e2sample: e2sample.o e2lib.so
	g++ -o e2sample e2sample.o e2lib.so

e2sample.o: e2sample.cpp e2lib.h
	g++ -c e2sample.cpp

e2lib.so: e2lib.o
	ld -shared -o e2lib.so e2lib.o

e2lib.o: e2lib.c e2lib.h
	gcc -c e2lib.c

clean:
	rm -rf *.o *.so *.out e2sample

install:
	cp e2lib.so /usr/lib
