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
LINKFLAG     = -Wl,-rpath $(LIBDIR)  -lm
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

VALGRIND_PARAMS =  --leak-check=yes --leak-check=full --show-leak-kinds=all --show-reachable=yes --num-callers=20 --track-fds=yes

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

valgrind: clean $(CMDTOOL)
	valgrind $(VALGRIND_PARAMS) $(BINDIR)/$(CMDTOOL) -d $(PORT) -l -v 2>&1 | tee out-`date "+%Y.%m.%d-%H:%M:%S"`.log

valgrinddl: clean $(CMDTOOL)
	valgrind $(VALGRIND_PARAMS) $(BINDIR)/$(CMDTOOL) -d $(PORT) -f 1 -t 2 -v 2>&1 | tee out-`date "+%Y.%m.%d-%H:%M:%S"`.log

valgrindreal: clean $(CMDTOOL)
	valgrind $(VALGRIND_PARAMS) --max-stackframe=4147483632  $(BINDIR)/$(CMDTOOL) -d $(PORT) -f 5 -t 6 -v 2>&1 | tee real-out-`date "+%Y.%m.%d-%H:%M:%S"`.log

clean:
	rm -f $(LIBOBJECTS) $(BINOBJECTS) $(BINDIR)/$(CMDTOOL) $(LIBDIR)/$(LIBFILE)
