CC	= g++
LIBS	= -lccn -lssl -lcrypto
CFLAGS	= -c -Wall

SOURCES	= main.cc \
	  sessionenum.cc \
	  datagen.cc \
	  announcement.cc \
	  debugbox.cc

OBJECTS	= $(SOURCES:.cc=.o)
TARGET	= acemu

all: $(SOURCES) $(TARGET)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(LIBS) $(OBJECTS) -o $@

clean:
	rm $(TARGET)
	rm $(OBJECTS)
