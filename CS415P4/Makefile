CFLAGS=-W -Wall -I/usr/local/include
LDFLAGS=-L/usr/local/lib
LIBRARIES=-lADTs -lpthread
PROGS=test mthtest ttest
WHICHSOURCE=strheap

all: $(PROGS)

test: test.o $(WHICHSOURCE).o
	gcc -o $@ $^ $(LIBRARIES)

mthtest: mthtest.o $(WHICHSOURCE).o
	gcc -o $@ $^ $(LIBRARIES)

ttest: ttest.o table.o row.o $(WHICHSOURCE).o
	gcc -o $@ $^ $(LIBRARIES)

test.o: test.c
mthtest.o: mthtest.c
ttest.o: ttest.c
table.o: table.c
row.o: row.c
$(WHICHSOURCE).o: $(WHICHSOURCE).c

clean:
	rm -f $(PROGS) *.o
