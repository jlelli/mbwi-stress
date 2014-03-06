CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lm -lrt -pthread
SOURCES=test1.c dl_syscalls.c rt_utils.c misc_utils.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=test1

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) 

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)

distclean:
	rm -rf *.o *.dat $(EXECUTABLE)
