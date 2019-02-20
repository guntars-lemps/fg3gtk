CFLAGS=-g -Wall -Wextra `pkg-config --cflags gtk+-3.0 gmodule-2.0`
LDFLAGS=`pkg-config --libs gtk+-3.0` -rdynamic -lm
CC=gcc

OBJECTS=fg3gtk.o ini.o


fg3gtk: fg3glade.h $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o fg3gtk $(LDFLAGS)

fg3glade.h: fg3.glade
	xxd -i  ./fg3.glade > fg3glade.h

clean:
	-rm fg3gtk fg3glade.h fg3gtk.o ini.o