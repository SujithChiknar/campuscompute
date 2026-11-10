CC = gcc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -g -pthread -MMD -MP
LDLIBS += -pthread -lrt
objects := $(patsubst %.c,%.o,$(wildcard src/*.c))

.PHONY: all clean test rebuild
all: campuscompute
campuscompute: $(objects)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
-include $(objects:.o=.d)
clean:
	rm -f campuscompute $(objects) $(objects:.o=.d)
test: campuscompute
	bash tests/test_all.sh
rebuild: clean
	$(MAKE) all
