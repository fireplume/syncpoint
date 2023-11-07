SHELL := /bin/bash
CC := gcc

INSTALL_DIR := out

LIB_CFLAGS := -g -std=gnu11 -fPIC -O0 -Wall -I.
LIB_LDFLAGS := -shared -lpthread

TEST_CFLAGS := -g -std=gnu11 -fpie -O0 -Wall -I. -L$(INSTALL_DIR) -lsyncpoint
TEST_LDLAGS := -L$(INSTALL_DIR) -lsyncpoint -Wl,-rpath $(CURDIR)/$(INSTALL_DIR)

.PHONY: clean

all: $(INSTALL_DIR) libsyncpoint.so unittest

$(INSTALL_DIR):
	mkdir $(INSTALL_DIR)

libsyncpoint.so: syncpoint.c
	$(CC) $(LIB_CFLAGS) syncpoint.c -o $(INSTALL_DIR)/libsyncpoint.so $(LIB_LDFLAGS)

unittest: unittest.c $(INSTALL_DIR)/libsyncpoint.so
	$(CC) $(TEST_CFLAGS) unittest.c $(TEST_LDLAGS) -o $(INSTALL_DIR)/unittest

clean:
	rm -fr $(INSTALL_DIR)
