CC=aarch64-linux-gnu-gcc
CFLAGS=-static
TARGETS=rotd professor student
OBJECTS=wrappers.o
TESTS=

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS) $(OBJECTS) $(TESTS)