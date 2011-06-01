CC      = g++
LIBS    = -lccn -lssl -lcrypto -lpthread
DEFS    = -DTIXML_USE_STL -DDEBUG
CFLAGS	= -c -Wall

SOURCES	= main.cc \
	  act.cc \
	  sessionenum.cc \
	  datagen.cc \
	  announcement.cc \
	  confwizard.cc \
	  debugbox.cc \
	  tinyxml.cc \
	  tinystr.cc \
	  tinyxmlerror.cc \
	  tinyxmlparser.cc \
	  base64.cc

OBJECTS	= $(SOURCES:.cc=.o)
TARGET	= acemu

all: $(SOURCES) $(TARGET)

.cc.o:
	$(CC) $(CFLAGS) $(DEFS) $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(LIBS) $(OBJECTS) -o $@

clean:
	rm $(OBJECTS)
	rm $(TARGET)
