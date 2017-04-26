SHELL = /bin/sh
CC    = gcc

LIBFILE  = libsentinel.so
SRCDIR  = src
export TMPSRC = $(SRCDIR)
LIBSOURCES = $(SRCDIR)/libsentinel.c
BINSOURCES = $(SRCDIR)/download.c
#SOURCES := $(shell export SRCDIR="$(SRCDIR)"; echo $${SRCDIR}/*.c)
CMDTOOL = download
LIBOBJECTS = $(LIBSOURCES:.c=.o)
BINOBJECTS = $(BINSOURCES:.c=.o)
INC_DIR = include
DESTDIR = .
PREFIX = $(DESTDIR)/usr/local
LIBDIR = $(PREFIX)/lib
BINDIR = $(PREFIX)/bin

CFLAGS       = -fPIC -pedantic -Wall -Wextra -march=native -ggdb3 -I$(INC_DIR)
DEBUGFLAGS   = -O0 -D _DEBUG
FLAGS        = -std=gnu99 -I$(INC_DIR)
LDFLAGS      = -shared
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

all: check $(LIBFILE) $(CMDTOOL)

check:
	if [ ! -e $(LIBDIR) ]; then mkdir -p $(LIBDIR); fi; \
	if [ ! -e $(BINDIR) ]; then mkdir -p $(BINDIR); fi

$(LIBFILE): $(LIBOBJECTS)
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -o $@ $(LIBOBJECTS)
$(CMDTOOL): $(LIBFILE)
	$(CC) $(FLAGS) $(CFLAGS) -L$(LIBDIR)/$(LIBFILE) $(DEBUGFLAGS) -o $(BINDIR)/$(CMDTOOL) $(BINOBJECTS)
clean:
	rm -f $(OBJECTS) $(PREFIX)/* $(BINDIR)/* $(LIBDIR)/*
