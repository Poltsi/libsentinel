SHELL = /bin/sh
CC    = gcc

TARGET  = libsentinel.so
SRCDIR  = src
export TMPSRC = $(SRCDIR)
SOURCES := $(shell export SRCDIR="$(SRCDIR)"; echo $${SRCDIR}/*.c)
OBJECTS = $(SOURCES:.c=.o)
INC_DIR = include
DESTDIR = .
PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin

CFLAGS       = -fPIC -pedantic -Wall -Wextra -march=native -ggdb3 -I$(INC_DIR)
DEBUGFLAGS   = -O0 -D _DEBUG
FLAGS        = -std=gnu99 -I$(INC_DIR)
LDFLAGS      = -shared
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(OBJECTS)

clean:
	rm -f $(TARGET) $(OBJECTS) $(PREFIX)/* $(BINDIR)/*
