CC=gcc
CFLAGS=-std=c99 -c -Wall
LDFLAGS=-lm
SOURCES=VPKTool.c vpk.h
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=vpktool

linux:   CC=gcc
mingw32: CC=i686-w64-mingw32-gcc
mingw64: CC=x86_64-w64-mingw32-gcc

all:
	@echo "Please specify a build platform (linux, mingw32, mingw64)"
linux: $(SOURCES) $(EXECUTABLE)
mingw32: $(SOURCES) $(EXECUTABLE)
mingw64: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f *.o $(EXECUTABLE)