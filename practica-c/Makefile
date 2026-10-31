CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -g -fsanitize=address,undefined
LDFLAGS = -fsanitize=address,undefined

SRCS  = $(wildcard [0-9][0-9]-*.c)
BINS  = $(SRCS:.c=)

.PHONY: all clean diag
all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

diag: 00-diagnostico
	@./00-diagnostico

clean:
	rm -f $(BINS)

test: $(BINS)
	@for b in $(BINS); do echo "=== $$b ==="; ./$$b || true; done
