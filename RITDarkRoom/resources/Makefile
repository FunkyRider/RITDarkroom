EXECUTABLE = cr2raw

CC=g++
CFLAGS=-c -Wall -O0 -fopenmp -msse4.1 -DDEBUG -g	# -O3
INCLUDES = -I.
LIBS=-lpthread -ljpeg -ljpeg -lgomp
LDFLAGS =-L/usr/lib -L/lib

SOURCES = src/image.cpp \
		src/cr2raw.cpp \
		src/cr2plugins.cpp \
		src/denoise.cpp \
		src/tile.cpp \
		src/string.cpp \
		src/resample.cpp \
		src/sharpen.cpp \
		src/cr2conv.cpp \
		src/cr2client.cpp \
		src/cr2develop.cpp \
		src/cr2hot.cpp \
		src/main.cpp

OBJECTS=$(SOURCES:.cpp=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
	
clean:
	rm -f $(EXECUTABLE) src/*.o
