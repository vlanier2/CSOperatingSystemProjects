CFLAGS=-W -Wall -I.. -g
LDFLAGS=-L.. -g
PROGRAMS=uspsv1 uspsv2 uspsv3 uspsv4
OBJECTS=p1fxns.o uspsutils.o
LIBRARIES=-lADTs

all : $(PROGRAMS)

uspsv1: uspsv1.o $(OBJECTS)
	gcc -o $@ $^

uspsv2: uspsv2.o $(OBJECTS)
	gcc -o $@ $^

uspsv3: uspsv3.o processdata.o $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv4: uspsv4.o processdata.o procinfo.o $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $^ $(LIBRARIES)

uspsv1.o : uspsv1.c uspsutils.h
uspsv2.o : uspsv2.c uspsutils.h
uspsv3.o : uspsv3.c uspsutils.h
uspsv4.o : uspsv4.c uspsutils.h
p1fxns.o : p1fxns.c p1fxns.h
uspsutils.o : uspsutils.c p1fxns.h
processdata.o : processdata.c p1fxns.h
procinfo.o : procinfo.h p1fxns.h

clean:
	rm -f $(PROGRAMS) $(OBJECTS) processdata.o procinfo.o
	rm -f uspsv1.o uspsv2.o uspsv3.o uspsv4.o