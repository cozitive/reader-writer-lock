CC=aarch64-linux-gnu-gcc
CFLAGS=-static
TARGETS=professor student
TESTS=

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) $(TESTS)