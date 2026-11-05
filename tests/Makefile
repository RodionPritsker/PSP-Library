CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2 -I../include

all: test_core

test_core: ../src/core/core.c ../src/core/storage.c ../src/core/navigation.c ../src/core/demo.c ../src/tracker/psp_tracker.c ../src/tracker/psp_fs.c test_core.c
	$(CC) $(CFLAGS) $^ -o $@

run: test_core
	./test_core

clean:
	$(RM) test_core test_storage.db test_storage.db.bak test_storage.db.tmp
