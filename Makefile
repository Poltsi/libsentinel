SHELL = /bin/sh
CC    = gcc

LIBNAME  = sentinel
LIBFILE  = lib$(LIBNAME).so
CMDTOOL = download
SRCDIR  = src
LIBSOURCES = $(SRCDIR)/lib$(LIBNAME).c
BINSOURCES = $(SRCDIR)/$(CMDTOOL).c
#SOURCES := $(shell export SRCDIR="$(SRCDIR)"; echo $${SRCDIR}/*.c)
LIBOBJECTS = $(LIBSOURCES:.c=.o)
BINOBJECTS = $(BINSOURCES:.c=.o)
INC_DIR = include
DESTDIR = .
PREFIX = $(DESTDIR)/usr/local
LIBDIR = $(PREFIX)/lib
BINDIR = $(PREFIX)/bin

CFLAGS       = -fPIC -pedantic -Wall -Wextra -march=native -ggdb3 -I$(INC_DIR)
DEBUGFLAGS   = -O0 -D _DEBUG
FLAGS        = -std=gnu99
LDFLAGS      = -shared
LINKFLAG     = -Wl,-rpath $(LIBDIR)
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

all: check $(LIBFILE) $(CMDTOOL)

check:
	if [ ! -e $(LIBDIR) ]; then mkdir -p $(LIBDIR); fi; \
	if [ ! -e $(BINDIR) ]; then mkdir -p $(BINDIR); fi

.c.o: $<
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -c $*.c -o $*.o

$(LIBFILE): $(LIBOBJECTS)
	$(CC) $(LDFLAGS) -o $(LIBDIR)/$@ $(LIBOBJECTS)

$(CMDTOOL): $(LIBFILE) $(BINOBJECTS)
	$(CC) $(FLAGS) $(CFLAGS) -L$(LIBDIR) $(BINOBJECTS) -l$(LIBNAME) $(LINKFLAG) $(DEBUGFLAGS) -o $(BINDIR)/$(CMDTOOL)

clean:
	rm -f $(LIBOBJECTS) $(BINOBJECTS) $(BINDIR)/$(CMDTOOL) $(LIBDIR)/$(LIBFILE)
