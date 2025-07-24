CC = gcc
CFLAGS = -Wall -g -D_FILE_OFFSET_BITS=64 
OBJS = chanfs.o fs.o fs_utils.o chan_parse.o textproc.o
CPPFLAGS=
LDFLAGS = -pthread -lfuse -lcurl -lcjson -llexbor 
OS = $(shell uname -s)

# If we are using MacOS, then add the libraries added by homebrew.
ifeq ($(OS), Darwin)
	CFLAGS := -I/opt/homebrew/include $(CFLAGS)
	LDFLAGS := -L/opt/homebrew/lib $(LDFLAGS)
endif

all: chanfs
chanfs: $(OBJS)
clean: 
	rm -f chanfs $(OBJS)	 
