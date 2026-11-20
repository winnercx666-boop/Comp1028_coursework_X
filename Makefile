CC = gcc
CFLAGS = -std=c17 -O2 -Wall -Wextra -Wpedantic
# Optional existing folder adapter; use CPPFLAGS= for standard-C list input.
CPPFLAGS = -DENABLE_DIRECTORY_INPUT
SOURCES = coursework.c collection.c index.c statistics.c persistence.c

all: coursework.exe

coursework.exe: $(SOURCES) search_index.h collection.h statistics.h persistence.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

test:
	$(CC) $(CFLAGS) -I. tests/test_features.c index.c statistics.c persistence.c -o test_features.exe
	./test_features.exe

.PHONY: all test
