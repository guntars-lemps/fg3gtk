CFLAGS=-g -Wall -Wextra `pkg-config --cflags gtk+-3.0`
LDFLAGS=`pkg-config --libs gtk+-3.0`
CC=gcc

OBJECTS=main.o

#GLIB_COMPILE_RESOURCES = 'pkg-config --variable=glib_compile_resources gio-2.0)'

#resources = $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies fg3.glade

#resources.c: gresource.xml $(resources)
#	$(GLIB_COMPILE_RESOURCES) gresource.xml --target=$0 --sourcedir=. --geneate-source

# xxd -i  ./fg3.glade > fg3glade.h


fg3gtk: fg3glade.h $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o fg3gtk $(LDFLAGS)

fg3glade.h: fg3.glade
	xxd -i  ./fg3.glade > fg3glade.h

clean:
	rm fg3gtk fg3glade.h main.o