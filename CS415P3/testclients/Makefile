CFLAGS=-W -Wall -pedantic -I/usr/local/include -g
LDFLAGS=-L/usr/local/lib
PROGRAMS=echoclient dts-client
LIBRARIES=-lBXP -lpthread -lADTs

all: $(PROGRAMS)

echoclient: echoclient.o
	gcc -o $@ $^ $(LIBRARIES)

dts-client: dts-client.o
	gcc -o $@ $^ $(LIBRARIES)

echoclient.o: echoclient.c
dts-client.o: dts-client.c

clean:
	rm -f $(PROGRAMS) *.o
