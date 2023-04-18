CC=aarch64-linux-gnu-gcc
CFLAGS=-static -g
TARGETS=rotd professor student
OBJECTS=wrappers.o

all: $(TARGETS)

rotd: rotd.c
	$(CC) $(CFLAGS) -o $@ $<

wrappers.o: wrappers.c
	$(CC) $(CFLAGS) -c -o $@ $<

professor: professor.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

student: student.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) $(OBJECTS)