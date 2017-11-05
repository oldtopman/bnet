CC=gcc
CFLAGS=-g -Wall -Wextra -pedantic --std=c99
LDFLAGS=
SOURCES=src/bnet.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bnet

all: $(SOURCES) $(EXECUTABLE)

clean:
	rm -f src/*.o
	rm -f bnet

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.o:
	$(CC) $(CFLAGS) $< -o $@

